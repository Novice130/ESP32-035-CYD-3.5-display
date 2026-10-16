// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    POKeMON Kalos tribute - self-playing display + 1-tap / 2-tap game
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch, RGB LED, Speaker (GPIO 26)
//
// The game renders into a 240x160 framebuffer (exact GBA resolution) which is
// pushed 2x-scaled to the panel. Only the rows/columns that changed since the
// last frame are sent, so static battle scenes cost almost nothing on the bus.
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "game.h"

// --- PIN DEFINITIONS FOR CYD 3.5" (ESP32-035) ---
#define CYD_BACKLIGHT   27   // Backlight is GPIO 27 (Active HIGH)
#define CYD_LED_RED     4    // Active LOW (0 = ON, 1 = OFF)
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_SPEAKER_PIN 26   // Audio Amplifier

// Touch Screen SPI (Shared HSPI bus: CLK=14, MISO=12, MOSI=13, CS=33)
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

#define TAP_DEBOUNCE_MS   45   // release must last this long to count
#define DOUBLE_TAP_MS     300  // window for the second tap

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

void plat_sfx(uint8_t id, uint16_t p) {
    switch (id) {
        case SFX_TAP:       PLAY(false, {1800, 25}); break;
        case SFX_RUSTLE:    PLAY(false, {300, 18}, {240, 18}); break;
        case SFX_ENCOUNTER: PLAY(true, {523, 45}, {659, 45}, {784, 45}, {1047, 45}, {988, 70}, {1047, 120}); break;
        case SFX_LEGEND:    PLAY(true, {196, 130}, {185, 130}, {175, 160}, {0, 40}, {233, 120}, {220, 260}); break;
        case SFX_CRY: {
            Note n[4] = {{p, 50}, {(uint16_t)(p * 5 / 4), 60}, {(uint16_t)(p * 4 / 5), 50}, {(uint16_t)(p * 11 / 10), 80}};
            playNotes(n, 4, true);
            break;
        }
        case SFX_HIT:       PLAY(true, {180, 40}, {120, 50}); break;
        case SFX_SUPER:     PLAY(true, {240, 30}, {160, 30}, {110, 70}); break;
        case SFX_WEAK:      PLAY(true, {320, 40}); break;
        case SFX_BOLT:      PLAY(true, {2000, 20}, {1400, 20}, {2400, 20}, {1600, 20}, {2200, 30}, {1200, 30}); break;
        case SFX_FIRE:      PLAY(true, {400, 40}, {520, 40}, {450, 40}, {620, 60}); break;
        case SFX_WATER:     PLAY(true, {900, 30}, {1100, 30}, {1300, 30}, {1100, 30}, {1400, 40}); break;
        case SFX_LEAF:      PLAY(true, {1200, 25}, {1500, 25}, {1200, 25}, {1600, 30}); break;
        case SFX_WIND:      PLAY(true, {700, 30}, {800, 30}, {900, 30}, {800, 30}, {700, 40}); break;
        case SFX_MOON:      PLAY(true, {1047, 90}, {1319, 90}, {1568, 90}, {2093, 200}); break;
        case SFX_DARK:      PLAY(true, {150, 90}, {120, 90}, {100, 180}, {80, 200}); break;
        case SFX_QUAKE:     PLAY(true, {90, 60}, {70, 60}, {90, 60}, {60, 120}); break;
        case SFX_THROW:     PLAY(false, {600, 20}, {800, 20}, {1000, 20}, {1200, 20}); break;
        case SFX_POP:       PLAY(true, {1500, 30}, {900, 30}); break;
        case SFX_SHAKE:     PLAY(false, {250, 30}, {0, 40}, {250, 30}); break;
        case SFX_CATCH:     PLAY(true, {784, 100}, {988, 100}, {1175, 100}, {0, 30}, {1175, 80}, {1568, 320}); break;
        case SFX_BREAK:     PLAY(true, {1200, 30}, {600, 30}, {300, 60}); break;
        case SFX_FAINT:     PLAY(true, {600, 60}, {500, 60}, {400, 60}, {300, 140}); break;
        case SFX_LEVELUP:   PLAY(true, {1047, 80}, {1319, 80}, {1568, 80}, {0, 40}, {1568, 80}, {2093, 220}); break;
        case SFX_EXP:       PLAY(false, {1400, 30}, {1600, 30}, {1800, 30}); break;
        case SFX_MODE:      PLAY(true, {784, 60}, {1175, 120}); break;
    }
}

// -----------------------------------------------------------------------------
// TOUCH: single vs double tap detector
// -----------------------------------------------------------------------------
enum TouchState { TS_IDLE, TS_DOWN, TS_WAIT_SECOND, TS_DOWN_SECOND };
TouchState touchState = TS_IDLE;
unsigned long lastSeenDown = 0, releasedAt = 0;
Tap tapQueue = TAP_NONE;

bool touchDown() {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    return p.z >= 500 && p.z < 3900;  // reject phantom/noise readings
}

void pollTouch() {
    unsigned long now = millis();
    bool down = touchDown();
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
    Serial.println("\n[ESP32-035] Starting POKeMON Kalos tribute (240x160 @ 2x)...");

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

    // 4. Touch CS setup
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    // 5. TFT Display Initialisation (Standard Working Landscape Rotation 1)
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(false);  // pushRect pre-swaps
    tft.fillScreen(TFT_BLACK);

    // 6. Touch Controller Initialisation
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    // 7. Framebuffers (2 x 75 KB)
    fb = (uint16_t*)malloc(FB_W * FB_H * 2);
    prevFb = (uint16_t*)malloc(FB_W * FB_H * 2);
    if (!fb) {
        Serial.println("[ESP32-035] FATAL: framebuffer alloc failed");
        while (true) delay(1000);
    }
    if (!prevFb) Serial.println("[ESP32-035] No memory for diff buffer, pushing full frames");
    else memset(prevFb, 0, FB_W * FB_H * 2);  // matches the black screen
    Serial.printf("[ESP32-035] Heap free after buffers: %u bytes\n", ESP.getFreeHeap());

    game_init(esp_random() | 1);
}

// -----------------------------------------------------------------------------
// MAIN LOOP: fixed 30 Hz game ticks, render when something ticked
// -----------------------------------------------------------------------------
void loop() {
    static unsigned long nextTick = millis();
    static unsigned long fpsT = millis();
    static int frames = 0;

    pollTouch();
    updateLED();

    unsigned long now = millis();
    int ticked = 0;
    while ((long)(now - nextTick) >= 0 && ticked < 3) {
        game_tick(takeTap());
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
        Serial.printf("[ESP32-035] %.1f fps, mode=%s\n", frames * 1000.0f / (now - fpsT), playMode ? "PLAY" : "DEMO");
        frames = 0;
        fpsT = now;
    }
}
