// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    Cloud-game thin client - the game runs on another machine (a Sipeed NanoKVM),
//          this board only draws its frames and sends back one button
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch (read through TFT_eSPI), LDR (GPIO 34), Speaker, RGB LED
//
// The server (Sipeed_Examples/cyd-cloud-game, e.g. flappy_server) renders 240x160 palette-indexed
// frames and sends them run-length coded. Every loop this board asks for the next frame, sending
// the button state with the request; it decodes the frame into an 8-bit framebuffer, noting which
// pixels changed, and pushes only those rows (2x scaled, DMA) to the panel. Sound effects and LED
// flashes are played here when the frame says so.
//
// Button: cover the light sensor, or tap anywhere on the screen.
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#if __has_include("config.h")
#include "config.h"   // WIFI_SSID / WIFI_PASSWORD / GAME_HOST / GAME_PORT (git-ignored)
#endif
#ifndef WIFI_SSID
#error "Copy include/config.example.h to include/config.h and fill in your Wi-Fi and game server"
#endif
#ifndef GAME_HOST
#define GAME_HOST "192.168.0.222"
#endif
#ifndef GAME_PORT
#define GAME_PORT 8766
#endif

// --- PIN DEFINITIONS FOR CYD 3.5" (ESP32-035) ---
#define CYD_BACKLIGHT   27   // Backlight is GPIO 27 (Active HIGH)
#define CYD_LED_RED     4    // Active LOW (0 = ON, 1 = OFF)
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_SPEAKER_PIN 26
#define CYD_LDR_PIN     34   // Light sensor (ADC1): cover it = button

// Touch shares the display's HSPI bus (CS=33, TOUCH_CS in platformio.ini). It's read through
// TFT_eSPI, which arbitrates the bus with the display's DMA. A damaged panel reads ~4100 all the
// time, so readings above TOUCH_Z_MAX are ignored rather than counted as a press.
#define XPT2046_CS       33
#define TOUCH_Z_MIN      350
#define TOUCH_Z_MAX      3900
#define TOUCH_RELEASE_MS 60    // a touch has ended once it has been off this long

// Light sensor, same detector as mario-arcade / video-clip, read on the ADC's 0 dB range (0-1.1 V):
// at the default 11 dB range a covered sensor sits at the bottom of the scale and flickers to 0.
#define LDR_DARK_IS_LOW false
#define LDR_COVER_RISE  60     // measured at 0 dB on this board: room 0, fingertip over it 60-500
#define LDR_RELEASE_MS  150

#define MAX_W 240
#define MAX_H 160
#define FRAME_MIN_MS 30        // ask for at most ~33 frames a second (the game ticks at 30 Hz)
#define BAND_ROWS 4            // source rows per DMA push
#define SPAN_GAP 12            // changed stretches closer than this are pushed as one

enum Sfx { SFX_FLAP = 1, SFX_SCORE = 2, SFX_HIT = 4, SFX_DIE = 8, SFX_SWOOSH = 16 };

TFT_eSPI tft = TFT_eSPI();
WiFiClient game;

static int fw = 0, fh = 0, scale = 2, offX = 0, offY = 0;   // frame size, zoom, top-left on the panel
static uint16_t pal[256];              // big-endian RGB565, ready for DMA
static uint8_t* fb = nullptr;          // fw*fh palette indices, what's on the panel now
static uint8_t* rx = nullptr;          // one compressed frame
static uint16_t* strip[2];
static int stripSel = 0;
static uint8_t changed[MAX_H / BAND_ROWS][MAX_W / 8];   // per band: one bit per changed column

// -----------------------------------------------------------------------------
// INPUT: touch + light sensor -> a held flag and a running press count
// -----------------------------------------------------------------------------
static uint16_t presses = 0;
static bool touchHeld = false;
static unsigned long touchSeenAt = 0;
int ldrBase = -1;
bool ldrCovered = false;
unsigned long ldrCoveredAt = 0, ldrBaseAt = 0, ldrLastDark = 0;

