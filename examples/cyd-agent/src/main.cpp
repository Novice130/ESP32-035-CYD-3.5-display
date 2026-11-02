// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    CYD Agent - a cute desk pet: tap it for your computer's CPU, RAM and top apps,
//          and optionally give it an LLM brain
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch, RGB LED, LDR, Speaker (GPIO 26)
//
// The faces render at 240x160 on core 1, pushed 2x. Requests to tools/agent_proxy.py
// run in their own task on core 0, so a slow network never freezes the animation.
// The API key stays on the computer: this firmware only knows a LAN token.
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <XPT2046_Touchscreen.h>
#if __has_include("config.h")
#include "config.h"   // WIFI_SSID / WIFI_PASSWORD / AGENT_HOST / AGENT_PORT / AGENT_TOKEN (git-ignored)
#endif
#include "game.h"

// --- PIN DEFINITIONS FOR CYD 3.5" (ESP32-035) ---
#define CYD_BACKLIGHT   27   // Backlight is GPIO 27 (Active HIGH)
#define CYD_LED_RED     4    // Active LOW (0 = ON, 1 = OFF)
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_SPEAKER_PIN 26   // Audio Amplifier
#define CYD_LDR_PIN     34   // Light sensor (ADC1, works with Wi-Fi on)

// Touch Screen SPI (Shared HSPI bus: CLK=14, MISO=12, MOSI=13, CS=33)
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

// This board's touch glass is cracked: the crack shorts the panel, so even with nobody touching
// it reads "pressed" at one fixed spot with Z pegged at ~4095. A real press still pulls raw X well
// below that spot (idle 1697..1830, pressed < 1680), but where you pressed can't be recovered.
// So a press = X dropping SHORT_MARGIN below the learned idle level, and the whole screen is one
// button. Same method as the Mario touch fix. A healthy panel (Z below the cap) works as normal too.
#define TOUCH_Z_MIN       500
#define TOUCH_Z_SHORT     3900 // Z at/above this = the crack short is what's being read
#define SHORT_MARGIN      150  // raw X drop that counts as a press (85 let idle noise through)
#define PRESS_SAMPLES     2    // readings in a row below the margin before a press starts
#define TAP_DEBOUNCE_MS   90   // release must last this long to count (bridges noisy samples)
#define DOUBLE_TAP_MS     300  // window for the second tap

// Backup button for when the glass stops reading fingers: cover the LDR quickly = 1 tap, twice = 2 taps
#define LDR_COVER_RISE    60   // at 0 dB: room 0, fingertip over it 60-500 (measured for cloud-game)
#define LDR_RELEASE_MS    150  // the covered reading flickers: released = low this long
#define LDR_TAP_MS        1500 // covered longer than this = the room got dark, not a tap
#define LDR_DOUBLE_MS     700

#define LDR_DARK_RAW      200  // at 0 dB this board reads ~0 in a lit room and rises as it gets dark

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);

static uint16_t* prevFb = nullptr;   // last frame sent to the panel (for diffing)
static uint16_t lineBuf[FB_W * 2];   // one 2x-wide output row, byte-swapped

// -----------------------------------------------------------------------------
// LED & AUDIO (platform hooks called by game.h)
// -----------------------------------------------------------------------------
void setRGB(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

unsigned long ledOffTime = 0;
bool ledActive = false;

void plat_led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) {
    setRGB(r > 110, g > 110, b > 110);
    ledOffTime = millis() + ms;
    ledActive = true;
}

void updateLED() {
    if (ledActive && millis() > ledOffTime) {
        setRGB(false, false, false);
        ledActive = false;
    }
}

// tone() on this core queues notes into a background task, so a whole jingle
// can be enqueued at once without blocking. busyUntil keeps small effects from
// piling up behind a melody.
unsigned long audioBusyUntil = 0;

struct Note { uint16_t hz, ms; };

void playNotes(const Note* n, int count, bool important) {
    unsigned long now = millis();
    if (!important && now < audioBusyUntil) return;
    unsigned long total = 0;
    for (int i = 0; i < count; i++) {
        tone(CYD_SPEAKER_PIN, n[i].hz, n[i].ms);
        total += n[i].ms;
    }
    audioBusyUntil = (now > audioBusyUntil ? now : audioBusyUntil) + total;
}

