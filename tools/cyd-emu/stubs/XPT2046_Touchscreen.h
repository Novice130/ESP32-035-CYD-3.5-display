// Touch stub: reports a press while the emulator's scripted tap is active.
#pragma once
#include "Arduino.h"
#include "SPI.h"

bool emu_touch_active();  // emu.cpp

class TS_Point {
   public:
    TS_Point() : x(0), y(0), z(0) {}
    TS_Point(int16_t x, int16_t y, int16_t z) : x(x), y(y), z(z) {}
    int16_t x, y, z;
};

class XPT2046_Touchscreen {
   public:
    XPT2046_Touchscreen(uint8_t, uint8_t = 255) {}
    bool begin(SPIClass& = SPI) { return true; }
    void setRotation(uint8_t) {}
    bool touched() { return emu_touch_active(); }
    bool tirqTouched() { return emu_touch_active(); }
    TS_Point getPoint() { return emu_touch_active() ? TS_Point(2000, 2000, 1200) : TS_Point(); }
};
