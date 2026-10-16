// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R) Multi-Screen Dashboard
// Role:    NOC IT Infrastructure & Live Network Diagnostics Monitor
// Hardware: ESP32-WROOM-32, 3.5" 320x480 TFT (ST7796), XPT2046 Touch, RGB LED, LDR
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>

// Configuration (Uses private config.h if present, otherwise config.example.h)
#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

// --- PIN DEFINITIONS FOR CYD 3.5" (ESP32-035) ---
#define CYD_BACKLIGHT   27   // Backlight is GPIO 27 (Drives transistor Q2)
#define CYD_LED_RED     4
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_LDR_PIN     34

// Touch Screen SPI (Shares HSPI bus: CLK=14, MISO=12, MOSI=13)
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

// Portrait Screen Dimensions
#define SCREEN_W        320
#define SCREEN_H        480

// UI Screens
enum ScreenState {
    SCREEN_DASHBOARD = 0,
    SCREEN_DIAGNOSTICS = 1
};

struct ServerNode {
    const char* name;
    const char* ip;
    uint16_t port;
    const char* role;
    bool isOnline;
    int latencyMs;
};

// Server Nodes definition
ServerNode nodes[4] = {
    {NODE_1_NAME, NODE_1_IP, NODE_1_PORT, NODE_1_ROLE, false, -1},
    {NODE_2_NAME, NODE_2_IP, NODE_2_PORT, NODE_2_ROLE, false, -1},
    {NODE_3_NAME, NODE_3_IP, NODE_3_PORT, NODE_3_ROLE, false, -1},
    {NODE_4_NAME, NODE_4_IP, NODE_4_PORT, NODE_4_ROLE, false, -1}
};

// Hardware Instances
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
// Use Polling Mode (omit IRQ pin) to prevent floating interrupt loops on GPIO 36
XPT2046_Touchscreen ts(XPT2046_CS);

// Global State
ScreenState currentScreen = SCREEN_DASHBOARD;
unsigned long lastSensorRead = 0;
unsigned long lastTouchTime = 0;
unsigned long ledTurnOffTime = 0;
bool ledIsActive = false;
int touchColorIndex = 0;
int touchCount = 0;
int lastTouchX = -1;
int lastTouchY = -1;

// Internet reachability via Cloudflare/Google DNS
bool internetOnline = false;
int internetLatencyMs = -1;
wl_status_t lastReportedWifiStatus = WL_IDLE_STATUS;

// Color Palette for RGB LED
const char* colorNames[] = {"RED", "GREEN", "BLUE", "CYAN", "YELLOW", "MAGENTA", "WHITE"};
const uint16_t tftColors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_CYAN, TFT_YELLOW, TFT_MAGENTA, TFT_WHITE};

