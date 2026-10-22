// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    Screen mirror - a computer streams its screen to this board, over USB or Wi-Fi
// Hardware: 3.5" 480x320 TFT (ST7796), RGB LED
//
// Three ways in, same frames:
//   USB:      the website (docs/ in this repo, Chrome or Edge) captures a screen, window or tab
//             and sends it over the USB cable with Web Serial. Nothing to configure.
//   Web+WiFi: the website can also save a Wi-Fi name and password on the board (over the cable,
//             once). The board then joins Wi-Fi as <name>.local and the website sends frames
//             to http://<name>.local/frame (Chrome asks once to allow local network access).
//   Mac:      tools/mirror.sh on a Mac connects over Wi-Fi to MIRROR_PORT.
// Either side sends only the rectangle that changed since the last frame, as a baseline JPEG
// (full 480x320, or 240x160 to be shown 2x). A task on core 0 receives it into one of two
// buffers and acks straight away, so the sender prepares the next frame while core 1 decodes
// this one with JPEGDEC, a few 16x16 blocks at a time, and pushes the blocks to the panel by DMA.
// ==============================================================================

#include <Arduino.h>
#include <JPEGDEC.h>   // before TFT_eSPI.h: that sets FS_NO_GLOBALS, which hides File
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <lwip/sockets.h>

// The web installer's build defines MIRROR_NO_CONFIG, so a local config.h (with a Wi-Fi
// password) can never end up in a published binary. Wi-Fi saved from the website wins over it.
#if __has_include("config.h") && !defined(MIRROR_NO_CONFIG)
#include "config.h"   // WIFI_SSID / WIFI_PASSWORD / MIRROR_HOSTNAME / MIRROR_PORT (git-ignored)
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""   // none built in: set it up from the website, or use USB only
#define WIFI_PASSWORD ""
#endif
#ifndef MIRROR_HOSTNAME
#define MIRROR_HOSTNAME "cyd"
#endif
#ifndef MIRROR_PORT
#define MIRROR_PORT 8767
#endif
#ifndef USB_BAUD
#define USB_BAUD 2000000   // the web page opens the port at the same speed
#endif
#define HTTP_PORT 80
#define MIRROR_WEB_URL "robo.learnnovice.com"

// --- PIN DEFINITIONS FOR CYD 3.5" (ESP32-035) ---
// Board-specific pins and panel size come from the PlatformIO env (TFT_BL, TOUCH_CS, TFT_WIDTH,
// TFT_HEIGHT), so one source builds for the 3.5" ESP32-035 / 3248S035 and the 2.8" 2432S028R.
#define CYD_BACKLIGHT   TFT_BL   // 27 on the 3.5", 21 on the 2.8" (active HIGH on both)
#define CYD_LED_RED     4    // Active LOW (0 = ON, 1 = OFF), same pins on every CYD
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define XPT2046_CS      TOUCH_CS   // 33: keep the touch chip deselected

// landscape (setRotation(1)): 480x320 on the 3.5", 320x240 on the 2.8"
#define PANEL_W (TFT_HEIGHT > TFT_WIDTH ? TFT_HEIGHT : TFT_WIDTH)
#define PANEL_H (TFT_HEIGHT > TFT_WIDTH ? TFT_WIDTH : TFT_HEIGHT)
#define CX (PANEL_W / 2)
#define Y(y) ((y) * PANEL_H / 320)       // layout was drawn for 320 rows: squeeze it on smaller panels
#define SMALL_PANEL (PANEL_W < 400)      // 320 wide: long lines get shorter wording
#define MAX_SCALE 2
#define MAX_FRAME_BYTES (40 * 1024)   // biggest JPEG per buffer; the sender lowers quality to fit
#define IDLE_TIMEOUT_MS 10000         // senders ping every 2 s while their screen is still
#define USB_RX_BUFFER (24 * 1024)     // > 2 MBd x the longest wait for a free buffer (16 KB overflowed)
// Memory is tight with Wi-Fi up: 2 x 40 KB frames + 2 x 16 KB DMA strips + this + the Wi-Fi stack.
// With 48 KB frames, Wi-Fi ran out ("Not enough memory to allocate buffer").
// The UART chip holds 128 bytes. The core's default moves them to USB_RX_BUFFER only once 112 have
// arrived; at 32 there's more headroom at 2 MBd. The UART interrupt runs on core 0 (on core 1 the
// JPEG decoding and display DMA made it lose bytes), but there the Wi-Fi driver holds it off too
// (~220 FIFO overflows in 8 s), so Wi-Fi is paused while frames come over USB (wifiPause) and
// restarted once USB has been quiet for
// WIFI_RESUME_MS (switching the radio on and off is itself disruptive, so not on every reconnect).
#define WIFI_RESUME_MS 15000
#define USB_RX_FIFO_FULL 32
#define WEB_SLOTS 5                   // browser connections served at once (Chrome opens up to 6)

