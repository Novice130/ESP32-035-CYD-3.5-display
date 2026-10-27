// ESP-NOW stub: nothing goes on air. Received packets come from the example's optional
// tools/emu_feed.cpp, which calls emu_espnow_receive() from emu_tick() on a scripted timeline.
#pragma once
#include "Arduino.h"

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_NOW_ETH_ALEN 6

typedef void (*esp_now_recv_cb_t)(const uint8_t* mac_addr, const uint8_t* data, int data_len);

typedef struct {
    uint8_t peer_addr[ESP_NOW_ETH_ALEN];
    uint8_t lmk[16];
    uint8_t channel;
    int ifidx;
    bool encrypt;
    void* priv;
} esp_now_peer_info_t;

inline esp_now_recv_cb_t emu_espnow_cb = nullptr;

static inline esp_err_t esp_now_init() { return ESP_OK; }
static inline esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t cb) { emu_espnow_cb = cb; return ESP_OK; }
static inline esp_err_t esp_now_add_peer(const esp_now_peer_info_t*) { return ESP_OK; }
static inline esp_err_t esp_now_send(const uint8_t*, const uint8_t*, size_t) { return ESP_OK; }

static inline void emu_espnow_receive(const uint8_t* data, int len) {
    static const uint8_t mac[ESP_NOW_ETH_ALEN] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x01};
    if (emu_espnow_cb) emu_espnow_cb(mac, data, len);
}
