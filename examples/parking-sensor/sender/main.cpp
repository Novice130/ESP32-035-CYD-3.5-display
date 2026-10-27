// Parking sensor, sender side: plain ESP32 dev board + HC-SR04 ultrasonic sensor.
// Pings about 16 times a second, takes the median of the last 3 readings and
// broadcasts it over ESP-NOW. Every reading is also printed on the serial monitor.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "packet.h"

#define TRIG_PIN 15
#define ECHO_PIN 2             // also the on-board blue LED: it flickers with each echo
#define ECHO_TIMEOUT_US 25000  // ~4.3 m round trip; longer = nothing in range
#define PING_INTERVAL_MS 60    // HC-SR04 needs ~60 ms for old echoes to die out

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint32_t seq = 0;
static int16_t history[3] = {-1, -1, -1};

int16_t pingMm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    unsigned long us = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
    if (us == 0) return -1;
    return (int16_t)(us * 10 / 58);  // sound: 58 us per cm, there and back
}

int16_t median3(int16_t a, int16_t b, int16_t c) {
    if (a > b) { int16_t t = a; a = b; b = t; }
    if (b > c) { int16_t t = b; b = c; c = t; }
    return a > b ? a : b;
}

void setup() {
    Serial.begin(115200);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    WiFi.mode(WIFI_STA);  // ESP-NOW only, never joins a network
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] init failed");
        return;
    }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
    Serial.printf("[SONAR] ready, TRIG=%d ECHO=%d, MAC %s\n", TRIG_PIN, ECHO_PIN,
                  WiFi.macAddress().c_str());
}

void loop() {
    static unsigned long nextPing = 0;
    if (millis() < nextPing) return;
    nextPing = millis() + PING_INTERVAL_MS;

    history[seq % 3] = pingMm();
    int16_t mm = median3(history[0], history[1], history[2]);

    DistancePacket p = {PACKET_MAGIC, seq++, mm};
    esp_now_send(BROADCAST, (const uint8_t *)&p, sizeof(p));

    if (mm < 0) Serial.println("[SONAR] no echo");
    else Serial.printf("[SONAR] %.1f cm\n", mm / 10.0f);
}