static const uint32_t USB_HELLO = 'C' << 24 | 'Y' << 16 | 'M' << 8 | 'H';   // "CYMH" asks for the hello

// What the receive task (core 0) hands the drawing loop (core 1), in order: a filled buffer
// index, or an event. All drawing stays on core 1.
enum { EV_CONNECTED = 10, EV_WAITING = 11, EV_WIFI = 12, EV_COLOURS = 13 };

// Message header (little endian). Tags:
//   'J' frame: JPEG of len bytes drawn at x, y, zoomed by scale. len = 0 is a keep-alive ping.
//   'Q' the sender stopped.
//   'W' (USB) save Wi-Fi: len bytes = u8 n + name of the network, u8 n + password, u8 n + board name.
//   'U' (USB) about to stream: pauses Wi-Fi now (see wifiPause), acked once it's off.
//   'I' next colour fix (normal, inverted, red/blue swapped, both), saved on the board. For panels
//       whose lot differs from the build (inverted IPS panels, BGR panels).
//   'S' (USB) Wi-Fi status: the reply is "CYMs" + u8 state (0 not set up, 1 joining, 2 joined, 3 paused)
//       + u8 n + board name + u8 n + IP address. 'W' gets the same reply instead of 'K'.
struct __attribute__((packed)) FrameHeader {
    uint8_t tag;
    uint8_t scale;    // 1 or 2: each decoded pixel becomes scale x scale panel pixels
    uint16_t x, y;    // top-left on the panel
    uint16_t pad;
    uint32_t len;     // bytes that follow
};

enum Link { LINK_NONE, LINK_MAC, LINK_USB, LINK_WEB };
static const char* const LINK_NAME[] = {"-", "Mac over Wi-Fi", "web page over USB", "web page over Wi-Fi"};

TFT_eSPI tft = TFT_eSPI();
JPEGDEC jpeg;
WiFiServer server(MIRROR_PORT);       // tools/mirror.sh
WiFiServer web(HTTP_PORT);            // the website over Wi-Fi
WiFiClient mac, webClient[WEB_SLOTS];
Preferences prefs;
static volatile Link sender = LINK_NONE;   // who is sending; only the receive task changes it

// Wi-Fi settings: saved from the website (Preferences "mirror"), else config.h
static char wifiSsid[33], wifiPass[65], boardName[25];
// Colour fix chosen on the website ('I'): bit 0 = invert the panel, bit 1 = swap red and blue.
// Same CYD model numbers ship with different panel lots, so this fixes colours without a rebuild.
static volatile uint8_t colourMode = 0;
#ifdef TFT_INVERSION_ON
static const bool INVERTED_BY_DEFAULT = true;
#else
static const bool INVERTED_BY_DEFAULT = false;
#endif

// big-endian RGB565 (as JPEGDEC hands it over) with red and blue swapped
static inline uint16_t swapRB(uint16_t v) {
    uint16_t n = (uint16_t)((v >> 8) | (v << 8));
    n = (uint16_t)((n >> 11) | (n & 0x07E0) | (n << 11));
    return (uint16_t)((n >> 8) | (n << 8));
}
// a TFT_eSPI colour for text, swapped the same way when needed
static uint16_t C(uint16_t c) {
    return (colourMode & 2) ? (uint16_t)((c >> 11) | (c & 0x07E0) | (c << 11)) : c;
}
static void applyColours() { tft.invertDisplay(INVERTED_BY_DEFAULT ^ (colourMode & 1)); }
static volatile bool wifiRestart = false, wifiPause = false, wifiPaused = false;
static volatile unsigned long usbLastUsed = 0;
static bool netUp = false;   // Wi-Fi joined and mDNS running (receive task only)