#define PLAY(important, ...) do { static const Note _n[] = {__VA_ARGS__}; \
    playNotes(_n, sizeof(_n) / sizeof(_n[0]), important); } while (0)

void plat_sfx(uint8_t id) {
    switch (id) {
        case SFX_TAP:      PLAY(false, {1500, 12}); break;
        case SFX_CHIRP:    PLAY(false, {1319, 40}, {1760, 60}); break;
        case SFX_HAPPY:    PLAY(true, {1047, 70}, {1319, 70}, {1568, 130}); break;
        case SFX_LOVE:     PLAY(true, {1319, 110}, {1047, 110}, {1568, 220}); break;
        case SFX_SAD:      PLAY(true, {659, 140}, {587, 140}, {494, 260}); break;
        case SFX_SURPRISE: PLAY(true, {880, 40}, {1760, 110}); break;
        case SFX_SLEEPY:   PLAY(true, {523, 170}, {440, 170}, {349, 320}); break;
        case SFX_SWITCH:   PLAY(true, {784, 60}, {1175, 120}); break;
    }
}

// -----------------------------------------------------------------------------
// AGENT TASK: one HTTP request at a time on core 0
// -----------------------------------------------------------------------------
#ifdef WIFI_SSID
#define HAS_AGENT 1
#else
#define HAS_AGENT 0
#endif

enum AskState : uint8_t { ASK_IDLE, ASK_PENDING, ASK_DONE, ASK_FAILED };
static volatile AskState askState = ASK_IDLE;
static char askKind[16], askSession[24], askBody[128];
static char replyBuf[768];
static TaskHandle_t agentTaskHandle = nullptr;

// Body travels as plain "key=value" lines; no JSON library needed on the board.
bool plat_ask(const char* kind, const char* session) {
#if HAS_AGENT
    if (askState != ASK_IDLE || WiFi.status() != WL_CONNECTED) return false;
    snprintf(askKind, sizeof(askKind), "%s", kind);
    snprintf(askSession, sizeof(askSession), "%s", session);
    int raw = analogRead(CYD_LDR_PIN);
    snprintf(askBody, sizeof(askBody), "kind=%s\nsession=%s\nlight=%d\npets=%d\nfriend=%d\nplay=%d\nuptime=%lu\n",
             askKind, askSession, raw >= LDR_DARK_RAW ? 0 : 100 - raw * 100 / LDR_DARK_RAW, pets, (int)pose.who,
             playMode ? 1 : 0, millis() / 1000);
    askState = ASK_PENDING;
    xTaskNotifyGive(agentTaskHandle);
    return true;
#else
    return false;
#endif
}

bool plat_stats() {
#if HAS_AGENT
    if (askState != ASK_IDLE || WiFi.status() != WL_CONNECTED) return false;
    strcpy(askKind, "stats");
    askState = ASK_PENDING;
    xTaskNotifyGive(agentTaskHandle);
    return true;
#else
    return false;
#endif
}

#if HAS_AGENT
static bool agentRequest() {
    WiFiClient c;
    c.setTimeout(25);
    if (!c.connect(AGENT_HOST, AGENT_PORT, 3000)) return false;
    if (!strcmp(askKind, "stats")) {
        c.printf("GET /stats HTTP/1.1\r\nHost: %s\r\nX-Agent-Token: %s\r\nConnection: close\r\n\r\n",
                 AGENT_HOST, AGENT_TOKEN);
    } else {
        c.printf("POST /event HTTP/1.1\r\nHost: %s\r\nX-Agent-Token: %s\r\nContent-Type: text/plain\r\n"
                 "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
                 AGENT_HOST, AGENT_TOKEN, (int)strlen(askBody), askBody);
    }

    // status line, then skip headers
    String status = c.readStringUntil('\n');
    if (!status.startsWith("HTTP/1.1 200") && !status.startsWith("HTTP/1.0 200")) {
        Serial.printf("[Agent] %s\n", status.c_str());
        c.stop();
        return false;
    }
    while (c.connected() || c.available()) {
        String h = c.readStringUntil('\n');
        if (h.length() <= 1) break;
    }
    size_t n = 0;
    unsigned long start = millis();
    while ((c.connected() || c.available()) && n < sizeof(replyBuf) - 1 && millis() - start < 25000) {
        int got = c.read((uint8_t*)replyBuf + n, sizeof(replyBuf) - 1 - n);
        if (got > 0) n += got;
        else delay(5);
    }
    replyBuf[n] = 0;
    c.stop();
    return true;
}

