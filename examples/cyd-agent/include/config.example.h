// Copy to config.h (git-ignored) and fill in. The ESP32 only joins 2.4 GHz networks.
// Without a config.h the pet still works: it just uses its built-in offline reactions.
#pragma once
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""
#define AGENT_HOST    "192.168.0.10"   // the computer running tools/agent_proxy.py
#define AGENT_PORT    8787
#define AGENT_TOKEN   ""               // same value as CYD_AGENT_TOKEN on the proxy