// Control Onboard RGB LED (Active LOW: 0 = ON, 1 = OFF)
void setRGB(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

void triggerLEDFlash() {
    touchColorIndex = (touchColorIndex + 1) % 7;
    bool r = (touchColorIndex == 0 || touchColorIndex == 4 || touchColorIndex == 5 || touchColorIndex == 6);
    bool g = (touchColorIndex == 1 || touchColorIndex == 3 || touchColorIndex == 4 || touchColorIndex == 6);
    bool b = (touchColorIndex == 2 || touchColorIndex == 3 || touchColorIndex == 5 || touchColorIndex == 6);
    setRGB(r, g, b);
    ledIsActive = true;
    ledTurnOffTime = millis() + 450; // Keep illuminated for 450ms
    Serial.printf("[RGB LED] Flashed color: %s\n", colorNames[touchColorIndex]);
}

// -----------------------------------------------------------------------------
// BACKGROUND NETWORK TASK (Runs independently on ESP32 Core 0)
// -----------------------------------------------------------------------------
void networkTask(void* pvParameters) {
    if (strlen(WIFI_SSID) > 0) {
        Serial.printf("[WiFi] Initializing station connection to '%s'...\n", WIFI_SSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    bool wasConnected = false;
    int checkCycles = 0;

    for (;;) {
        checkCycles++;
        wl_status_t s = WiFi.status();

        if (s == WL_CONNECTED) {
            if (!wasConnected) {
                wasConnected = true;
                Serial.printf("\n[WiFi] CONNECTED to '%s'!\n", WIFI_SSID);
                Serial.printf("[WiFi] Local IP:   %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("[WiFi] Gateway IP: %s\n", WiFi.gatewayIP().toString().c_str());
                Serial.printf("[WiFi] Signal:     %d dBm\n\n", WiFi.RSSI());
            }

            // 1. Probe Public Internet via DNS port (1.1.1.1:53)
            WiFiClient testClient;
            testClient.setTimeout(1);
            unsigned long tStart = millis();
            if (testClient.connect("1.1.1.1", 53, 1200)) {
                internetOnline = true;
                internetLatencyMs = millis() - tStart;
                testClient.stop();
                Serial.printf("[Network] Internet ONLINE (%d ms)\n", internetLatencyMs);
            } else {
                internetOnline = false;
                internetLatencyMs = -1;
                Serial.println("[Network] Internet probe (1.1.1.1:53) timed out.");
            }

            // 2. Probe Infrastructure Nodes
            for (int i = 0; i < 4; i++) {
                WiFiClient nodeClient;
                nodeClient.setTimeout(1);
                unsigned long nStart = millis();
                if (nodeClient.connect(nodes[i].ip, nodes[i].port, 300)) {
                    nodes[i].isOnline = true;
                    nodes[i].latencyMs = millis() - nStart;
                    nodeClient.stop();
                } else {
                    nodes[i].isOnline = false;
                    nodes[i].latencyMs = -1;
                }
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else {
            if (wasConnected) {
                wasConnected = false;
                Serial.println("[WiFi] Connection lost. Reconnecting...");
            }
            if (checkCycles % 5 == 0) {
                Serial.printf("[WiFi] Connecting to '%s'... Status: %d\n", WIFI_SSID, (int)s);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(4000)); // Probe cycle every 4 seconds
    }
}

// -----------------------------------------------------------------------------
// UI RENDERING: SCREEN 0 - DASHBOARD OVERVIEW (320x480 Portrait)
// -----------------------------------------------------------------------------
void drawHeaderWiFiStatus() {
    tft.fillRect(145, 6, 98, 26, tft.color565(15, 23, 42));
    if (WiFi.status() == WL_CONNECTED) {
        tft.fillCircle(155, 19, 4, TFT_GREEN);
        tft.setTextColor(TFT_GREEN, tft.color565(15, 23, 42));
        tft.drawString("Hotspot OK", 164, 11, 2);
    } else if (strlen(WIFI_SSID) > 0) {
        tft.fillCircle(155, 19, 4, TFT_YELLOW);
        tft.setTextColor(TFT_YELLOW, tft.color565(15, 23, 42));
        tft.drawString("Connect...", 164, 11, 2);
    } else {
        tft.fillCircle(155, 19, 4, tft.color565(100, 116, 139));
        tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
        tft.drawString("Standby", 164, 11, 2);
    }
}

void drawServerCard(int y, const ServerNode& node) {
    tft.fillRoundRect(8, y, 304, 42, 5, tft.color565(20, 26, 36));
    tft.drawRoundRect(8, y, 304, 42, 5, tft.color565(45, 55, 72));

    // Server Name
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));
    tft.drawString(node.name, 14, y + 6, 2);

    // IP & Role
    char descBuf[48];
    snprintf(descBuf, sizeof(descBuf), "%s | %s", node.ip, node.role);
    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(20, 26, 36));
    tft.drawString(descBuf, 14, y + 24, 1);

    // Determine status badge color and text
    uint16_t statusColor;
    char statusBuf[20];

    if (WiFi.status() == WL_CONNECTED) {
        if (node.isOnline) {
            statusColor = TFT_GREEN;
            snprintf(statusBuf, sizeof(statusBuf), "ON %dms", node.latencyMs);
        } else {
            statusColor = tft.color565(225, 29, 72); // Rose Red
            snprintf(statusBuf, sizeof(statusBuf), "OFFLINE");
        }
    } else {
        statusColor = tft.color565(56, 189, 248); // Cyan Standby
        snprintf(statusBuf, sizeof(statusBuf), "STANDBY");
    }

    // Status Pill on right of card
    tft.fillRoundRect(226, y + 9, 80, 24, 12, statusColor);
    tft.setTextColor(TFT_BLACK, statusColor);
    tft.drawCentreString(statusBuf, 266, y + 13, 2);
}

void drawInternetCard(int y) {
    tft.fillRoundRect(8, y, 304, 40, 5, tft.color565(18, 30, 49));
    tft.drawRoundRect(8, y, 304, 40, 5, tft.color565(56, 189, 248));

    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(18, 30, 49));
    tft.drawString("Hotspot Internet Gateway", 14, y + 6, 2);

    tft.setTextColor(tft.color565(160, 174, 192), tft.color565(18, 30, 49));
    tft.drawString("Public DNS Probe: 1.1.1.1:53", 14, y + 23, 1);

    uint16_t statusColor = internetOnline ? TFT_GREEN : tft.color565(234, 179, 8);
    const char* statusTxt = internetOnline ? "ONLINE" : "WAITING";

    tft.fillRoundRect(226, y + 8, 80, 24, 12, statusColor);
    tft.setTextColor(TFT_BLACK, statusColor);
    tft.drawCentreString(statusTxt, 266, y + 12, 2);
}

void drawDashboard() {
    tft.fillScreen(tft.color565(10, 14, 23));

    // Header Banner
    tft.fillRect(0, 0, SCREEN_W, 38, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 38, SCREEN_W, tft.color565(56, 189, 248));

    // Title
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(15, 23, 42));
    tft.drawString(NOC_TITLE, 8, 11, 2);

    // Live WiFi Status Indicator
    drawHeaderWiFiStatus();

    // Switch Screen Button in Top Right
    tft.fillRoundRect(250, 6, 62, 26, 4, tft.color565(30, 41, 59));
    tft.drawRoundRect(250, 6, 62, 26, 4, tft.color565(56, 189, 248));
    tft.setTextColor(TFT_WHITE, tft.color565(30, 41, 59));
    tft.drawCentreString("Info >", 281, 11, 2);

    // 4 Server Cards
    for (int i = 0; i < 4; i++) {
        drawServerCard(46 + (i * 47), nodes[i]);
    }

    // 5th Card: Live Hotspot Internet Gateway Status
    drawInternetCard(236);

    // Bottom Telemetry / Interactive Bar (y = 282 to 480)
    tft.fillRect(0, 282, SCREEN_W, 198, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 282, SCREEN_W, tft.color565(45, 55, 72));

    // Domain & Uptime
    char domBuf[36];
    snprintf(domBuf, sizeof(domBuf), "Domain: %s", DOMAIN_NAME);
    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
    tft.drawString(domBuf, 12, 290, 2);
    tft.drawString("Uptime: 00:00:00", 170, 290, 2);

    // LDR Room Light Sensor
    tft.drawString("LDR Light: ---", 12, 312, 2);

    // Touch Feedback Banner
    tft.setTextColor(TFT_YELLOW, tft.color565(15, 23, 42));
    tft.drawString("Tap button or cover LDR to test LED", 12, 334, 2);

    // Dedicated LED Tap Button (Centered at bottom)
    tft.fillRoundRect(16, 364, 288, 56, 8, tft.color565(24, 32, 47));
    tft.drawRoundRect(16, 364, 288, 56, 8, tft.color565(56, 189, 248));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(24, 32, 47));
    tft.drawCentreString("TAP TO FLASH ONBOARD LED", 160, 375, 2);
    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(24, 32, 47));
    tft.drawCentreString("(Cycles 7 Colors or cover corner sensor)", 160, 396, 1);

    tft.setTextColor(tft.color565(100, 116, 139), tft.color565(15, 23, 42));
    tft.drawCentreString("ESP32-035  *  ST7796 320x480", 160, 442, 1);
}

