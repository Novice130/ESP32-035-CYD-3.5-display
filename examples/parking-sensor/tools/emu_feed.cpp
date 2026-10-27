// Emulator only (tools/cyd-emu): plays the part of the sensor ESP32 so screenshots show
// real readings. No packets for the first 2 s (NO SIGNAL), then an object comes closer.
#include "Arduino.h"
#include "esp_now.h"
#include "packet.h"

struct Step { unsigned long fromMs; int16_t mm; };
static const Step SCRIPT[] = {
    {2000, -1},    // nothing in range: CLEAR, empty bar
    {4000, 1420},  // CLEAR
    {6000, 730},   // SLOW
    {8000, 340},   // CLOSE
    {10000, 85},   // STOP
};

void emu_tick() {
    static unsigned long nextSend = 0;
    static uint32_t seq = 0;
    unsigned long now = millis();
    if (now < SCRIPT[0].fromMs || now < nextSend) return;
    nextSend = now + 60;  // same rate as the real sender

    int16_t mm = SCRIPT[0].mm;
    for (const Step& s : SCRIPT)
        if (now >= s.fromMs) mm = s.mm;
    DistancePacket p = {PACKET_MAGIC, seq++, mm};
    emu_espnow_receive((const uint8_t*)&p, sizeof(p));
}
