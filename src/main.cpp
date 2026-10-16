// ==============================================================================
// Project: ESP32-035 (CYD 3.5" / ESP32-3248S035R) Working Guide & Hardware Test
// Role:    Physical Hardware Verification, Pinout Diagnostics & Touch Test
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch, RGB LED, LDR, Speaker
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// --- PHYSICAL PIN DEFINITIONS FOR ESP32-035 ---
#define CYD_BACKLIGHT   27   // Backlight is GPIO 27 (Active HIGH, drives Q2)
#define CYD_LED_RED     4    // Active LOW (0 = ON, 1 = OFF)
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_LDR_PIN     34   // Ambient Light Sensor (ADC1)
#define CYD_SPEAKER_PIN 26   // Audio Amplifier (NS4168)

// Touch Screen SPI (Shared HSPI bus: CLK=14, MISO=12, MOSI=13, CS=33)
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

#define SCREEN_W        480
#define SCREEN_H        320

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);

// Global State
unsigned long lastSensorRead = 0;
unsigned long lastTouchTime = 0;
unsigned long ledOffTime = 0;
bool ledActive = false;
int touchCount = 0;
int lastTouchX = -1;
int lastTouchY = -1;
int ledColorIdx = 0;

const char* colorNames[] = {"RED", "GREEN", "BLUE", "CYAN", "YELLOW", "MAGENTA", "WHITE"};
const uint16_t tftColors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_CYAN, TFT_YELLOW, TFT_MAGENTA, TFT_WHITE};

void setRGB(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

void triggerLED() {
    ledColorIdx = (ledColorIdx + 1) % 7;
    bool r = (ledColorIdx == 0 || ledColorIdx == 4 || ledColorIdx == 5 || ledColorIdx == 6);
    bool g = (ledColorIdx == 1 || ledColorIdx == 3 || ledColorIdx == 4 || ledColorIdx == 6);
    bool b = (ledColorIdx == 2 || ledColorIdx == 3 || ledColorIdx == 5 || ledColorIdx == 6);
    setRGB(r, g, b);
    ledOffTime = millis() + 450;
    ledActive = true;
    tone(CYD_SPEAKER_PIN, 880 + (ledColorIdx * 100), 60);
}

void drawDiagnosticsUI() {
    tft.fillScreen(tft.color565(10, 14, 23));

    // Top Header Bar
    tft.fillRect(0, 0, SCREEN_W, 36, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 36, SCREEN_W, tft.color565(56, 189, 248));

    tft.setTextColor(TFT_WHITE, tft.color565(15, 23, 42));
    tft.drawString("ESP32-035 (CYD 3.5\") HARDWARE DIAGNOSTICS", 14, 10, 2);

    tft.fillRoundRect(350, 7, 116, 22, 11, tft.color565(30, 41, 59));
    tft.setTextColor(TFT_GREEN, tft.color565(30, 41, 59));
    tft.drawCentreString("ST7796 480x320", 408, 10, 2);

    // Card 1: SoC & Display Subsystem
    tft.fillRoundRect(10, 44, 226, 96, 6, tft.color565(20, 26, 36));
    tft.drawRoundRect(10, 44, 226, 96, 6, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("SOC & DISPLAY", 18, 50, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));
    tft.drawString("Board: ESP32-035 / 3248S035R", 18, 68, 1);
    tft.drawString("Controller: ST7796 (480x320)", 18, 82, 1);
    tft.drawString("Backlight: GPIO 27 (Q2 Transistor)", 18, 96, 1);
    tft.drawString("Bus: HSPI @ 40MHz", 18, 110, 1);

    // Card 2: Peripherals & Audio
    tft.fillRoundRect(244, 44, 226, 96, 6, tft.color565(20, 26, 36));
    tft.drawRoundRect(244, 44, 226, 96, 6, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("PERIPHERALS & AUDIO", 252, 50, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));
    tft.drawString("Touch: XPT2046 (CS 33, CLK 14)", 252, 68, 1);
    tft.drawString("RGB LED: GPIO 4(R), 16(G), 17(B)", 252, 82, 1);
    tft.drawString("Light Sensor (LDR): GPIO 34 (ADC1)", 252, 96, 1);
    tft.drawString("Audio Speaker: GPIO 26 (NS4168)", 252, 110, 1);

    // Card 3: Live System Telemetry
    tft.fillRoundRect(10, 148, 460, 50, 6, tft.color565(18, 28, 45));
    tft.drawRoundRect(10, 148, 460, 50, 6, tft.color565(56, 189, 248));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(18, 28, 45));
    tft.drawString("LIVE HARDWARE TELEMETRY", 18, 154, 2);

    // Card 4: Examples Directory Guide
    tft.fillRoundRect(10, 206, 460, 48, 6, tft.color565(20, 26, 36));
    tft.drawRoundRect(10, 206, 460, 48, 6, tft.color565(45, 55, 72));
    tft.setTextColor(TFT_GREEN, tft.color565(20, 26, 36));
    tft.drawString("EXAMPLE REPOSITORIES (IN /examples/ DIRECTORY):", 18, 212, 1);
    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(20, 26, 36));
    tft.drawString("mario-arcade | manga-aura | neo-tokyo-synthwave | rasengan-vortex | matrix-rain | noc-dashboard", 18, 228, 1);

    // Bottom Touch Interactive Pad
    tft.fillRect(0, 262, SCREEN_W, 58, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 262, SCREEN_W, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
    tft.drawString("Touch Screen to test digitizer coordinates, LED pulse & Speaker chime:", 16, 270, 1);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Hardware Guide & Diagnostics Firmware Initializing...");

    // 1. Backlight GPIO 27 (Active HIGH)
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

    // 4. Touch CS setup
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    // 5. TFT Display Initialisation (Landscape 480x320)
    tft.init();
    tft.setRotation(1);
    Serial.printf("[ESP32-035] Display Initialized: %dx%d\n", tft.width(), tft.height());

    // 6. Touch Controller Initialisation
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    // 7. Paint Initial UI
    drawDiagnosticsUI();
    tone(CYD_SPEAKER_PIN, 1046, 120); // Startup high C chime
    setRGB(false, true, false); // Green flash
    delay(150);
    setRGB(false, false, false);
}