// -----------------------------------------------------------------------------
// UI RENDERING: SCREEN 1 - HARDWARE & NETWORK DIAGNOSTICS (320x480)
// -----------------------------------------------------------------------------
void drawDiagnostics() {
    tft.fillScreen(tft.color565(10, 14, 23));

    // Header Banner
    tft.fillRect(0, 0, SCREEN_W, 38, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 38, SCREEN_W, tft.color565(56, 189, 248));

    // Back Button in Top Left
    tft.fillRoundRect(8, 6, 68, 26, 4, tft.color565(30, 41, 59));
    tft.drawRoundRect(8, 6, 68, 26, 4, tft.color565(56, 189, 248));
    tft.setTextColor(TFT_WHITE, tft.color565(30, 41, 59));
    tft.drawCentreString("< Back", 42, 11, 2);

    // Title
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(15, 23, 42));
    tft.drawString("SYSTEM DIAGNOSTICS", 86, 11, 2);

    // Card 1: Board & MCU
    tft.fillRoundRect(8, 46, 304, 76, 5, tft.color565(20, 26, 36));
    tft.drawRoundRect(8, 46, 304, 76, 5, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("HARDWARE SPECS", 16, 52, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));
    tft.drawString("Board: ESP32-035 (3.5\" CYD)", 16, 68, 1);
    tft.drawString("SoC: ESP32 Dual-Core @ 240MHz | 4MB Flash", 16, 82, 1);
    tft.drawString("Display: ST7796 320x480 on HSPI 40MHz", 16, 96, 1);
    tft.drawString("Backlight: GPIO 27 (Transistor Q2)", 16, 108, 1);

    // Card 2: Live Network / WiFi
    tft.fillRoundRect(8, 130, 304, 86, 5, tft.color565(20, 26, 36));
    tft.drawRoundRect(8, 130, 304, 86, 5, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("LIVE NETWORK TELEMETRY", 16, 136, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));

    if (WiFi.status() == WL_CONNECTED) {
        char ipBuf[40];
        snprintf(ipBuf, sizeof(ipBuf), "IP: %s", WiFi.localIP().toString().c_str());
        tft.drawString(ipBuf, 16, 154, 1);

        char rssiBuf[48];
        snprintf(rssiBuf, sizeof(rssiBuf), "Hotspot: %s (%d dBm)", WIFI_SSID, WiFi.RSSI());
        tft.drawString(rssiBuf, 16, 168, 1);

        char gwBuf[40];
        snprintf(gwBuf, sizeof(gwBuf), "Gateway: %s", WiFi.gatewayIP().toString().c_str());
        tft.drawString(gwBuf, 16, 182, 1);

        char netBuf[40];
        snprintf(netBuf, sizeof(netBuf), "Internet: %s (%d ms)", internetOnline ? "ONLINE" : "WAITING", internetLatencyMs);
        tft.setTextColor(internetOnline ? TFT_GREEN : TFT_YELLOW, tft.color565(20, 26, 36));
        tft.drawString(netBuf, 16, 196, 1);
    } else {
        tft.drawString("Status: Connecting to Hotspot...", 16, 154, 1);
        char ssidBuf[40];
        snprintf(ssidBuf, sizeof(ssidBuf), "Target SSID: %s", WIFI_SSID);
        tft.drawString(ssidBuf, 16, 168, 1);
        tft.drawString("Note: Hotspot must be 2.4 GHz band", 16, 184, 1);
    }

    // Card 3: Memory & Runtime
    tft.fillRoundRect(8, 224, 304, 64, 5, tft.color565(20, 26, 36));
    tft.drawRoundRect(8, 224, 304, 64, 5, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("SYSTEM HEALTH", 16, 230, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));

    char heapBuf[48];
    snprintf(heapBuf, sizeof(heapBuf), "Free Heap RAM: %u KB (Min: %u KB)", ESP.getFreeHeap() / 1024, ESP.getMinFreeHeap() / 1024);
    tft.drawString(heapBuf, 16, 248, 1);
    tft.drawString("Tasks: Core 1 (UI) & Core 0 (Network)", 16, 262, 1);

    // Interactive Touch Pad at bottom
    tft.fillRoundRect(8, 296, 304, 164, 5, tft.color565(24, 32, 47));
    tft.drawRoundRect(8, 296, 304, 164, 5, tft.color565(56, 189, 248));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(24, 32, 47));
    tft.drawString("INTERACTIVE TOUCH & OPTICAL SENSOR", 16, 304, 2);

    tft.setTextColor(TFT_WHITE, tft.color565(24, 32, 47));
    tft.drawString("Press screen or cover LDR sensor to test LED", 16, 324, 1);

    char touchInfo[64];
    if (lastTouchX >= 0) {
        snprintf(touchInfo, sizeof(touchInfo), "Last Valid Tap: X:%d Y:%d", lastTouchX, lastTouchY);
    } else {
        snprintf(touchInfo, sizeof(touchInfo), "Touch Ready (Crack filter active)");
    }
    tft.setTextColor(TFT_YELLOW, tft.color565(24, 32, 47));
    tft.drawString(touchInfo, 16, 344, 2);

    char colInfo[48];
    snprintf(colInfo, sizeof(colInfo), "Active Color: %s", colorNames[touchColorIndex]);
    tft.setTextColor(tftColors[touchColorIndex % 7], tft.color565(24, 32, 47));
    tft.drawString(colInfo, 16, 368, 2);

    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(24, 32, 47));
    tft.drawCentreString("LED turns ON only on intentional tap", 160, 420, 1);
}

