// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    Pomodoro Hourglass - seven animated hourglass scenes, quotes, time tracker
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch, RGB LED, LDR, Speaker
//
// Draws at the panel's native 480x320. Backgrounds live in flash; each frame only
// the rectangle around the glass is rebuilt in RAM and pushed.
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "pomodoro.h"

// --- PHYSICAL PIN DEFINITIONS FOR ESP32-035 ---
#define CYD_BACKLIGHT   27   // Backlight is GPIO 27 (Active HIGH)
#define CYD_LED_RED     4    // Active LOW (0 = ON, 1 = OFF)
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_SPEAKER_PIN 26   // Audio Amplifier (NS4168)
#define CYD_LDR_PIN     34   // Ambient Light Sensor (ADC1, 0dB attenuation)

// Touch Screen SPI (Shared HSPI bus: CLK=14, MISO=12, MOSI=13, CS=33)
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

#define TAP_DEBOUNCE_MS 40

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);


// -----------------------------------------------------------------------------
// LED & AUDIO PLATFORM HOOKS
// -----------------------------------------------------------------------------
void setRGB(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

static unsigned long ledOffTime = 0;
static bool ledActive = false;

void plat_led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) {
    setRGB(r > 100, g > 100, b > 100);
    ledOffTime = millis() + ms;
    ledActive = true;
}

void updateLED() {
    if (ledActive && millis() > ledOffTime) {
        setRGB(false, false, false);
        ledActive = false;
    }
}

static unsigned long audioBusyUntil = 0;
struct Note { uint16_t hz, ms; };

void playNotes(const Note* notes, int count, bool important) {
    unsigned long now = millis();
    if (!important && now < audioBusyUntil) return;
    unsigned long total = 0;
    for (int i = 0; i < count; i++) {
        tone(CYD_SPEAKER_PIN, notes[i].hz, notes[i].ms);
        total += notes[i].ms;
    }
    audioBusyUntil = (now > audioBusyUntil ? now : audioBusyUntil) + total;
}

#define PLAY(important, ...) do { static const Note _n[] = {__VA_ARGS__}; \
    playNotes(_n, sizeof(_n) / sizeof(_n[0]), important); } while (0)

void plat_sfx(uint8_t id, uint16_t p) {
    switch (id) {
        case SFX_DRIP:
            PLAY(false, {p ? p : (uint16_t)1200, 16});
            break;
        case SFX_TAP:
            PLAY(false, {p ? p : (uint16_t)2000, 22});
            break;
        case SFX_CHIME:
            PLAY(true, {523, 70}, {659, 70}, {784, 120});
            break;
        case SFX_MILESTONE:
            PLAY(true, {784, 60}, {988, 60}, {1319, 140});
            break;
        case SFX_COMPLETE:
            PLAY(true, {523, 90}, {659, 90}, {784, 90}, {1047, 240});
            break;
    }
}

// -----------------------------------------------------------------------------
// LDR SENSOR DETECTOR
// Measured at 0 dB on this CYD board: room ~0-20, fingertip over it 60-500
// - Brief Tap (< 1.2s): Next scene
// - Hold about 2s (1.5s - 4.9s, acts on release): Starts again. Continues if stopped, else restarts the session
// - Hold 5s (acts while still covered): STOPS the timer
// -----------------------------------------------------------------------------
#define LDR_COVER_RISE  45    // sensitive threshold so even a light touch/shadow triggers
#define LDR_RELEASE_MS  100   // debounce release
#define LDR_STOP_MS     5000  // 5 seconds hold stops the timer
#define LDR_RESTART_MS  1500  // about 2 seconds (1.5 - 4.9) starts it again

static int ldrBase = -1;
static int ldrCurrent = 0;
static bool ldrCovered = false;
static unsigned long ldrCoveredAt = 0;
static unsigned long ldrLastDark = 0;
static unsigned long ldrBaseAt = 0;
static bool ldrTapTriggered = false;
static bool ldrHoldTriggered = false;
static bool ldrStopTriggered = false;
static bool ldrStopHandled = false;

