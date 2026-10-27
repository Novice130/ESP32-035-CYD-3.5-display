// Parking sensor, display side: CYD 3.5" (ST7796 480x320, XPT2046 touch).
// Receives the distance over ESP-NOW and shows it as a big number, a zone label and
// a bar. The RGB LED shows the zone, the speaker beeps faster as things get closer.
// Tap the screen to mute / unmute.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "packet.h"

#define CYD_BACKLIGHT   27
#define CYD_LED_RED     4   // RGB LED is active LOW
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_SPEAKER_PIN 26
#define XPT2046_CS      33

#define LINK_TIMEOUT_MS 1000  // no packet for this long = "NO SIGNAL"
#define BAR_RANGE_CM    200   // bar is empty at 200 cm and beyond, full at 0
#define BEEP_HZ         2400
#define BEEP_MS         50

TFT_eSPI tft;
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);

// Written by the ESP-NOW callback (Wi-Fi task), read by loop()
static volatile int16_t rxMm = -1;
static volatile unsigned long rxAt = 0;
static volatile bool rxAny = false;

static bool muted = false;

enum Zone { Z_NOSIGNAL, Z_NOECHO, Z_CLEAR, Z_SLOW, Z_CLOSE, Z_STOP };

struct ZoneStyle { const char *label; uint16_t color; bool r, g, b; };
static const ZoneStyle ZONES[] = {
    {"NO SIGNAL", TFT_DARKGREY, 0, 0, 1},
    {"CLEAR",     TFT_GREEN,    0, 1, 0},  // nothing within range
    {"CLEAR",     TFT_GREEN,    0, 1, 0},
    {"SLOW",      TFT_YELLOW,   1, 1, 0},
    {"CLOSE",     TFT_ORANGE,   1, 1, 0},
    {"STOP",      TFT_RED,      1, 0, 0},
};

Zone zoneFor(bool linked, int16_t mm) {
    if (!linked) return Z_NOSIGNAL;
    if (mm < 0) return Z_NOECHO;
    if (mm >= 1000) return Z_CLEAR;
    if (mm >= 500) return Z_SLOW;
    if (mm >= 200) return Z_CLOSE;
    return Z_STOP;
}

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(DistancePacket)) return;
    DistancePacket p;
    memcpy(&p, data, sizeof(p));
    if (p.magic != PACKET_MAGIC) return;
    rxMm = p.mm;
    rxAt = millis();
    rxAny = true;
}

void setLed(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

// -----------------------------------------------------------------------------
// DRAWING: only the parts that changed are redrawn, so the screen never flickers
// -----------------------------------------------------------------------------
void drawStatic() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("DISTANCE", 20, 14, 4);
    tft.drawRoundRect(18, 248, 444, 52, 8, TFT_DARKGREY);
}

void drawMute() {
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(muted ? TFT_RED : TFT_DARKGREY, TFT_BLACK);
    tft.setTextPadding(tft.textWidth("SOUND OFF", 2));
    tft.drawString(muted ? "SOUND OFF" : "SOUND ON", 462, 20, 2);
    tft.setTextPadding(0);
}

void drawNumber(int cm, uint16_t color) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextPadding(tft.textWidth("888", 8));
    if (cm < 0) tft.drawString("---", 330, 110, 8);
    else tft.drawNumber(cm, 330, 110, 8);
    tft.setTextPadding(0);
    tft.setTextDatum(BL_DATUM);
    tft.drawString("cm", 342, 140, 4);
}

void drawZone(Zone z) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ZONES[z].color, TFT_BLACK);
    tft.setTextPadding(440);
    tft.drawString(ZONES[z].label, 240, 205, 4);
    tft.setTextPadding(0);
}

void drawBar(int fill, uint16_t color) {  // fill: 0..440 px
    tft.fillRect(20, 250, fill, 48, color);
    tft.fillRect(20 + fill, 250, 440 - fill, 48, TFT_BLACK);
}

// -----------------------------------------------------------------------------
// TOUCH: any tap toggles mute
// -----------------------------------------------------------------------------
bool touchDown() {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    return p.z >= 500 && p.z < 3900;  // reject phantom/noise readings
}

bool tapped() {
    static bool wasDown = false;
    static unsigned long lastDown = 0;
    bool down = touchDown();
    unsigned long now = millis();
    bool tap = down && !wasDown && now - lastDown > 200;
    if (down) lastDown = now;
    wasDown = down;
    return tap;
}

void setup() {
    Serial.begin(115200);

    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);
    pinMode(CYD_LED_RED, OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE, OUTPUT);
    setLed(0, 0, 0);
    pinMode(CYD_SPEAKER_PIN, OUTPUT);
    digitalWrite(CYD_SPEAKER_PIN, LOW);
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    tft.init();
    tft.setRotation(1);
    touchSPI.begin(14, 12, 13, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    drawStatic();
    drawMute();

    WiFi.mode(WIFI_STA);  // ESP-NOW only, never joins a network
    if (esp_now_init() != ESP_OK) Serial.println("[ESPNOW] init failed");
    esp_now_register_recv_cb(onReceive);
    Serial.printf("[PARK] display ready, MAC %s\n", WiFi.macAddress().c_str());
}

void loop() {
    static int shownCm = -2;
    static Zone shownZone = (Zone)-1;
    static int shownBar = -1;
    static unsigned long nextBeep = 0, nextLog = 0;
    unsigned long now = millis();

    if (tapped()) {
        muted = !muted;
        drawMute();
    }

    bool linked = rxAny && now - rxAt < LINK_TIMEOUT_MS;
    int16_t mm = rxMm;
    Zone z = zoneFor(linked, mm);
    int cm = (linked && mm >= 0) ? (mm + 5) / 10 : -1;
    int bar = (cm < 0 || cm >= BAR_RANGE_CM) ? 0 : 440 - cm * 440 / BAR_RANGE_CM;

    if (cm != shownCm || z != shownZone) drawNumber(cm, ZONES[z].color);
    if (z != shownZone) {
        drawZone(z);
        setLed(ZONES[z].r, ZONES[z].g, ZONES[z].b);
    }
    if (bar != shownBar || z != shownZone) drawBar(bar, ZONES[z].color);
    shownCm = cm;
    shownZone = z;
    shownBar = bar;

    // Beep every (cm x 8) ms below 1 m: ~800 ms at 99 cm, nearly solid under 10 cm
    if (!muted && cm >= 0 && cm < 100 && now >= nextBeep) {
        tone(CYD_SPEAKER_PIN, BEEP_HZ, BEEP_MS);
        nextBeep = now + max(cm * 8, BEEP_MS + 20);
    }

    if (now >= nextLog) {
        nextLog = now + 1000;
        if (linked) Serial.printf("[PARK] %d cm (%s)\n", cm, ZONES[z].label);
        else Serial.println("[PARK] no signal from sender");
    }
    delay(10);
}
