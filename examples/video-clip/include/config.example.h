// Copy to config.h (git-ignored) to stream the clip over Wi-Fi from clip_server
// (Sipeed_Examples/cyd-stream) instead of playing the flash clip. Leave WIFI_SSID empty for flash only.
// The ESP32 only joins 2.4 GHz networks; if it can't join, it prints the networks it sees on serial.
#pragma once
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""
#define STREAM_HOST   "192.168.0.222"   // the machine running clip_server
#define STREAM_PORT   8765