void pollLdr() {
    unsigned long now = millis();
    int v = analogRead(CYD_LDR_PIN);
    ldrCurrent = v;
    if (ldrBase < 0) ldrBase = v;

    if (!ldrCovered) {
        if (v - ldrBase >= LDR_COVER_RISE) {
            ldrCovered = true;
            ldrCoveredAt = ldrLastDark = now;
            ldrStopHandled = false;
            Serial.printf("[LDR] Cover detected! raw=%d base=%d diff=%d\n", v, ldrBase, v - ldrBase);
        } else if (now - ldrBaseAt >= 100) {
            ldrBaseAt = now;
            ldrBase += (v - ldrBase) / 16; // slowly follow room ambient light
        }
    } else if (v - ldrBase >= LDR_COVER_RISE / 2) {
        ldrLastDark = now;
        unsigned long held = now - ldrCoveredAt;

        // 5 SECONDS CONTINUOUS HOLD -> STOP / PAUSE TIMER!
        if (held >= LDR_STOP_MS && !ldrStopHandled) {
            ldrStopTriggered = true;
            ldrStopHandled = true;
            Serial.printf("[LDR] 5 SECONDS HOLD REACHED (%lu ms) -> STOPPING TIMER!\n", held);
        }

        if (held > 20000) {
            // Room stayed dark for 20s (lights turned off): reset baseline
            ldrCovered = false;
            ldrBase = v;
        }
    } else if (now - ldrLastDark >= LDR_RELEASE_MS) {
        // Finger released
        if (!ldrStopHandled) {
            unsigned long held = ldrLastDark - ldrCoveredAt;
            if (held >= LDR_RESTART_MS) {
                ldrHoldTriggered = true;
                Serial.printf("[LDR] Medium hold (%lu ms) -> Starting again!\n", held);
            } else if (held >= 30) {
                ldrTapTriggered = true;
                Serial.printf("[LDR] Tap released (%lu ms) -> Next scene\n", held);
            }
        }
        ldrCovered = false;
        ldrStopHandled = false;
    }
}

// -----------------------------------------------------------------------------
// TOUCH CONTROLLER: Adaptive calibration for broken/cracked digitizers
// On this board, a crack in the glass shorts the resistive layers, pegging Z at ~4095.
// A real press pulls the raw X reading well below that spot (idle ~1700-1830, pressed < 1680).
// For an unbroken screen (Z 500..3900), normal pressure detection is used.
// Holding a press for 1.2s also switches to the next scene.
// -----------------------------------------------------------------------------
#define TOUCH_Z_MIN     500
#define TOUCH_Z_SHORT   3900  // Z at/above this = cracked glass short
#define SHORT_MARGIN    150   // raw X drop below idle short level that counts as a press
#define PRESS_SAMPLES   2     // readings in a row below the margin before press starts
#define RELEASE_MS      90    // finger must be gone this long to count as released

static bool fingerDown = false;
static unsigned long lastSeenDown = 0;
static unsigned long pressStart = 0;
static int shortX = -1;       // learned idle X coordinate of the crack short
static int belowCount = 0;
static bool displayTapTriggered = false;
static bool longPressHandled = false;
static bool displayLongPressTriggered = false;

bool touchDown(TS_Point& p) {
    if (!ts.touched()) return false;
    p = ts.getPoint();
    if (p.z < TOUCH_Z_MIN) return false;
    if (p.z < TOUCH_Z_SHORT) return true;  // normal press on an undamaged panel

    // Crack short handling:
    if (shortX < 0) {
        shortX = p.x;
        Serial.printf("[Touch] Crack short detected at raw x=%d z=%d -> adaptive X-drop detection enabled\n", p.x, p.z);
        return false;
    }
    bool down = (p.x < shortX - SHORT_MARGIN);
    belowCount = down ? belowCount + 1 : 0;
    if (!down && !fingerDown) {
        shortX += (p.x - shortX) / 32;  // follow slow thermal/idle drift
    }
    return fingerDown ? down : (belowCount >= PRESS_SAMPLES);
}

void pollTouch() {
    pollLdr();
    unsigned long now = millis();
    TS_Point p;
    bool down = touchDown(p);

    if (down) {
        if (!fingerDown) {
            fingerDown = true;
            pressStart = now;
            longPressHandled = false;
        }
        // Long press detection: holding a finger on the display for 1.2s = next scene
        if (!longPressHandled && (now - pressStart >= 1200)) {
            longPressHandled = true;
            displayLongPressTriggered = true;
            Serial.println("[Touch] Long press on display (1.2s) -> Next scene");
        }
        lastSeenDown = now;
    } else if (fingerDown && now - lastSeenDown > RELEASE_MS) {
        if (!longPressHandled && (now - pressStart >= 40)) {
            displayTapTriggered = true;
            Serial.printf("[Touch] Tap registered! raw %d,%d z=%d (shortX=%d)\n", p.x, p.y, p.z, shortX);
        }
        fingerDown = false;
        longPressHandled = false;
    }
}

TapType takeTap() {
    if (ldrStopTriggered) {
        ldrStopTriggered = false;
        return TAP_LDR_STOP;
    }
    if (ldrTapTriggered || displayLongPressTriggered) {
        ldrTapTriggered = false;
        displayLongPressTriggered = false;
        return TAP_LDR_TAP;
    }
    if (ldrHoldTriggered) {
        ldrHoldTriggered = false;
        return TAP_LDR_HOLD;
    }
    if (displayTapTriggered) {
        displayTapTriggered = false;
        return TAP_DISPLAY;
    }
    return TAP_NONE;
}