static void pollTouch() {
    unsigned long now = millis();
    uint16_t z = tft.getTouchRawZ();
    bool down = z >= TOUCH_Z_MIN && z < TOUCH_Z_MAX;
    if (down) {
        if (!touchHeld) presses++;
        touchHeld = true;
        touchSeenAt = now;
    } else if (touchHeld && now - touchSeenAt > TOUCH_RELEASE_MS) {
        touchHeld = false;
    }
}

static void pollLdr() {
    unsigned long now = millis();
    int v = analogRead(CYD_LDR_PIN);
    if (LDR_DARK_IS_LOW) v = 4095 - v;
    if (ldrBase < 0) ldrBase = v;
    if (!ldrCovered) {
        if (v - ldrBase >= LDR_COVER_RISE) {
            ldrCovered = true;
            ldrCoveredAt = ldrLastDark = now;
            presses++;
        } else if (now - ldrBaseAt >= 100) {   // follow the room light slowly (~1.5 s)
            ldrBaseAt = now;
            ldrBase += (v - ldrBase) / 16;
        }
    } else if (v - ldrBase >= LDR_COVER_RISE / 2) {
        ldrLastDark = now;
        if (now - ldrCoveredAt > 10000) {   // dark for 10 s: the room got darker, not a finger
            ldrCovered = false;
            ldrBase = v;
        }
    } else if (now - ldrLastDark >= LDR_RELEASE_MS) {
        ldrCovered = false;
    }
}

// -----------------------------------------------------------------------------
// SOUND + LED (tone() queues notes in the background, so this never blocks)
// -----------------------------------------------------------------------------
static unsigned long ledOffAt = 0;

static void led(bool r, bool g, bool b, unsigned ms) {
    digitalWrite(CYD_LED_RED, !r);
    digitalWrite(CYD_LED_GREEN, !g);
    digitalWrite(CYD_LED_BLUE, !b);
    ledOffAt = millis() + ms;
}

static void playSfx(uint32_t s) {
    if (s & SFX_HIT) {
        tone(CYD_SPEAKER_PIN, 196, 90);
        tone(CYD_SPEAKER_PIN, 147, 140);
        led(true, false, false, 400);
    } else if (s & SFX_SCORE) {
        tone(CYD_SPEAKER_PIN, 1568, 50);
        tone(CYD_SPEAKER_PIN, 2093, 90);
        led(false, true, false, 120);
    } else if (s & SFX_FLAP) {
        tone(CYD_SPEAKER_PIN, 988, 18);
        tone(CYD_SPEAKER_PIN, 1319, 22);
    }
    if (s & SFX_DIE) {
        tone(CYD_SPEAKER_PIN, 523, 70);
        tone(CYD_SPEAKER_PIN, 392, 70);
        tone(CYD_SPEAKER_PIN, 262, 160);
    }
    if (s & SFX_SWOOSH) {
        tone(CYD_SPEAKER_PIN, 660, 25);
        tone(CYD_SPEAKER_PIN, 880, 25);
    }
}

// -----------------------------------------------------------------------------
// NETWORK
// -----------------------------------------------------------------------------
// chunked reads: readBytes() fetches one byte at a time, which costs ~20 ms per frame
static bool readAll(void* dst, size_t n) {
    uint8_t* p = (uint8_t*)dst;
    unsigned long last = millis();
    while (n) {
        int r = game.available() ? game.read(p, n) : 0;
        if (r > 0) { p += r; n -= r; last = millis(); continue; }
        if (!game.connected() || millis() - last > 3000) return false;
        delay(1);
    }
    return true;
}

static void status(const char* line1, const char* line2 = "") {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(line1, 240, 145, 4);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(line2, 240, 185, 2);
}

static void joinWifi() {
    status("Joining Wi-Fi", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);   // modem sleep adds 100+ ms to every frame
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        for (int i = 0; i < 150 && WiFi.status() != WL_CONNECTED; i++) delay(100);
        if (WiFi.status() == WL_CONNECTED) break;
        Serial.printf("[WiFi] can't join \"%s\". 2.4 GHz networks in range:\n", WIFI_SSID);
        WiFi.disconnect();
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) Serial.printf("[WiFi]   \"%s\"  %d dBm\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    Serial.printf("[WiFi] joined \"%s\" as %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
}

