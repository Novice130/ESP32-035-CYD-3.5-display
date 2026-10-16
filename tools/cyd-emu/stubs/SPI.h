// SPI stub: every byte TFT_eSPI sends goes to the emulated ST7796 panel in emu.cpp.
#pragma once
#include "Arduino.h"

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3
#define HSPI 2
#define VSPI 3
#define FSPI 1

void emu_spi_byte(uint8_t b);  // emu.cpp

struct SPISettings {
    SPISettings() {}
    SPISettings(uint32_t, uint8_t, uint8_t) {}
};

class SPIClass {
   public:
    SPIClass(int bus = VSPI) : bus(bus) {}
    void begin(int8_t = -1, int8_t = -1, int8_t = -1, int8_t = -1) {}
    void end() {}
    void beginTransaction(SPISettings) {}
    void endTransaction() {}
    void setFrequency(uint32_t) {}
    void setHwCs(bool) {}
    void setBitOrder(uint8_t) {}
    void setDataMode(uint8_t) {}
    bool pins(int8_t, int8_t, int8_t, int8_t) { return true; }
    // touch controller shares the bus in hardware; only the display bus is emulated
    uint8_t transfer(uint8_t b) { if (bus != -1) emu_spi_byte(b); return 0; }
    uint16_t transfer16(uint16_t w) { transfer(w >> 8); transfer(w & 255); return 0; }
    uint32_t transfer32(uint32_t w) { transfer16(w >> 16); transfer16(w & 0xFFFF); return 0; }
    void transfer(void* buf, size_t n) { auto* p = (uint8_t*)buf; while (n--) transfer(*p++); }
    void transferBytes(const uint8_t* d, uint8_t* out, uint32_t n) { (void)out; while (n--) transfer(*d++); }
    void writeBytes(const uint8_t* d, uint32_t n) { while (n--) transfer(*d++); }
    void write(uint8_t b) { transfer(b); }
    void write16(uint16_t w) { transfer16(w); }
    void write32(uint32_t w) { transfer32(w); }
    void writePixels(const void* d, uint32_t n) { writeBytes((const uint8_t*)d, n); }
    int bus;
};
extern SPIClass SPI;
