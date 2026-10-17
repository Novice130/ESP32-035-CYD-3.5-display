// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    Side-scrolling plumber platformer (fan tribute) - plays itself, and you
//          can take over any time with three on-screen buttons (JUMP / FIRE / RUN)
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch, RGB LED, Speaker (GPIO 26)
//
// The game renders into a 240x160 framebuffer which is pushed 2x-scaled to the
// panel. Only the rows/columns that changed since the last frame are sent.
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
#define CYD_LDR_PIN     34   // Light sensor (ADC1): covering it is a backup JUMP button

// Touch Screen SPI (Shared HSPI bus: CLK=14, MISO=12, MOSI=13, CS=33)
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

// Touch calibration: raw XPT2046 range -> 480x320 panel. Every press is printed on
// Serial ("[Touch] raw ... -> x,y"); if a button reacts on the wrong side, flip here.
#define TOUCH_RAW_MIN   200
#define TOUCH_RAW_MAX   3800
#define TOUCH_FLIP_X    false
#define TOUCH_FLIP_Y    false

// Button hit zones on the panel: the whole right strip, split into three bands
// around the drawn buttons (game.h draws them at fb y 50 / 92 / 134 = panel 100 / 184 / 268)
#define BTN_STRIP_X     400
#define BTN_SPLIT_1     142
#define BTN_SPLIT_2     226
#define RELEASE_MS      45   // finger must be gone this long to count as released
#define PUSH_CHUNK      16   // framebuffer rows per SPI burst; touch is read between bursts
#define EARLY_TICK_MS   10   // a new press ticks right away if the last tick was this long ago

// Light-sensor JUMP: a cracked touch panel can short and stop reading fingers, so
// covering the LDR also works as JUMP (hold it covered = higher jump). On this board
// the reading is ~0 in a lit room and rises in the dark. The room level is tracked
// slowly; rising LDR_COVER_RISE above it counts as covered.
#define LDR_DARK_IS_LOW false // true if the ADC reading drops when the sensor is covered
#define LDR_COVER_RISE  120   // measured: room 0-20, fingertip over it 100-1700
#define LDR_RELEASE_MS  150   // the covered reading flickers to 0; released = low this long
#define LDR_DEMO_MS     1500  // in DEMO a cover only takes over if released this soon (lights off != tap)

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

// original short effects (no melodies from the games)
void plat_sfx(uint8_t id, uint16_t p) {
    switch (id) {
        case SFX_JUMP:      PLAY(false, {300, 20}, {400, 20}, {520, 25}, {660, 30}); break;
        case SFX_JUMP_BIG:  PLAY(false, {200, 20}, {280, 20}, {380, 25}, {480, 30}); break;
        case SFX_COIN:      PLAY(false, {1400, 40}, {1870, 110}); break;
        case SFX_STOMP:     PLAY(true, {220, 30}, {160, 40}); break;
        case SFX_BUMP:      PLAY(false, {150, 40}); break;
        case SFX_BREAK:     PLAY(true, {600, 20}, {300, 20}, {150, 40}); break;
        case SFX_SPROUT:    PLAY(true, {400, 35}, {500, 35}, {600, 35}, {700, 35}, {800, 35}); break;
        case SFX_POWERUP:   PLAY(true, {523, 50}, {659, 50}, {784, 50}, {1047, 50}, {784, 50}, {1047, 50}, {1319, 120}); break;
        case SFX_POWERDOWN: PLAY(true, {800, 60}, {600, 60}, {450, 60}, {300, 100}); break;
        case SFX_FIRE:      PLAY(false, {900, 20}, {1400, 30}); break;
        case SFX_KICK:      PLAY(true, {700, 25}, {500, 35}); break;
        case SFX_ONEUP:     PLAY(true, {1319, 60}, {1568, 60}, {2093, 60}, {1760, 60}, {2093, 60}, {2637, 120}); break;
        case SFX_DEATH:     PLAY(true, {988, 80}, {0, 60}, {740, 100}, {622, 100}, {523, 100}, {392, 250}); break;
        case SFX_FLAG:      PLAY(true, {1568, 35}, {1397, 35}, {1245, 35}, {1109, 35}, {988, 35}, {880, 35}, {784, 35}, {698, 35}, {622, 80}); break;
        case SFX_CLEAR:     PLAY(true, {523, 90}, {659, 90}, {784, 90}, {1047, 180}, {988, 90}, {1175, 90}, {1568, 300}); break;
        case SFX_GAMEOVER:  PLAY(true, {523, 150}, {392, 150}, {330, 150}, {262, 400}); break;
        case SFX_MODE:      PLAY(true, {784, 60}, {1175, 120}); break;
        case SFX_TALLY:     PLAY(false, {1800, 15}); break;
        case SFX_RUN:
            if (p) PLAY(false, {600, 30}, {900, 40});
            else PLAY(false, {900, 30}, {600, 40});
            break;
    }
}