// two JPEG buffers: one being drawn while the next frame arrives in the other
static uint8_t* jpgBuf[2];
static FrameHeader jpgHdr[2];
static QueueHandle_t freeQ, readyQ;   // buffer indices free to fill / items for the drawing loop
static volatile uint32_t netUs = 0;   // time spent receiving frames, for the stats
static volatile uint32_t usbLost = 0, usbFull = 0;   // USB receiver overflows: chip FIFO / ring buffer

static uint16_t* strip[2];            // scaled pixel blocks, ping-pong for DMA
static int stripSel = 0;
static int drawX, drawY, drawScale;   // where the current JPEG goes

// While the web page is streaming over USB, the serial line carries its acks: no log lines then.
#define LOG(...) do { if (sender != LINK_USB) Serial.printf(__VA_ARGS__); } while (0)

static void led(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED, !r);
    digitalWrite(CYD_LED_GREEN, !g);
    digitalWrite(CYD_LED_BLUE, !b);
}

static void pauseWifi() {
    wifiPause = false;
    if (wifiPaused || !wifiSsid[0]) return;
    wifiPaused = true;
    MDNS.end();
    WiFi.disconnect(true);
    netUp = false;
    LOG("[WiFi] paused while the USB cable is in use\n");
}

// 0 not set up, 1 joining, 2 joined, 3 set up but switched off while the cable is in use
static uint8_t wifiState() { return !wifiSsid[0] ? 0 : wifiPaused ? 3 : WiFi.status() == WL_CONNECTED ? 2 : 1; }

// -----------------------------------------------------------------------------
// SCREENS
// -----------------------------------------------------------------------------
static void status(const char* line1, const char* line2 = "") {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C(TFT_WHITE), TFT_BLACK);
    tft.drawString(line1, CX, Y(145), 4);
    tft.setTextColor(C(TFT_DARKGREY), TFT_BLACK);
    tft.drawString(line2, CX, Y(185), 2);
}

static void showWaiting() {
    char line[96];
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C(TFT_WHITE), TFT_BLACK);
    tft.drawString("Screen mirror", CX, Y(50), 4);
    tft.setTextColor(C(TFT_YELLOW), TFT_BLACK);
    tft.drawString("Open this website in Chrome or Edge:", CX, Y(100), 2);
    tft.setTextColor(C(TFT_CYAN), TFT_BLACK);
    tft.drawString(MIRROR_WEB_URL, CX, Y(124), 2);
    tft.setTextColor(C(TFT_YELLOW), TFT_BLACK);
    tft.drawString(SMALL_PANEL ? "then click  Share my screen." : "connect the board, then click  Share my screen.", CX, Y(148), 2);

    switch (wifiState()) {
    case 2:
        tft.setTextColor(C(TFT_GREEN), TFT_BLACK);
        tft.drawString("Wi-Fi is ready. Board name:", CX, Y(200), 2);
        snprintf(line, sizeof(line), "%s.local", boardName);
        tft.drawString(line, CX, Y(228), 4);
        tft.setTextColor(C(TFT_DARKGREY), TFT_BLACK);
        snprintf(line, sizeof(line), "or address %s", WiFi.localIP().toString().c_str());
        tft.drawString(line, CX, Y(260), 2);
        break;
    case 1:
        tft.setTextColor(C(TFT_DARKGREY), TFT_BLACK);
        snprintf(line, sizeof(line), SMALL_PANEL ? "Joining \"%s\"..." : "Wi-Fi: joining \"%s\"...", wifiSsid);
        tft.drawString(line, CX, Y(210), 2);
        tft.drawString("(it must be a 2.4 GHz network)", CX, Y(232), 2);
        break;
    default:
        tft.setTextColor(C(TFT_DARKGREY), TFT_BLACK);
        tft.drawString("Using the USB cable.", CX, Y(210), 2);
        tft.drawString(SMALL_PANEL ? "Set up Wi-Fi on the website." : "Set up Wi-Fi on the website to use it without the cable.",
                       CX, Y(232), 2);
    }
}

// -----------------------------------------------------------------------------
// WI-FI SETTINGS
// -----------------------------------------------------------------------------
static void loadWifi() {
    prefs.begin("mirror", true);
    String s = prefs.getString("ssid", WIFI_SSID), p = prefs.getString("pass", WIFI_PASSWORD),
           n = prefs.getString("name", MIRROR_HOSTNAME);
    colourMode = prefs.getUChar("colours", 0) & 3;
    prefs.end();
    strlcpy(wifiSsid, s.c_str(), sizeof(wifiSsid));
    strlcpy(wifiPass, p.c_str(), sizeof(wifiPass));
    strlcpy(boardName, n.length() ? n.c_str() : "cyd", sizeof(boardName));
}