void loop() {
    unsigned long now = millis();

    // Turn off LED after duration
    if (ledActive && now > ledOffTime) {
        setRGB(false, false, false);
        ledActive = false;
    }

    // Update Telemetry every 1 second
    if (now - lastSensorRead > 1000) {
        lastSensorRead = now;

        int ldrRaw = analogRead(CYD_LDR_PIN);
        int lightPct = map(constrain(ldrRaw, 100, 3800), 100, 3800, 0, 100);

        char teleBuf[64];
        snprintf(teleBuf, sizeof(teleBuf), "Free Heap: %u KB | LDR Ambient: %d%% (%d) | Uptime: %lus",
                 ESP.getFreeHeap() / 1024, lightPct, ldrRaw, now / 1000);
        tft.fillRect(18, 172, 440, 16, tft.color565(18, 28, 45));
        tft.setTextColor(TFT_WHITE, tft.color565(18, 28, 45));
        tft.drawString(teleBuf, 18, 172, 1);
    }

    // Touch Screen Interaction
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        if (p.z >= 500 && p.z < 3900) {
            int touchX = map(p.x, 200, 3800, 0, SCREEN_W);
            int touchY = map(p.y, 200, 3800, 0, SCREEN_H);
            touchX = constrain(touchX, 0, SCREEN_W - 1);
            touchY = constrain(touchY, 0, SCREEN_H - 1);

            if (now - lastTouchTime > 200) {
                lastTouchTime = now;
                touchCount++;
                lastTouchX = touchX;
                lastTouchY = touchY;

                triggerLED();

                // Draw touch ripple on screen
                uint16_t col = tftColors[ledColorIdx % 7];
                tft.drawCircle(touchX, touchY, 6, col);
                tft.drawCircle(touchX, touchY, 7, col);

                char tBuf[64];
                snprintf(tBuf, sizeof(tBuf), "Touch #%d: X:%d Y:%d (Raw Z:%d) | Active LED: %s     ",
                         touchCount, touchX, touchY, p.z, colorNames[ledColorIdx]);
                tft.fillRect(16, 290, 448, 18, tft.color565(15, 23, 42));
                tft.setTextColor(TFT_YELLOW, tft.color565(15, 23, 42));
                tft.drawString(tBuf, 16, 290, 2);
            }
        }
    }

    delay(20);
}
