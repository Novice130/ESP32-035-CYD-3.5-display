// ==============================================================================
// Example: Neo-Tokyo 2088 Synthwave Horizon
// Hardware: ESP32-035 (CYD 3.5" / ST7796 480x320)
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <math.h>

#define CYD_BACKLIGHT   27
#define CYD_LED_RED     4
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17

#define SCREEN_W        480
#define SCREEN_H        320
#define SYNTH_HORIZON   180

TFT_eSPI tft = TFT_eSPI();
float gridScroll = 0.0f;

void setRGB(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

void setup() {
    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);
    pinMode(CYD_LED_RED, OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE, OUTPUT);
    setRGB(false, false, false);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(tft.color565(10, 6, 24));

    // Mt. Fuji & Sun
    tft.fillTriangle(240, 95, 110, SYNTH_HORIZON, 370, SYNTH_HORIZON, tft.color565(25, 12, 50));
    tft.fillTriangle(240, 95, 210, 125, 270, 125, tft.color565(255, 160, 230));

    for (int r = 48; r > 0; r--) {
        tft.drawCircle(240, 105, r, tft.color565(255, map(r, 0, 48, 240, 20), 80));
    }
    for (int sl = 0; sl < 5; sl++) {
        tft.fillRect(180, 108 + (sl * 7), 120, 2 + sl / 2, tft.color565(10, 6, 24));
    }
    tft.drawFastHLine(0, SYNTH_HORIZON, SCREEN_W, tft.color565(255, 40, 180));
}

void loop() {
    gridScroll += 0.06f;
    if (gridScroll >= 1.0f) gridScroll -= 1.0f;

    tft.fillRect(0, SYNTH_HORIZON + 1, SCREEN_W, SCREEN_H - SYNTH_HORIZON - 1, tft.color565(8, 4, 18));

    int vpX = 240, vpY = SYNTH_HORIZON;
    for (int x = -100; x <= SCREEN_W + 100; x += 40) {
        tft.drawLine(vpX, vpY, x, SCREEN_H, tft.color565(240, 20, 160));
    }

    for (int i = 0; i < 7; i++) {
        float norm = ((float)i + gridScroll) / 7.0f;
        int gy = SYNTH_HORIZON + (int)(powf(norm, 2.2f) * (SCREEN_H - SYNTH_HORIZON));
        if (gy > SYNTH_HORIZON && gy < SCREEN_H) {
            tft.drawFastHLine(0, gy, SCREEN_W, tft.color565((uint8_t)(norm * 255), 255 - (uint8_t)(norm * 255), 255));
        }
    }

    tft.setTextColor(tft.color565(255, 40, 180), tft.color565(10, 6, 24));
    tft.drawString("NEO TOKYO 2088 // SYNTHWAVE", 20, 10, 2);

    setRGB(true, false, sinf(millis() * 0.005f) > 0);
    delay(20);
}
