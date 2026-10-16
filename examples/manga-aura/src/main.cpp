// ==============================================================================
// Example: Manga Ultra Instinct & Speedlines
// Hardware: ESP32-035 (CYD 3.5" / ST7796 320x480 or 480x320)
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>

#define CYD_BACKLIGHT   27
#define CYD_LED_RED     4
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_LDR_PIN     34

#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

#define SCREEN_W        480
#define SCREEN_H        320

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);

void setRGB(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

struct MangaRay { float angle; int rInner, rOuter; uint16_t color; };
MangaRay rays[28];
MangaRay oldRays[28];

void drawAnimeEye(int cx, int cy, bool isLeft, float pulse) {
    int dir = isLeft ? -1 : 1;
    tft.drawLine(cx - dir * 42, cy - 26, cx + dir * 36, cy - 14, TFT_WHITE);
    tft.drawLine(cx - dir * 42, cy - 25, cx + dir * 36, cy - 13, TFT_CYAN);
    tft.drawLine(cx - dir * 38, cy - 10, cx - dir * 10, cy - 15, TFT_WHITE);
    tft.drawLine(cx - dir * 10, cy - 15, cx + dir * 34, cy - 6, TFT_WHITE);
    tft.drawLine(cx - dir * 20, cy + 14, cx + dir * 24, cy + 12, TFT_WHITE);

    int irisRadius = 14 + (int)(pulse * 2.5f);
    tft.fillCircle(cx, cy, irisRadius, tft.color565(0, 180, 255));
    tft.drawCircle(cx, cy, irisRadius + 1, TFT_WHITE);
    tft.fillCircle(cx, cy, irisRadius - 4, tft.color565(120, 235, 255));
    tft.fillCircle(cx, cy, 5 + (int)(pulse * 1.5f), TFT_BLACK);
    tft.fillCircle(cx + dir * 4, cy - 4, 3, TFT_WHITE);
}

void setup() {
    Serial.begin(115200);
    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);
    pinMode(CYD_LED_RED, OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE, OUTPUT);
    setRGB(false, false, false);

    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    for (int i = 0; i < 28; i++) {
        rays[i].angle = (float)i * (6.28318f / 28.0f);
        rays[i].rInner = random(80, 120);
        rays[i].rOuter = random(200, 310);
        rays[i].color = (random(100) > 40) ? TFT_WHITE : TFT_CYAN;
        oldRays[i] = rays[i];
    }
}

void loop() {
    int eyeCY = 160;
    // Erase old rays
    for (int i = 0; i < 28; i++) {
        int x0 = 240 + (int)(cosf(oldRays[i].angle) * oldRays[i].rInner);
        int y0 = eyeCY + (int)(sinf(oldRays[i].angle) * oldRays[i].rInner);
        int x1 = 240 + (int)(cosf(oldRays[i].angle) * oldRays[i].rOuter);
        int y1 = eyeCY + (int)(sinf(oldRays[i].angle) * oldRays[i].rOuter);
        tft.drawLine(x0, y0, x1, y1, TFT_BLACK);
    }
    // Draw new rays
    for (int i = 0; i < 28; i++) {
        oldRays[i] = rays[i];
        rays[i].rInner = random(80, 120);
        rays[i].rOuter = random(200, 310);
        rays[i].color = (random(100) > 30) ? TFT_WHITE : TFT_CYAN;
        int x0 = 240 + (int)(cosf(rays[i].angle) * rays[i].rInner);
        int y0 = eyeCY + (int)(sinf(rays[i].angle) * rays[i].rInner);
        int x1 = 240 + (int)(cosf(rays[i].angle) * rays[i].rOuter);
        int y1 = eyeCY + (int)(sinf(rays[i].angle) * rays[i].rOuter);
        tft.drawLine(x0, y0, x1, y1, rays[i].color);
    }

    float pulse = sinf(millis() * 0.007f);
    drawAnimeEye(185, eyeCY, true, pulse);
    drawAnimeEye(295, eyeCY, false, pulse);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("GOGOGO (ゴゴゴ)", 20, 280, 2);
    tft.drawString("DODODO (ドドド)", 340, 280, 2);

    setRGB(false, pulse > 0.1f, true);
    delay(20);
}
