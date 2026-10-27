// Emulator only (tools/cyd-emu): plays the part of the sensor ESP32 so screenshots show
// real readings. No packets for the first 2 s (NO SIGNAL), then an object comes closer;
// the PIR sees someone from 6 s on.
#include "Arduino.h"
#include "esp_now.h"
#include "packet.h"

struct Step { unsigned long fromMs; int16_t mm; uint8_t motion; };
static const Step SCRIPT[] = {
    {2000, -1, 0},    // nothing in range: CLEAR, empty bar
    {4000, 1420, 0},  // CLEAR
    {6000, 730, 1},   // SLOW, someone moving
    {8000, 340, 1},   // CLOSE
    {10000, 85, 1},   // STOP
};

void emu_tick() {
    static unsigned long nextSend = 0;
    static uint32_t seq = 0;
    unsigned long now = millis();
    if (now < SCRIPT[0].fromMs || now < nextSend) return;
    nextSend = now + 60;  // same rate as the real sender

    const Step* cur = &SCRIPT[0];
    for (const Step& s : SCRIPT)
        if (now >= s.fromMs) cur = &s;
    DistancePacket p = {PACKET_MAGIC, seq++, cur->mm, cur->motion};
    emu_espnow_receive((const uint8_t*)&p, sizeof(p));
}