static void startWifi() {   // joins in the background; USB keeps working meanwhile
    WiFi.disconnect(true);
    if (!wifiSsid[0]) return;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);   // modem sleep adds 100+ ms to every frame
    WiFi.setHostname(boardName);
    WiFi.begin(wifiSsid, wifiPass);
}

// 'W' payload: three length-prefixed strings. The board name keeps only a-z, 0-9 and '-'.
static bool saveWifi(const uint8_t* p, size_t n) {
    char out[3][65];
    for (int i = 0; i < 3; i++) {
        if (!n) return false;
        size_t k = *p++;
        n--;
        if (k > n || k > 64) return false;
        memcpy(out[i], p, k);
        out[i][k] = 0;
        p += k;
        n -= k;
    }
    if (strlen(out[0]) > 32) return false;
    char name[25];
    size_t j = 0;
    for (const char* c = out[2]; *c && j < sizeof(name) - 1; c++) {
        char ch = tolower(*c);
        if (isalnum((unsigned char)ch) || ch == '-') name[j++] = ch;
    }
    name[j] = 0;
    prefs.begin("mirror", false);
    prefs.putString("ssid", out[0]);
    prefs.putString("pass", out[1]);
    prefs.putString("name", j ? name : "cyd");
    prefs.end();
    loadWifi();
    wifiRestart = true;
    return true;
}

// -----------------------------------------------------------------------------
// RECEIVING
// -----------------------------------------------------------------------------
// Chunked reads from the Wi-Fi client c, or from USB when c is null. Stream::readBytes() fetches
// one byte at a time, which takes ~0.1 s per 4 KB from a WiFiClient.
static bool readAll(Stream& s, void* dst, size_t n, WiFiClient* c = nullptr) {
    uint8_t* p = (uint8_t*)dst;
    unsigned long last = millis();
    while (n) {
        int a = s.available();
        size_t want = min((size_t)max(a, 0), n);
        int r = a <= 0 ? 0 : c ? c->read(p, want) : (int)Serial.read(p, want);
        if (r > 0) { p += r; n -= r; last = millis(); continue; }
        if ((c && !c->connected()) || millis() - last > 3000) return false;
        delay(1);
    }
    return true;
}

// hello: magic, panel size, the biggest JPEG this board takes, the biggest zoom (13 bytes)
struct __attribute__((packed)) Hello {
    char magic[4];
    uint16_t w, h;
    uint32_t maxBytes;
    uint8_t maxScale;
};
static const Hello HELLO = {{'C', 'Y', 'M', '1'}, PANEL_W, PANEL_H, MAX_FRAME_BYTES, MAX_SCALE};

static size_t wifiStatus(uint8_t* out) {   // the 'S' / 'W' reply
    String ip = WiFi.localIP().toString();
    size_t n = 0, k;
    memcpy(out, "CYMs", 4);
    n = 4;
    out[n++] = wifiState();
    out[n++] = k = strlen(boardName);
    memcpy(out + n, boardName, k);
    n += k;
    out[n++] = k = ip.length();
    memcpy(out + n, ip.c_str(), k);
    return n + k;
}

static void post(int item) { xQueueSend(readyQ, &item, portMAX_DELAY); }

static void dropSender(const char* why) {
    if (sender == LINK_NONE) return;
    if (sender == LINK_MAC) mac.stop();
    Link was = sender;
    sender = LINK_NONE;
    if (was == LINK_USB) usbLastUsed = millis();   // Wi-Fi comes back WIFI_RESUME_MS after this
    LOG("[Mirror] %s stopped (%s; USB overflows so far: FIFO %u, buffer %u)\n", LINK_NAME[was], why,
        (unsigned)usbLost, (unsigned)usbFull);
    post(EV_WAITING);
}

// the newest sender takes over the screen
static void claim(Link l) {
    if (sender == l) return;
    if (sender == LINK_MAC) mac.stop();
    if (l == LINK_MAC) LOG("[Mirror] Mac connected over Wi-Fi from %s\n", mac.remoteIP().toString().c_str());
    else LOG("[Mirror] %s connected\n", LINK_NAME[l]);
    sender = l;
    post(EV_CONNECTED);
}

