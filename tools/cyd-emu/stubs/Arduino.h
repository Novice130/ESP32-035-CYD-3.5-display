// Minimal Arduino core for running CYD examples on a desktop (screenshot emulator).
// Time is virtual: it only advances through delay()/delayMicroseconds() and the
// emulator's per-loop frame cost, so runs are deterministic and fast.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <algorithm>
#include <string>

typedef uint8_t byte;
typedef bool boolean;

#define PROGMEM
#define PSTR(s) (s)
#define F(s) (s)
#define pgm_read_byte(a) (*(const uint8_t*)(a))
#define pgm_read_word(a) (*(const uint16_t*)(a))
// TFT_eSPI only uses dword reads for pointers into font tables
#define pgm_read_dword(a) (*(const uintptr_t*)(a))
#define pgm_read_ptr(a) (*(void* const*)(a))
#define IRAM_ATTR
#define ICACHE_RAM_ATTR

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LSBFIRST 0
#define MSBFIRST 1
#define PI 3.14159265358979f
#define DEG_TO_RAD 0.01745329f
#define RAD_TO_DEG 57.2957795f
#define TWO_PI 6.28318530718f
#define HALF_PI 1.57079632679f

using std::min;
using std::max;
template <class T, class L, class H> static inline T constrain(T x, L lo, H hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#define bitRead(v, b) (((v) >> (b)) & 1)
#define bit(b) (1UL << (b))
#define _BV(b) (1UL << (b))
#define yield()
#define digitalPinToBitMask(p) (1UL << ((p) & 31))
static inline char* ltoa(long v, char* buf, int base) {
    if (base == 10) { snprintf(buf, 24, "%ld", v); return buf; }
    char t[40]; int i = 0; unsigned long u = v < 0 && base == 10 ? -v : (unsigned long)v;
    do { int d = u % base; t[i++] = d < 10 ? '0' + d : 'a' + d - 10; u /= base; } while (u);
    int k = 0; while (i) buf[k++] = t[--i]; buf[k] = 0; return buf;
}
static inline char* ultoa(unsigned long v, char* buf, int base) { return ltoa((long)v, buf, base); }

// ---- provided by emu.cpp
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
void pinMode(uint8_t pin, uint8_t mode);
int analogRead(uint8_t pin);
void tone(uint8_t pin, unsigned int freq, unsigned long dur = 0);
void noTone(uint8_t pin);
long random(long max);
long random(long min, long max);
void randomSeed(unsigned long s);
static inline void analogWrite(uint8_t, int) {}
static inline void ledcSetup(int, int, int) {}
static inline void ledcAttachPin(int, int) {}
static inline void ledcWrite(int, int) {}
static inline void ledcWriteTone(int, int) {}

// ---- String (just enough for TFT_eSPI and the examples)
class String {
   public:
    std::string s;
    String() {}
    String(const char* c) : s(c ? c : "") {}
    String(const std::string& c) : s(c) {}
    String(char c) : s(1, c) {}
    String(int v) : s(std::to_string(v)) {}
    String(unsigned int v) : s(std::to_string(v)) {}
    String(long v) : s(std::to_string(v)) {}
    String(unsigned long v) : s(std::to_string(v)) {}
    String(float v, int dec = 2) { char b[32]; snprintf(b, sizeof b, "%.*f", dec, v); s = b; }
    String(double v, int dec = 2) { char b[32]; snprintf(b, sizeof b, "%.*f", dec, v); s = b; }
    const char* c_str() const { return s.c_str(); }
    unsigned int length() const { return (unsigned)s.size(); }
    void toCharArray(char* buf, unsigned int n) const { snprintf(buf, n, "%s", s.c_str()); }
    char charAt(unsigned int i) const { return i < s.size() ? s[i] : 0; }
    char operator[](unsigned int i) const { return charAt(i); }
    String operator+(const String& o) const { return String(s + o.s); }
    String& operator+=(const String& o) { s += o.s; return *this; }
    bool operator==(const String& o) const { return s == o.s; }
    bool operator!=(const String& o) const { return s != o.s; }
    String substring(unsigned a, unsigned b = 0xFFFFFFFF) const { return String(s.substr(a, b == 0xFFFFFFFF ? std::string::npos : b - a)); }
    int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
    int toInt() const { return atoi(s.c_str()); }
    void reserve(unsigned) {}
};
static inline String operator+(const char* a, const String& b) { return String(a) + b; }

// ---- Print / Serial
class Print {
   public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t* b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
    size_t print(const char* s) { size_t k = 0; while (s && *s) k += write((uint8_t)*s++); return k; }
    size_t print(const String& s) { return print(s.c_str()); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(int v) { return print(String(v)); }
    size_t print(unsigned int v) { return print(String(v)); }
    size_t print(long v) { return print(String(v)); }
    size_t print(unsigned long v) { return print(String(v)); }
    size_t print(double v, int d = 2) { return print(String(v, d)); }
    size_t println() { return print("\n"); }
    template <class T> size_t println(T v) { size_t k = print(v); return k + println(); }
    size_t printf(const char* fmt, ...) {
        char b[512]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); return print(b);
    }
};

class HardwareSerial : public Print {
   public:
    void begin(unsigned long) {}
    size_t write(uint8_t c) override { fputc(c, stderr); return 1; }
    int available() { return 0; }
    int read() { return -1; }
    operator bool() const { return true; }
};
extern HardwareSerial Serial;

// ---- ESP32 bits some examples touch
struct EspClass {
    uint32_t getFreeHeap() { return 214312; }
    uint32_t getMinFreeHeap() { return 198220; }
    uint32_t getHeapSize() { return 327680; }
    uint32_t getCpuFreqMHz() { return 240; }
    void restart() { exit(0); }
};
extern EspClass ESP;
static inline uint32_t esp_random() { return (uint32_t)random(0x7FFFFFFF); }

typedef void* TaskHandle_t;
typedef int BaseType_t;
#define pdPASS 1
#define portMAX_DELAY 0xFFFFFFFF
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(x) (x)
// background tasks are not run in the emulator (they'd block forever)
static inline BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, uint32_t, void*, int, TaskHandle_t*, int) { return pdPASS; }
static inline BaseType_t xTaskCreate(void (*)(void*), const char*, uint32_t, void*, int, TaskHandle_t*) { return pdPASS; }
static inline void vTaskDelay(uint32_t ms) { delay(ms); }
static inline void vTaskDelete(void*) {}
typedef void* SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (void*)1; }
static inline int xSemaphoreTake(SemaphoreHandle_t, uint32_t) { return 1; }
static inline int xSemaphoreGive(SemaphoreHandle_t) { return 1; }
