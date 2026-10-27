#pragma once
#include <stdint.h>

// One ESP-NOW broadcast from the sensor ESP32 to the CYD.
#define PACKET_MAGIC 0x50524B31  // "PRK1"

struct __attribute__((packed)) DistancePacket {
    uint32_t magic;
    uint32_t seq;
    int16_t  mm;      // distance in millimetres, -1 = no echo (nothing in range)
    uint8_t  motion;  // 1 = the PIR sensor sees someone moving
};