// -----------------------------------------------------------------------------
// SETUP & INITIALIZATION
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Initializing Clean Portrait Dashboard...");

    // 1. Backlight: GPIO 27 on ESP32-035 (Drives Transistor Q2)
    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);
    Serial.println("[ESP32-035] Backlight GPIO 27 set HIGH.");

    // 2. Configure Onboard RGB LED (Turn OFF initially - NO continuous blinking)
    pinMode(CYD_LED_RED,   OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE,  OUTPUT);
    setRGB(false, false, false); // All OFF
    Serial.println("[ESP32-035] RGB LED configured (OFF by default).");

    // 3. Configure LDR (Light Sensor)
    pinMode(CYD_LDR_PIN, INPUT);

    // 4. Ensure Touch CS is deselected before initializing TFT
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    // 5. Initialize TFT Display in Native Portrait (320x480)
    tft.init();
    tft.setRotation(0); // 0 = Native Portrait 320x480
    Serial.printf("[ESP32-035] TFT Initialized: %dx%d\n", tft.width(), tft.height());

    // 6. Initialize Touch Screen (XPT2046 on shared HSPI in Polling Mode)
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(0); // Match Portrait orientation
    Serial.println("[ESP32-035] Touch Initialized in Polling Mode.");

    // 7. Render Initial Dashboard Screen
    drawDashboard();
    Serial.println("[ESP32-035] Dashboard Rendered.");

    // 8. Launch Background Network Task on Core 0
    xTaskCreatePinnedToCore(
        networkTask,
        "NetworkTask",
        4096,
        NULL,
        1,
        NULL,
        0
    );
    Serial.println("[ESP32-035] Network Task Launched on Core 0.");
}

