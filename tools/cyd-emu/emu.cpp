// =============================================================================
//  cyd-emu: run a CYD example on the desktop and capture screenshots.
//
//  The real TFT_eSPI library is compiled against stub Arduino/SPI headers.
//  Every SPI byte lands here, where a tiny ST7796 model decodes the command
//  stream (CASET / RASET / RAMWR) into a 480x320 RGB565 image - exactly what
//  the panel would show. Time is virtual, so runs are fast and repeatable.
//
//  usage: ./example_emu OUT_PREFIX MS[,MS...] [tap_ms ...]
//    writes OUT_PREFIX_<ms>.ppm at each capture time
// =============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "Arduino.h"
#include "SPI.h"
#include "WiFi.h"

void setup();
void loop();

HardwareSerial Serial;
EspClass ESP;
SPIClass SPI(VSPI);
WiFiClass WiFi;

// ------------------------------------------------------------------ clock
static unsigned long long nowUs = 0;
unsigned long millis() { return (unsigned long)(nowUs / 1000); }
unsigned long micros() { return (unsigned long)nowUs; }
void delay(unsigned long ms) { nowUs += ms * 1000ULL; }
void delayMicroseconds(unsigned int us) { nowUs += us; }

static unsigned long rngState = 1;
long random(long mx) {
    rngState = rngState * 1103515245UL + 12345UL;
    return mx > 0 ? (long)((rngState >> 8) % (unsigned long)mx) : 0;
}
long random(long mn, long mx) { return mx > mn ? mn + random(mx - mn) : mn; }
void randomSeed(unsigned long s) { rngState = s ? s : 1; }
void pinMode(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return HIGH; }
int analogRead(uint8_t) { return 2200; }  // LDR: bright room
void tone(uint8_t, unsigned int, unsigned long) {}
void noTone(uint8_t) {}

// ------------------------------------------------------------------ touch script
static std::vector<unsigned long> taps;
bool emu_touch_active() {
    unsigned long t = millis();
    for (unsigned long a : taps)
        if (t >= a && t < a + 120) return true;
    return false;
}

// ------------------------------------------------------------------ ST7796 model
#ifndef TFT_DC
#define TFT_DC 2
#endif
// MADCTL bit MV (0x20) swaps rows/columns: set = landscape 480x320, clear = portrait 320x480.
// Mirror bits are ignored on purpose: TFT_eSPI already sends logical coordinates.
static int W = 480, H = 320;
static uint16_t panel[480 * 480];
static bool dcData = true;
static uint8_t cmd = 0;
static int argIdx = 0;
static uint8_t args[4];
static int xs = 0, xe = W - 1, ys = 0, ye = H - 1, cx = 0, cy = 0;
static int pixHi = -1;
static long long pixelsWritten = 0;

void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin == TFT_DC) dcData = val == HIGH;
}

void emu_spi_byte(uint8_t b) {
    if (!dcData) {  // command byte
        cmd = b;
        argIdx = 0;
        pixHi = -1;
        if (cmd == 0x2C) { cx = xs; cy = ys; }
        return;
    }
    switch (cmd) {
        case 0x2A:
        case 0x2B:
            if (argIdx < 4) args[argIdx] = b;
            if (++argIdx == 4) {
                int a = (args[0] << 8) | args[1], e = (args[2] << 8) | args[3];
                if (cmd == 0x2A) { xs = a; xe = e; } else { ys = a; ye = e; }
            }
            break;
        case 0x2C:
            if (pixHi < 0) { pixHi = b; break; }
            {
                uint16_t c = (uint16_t)((pixHi << 8) | b);
                pixHi = -1;
                if (cx < W && cy < H && cx >= 0 && cy >= 0) panel[cy * W + cx] = c;
                pixelsWritten++;
                if (++cx > xe) { cx = xs; if (++cy > ye) cy = ys; }
            }
            break;
        case 0x36:
            if (argIdx++ == 0) {
                int nw = (b & 0x20) ? 480 : 320, nh = (b & 0x20) ? 320 : 480;
                if (nw != W) { W = nw; H = nh; memset(panel, 0, sizeof panel); }
            }
            break;
        default: break;
    }
}

static void savePPM(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint16_t c = panel[i];
        uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63),
                          (uint8_t)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

// virtual cost of one loop() pass, so loops without delay() still advance time
// (roughly what a busy frame costs on the real 40-80 MHz SPI bus)
static void chargeFrameTime(long long px) {
    unsigned long long us = 2000 + (unsigned long long)(px * 16 / 40);  // 16 bits @ 40 MHz
    nowUs += us;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s OUT_PREFIX MS[,MS...] [tap_ms ...]\n", argv[0]); return 1; }
    std::vector<unsigned long> shots;
    for (char* p = strtok(argv[2], ","); p; p = strtok(nullptr, ",")) shots.push_back(strtoul(p, nullptr, 10));
    for (int i = 3; i < argc; i++) taps.push_back(strtoul(argv[i], nullptr, 10));
    unsigned long last = 0;
    for (auto s : shots) last = s > last ? s : last;

    setup();
    size_t next = 0;
    while (next < shots.size()) {
        long long before = pixelsWritten;
        loop();
        chargeFrameTime(pixelsWritten - before);
        while (next < shots.size() && millis() >= shots[next]) {
            char path[512];
            snprintf(path, sizeof path, "%s_%lu.ppm", argv[1], shots[next]);
            savePPM(path);
            fprintf(stderr, "[emu] captured %s\n", path);
            next++;
        }
        if (millis() > last + 1000) break;
    }
    return 0;
}