// One message after its header has been read: receives the frame into a free buffer and queues it.
// Returns the reply to send (reply/replyLen), or false if the stream is broken.
static bool handle(Stream& s, Link from, const FrameHeader& h, WiFiClient* c, uint8_t* reply, size_t& replyLen) {
    replyLen = 1;
    reply[0] = 'K';   // ack before drawing: the sender prepares the next frame meanwhile
    if (h.tag == 'Q') {
        if (sender == from) dropSender("stopped");
        return true;
    }
    if (h.tag == 'U') {
        if (from == LINK_USB) pauseWifi();
        return true;
    }
    if (h.tag == 'I') {
        colourMode = (colourMode + 1) & 3;
        prefs.begin("mirror", false);
        prefs.putUChar("colours", colourMode);
        prefs.end();
        LOG("[Mirror] colour fix %d (invert %d, swap red/blue %d)\n", colourMode, colourMode & 1, colourMode >> 1);
        post(EV_COLOURS);
        return true;
    }
    if (h.tag == 'W' || h.tag == 'S') {
        uint8_t payload[200];
        if (h.len > sizeof(payload) || !readAll(s, payload, h.len, c)) return false;
        if (h.tag == 'W' && saveWifi(payload, h.len)) LOG("[WiFi] new settings saved: \"%s\" as %s\n", wifiSsid, boardName);
        replyLen = wifiStatus(reply);
        return true;
    }
    if (h.tag != 'J') return false;
    claim(from);
    if (!h.len) return true;   // keep-alive (its other fields don't matter)
    if (h.len > MAX_FRAME_BYTES || h.scale < 1 || h.scale > MAX_SCALE || h.x >= PANEL_W || h.y >= PANEL_H) return false;
    if (from == LINK_USB && !wifiPaused && wifiSsid[0]) wifiPause = true;
    int buf;
    xQueueReceive(freeQ, &buf, portMAX_DELAY);   // waits only while both buffers are full
    uint32_t t0 = micros();
    if (!readAll(s, jpgBuf[buf], h.len, c)) { xQueueSend(freeQ, &buf, 0); return false; }
    jpgHdr[buf] = h;
    netUs += micros() - t0;
    post(buf);
    return true;
}

// --- the website over Wi-Fi: a tiny HTTP server ---------------------------------------------
static void httpReply(WiFiClient& c, int code, const void* body, size_t n) {
    char head[320];
    int k = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                     "Access-Control-Allow-Headers: *\r\nAccess-Control-Allow-Private-Network: true\r\n"
                     "Content-Type: application/octet-stream\r\nContent-Length: %u\r\nConnection: keep-alive\r\n\r\n",
                     code, code == 200 ? "OK" : code == 204 ? "No Content" : "Error", (unsigned)n);
    // One non-blocking send. WiFiClient::write() retries a full socket for up to 10 s, which froze
    // the whole receive task when the browser had gone quiet on a weak link; drop that connection.
    static uint8_t out[sizeof(head) + 240];   // every body here is small (the biggest is /stats)
    if (k + n > sizeof(out)) n = 0;
    memcpy(out, head, k);
    if (n) memcpy(out + k, body, n);
    if (send(c.fd(), out, k + n, MSG_DONTWAIT) != (int)(k + n)) c.stop();
}