// -----------------------------------------------------------------------------
// MAIN LOOP (Runs on Core 1)
// -----------------------------------------------------------------------------
void loop() {
    unsigned long now = millis();

    // 1. Turn off RGB LED after tap duration expires
    if (ledIsActive && now > ledTurnOffTime) {
        setRGB(false, false, false);
        ledIsActive = false;
    }

    // 2. Optical Sensor Tap Detection via LDR (Photoresistor on GPIO 34)
    // If the touch digitizer has physical glass damage, covering the corner sensor functions as an optical tap!
    static bool ldrWasCovered = true;
    int ldrRaw = analogRead(CYD_LDR_PIN);
    if (ldrRaw < 400 && !ldrWasCovered && (now - lastTouchTime > 400)) {
        ldrWasCovered = true;
        lastTouchTime = now;
        Serial.printf("[Optical Tap] LDR covered (Raw: %d) -> Triggering LED!\n", ldrRaw);
        triggerLEDFlash();

        if (currentScreen == SCREEN_DASHBOARD) {
            char ldrTapBuf[48];
            snprintf(ldrTapBuf, sizeof(ldrTapBuf), "Optical Tap! LED: %s     ", colorNames[touchColorIndex]);
            tft.setTextColor(tftColors[touchColorIndex % 7], tft.color565(15, 23, 42));
            tft.drawString(ldrTapBuf, 12, 334, 2);
        }
    } else if (ldrRaw > 650) {
        ldrWasCovered = false;
    }

    // 3. Update Live Telemetry & Badges (every 1 second)
    if (now - lastSensorRead > 1000) {
        lastSensorRead = now;

        int lightPct = map(constrain(ldrRaw, 100, 3800), 100, 3800, 0, 100);

        if (currentScreen == SCREEN_DASHBOARD) {
            // Update WiFi Status badge if changed
            wl_status_t curWifi = WiFi.status();
            if (curWifi != lastReportedWifiStatus) {
                lastReportedWifiStatus = curWifi;
                drawHeaderWiFiStatus();
            }

            // Update LDR display
            char ldrBuf[40];
            snprintf(ldrBuf, sizeof(ldrBuf), "LDR Light: %d%% (%d)   ", lightPct, ldrRaw);
            tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
            tft.drawString(ldrBuf, 12, 312, 2);

            // Update Uptime Clock
            unsigned long sec = now / 1000;
            unsigned long s = sec % 60;
            unsigned long m = (sec / 60) % 60;
            unsigned long h = (sec / 3600);
            char upBuf[32];
            snprintf(upBuf, sizeof(upBuf), "Uptime: %02lu:%02lu:%02lu", h, m, s);
            tft.drawString(upBuf, 170, 290, 2);

            // Re-render server cards to update live ONLINE/OFFLINE status
            for (int i = 0; i < 4; i++) {
                drawServerCard(46 + (i * 47), nodes[i]);
            }

            // Re-render internet card
            drawInternetCard(236);
        }
    }

    // 4. Clean Touch Handling with Hardware Crack Deadzone Filtering
    if (ts.touched()) {
        TS_Point p = ts.getPoint();

        // Check if there is actual pressure
        if (p.z >= 500) {
            // Map raw coordinates to 320x480 Portrait
            int touchX = map(p.x, 200, 3800, 0, SCREEN_W);
            int touchY = map(p.y, 200, 3800, 0, SCREEN_H);
            touchX = constrain(touchX, 0, SCREEN_W - 1);
            touchY = constrain(touchY, 0, SCREEN_H - 1);

            // CRACK DEADZONE FILTER:
            // A physical fracture on the glass causes a continuous short circuit at X: 20..70, Y: 195..230 with Z ~ 4100.
            // Any reading in this hardware fault window is discarded so the LED does NOT blink continuously.
            bool isCrackShort = (touchX >= 10 && touchX <= 85 && touchY >= 180 && touchY <= 245);

            if (!isCrackShort && (now - lastTouchTime > 250)) {
                lastTouchTime = now;
                touchCount++;
                lastTouchX = touchX;
                lastTouchY = touchY;

                Serial.printf("[Touch #%d] X:%d Y:%d Z:%d\n", touchCount, touchX, touchY, p.z);

                // NAVIGATION: SCREEN 0 (DASHBOARD)
                if (currentScreen == SCREEN_DASHBOARD) {
                    // Check top-right button "Info >" (x >= 235, y <= 45)
                    if (touchX >= 235 && touchY <= 45) {
                        currentScreen = SCREEN_DIAGNOSTICS;
                        drawDiagnostics();
                        delay(150);
                        return;
                    }

                    // Check dedicated bottom LED Tap Button (x: 16..304, y: 360..425)
                    if (touchY >= 360 && touchY <= 430 && touchX >= 15 && touchX <= 305) {
                        triggerLEDFlash();
                    }

                    // Visual touch ripple marker on the screen
                    uint16_t activeCol = tftColors[touchColorIndex % 7];
                    tft.drawCircle(touchX, touchY, 6, activeCol);
                    tft.drawCircle(touchX, touchY, 7, activeCol);

                    // Update touch banner
                    char touchBuf[48];
                    snprintf(touchBuf, sizeof(touchBuf), "Touch #%d: X:%d Y:%d (LED: %s)  ", touchCount, touchX, touchY, colorNames[touchColorIndex]);
                    tft.setTextColor(TFT_YELLOW, tft.color565(15, 23, 42));
                    tft.drawString(touchBuf, 12, 334, 2);
                }
                // NAVIGATION: SCREEN 1 (DIAGNOSTICS)
                else if (currentScreen == SCREEN_DIAGNOSTICS) {
                    // Check top-left button "< Back" (x <= 90, y <= 45)
                    if (touchX <= 90 && touchY <= 45) {
                        currentScreen = SCREEN_DASHBOARD;
                        drawDashboard();
                        delay(150);
                        return;
                    }

                    // Bottom pad tap
                    triggerLEDFlash();
                    uint16_t activeCol = tftColors[touchColorIndex % 7];
                    tft.drawCircle(touchX, touchY, 6, activeCol);
                    tft.drawCircle(touchX, touchY, 7, activeCol);

                    char touchInfo[64];
                    snprintf(touchInfo, sizeof(touchInfo), "Touch #%d: X:%d Y:%d (Valid)    ", touchCount, touchX, touchY);
                    tft.setTextColor(TFT_YELLOW, tft.color565(24, 32, 47));
                    tft.drawString(touchInfo, 16, 344, 2);

                    char colInfo[48];
                    snprintf(colInfo, sizeof(colInfo), "Active Color: %s      ", colorNames[touchColorIndex]);
                    tft.setTextColor(tftColors[touchColorIndex % 7], tft.color565(24, 32, 47));
                    tft.drawString(colInfo, 16, 368, 2);
                }
            }
        }
    }
}