// -----------------------------------------------------------------------------
// TOUCH: three button zones on the right edge
// -----------------------------------------------------------------------------
enum Zone { Z_OUTSIDE = -2, Z_NONE = -1, Z_JUMP = 0, Z_FIRE = 1, Z_RUN = 2 };

int heldZone = Z_NONE;
bool fingerDown = false;
unsigned long lastSeenDown = 0;
bool latchJump = false, latchFire = false, latchRun = false;
unsigned long shortSeenAt = 0;  // last reading that looked like the cracked-glass short
uint32_t pressCount = 0;  // bumped on every new press

int readZone() {
    if (!ts.touched()) return Z_NONE;
    TS_Point p = ts.getPoint();
    if (p.z >= 3900) shortSeenAt = millis();
    if (p.z < 500 || p.z >= 3900) {  // reject phantom/noise readings
        static unsigned long lastLog = 0;
        if (millis() - lastLog > 1000) {
            lastLog = millis();
            Serial.printf("[Touch] ignored raw %d,%d z=%d\n", p.x, p.y, p.z);
        }
        return Z_NONE;
    }
    int x = constrain(map(p.x, TOUCH_RAW_MIN, TOUCH_RAW_MAX, 0, 480), 0, 479);
    int y = constrain(map(p.y, TOUCH_RAW_MIN, TOUCH_RAW_MAX, 0, 320), 0, 319);
    if (TOUCH_FLIP_X) x = 479 - x;
    if (TOUCH_FLIP_Y) y = 319 - y;
    int z = x < BTN_STRIP_X ? Z_OUTSIDE : (y < BTN_SPLIT_1 ? Z_JUMP : (y < BTN_SPLIT_2 ? Z_FIRE : Z_RUN));
    if (!fingerDown) {
        static const char* NAMES[] = {"JUMP", "FIRE", "RUN"};
        Serial.printf("[Touch] raw %d,%d -> %d,%d %s\n", p.x, p.y, x, y, z >= 0 ? NAMES[z] : "(not a button)");
    }
    return z;
}

void pollLdr();

void pollTouch() {
    pollLdr();
    unsigned long now = millis();
    int z = readZone();
    if (z != Z_NONE) {
        fingerDown = true;
        lastSeenDown = now;
        if (z >= 0 && z != heldZone) {  // a new press (or slid onto another button)
            heldZone = z;
            pressCount++;
            if (z == Z_JUMP) latchJump = true;
            if (z == Z_FIRE) latchFire = true;
            if (z == Z_RUN) latchRun = true;
        }
    } else if (fingerDown && now - lastSeenDown > RELEASE_MS) {
        fingerDown = false;
        heldZone = Z_NONE;
    }
}

// ----- light sensor
int ldrBase = -1;           // room darkness level (bigger = darker)
bool ldrCovered = false;
unsigned long ldrCoveredAt = 0, ldrBaseAt = 0, ldrLastDark = 0;

int readDark() {
    int v = analogRead(CYD_LDR_PIN);
    return LDR_DARK_IS_LOW ? 4095 - v : v;
}