// GET /hello, POST /frame (body = header + JPEG), OPTIONS (CORS / local-network preflight)
static bool serveHttp(WiFiClient& c, unsigned long& lastHeard) {
    c.setTimeout(2);
    String req = c.readStringUntil('\n');
    if (!req.length()) return false;
    size_t len = 0;
    for (;;) {
        String line = c.readStringUntil('\n');
        if (line.length() <= 1) break;   // "\r": end of the headers
        if (line.substring(0, 15).equalsIgnoreCase("content-length:")) len = line.substring(15).toInt();
    }
    if (req.startsWith("OPTIONS")) { httpReply(c, 204, nullptr, 0); return true; }
    if (req.startsWith("GET /hello")) { httpReply(c, 200, &HELLO, sizeof(HELLO)); return true; }
    if (req.startsWith("POST /frame")) {
        FrameHeader h;
        uint8_t reply[80];
        size_t replyLen;
        if (len < sizeof(h) || !readAll(c, &h, sizeof(h), &c) || h.len != len - sizeof(h) || h.tag == 'W' ||
            !handle(c, LINK_WEB, h, &c, reply, replyLen)) {
            httpReply(c, 400, "bad frame", 9);
            return false;
        }
        lastHeard = millis();
        httpReply(c, 200, reply, replyLen);
        return true;
    }
    if (req.startsWith("GET /stats")) {   // for finding out what went wrong: did it restart, is memory short?
        static const char* const REASON[] = {"unknown", "power on", "external", "software", "crash", "interrupt watchdog",
                                             "task watchdog", "other watchdog", "deep sleep", "brownout (power dipped)", "SDIO"};
        int r = esp_reset_reason();
        char msg[220];
        int n = snprintf(msg, sizeof(msg),
                         "up %lu s\nlast restart: %s\nheap free %u, lowest ever %u, largest block %u\nWi-Fi signal %d dBm\n",
                         millis() / 1000, r >= 0 && r <= 10 ? REASON[r] : "?", (unsigned)ESP.getFreeHeap(),
                         (unsigned)ESP.getMinFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                         WiFi.RSSI());
        httpReply(c, 200, msg, n);
        return true;
    }
    if (req.startsWith("GET / ")) {
        static const char msg[] = "CYD screen mirror. Use the website to share a screen.";
        httpReply(c, 200, msg, sizeof(msg) - 1);
        return true;
    }
    httpReply(c, 404, "not found", 9);
    return true;
}

