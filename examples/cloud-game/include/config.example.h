// Copy to config.h (git-ignored) and fill in. The ESP32 only joins 2.4 GHz networks;
// if it can't join, it prints the networks it sees on serial.
#pragma once
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""
#define GAME_HOST     "192.168.0.222"   // the machine running the game server (e.g. flappy_server)
#define GAME_PORT     8766