// connect + handshake: frame size and palette
static bool connectGame() {
    char addr[48];
    snprintf(addr, sizeof(addr), "%s:%d", GAME_HOST, GAME_PORT);
    status("Connecting to the game", addr);
    if (!game.connect(GAME_HOST, GAME_PORT, 3000)) return false;
    game.setNoDelay(true);
    game.setTimeout(3);   // seconds
    char magic[4];
    uint16_t v[3];
    if (game.write((const uint8_t*)"HI1", 4) != 4 || !readAll(magic, 4) || memcmp(magic, "CGM1", 4) ||
        !readAll(v, 6) || !v[0] || !v[1] || v[0] > MAX_W || v[1] > MAX_H || !v[2] || v[2] > 256) {
        game.stop();
        return false;
    }
    for (int i = 0; i < v[2]; i++) {
        uint16_t c;
        if (!readAll(&c, 2)) { game.stop(); return false; }
        pal[i] = (uint16_t)(c >> 8 | c << 8);
    }
    fw = v[0];
    fh = v[1];
    scale = min(480 / fw, 320 / fh);
    offX = (480 - fw * scale) / 2;
    offY = (320 - fh * scale) / 2;
    memset(fb, 0xFF, MAX_W * MAX_H);   // nothing matches: the first frame is drawn in full
    tft.fillScreen(TFT_BLACK);
    Serial.printf("[Game] %s: %dx%d x%d, %d colours\n", addr, fw, fh, scale, v[2]);
    return true;
}

// -----------------------------------------------------------------------------
// FRAMES: decode the run-length data into fb, then push the rows that changed
// -----------------------------------------------------------------------------
static inline void put(int pos, uint8_t v) {
    if (fb[pos] == v) return;
    fb[pos] = v;
    int y = pos / fw, x = pos - y * fw;
    changed[y / BAND_ROWS][x >> 3] |= 1 << (x & 7);
}

static bool decode(const uint8_t* p, size_t len) {
    const uint8_t* end = p + len;
    const int total = fw * fh;
    int pos = 0;
    memset(changed, 0, sizeof(changed));
    while (p < end && pos < total) {
        uint8_t b = *p++;
        if (b < 128) {
            if (p >= end) return false;
            uint8_t v = *p++;
            for (int k = 0; k <= b && pos < total; k++) put(pos++, v);
        } else {
            for (int k = 0; k < b - 127 && p < end && pos < total; k++) put(pos++, *p++);
        }
    }
    return pos == total;
}

// push one stretch [lo, hi] of a band, scaled up
static void pushSpan(int y, int n, int lo, int hi) {
    const int w = hi - lo + 1, ow = w * scale;
    uint16_t* out = strip[stripSel];
    stripSel ^= 1;
    for (int k = 0; k < n; k++) {
        const uint8_t* src = fb + (y + k) * fw + lo;
        uint16_t* row = out + k * scale * ow;
        for (int x = 0; x < w; x++)
            for (int s = 0; s < scale; s++) row[x * scale + s] = pal[src[x]];
        for (int s = 1; s < scale; s++) memcpy(row + s * ow, row, ow * 2);
    }
    tft.pushImageDMA(offX + lo * scale, offY + y * scale, ow, n * scale, out);
}

// Each band is pushed as separate stretches of changed columns (a moving pipe only changes near
// its edges and stripes), merging stretches less than SPAN_GAP apart. Returns pixels pushed.
static int pushChanged() {
    int pixels = 0;
    tft.startWrite();
    for (int y = 0; y < fh; y += BAND_ROWS) {
        const uint8_t* bits = changed[y / BAND_ROWS];
        const int n = min(BAND_ROWS, fh - y);
        int lo = -1, hi = -1;
        for (int x = 0; x <= fw; x++) {
            bool c = x < fw && (bits[x >> 3] >> (x & 7) & 1);
            if (c) {
                if (lo >= 0 && x - hi > SPAN_GAP) { pushSpan(y, n, lo, hi); pixels += (hi - lo + 1) * n; lo = -1; }
                if (lo < 0) lo = x;
                hi = x;
            }
        }
        if (lo >= 0) { pushSpan(y, n, lo, hi); pixels += (hi - lo + 1) * n; }
    }
    tft.dmaWait();
    tft.endWrite();   // frees the shared bus for the touch chip
    return pixels;
}

