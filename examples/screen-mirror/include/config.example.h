// Copy to config.h (git-ignored) and fill in. The ESP32 only joins 2.4 GHz networks;
// if it can't join, it prints the networks it sees on serial.
#pragma once
#define WIFI_SSID       ""
#define WIFI_PASSWORD   ""
#define MIRROR_HOSTNAME "cyd"    // the Mac finds the board as cyd.local
#define MIRROR_PORT     8767