static void agentTask(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        unsigned long t0 = millis();
        bool ok = agentRequest();
        if (!ok || strcmp(askKind, "stats"))   // stats arrive every 2 s: only log the failures
            Serial.printf("[Agent] %s %s in %lu ms, heap %u, largest %u\n", askKind, ok ? "ok" : "FAILED",
                          millis() - t0, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
        askState = ok ? ASK_DONE : ASK_FAILED;
    }
}
#endif

static void pollAgent() {
    if (askState == ASK_DONE) {
        // copy out first: the reply may start the next step of the agent loop
        static char reply[sizeof(replyBuf)];
        memcpy(reply, replyBuf, sizeof(reply));
        bool isStats = !strcmp(askKind, "stats");
        askState = ASK_IDLE;
        if (isStats) game_apply_stats(reply);
        else game_apply_reply(reply);
    } else if (askState == ASK_FAILED) {
        bool isStats = !strcmp(askKind, "stats");
        askState = ASK_IDLE;
        if (isStats) game_stats_failed();
        else game_reply_failed();
    }
}

// 0 = dark .. 100 = bright, smoothed. Sampled a few times a second: enough for room lights.
static int readLight() {
    static float smooth = -1;
    static unsigned long nextAt = 0;
    if (millis() >= nextAt) {
        nextAt = millis() + 200;
        int raw = analogRead(CYD_LDR_PIN);
        float v = raw >= LDR_DARK_RAW ? 0 : 100.f - raw * 100.f / LDR_DARK_RAW;
        smooth = smooth < 0 ? v : smooth + (v - smooth) * 0.3f;
    }
    return (int)smooth;
}

// -----------------------------------------------------------------------------
// TOUCH: single vs double tap detector
// -----------------------------------------------------------------------------
enum TouchState { TS_IDLE, TS_DOWN, TS_WAIT_SECOND, TS_DOWN_SECOND };
TouchState touchState = TS_IDLE;
unsigned long lastSeenDown = 0, releasedAt = 0;
Tap tapQueue = TAP_NONE;

int shortX = -1;  // raw X the crack short reads when nobody touches (learned, -1 = not seen yet)

bool touchDown(bool pressedNow) {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    if (p.z < TOUCH_Z_MIN) return false;
    if (p.z < TOUCH_Z_SHORT) return true;  // normal press on an undamaged part of the panel
    if (shortX < 0) {
        shortX = p.x;
        Serial.printf("[Touch] crack short detected at raw x=%d, using X-drop press detection\n", p.x);
        return false;
    }
    static int below = 0;
    bool down = p.x < shortX - SHORT_MARGIN;
    below = down ? below + 1 : 0;
    if (!down && !pressedNow) shortX += (p.x - shortX) / 32;  // follow slow drift of the idle level
    return pressedNow ? down : below >= PRESS_SAMPLES;        // one stray low sample can't start a press
}

// ----- light sensor as a backup button
int ldrBase = -1;           // room darkness level (bigger = darker)
bool ldrCovered = false, ldrTapPending = false;
unsigned long ldrCoveredAt = 0, ldrBaseAt = 0, ldrLastDark = 0, ldrReleasedAt = 0;

