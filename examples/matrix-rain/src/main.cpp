// ==============================================================================
// Example: Cyber Katakana Matrix Rain
// Hardware: ESP32-035 (CYD 3.5" / ST7796 480x320)
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define CYD_BACKLIGHT   27
#define CYD_LED_RED     4
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17

#define SCREEN_W        480
#define SCREEN_H        320

TFT_eSPI tft = TFT_eSPI();

struct MatCol { float headY, speed; int trailLen; uint8_t idx; };
MatCol mCols[28];
int tick = 0;

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
    tft.fillScreen(TFT_BLACK);

    for (int c = 0; c < 28; c++) {
        mCols[c].headY = random(-280, 0);
        mCols[c].speed = random(5, 13) * 0.75f;
        mCols[c].trailLen = random(100, 220);
        mCols[c].idx = random(16);
    }
}

void loop() {
    tick++;
    for (int c = 0; c < 28; c++) {
        int colX = c * 17;
        int curY = (int)mCols[c].headY;
        int tailY = curY - mCols[c].trailLen;
        if (tailY >= 0 && tailY < SCREEN_H) tft.fillRect(colX, tailY, 16, 24, TFT_BLACK);

        mCols[c].headY += mCols[c].speed;
        int newY = (int)mCols[c].headY;
        if (newY >= 0 && newY < SCREEN_H) {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawChar(65 + (mCols[c].idx % 26), colX, newY, 2);
        }
        if (curY >= 0 && curY < SCREEN_H) {
            tft.setTextColor(tft.color565(0, 240, 80), TFT_BLACK);
            tft.drawChar(65 + ((mCols[c].idx + 1) % 26), colX, curY, 2);
        }
        if (mCols[c].headY - mCols[c].trailLen > SCREEN_H) {
            mCols[c].headY = random(-100, -20);
            mCols[c].speed = random(5, 13) * 0.75f;
        }
    }
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("CYBERSTREAM MATRIX v4.2", 20, 10, 2);

    setRGB(false, (tick % 6 < 4), false);
    delay(20);
}