void pollLdr() {
    unsigned long now = millis();
    int v = readDark();
    if (ldrBase < 0) ldrBase = v;
    if (!ldrCovered) {
        if (v - ldrBase >= LDR_COVER_RISE) {
            ldrCovered = true;
            ldrCoveredAt = ldrLastDark = now;
            Serial.printf("[LDR] covered %d (room %d)\n", v, ldrBase);
            if (playMode) {  // act at once while playing
                latchJump = true;
                pressCount++;
            }
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
        if (!playMode && now - ldrCoveredAt < LDR_DEMO_MS) {  // a quick cover takes over the demo
            latchJump = true;
            pressCount++;
        }
    }
}

bool inputPending() { return latchJump || latchFire || latchRun; }

Input takeInput() {
    Input in;
    in.jumpPressed = latchJump;
    in.jumpHeld = heldZone == Z_JUMP || latchJump || ldrCovered;  // a tap shorter than a tick still counts once
    in.firePressed = latchFire;
    in.runPressed = latchRun;
    in.autoFire = shortSeenAt && millis() - shortSeenAt < 1000;  // FIRE can't be pressed: shoot by itself
    latchJump = latchFire = latchRun = false;
    return in;
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

// Sends the frame in bursts of PUSH_CHUNK rows and reads the touch panel between
// them. If a new press comes in, the rest of this frame is dropped so the press can
// be acted on now; prevFb only records rows that really reached the panel, so the
// next frame resends whatever was skipped.
void pushFrame() {
    if (!prevFb) {
        tft.startWrite();
        pushRect(0, FB_W - 1, 0, FB_H - 1);
        tft.endWrite();
        return;
    }
    uint32_t presses = pressCount;
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
        if (runStart < 0) continue;
        for (int y0 = runStart; y0 < y; y0 += PUSH_CHUNK) {
            int y1 = y0 + PUSH_CHUNK < y ? y0 + PUSH_CHUNK - 1 : y - 1;
            tft.startWrite();
            pushRect(xmin, xmax, y0, y1);
            tft.endWrite();
            memcpy(prevFb + y0 * FB_W, fb + y0 * FB_W, (y1 - y0 + 1) * FB_W * 2);
            pollTouch();
            if (pressCount != presses) return;
        }
    }
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Starting plumber platformer (240x160 @ 2x)...");

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
    pinMode(CYD_LDR_PIN, INPUT);
    analogSetPinAttenuation(CYD_LDR_PIN, ADC_0db);  // 0-1.1 V range: the LDR swing is small

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
    static unsigned long lastTickMs = 0;
    static unsigned long fpsT = millis();
    static int frames = 0;
    static uint32_t tickUs = 0, drawUs = 0, pushUs = 0, ticks = 0;

    pollTouch();
    updateLED();

    unsigned long now = millis();
    // a fresh press doesn't wait for its 33 ms slot
    if (inputPending() && (long)(nextTick - now) > 0 && now - lastTickMs >= EARLY_TICK_MS) nextTick = now;
    int ticked = 0;
    while ((long)(now - nextTick) >= 0 && ticked < 3) {
        uint32_t t0 = micros();
        game_tick(takeInput());
        tickUs += micros() - t0;
        ticks++;
        nextTick += 1000 / TICK_HZ;
        lastTickMs = now;
        ticked++;
    }
    if ((long)(now - nextTick) > 250) nextTick = now;  // don't spiral after a stall

    if (ticked) {
        uint32_t t0 = micros();
        game_render();
        uint32_t t1 = micros();
        pollTouch();
        pushFrame();
        drawUs += t1 - t0;
        pushUs += micros() - t1;
        frames++;
    }

    if (now - fpsT >= 5000) {
        Serial.printf("[ESP32-035] %.1f fps, mode=%s, tick %lu us, render %lu us, push %lu us, dark %d (room %d)\n",
                      frames * 1000.0f / (now - fpsT), playMode ? "PLAY" : "DEMO",
                      (unsigned long)(ticks ? tickUs / ticks : 0), (unsigned long)(frames ? drawUs / frames : 0),
                      (unsigned long)(frames ? pushUs / frames : 0), readDark(), ldrBase);
        frames = 0;
        ticks = tickUs = drawUs = pushUs = 0;
        fpsT = now;
    }
}