// Receive task (core 0, next to the Wi-Fi stack): accepts senders, receives frames into a free
// buffer, acks, queues them for drawing. Never touches the display.
static void netTask(void*) {
    unsigned long lastHeard = 0;
    bool serversUp = false;
    uint32_t usbWindow = 0;   // last 4 bytes seen on USB while it isn't the sender
    for (;;) {
        if (wifiPause) pauseWifi();   // frames are coming over USB: the radio would make the UART lose bytes
        if (wifiPaused && sender != LINK_USB && millis() - usbLastUsed > WIFI_RESUME_MS) wifiRestart = true;
        if (wifiRestart) {   // new settings from the website, or the USB cable is done
            wifiRestart = false;
            wifiPaused = false;
            if (sender == LINK_MAC || sender == LINK_WEB) dropSender("Wi-Fi changed");
            MDNS.end();
            netUp = false;
            startWifi();
            post(EV_WIFI);
        }
        bool up = WiFi.status() == WL_CONNECTED;
        if (up != netUp) {
            netUp = up;
            if (up) {
                MDNS.end();
                if (MDNS.begin(boardName)) {
                    MDNS.addService("http", "tcp", HTTP_PORT);
                    MDNS.addService("cydmirror", "tcp", MIRROR_PORT);
                }
                if (!serversUp) {
                    server.begin();
                    server.setNoDelay(true);
                    web.begin();
                    web.setNoDelay(true);
                    serversUp = true;
                }
                LOG("[WiFi] joined \"%s\" as %s.local (%s). Heap free: %u bytes (largest block %u)\n", wifiSsid,
                    boardName, WiFi.localIP().toString().c_str(), (unsigned)ESP.getFreeHeap(),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            }
            post(EV_WIFI);
        }

        // --- new connections ---
        if (serversUp && server.hasClient()) {
            mac = server.available();
            mac.setNoDelay(true);
            claim(LINK_MAC);
            mac.write((const uint8_t*)&HELLO, sizeof(HELLO));
            lastHeard = millis();
        }
        if (serversUp && web.hasClient()) {
            WiFiClient c = web.available();
            int slot = -1;
            for (int i = 0; i < WEB_SLOTS && slot < 0; i++)
                if (!webClient[i].connected()) slot = i;
            for (int i = 0; i < WEB_SLOTS && slot < 0; i++)   // all taken: close one with no request waiting
                if (!webClient[i].available()) slot = i;
            if (slot < 0) slot = 0;
            webClient[slot].stop();
            webClient[slot] = c;
            webClient[slot].setNoDelay(true);
        }
        while (sender != LINK_USB && Serial.available()) {   // the web page says "CYMH" when it starts
            usbWindow = usbWindow << 8 | Serial.read();
            if (usbWindow == USB_HELLO) {
                usbWindow = 0;
                claim(LINK_USB);
                Serial.write((const uint8_t*)&HELLO, sizeof(HELLO));
                lastHeard = millis();
            }
        }

        // --- the website over Wi-Fi: one request per call ---
        bool busy = false;
        for (int i = 0; i < WEB_SLOTS; i++) {
            if (webClient[i].connected() && webClient[i].available()) {
                busy = true;
                if (!serveHttp(webClient[i], lastHeard)) webClient[i].stop();
            }
        }

        // --- the Mac or the website over USB: a stream of messages ---
        if (sender == LINK_MAC && !mac.connected()) { dropSender("closed"); continue; }
        if (sender == LINK_USB) {
            // The page repeats "CYMH" until the hello arrives, so a late copy can follow it: every
            // message starts with one of these tags, so skip anything else here.
            while (Serial.available() && !strchr("JQWSU", Serial.peek())) Serial.read();
        }
        if (sender == LINK_USB || sender == LINK_MAC) {
            Stream& s = sender == LINK_USB ? (Stream&)Serial : (Stream&)mac;
            WiFiClient* c = sender == LINK_MAC ? &mac : nullptr;
            if (s.available() >= (int)sizeof(FrameHeader)) {
                Link from = sender;
                FrameHeader h;
                uint8_t reply[80];
                size_t replyLen;
                if (!readAll(s, &h, sizeof(h), c) || !handle(s, from, h, c, reply, replyLen)) {
                    dropSender("bad frame");
                    continue;
                }
                lastHeard = millis();
                if (s.write(reply, replyLen) != replyLen) dropSender("reply failed");
                continue;
            }
        }
        if (sender != LINK_NONE && millis() - lastHeard > IDLE_TIMEOUT_MS) dropSender("silent");
        if (!busy) delay(1);
    }
}

// -----------------------------------------------------------------------------
// DRAWING: JPEGDEC hands over up to MAX_BUFFERED_PIXELS at a time; each group is scaled into
// one strip buffer and queued for DMA, while the other buffer is filled with the next group.
// -----------------------------------------------------------------------------
static int drawBlocks(JPEGDRAW* d) {
    const int S = drawScale;
    const int px = drawX + d->x * S, py = drawY + d->y * S;
    int w = d->iWidthUsed, h = d->iHeight;
    if (px + w * S > PANEL_W) w = (PANEL_W - px) / S;   // never draw past the panel
    if (py + h * S > PANEL_H) h = (PANEL_H - py) / S;
    if (w <= 0 || h <= 0 || w * h > MAX_BUFFERED_PIXELS) return 1;

    uint16_t* out = strip[stripSel];
    stripSel ^= 1;
    if (S == 1) {
        for (int y = 0; y < h; y++) memcpy(out + y * w, d->pPixels + y * d->iWidth, w * 2);
    } else {
        const int ow = w * S;
        for (int y = 0; y < h; y++) {
            const uint16_t* src = d->pPixels + y * d->iWidth;
            uint16_t* row = out + y * S * ow;
            for (int x = 0; x < w; x++)
                for (int k = 0; k < S; k++) row[x * S + k] = src[x];
            for (int k = 1; k < S; k++) memcpy(row + k * ow, row, ow * 2);
        }
    }
    if (colourMode & 2)
        for (int i = 0, n = w * S * h * S; i < n; i++) out[i] = swapRB(out[i]);
    tft.pushImageDMA(px, py, w * S, h * S, out);
    return 1;
}

// returns the panel pixels covered, 0 on a decode error
static uint32_t drawJpeg(const uint8_t* data, const FrameHeader& h) {
    if (!jpeg.openRAM((uint8_t*)data, h.len, drawBlocks)) return 0;
    jpeg.setPixelType(RGB565_BIG_ENDIAN);   // panel byte order, so DMA sends it as-is
    drawX = h.x;
    drawY = h.y;
    drawScale = h.scale;
    tft.startWrite();
    int ok = jpeg.decode(0, 0, 0);
    tft.dmaWait();
    tft.endWrite();
    uint32_t covered = ok ? (uint32_t)jpeg.getWidth() * jpeg.getHeight() * h.scale * h.scale : 0;
    jpeg.close();
    return covered;
}

// -----------------------------------------------------------------------------
// SETUP / LOOP (core 1: drawing only)
// -----------------------------------------------------------------------------
// The UART driver puts its interrupt on the core that starts it, so start it from a task on core 0.
static void startSerial(void* done) {
    Serial.setRxBufferSize(USB_RX_BUFFER);
    Serial.begin(USB_BAUD, SERIAL_8N1, -1, -1, false, 20000UL, USB_RX_FIFO_FULL);
    Serial.onReceiveError([](hardwareSerial_error_t e) {
        if (e == UART_FIFO_OVF_ERROR) usbLost++;
        if (e == UART_BUFFER_FULL_ERROR) usbFull++;
    });
    xSemaphoreGive((SemaphoreHandle_t)done);
    vTaskDelete(nullptr);
}

void setup() {
    // the big buffers first, while memory is still in one piece
    for (int k = 0; k < 2; k++) {
        jpgBuf[k] = (uint8_t*)malloc(MAX_FRAME_BYTES);
        strip[k] = (uint16_t*)heap_caps_malloc(MAX_BUFFERED_PIXELS * MAX_SCALE * MAX_SCALE * 2, MALLOC_CAP_DMA);
    }
    SemaphoreHandle_t serialUp = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(startSerial, "serial-init", 4096, serialUp, 5, nullptr, 0);
    xSemaphoreTake(serialUp, portMAX_DELAY);
    Serial.println("\n[ESP32-035] Screen mirror starting...");

    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);
    pinMode(CYD_LED_RED, OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE, OUTPUT);
    led(false, false, false);
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(false);   // JPEGDEC outputs big-endian RGB565 already
    tft.fillScreen(TFT_BLACK);
    tft.initDMA();

    for (int k = 0; k < 2; k++)
        if (!jpgBuf[k] || !strip[k]) { status("Out of memory"); for (;;) delay(1000); }
    freeQ = xQueueCreate(2, sizeof(int));
    readyQ = xQueueCreate(8, sizeof(int));
    for (int k = 0; k < 2; k++) xQueueSend(freeQ, &k, 0);

    loadWifi();          // also the saved colour fix
    applyColours();
    startWifi();
    showWaiting();
    Serial.printf("[ESP32-035] Wi-Fi %s, USB at %d baud. Heap free: %u bytes (largest block %u)\n",
                  wifiSsid[0] ? "set up" : "not set up", USB_BAUD, (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    xTaskCreatePinnedToCore(netTask, "mirror-net", 6144, nullptr, 2, nullptr, 0);
}

void loop() {
    static unsigned long statT = millis(), wifiSince = millis();
    static uint32_t frames = 0, bytes = 0, drawUs = 0, pixels = 0;
    static bool waiting = true, warned = false;

    if (wifiState() == 2) wifiSince = millis();
    if (wifiState() == 1 && !warned && millis() - wifiSince > 20000) {
        LOG("[WiFi] can't join \"%s\" (it must be a 2.4 GHz network); USB still works\n", wifiSsid);
        warned = true;
    }

    int item;
    if (xQueueReceive(readyQ, &item, pdMS_TO_TICKS(100))) {
        if (item == EV_CONNECTED) {
            waiting = false;
            tft.fillScreen(TFT_BLACK);
            led(false, true, false);   // short green blink
            delay(30);
            led(false, false, false);
        } else if (item == EV_WAITING) {
            waiting = true;
            showWaiting();
        } else if (item == EV_COLOURS) {
            applyColours();
            if (waiting) showWaiting();   // its text colours changed
        } else if (item == EV_WIFI) {
            warned = false;
            wifiSince = millis();
            if (waiting) showWaiting();   // its Wi-Fi lines changed
        } else {
            uint32_t t0 = micros();
            uint32_t covered = drawJpeg(jpgBuf[item], jpgHdr[item]);
            if (!covered) LOG("[Mirror] JPEG error %d\n", jpeg.getLastError());
            drawUs += micros() - t0;
            frames++;
            bytes += jpgHdr[item].len;
            pixels += covered;
            xQueueSend(freeQ, &item, 0);
        }
    }

    unsigned long now = millis();
    if (now - statT >= 5000) {
        if (frames)
            LOG("[ESP32-035] %.1f fps, %.1f KB/frame, receive %.1f ms, draw %.1f ms (%u%% of panel per frame)\n",
                frames * 1000.0f / (now - statT), bytes / 1024.0f / frames, netUs / 1000.0f / frames,
                drawUs / 1000.0f / frames, (unsigned)(pixels * 100ULL / frames / (PANEL_W * PANEL_H)));
        frames = bytes = drawUs = pixels = netUs = 0;
        statT = now;
    }
}
