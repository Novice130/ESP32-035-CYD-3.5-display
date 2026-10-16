// ==============================================================================
// Example: Rasengan 3D Ki Energy Vortex
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

TFT_eSPI tft = TFT_eSPI();

struct KiPart { float angle, rad, speed, phi, dPhi; int oldX, oldY; uint16_t col; };
KiPart kiParts[90];
float shockR = 15.0f;

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

    for (int i = 0; i < 90; i++) {
        kiParts[i].rad = random(18, 90);
        kiParts[i].angle = random(0, 360) * 0.0174533f;
        kiParts[i].phi = random(-60, 60) * 0.0174533f;
        kiParts[i].speed = (random(20, 60) * 0.0018f) * ((i % 2 == 0) ? 1.0f : -1.0f);
        kiParts[i].dPhi = random(-20, 20) * 0.001f;
        kiParts[i].oldX = 240;
        kiParts[i].oldY = 160;
        kiParts[i].col = (random(100) > 40) ? TFT_CYAN : TFT_WHITE;
    }
}

void loop() {
    int cx = 240, cy = 160;

    if (shockR > 15 && shockR < 130) tft.drawCircle(cx, cy, (int)shockR, TFT_BLACK);
    shockR += 4.5f;
    if (shockR > 130.0f) shockR = 16.0f;
    tft.drawCircle(cx, cy, (int)shockR, tft.color565(0, map((int)shockR, 15, 130, 255, 40), 255));

    for (int i = 0; i < 90; i++) {
        tft.drawPixel(kiParts[i].oldX, kiParts[i].oldY, TFT_BLACK);
        kiParts[i].angle += kiParts[i].speed;
        kiParts[i].phi += kiParts[i].dPhi;
        if (kiParts[i].phi > 1.2f || kiParts[i].phi < -1.2f) kiParts[i].dPhi = -kiParts[i].dPhi;

        int px = cx + (int)(kiParts[i].rad * cosf(kiParts[i].angle) * cosf(kiParts[i].phi));
        int py = cy + (int)(kiParts[i].rad * sinf(kiParts[i].angle) * 0.55f + kiParts[i].rad * sinf(kiParts[i].phi) * 0.75f);
        kiParts[i].oldX = px;
        kiParts[i].oldY = py;
        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) tft.drawPixel(px, py, kiParts[i].col);
    }

    tft.fillCircle(cx, cy, 18, tft.color565(0, 200, 255));
    tft.fillCircle(cx, cy, 10, TFT_WHITE);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("RASENGAN KI SPHERE (螺旋丸)", 20, 10, 2);

    setRGB(false, true, random(100) > 40);
    delay(18);
}