void pollLdr() {
    unsigned long now = millis();
    int v = analogRead(CYD_LDR_PIN);
    if (ldrBase < 0) ldrBase = v;
    if (!ldrCovered) {
        if (v - ldrBase >= LDR_COVER_RISE) {
            ldrCovered = true;
            ldrCoveredAt = ldrLastDark = now;
            Serial.printf("[LDR] covered %d (room %d)\n", v, ldrBase);
        } else if (now - ldrBaseAt >= 100) {  // follow the room light slowly (~1.5 s)
            ldrBaseAt = now;
            ldrBase += (v - ldrBase) / 16;
        }
    } else if (v - ldrBase >= LDR_COVER_RISE / 2) {
        ldrLastDark = now;
        if (now - ldrCoveredAt > 10000) {  // dark for 10 s: the room got darker, not a finger
            ldrCovered = false;
            ldrBase = v;
        }
    } else if (now - ldrLastDark >= LDR_RELEASE_MS) {
        ldrCovered = false;
        if (now - ldrCoveredAt < LDR_TAP_MS) {
            if (ldrTapPending && now - ldrReleasedAt < LDR_DOUBLE_MS) {
                ldrTapPending = false;
                tapQueue = TAP_DOUBLE;
                Serial.println("[LDR] double tap");
            } else {
                ldrTapPending = true;
            }
            ldrReleasedAt = now;
        }
    }
    if (ldrTapPending && now - ldrReleasedAt >= LDR_DOUBLE_MS) {
        ldrTapPending = false;
        if (tapQueue == TAP_NONE) tapQueue = TAP_SINGLE;
        Serial.println("[LDR] single tap");
    }
}

void pollTouch() {
    pollLdr();
    unsigned long now = millis();
    bool down = touchDown(touchState == TS_DOWN || touchState == TS_DOWN_SECOND);
    if (down) lastSeenDown = now;
    bool released = !down && now - lastSeenDown > TAP_DEBOUNCE_MS;

    switch (touchState) {
        case TS_IDLE:
            if (down) touchState = TS_DOWN;
            break;
        case TS_DOWN:
            if (released) { touchState = TS_WAIT_SECOND; releasedAt = now; }
            break;
        case TS_WAIT_SECOND:
            if (down) {
                touchState = TS_DOWN_SECOND;
                tapQueue = TAP_DOUBLE;
                Serial.println("[Touch] DOUBLE tap");
            } else if (now - releasedAt > DOUBLE_TAP_MS) {
                touchState = TS_IDLE;
                if (tapQueue == TAP_NONE) tapQueue = TAP_SINGLE;
                Serial.println("[Touch] single tap");
            }
            break;
        case TS_DOWN_SECOND:
            if (released) touchState = TS_IDLE;
            break;
    }
}

Tap takeTap() {
    Tap t = tapQueue;
    tapQueue = TAP_NONE;
    return t;
}

// -----------------------------------------------------------------------------
// FRAME PUSH: diff against previous frame, send changed rectangles at 2x
// -----------------------------------------------------------------------------
static inline uint16_t swap16(uint16_t c) { return (c >> 8) | (c << 8); }

void pushRect(int x0, int x1, int y0, int y1) {
    int w = x1 - x0 + 1;
    tft.setAddrWindow(x0 * 2, y0 * 2, w * 2, (y1 - y0 + 1) * 2);
    for (int y = y0; y <= y1; y++) {
        const uint16_t* src = fb + y * FB_W + x0;
        for (int x = 0; x < w; x++) {
            uint16_t c = swap16(src[x]);
            lineBuf[x * 2] = c;
            lineBuf[x * 2 + 1] = c;
        }
        tft.pushPixels(lineBuf, w * 2);  // each source row becomes two panel rows
        tft.pushPixels(lineBuf, w * 2);
    }
}