// -----------------------------------------------------------------------------
// PANEL PUSH: px is native-endian RGB565 in RAM or flash, stride in pixels
// -----------------------------------------------------------------------------
void plat_push(int x, int y, int w, int h, const uint16_t* px, int stride) {
    tft.startWrite();
    tft.setAddrWindow(x, y, w, h);
    for (int r = 0; r < h; r++) tft.pushPixels(px + r * stride, w);
    tft.endWrite();
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Starting Pomodoro Hourglass (native 480x320)...");

    // 1. Backlight on GPIO 27 (Active HIGH)
    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);

    // 2. RGB LED (Active LOW)
    pinMode(CYD_LED_RED,   OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE,  OUTPUT);
    setRGB(false, false, false);

    // 3. Audio & Sensors
    pinMode(CYD_SPEAKER_PIN, OUTPUT);
    digitalWrite(CYD_SPEAKER_PIN, LOW);
    pinMode(CYD_LDR_PIN, INPUT);
    analogSetPinAttenuation(CYD_LDR_PIN, ADC_0db);

    // 4. Touch CS
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    // 5. TFT Display Initialisation (Landscape Rotation 1)
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);

    // 6. Touch Controller Initialisation
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    // 7. Work buffers
    heroBuf = (uint16_t*)malloc(HERO_MAX_PIXELS * 2);
    uiBuf = (uint16_t*)malloc(UI_BUF_PIXELS * 2);
    sceneArena = (uint8_t*)malloc(SCENE_ARENA_BYTES);
    if (!heroBuf || !uiBuf || !sceneArena) {
        Serial.println("[ESP32-035] FATAL: buffer allocation failed!");
        while (true) delay(1000);
    }
    Serial.printf("[ESP32-035] Heap free after buffers: %u bytes\n", ESP.getFreeHeap());

    // 8. Initialize Pomodoro Engine
    game_init(esp_random() | 1);
    plat_sfx(SFX_CHIME);
    plat_led(0, 180, 255, 500);
}

// -----------------------------------------------------------------------------
// MAIN LOOP: Fixed 30 Hz ticks + smooth dirty-rect rendering
// -----------------------------------------------------------------------------
void loop() {
    static unsigned long nextTick = millis();
    static unsigned long fpsTimer = millis();
    static int frameCount = 0;
    static unsigned long drawMicros = 0;
    static TapType pendingTap = TAP_NONE;

    pollTouch();
    updateLED();

    unsigned long now = millis();
    int ticked = 0;
    while ((long)(now - nextTick) >= 0 && ticked < 3) {
        TapType t = takeTap();
        if (pendingTap != TAP_NONE) { t = pendingTap; pendingTap = TAP_NONE; }
        game_tick(t);
        nextTick += 1000 / TICK_HZ;
        ticked++;
    }
    if ((long)(now - nextTick) > 250) nextTick = now;

    // Serial test keys: n = next scene, s = 5 s hold (stop), g = 2 s hold (start again),
    // h = jump to halfway, b = start a break, w = back to focus
    if (Serial.available()) {
        char key = Serial.read();
        if (key == 'n') pendingTap = TAP_LDR_TAP;
        else if (key == 's') pendingTap = TAP_LDR_STOP;
        else if (key == 'g') pendingTap = TAP_LDR_HOLD;
        else if (key == 'h') {
            if (pomodoro.state == STATE_BREAK) pomodoro.breakRemainingSec = pomodoro.breakTargetSec / 2;
            else pomodoro.sessionRemainingSec = pomodoro.sessionTargetSec / 2;
        } else if (key == 'b') {
            pomodoro.state = STATE_BREAK;
            pomodoro.breakRemainingSec = pomodoro.breakTargetSec;
        } else if (key == 'w') {
            pomodoro.restartWorkSession();
        }
    }

    if (ticked) {
        unsigned long t0 = micros();
        game_render();
        drawMicros += micros() - t0;
        pollTouch();
        frameCount++;
    }

    if (now - fpsTimer >= 5000) {
        float fps = frameCount * 1000.0f / (now - fpsTimer);
        Serial.printf("[ESP32-035] %.1f fps, frame %lu us, scene=%s, heap=%u, ldr=%d (base=%d), state=%d, target=%um, rem=%02u:%02u\n",
                      fps, frameCount ? drawMicros / frameCount : 0, SCENES[pomodoro.currentScene]->name, ESP.getFreeHeap(),
                      ldrCurrent, ldrBase, pomodoro.state, DURATIONS_MIN[pomodoro.durationIndex],
                      (unsigned)(pomodoro.sessionRemainingSec / 60), (unsigned)(pomodoro.sessionRemainingSec % 60));
        frameCount = 0;
        drawMicros = 0;
        fpsTimer = now;
    }
}
