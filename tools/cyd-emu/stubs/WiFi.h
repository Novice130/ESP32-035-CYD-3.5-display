// WiFi stub: always offline (examples fall back to their demo/offline screens).
#pragma once
#include "Arduino.h"

typedef enum { WL_IDLE_STATUS = 0, WL_NO_SSID_AVAIL = 1, WL_CONNECTED = 3, WL_CONNECT_FAILED = 4, WL_DISCONNECTED = 6 } wl_status_t;
#define WIFI_STA 1
#define WIFI_OFF 0

class IPAddress {
   public:
    IPAddress(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0) : o{a, b, c, d} {}
    String toString() const { char s[20]; snprintf(s, sizeof s, "%d.%d.%d.%d", o[0], o[1], o[2], o[3]); return String(s); }
    uint8_t o[4];
};

class WiFiClass {
   public:
    void mode(int) {}
    void begin(const char*, const char* = nullptr) {}
    void disconnect(bool = false) {}
    void reconnect() {}
    wl_status_t status() { return WL_DISCONNECTED; }
    IPAddress localIP() { return IPAddress(); }
    IPAddress gatewayIP() { return IPAddress(); }
    int RSSI() { return 0; }
    String SSID() { return String(""); }
    String macAddress() { return String("24:6F:28:00:00:00"); }
};
extern WiFiClass WiFi;

class WiFiClient {
   public:
    void setTimeout(int) {}
    int connect(const char*, uint16_t, int = 0) { return 0; }
    void stop() {}
    bool connected() { return false; }
};