void pushFrame() {
    tft.startWrite();
    if (!prevFb) {
        pushRect(0, FB_W - 1, 0, FB_H - 1);
        tft.endWrite();
        return;
    }
    int y = 0;
    while (y < FB_H) {
        // find the next run of changed rows and the union of their dirty columns
        int runStart = -1, xmin = FB_W, xmax = -1;
        for (; y < FB_H; y++) {
            const uint32_t* a = (const uint32_t*)(fb + y * FB_W);
            const uint32_t* b = (const uint32_t*)(prevFb + y * FB_W);
            int first = -1, last = -1;
            for (int i = 0; i < FB_W / 2; i++) {
                if (a[i] != b[i]) { if (first < 0) first = i; last = i; }
            }
            if (first < 0) {
                if (runStart >= 0) break;
                continue;
            }
            if (runStart < 0) runStart = y;
            if (first * 2 < xmin) xmin = first * 2;
            if (last * 2 + 1 > xmax) xmax = last * 2 + 1;
        }
        if (runStart >= 0) pushRect(xmin, xmax, runStart, y - 1);
    }
    tft.endWrite();
    memcpy(prevFb, fb, FB_W * FB_H * 2);
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Starting CYD Agent (240x160 @ 2x)...");

    // 1. Backlight on GPIO 27 (Active HIGH)
    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);

    // 2. RGB LED (Active LOW)
    pinMode(CYD_LED_RED,   OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE,  OUTPUT);
    setRGB(false, false, false);

    // 3. Audio
    pinMode(CYD_SPEAKER_PIN, OUTPUT);
    digitalWrite(CYD_SPEAKER_PIN, LOW);

    // 4. Touch CS setup and light sensor (0 dB: most sensitive, see LDR_DARK_RAW)
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);
    analogSetPinAttenuation(CYD_LDR_PIN, ADC_0db);

    // 5. TFT Display Initialisation (Standard Working Landscape Rotation 1)
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(false);  // pushRect pre-swaps
    tft.fillScreen(TFT_BLACK);

    // 6. Touch Controller Initialisation
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    // 7. Framebuffers (2 x 75 KB) + the background cache (75 KB, re-dithers every frame without it)
    fb = (uint16_t*)malloc(FB_W * FB_H * 2);
    prevFb = (uint16_t*)malloc(FB_W * FB_H * 2);
    if (!fb) {
        Serial.println("[ESP32-035] FATAL: framebuffer alloc failed");
        while (true) delay(1000);
    }
    if (!prevFb) Serial.println("[ESP32-035] No memory for diff buffer, pushing full frames");
    else memset(prevFb, 0, FB_W * FB_H * 2);  // matches the black screen
    bgCache = (uint16_t*)malloc(FB_W * FB_H * 2);
    Serial.printf("[ESP32-035] Heap free after buffers: %u bytes\n", ESP.getFreeHeap());

    game_init(esp_random() | 1);

#if HAS_AGENT
    // 8. Wi-Fi joins in the background: the pet is alive from the first frame
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    xTaskCreatePinnedToCore(agentTask, "agent", 6144, nullptr, 1, &agentTaskHandle, 0);
#else
    Serial.println("[ESP32-035] No config.h: offline reactions only");
#endif
}

// -----------------------------------------------------------------------------
// MAIN LOOP: fixed 30 Hz game ticks, render when something ticked
// -----------------------------------------------------------------------------
void loop() {
    static unsigned long nextTick = millis();
    static unsigned long fpsT = millis();
    static int frames = 0;

    // serial 's' / 'd' act like a single / double tap, for testing without touching the screen
    if (Serial.available()) {
        int ch = Serial.read();
        if (ch == 's') tapQueue = TAP_SINGLE;
        else if (ch == 'd') tapQueue = TAP_DOUBLE;
    }

    pollTouch();
    updateLED();
    pollAgent();

    unsigned long now = millis();
    int ticked = 0;
    while ((long)(now - nextTick) >= 0 && ticked < 3) {
        game_tick(takeTap(), readLight());
        nextTick += 1000 / TICK_HZ;
        ticked++;
    }
    if ((long)(now - nextTick) > 250) nextTick = now;  // don't spiral after a stall

    if (ticked) {
        game_render();
        pollTouch();
        pushFrame();
        frames++;
    }

    if (now - fpsT >= 5000) {
        Serial.printf("[ESP32-035] %.1f fps, mode=%s, view=%d, mood=%s, wifi=%d, cloud=%d, light=%d, ldr=%d, heap=%u/%u\n",
                      frames * 1000.0f / (now - fpsT), playMode ? "PLAY" : "DEMO", (int)view, MOOD_NAMES[mood],
                      WiFi.status() == WL_CONNECTED, cloudOnline, readLight(), analogRead(CYD_LDR_PIN),
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
        frames = 0;
        fpsT = now;
    }
}