// -----------------------------------------------------------------------------
// SETUP / LOOP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Cloud-game client starting...");

    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);
    pinMode(CYD_LED_RED, OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE, OUTPUT);
    led(false, false, false, 0);
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);
    pinMode(CYD_LDR_PIN, INPUT);
    analogSetPinAttenuation(CYD_LDR_PIN, ADC_0db);

    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(false);   // the palette is stored big-endian already
    tft.fillScreen(TFT_BLACK);
    tft.initDMA();

    fb = (uint8_t*)malloc(MAX_W * MAX_H);
    rx = (uint8_t*)malloc(MAX_W * MAX_H + MAX_W * MAX_H / 128 + 16);   // worst case for the coding
    for (int k = 0; k < 2; k++) strip[k] = (uint16_t*)heap_caps_malloc(MAX_W * BAND_ROWS * 9 * 2, MALLOC_CAP_DMA);
    if (!fb || !rx || !strip[0] || !strip[1]) { status("Out of memory"); for (;;) delay(1000); }

    joinWifi();
    Serial.printf("[ESP32-035] Heap free: %u bytes (largest block %u)\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static bool inFlight = false;          // a frame request is out, its reply not read yet
static unsigned long lastAsk = 0;

static bool askFrame() {
    uint8_t req[4] = {'F', (uint8_t)(touchHeld || ldrCovered), (uint8_t)(presses & 0xFF), (uint8_t)(presses >> 8)};
    lastAsk = millis();
    inFlight = game.write(req, 4) == 4;
    return inFlight;
}

void loop() {
    static unsigned long statT = millis();
    static uint32_t frames = 0, bytes = 0, netUs = 0, drawUs = 0, pixelsPushed = 0;

    pollTouch();
    pollLdr();
    if (ledOffAt && millis() > ledOffAt) { led(false, false, false, 0); ledOffAt = 0; }

    if (!game.connected()) {
        if (WiFi.status() != WL_CONNECTED) joinWifi();
        inFlight = false;
        if (!connectGame()) {
            status("Game server not reachable", GAME_HOST " - retrying");
            Serial.printf("[Game] can't reach %s:%d, retrying\n", GAME_HOST, GAME_PORT);
            delay(2000);
        }
        return;
    }
    if (!inFlight) {   // ask for a frame, sending the button with it
        if (millis() - lastAsk < FRAME_MIN_MS) { delay(1); return; }
        if (!askFrame()) { game.stop(); return; }
    }
    uint32_t t0 = micros();
    uint32_t head[2];
    if (!readAll(head, 8) || head[1] > MAX_W * MAX_H * 2 || !readAll(rx, head[1])) {
        Serial.println("[Game] connection lost");
        game.stop();
        return;
    }
    inFlight = false;
    uint32_t t1 = micros();
    if (head[0]) playSfx(head[0]);
    // pipeline: ask for the next frame now, so Wi-Fi works while this one is drawn
    pollTouch();
    pollLdr();
    if (millis() - lastAsk >= FRAME_MIN_MS && !askFrame()) { game.stop(); return; }
    if (!decode(rx, head[1])) Serial.println("[Game] bad frame");
    pixelsPushed += pushChanged();
    netUs += t1 - t0;
    drawUs += micros() - t1;
    frames++;
    bytes += head[1];

    unsigned long now = millis();
    if (now - statT >= 5000 && frames) {
        Serial.printf("[ESP32-035] %.1f fps, %.1f KB/frame, net %.1f ms, draw %.1f ms (%u%% of pixels redrawn)\n",
                      frames * 1000.0f / (now - statT), bytes / 1024.0f / frames, netUs / 1000.0f / frames,
                      drawUs / 1000.0f / frames, (unsigned)(pixelsPushed * 100 / frames / (fw * fh)));
        frames = bytes = netUs = drawUs = pixelsPushed = 0;
        statT = now;
    }
}
