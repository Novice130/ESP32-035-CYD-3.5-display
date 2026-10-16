// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R) Multi-Screen Dashboard
// Role:    NOC IT Infrastructure & Live Network Diagnostics Monitor
// Hardware: ESP32-WROOM-32, 3.5" 480x320 TFT (ST7796), XPT2046 Touch, RGB LED, LDR
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

// Touch Screen SPI (Shares the HSPI bus on ESP32-035: CLK=14, MISO=12, MOSI=13)
#define XPT2046_IRQ     36
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

// Screen Dimensions
#define SCREEN_W        480
#define SCREEN_H        320

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
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// Global State
ScreenState currentScreen = SCREEN_DASHBOARD;
unsigned long lastSensorRead = 0;
unsigned long ledTurnOffTime = 0;
bool ledIsActive = false;
int touchColorIndex = 0;
int touchCount = 0;
int lastTouchX = -1;
int lastTouchY = -1;

// Internet reachability
bool internetOnline = false;
int internetLatencyMs = -1;

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
    ledTurnOffTime = millis() + 350; // Keep illuminated for 350ms
}

// -----------------------------------------------------------------------------
// BACKGROUND NETWORK TASK (Runs independently on ESP32 Core 0)
// -----------------------------------------------------------------------------
void networkTask(void* pvParameters) {
    if (strlen(WIFI_SSID) > 0) {
        Serial.printf("[WiFi] Initializing connection to SSID: %s\n", WIFI_SSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            // 1. Probe Public Internet via DNS port (1.1.1.1:53)
            WiFiClient testClient;
            testClient.setTimeout(1);
            unsigned long tStart = millis();
            if (testClient.connect("1.1.1.1", 53, 500)) {
                internetOnline = true;
                internetLatencyMs = millis() - tStart;
                testClient.stop();
            } else {
                internetOnline = false;
                internetLatencyMs = -1;
            }

            // 2. Probe Infrastructure Nodes
            for (int i = 0; i < 4; i++) {
                WiFiClient nodeClient;
                nodeClient.setTimeout(1);
                unsigned long nStart = millis();
                if (nodeClient.connect(nodes[i].ip, nodes[i].port, 450)) {
                    nodes[i].isOnline = true;
                    nodes[i].latencyMs = millis() - nStart;
                    nodeClient.stop();
                } else {
                    nodes[i].isOnline = false;
                    nodes[i].latencyMs = -1;
                }
                vTaskDelay(pdMS_TO_TICKS(80)); // Brief pause between probes
            }
        }
        vTaskDelay(pdMS_TO_TICKS(8000)); // Probe cycle every 8 seconds
    }
}

// -----------------------------------------------------------------------------
// UI RENDERING: SCREEN 0 - DASHBOARD OVERVIEW
// -----------------------------------------------------------------------------
void drawServerCard(int y, const ServerNode& node) {
    // Card background
    tft.fillRoundRect(8, y, 464, 42, 6, tft.color565(20, 26, 36));
    tft.drawRoundRect(8, y, 464, 42, 6, tft.color565(45, 55, 72));

    // Server Name & IP / Role
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));
    tft.drawString(node.name, 20, y + 8, 4);

    char descBuf[64];
    snprintf(descBuf, sizeof(descBuf), "%s  |  %s", node.ip, node.role);
    tft.setTextColor(tft.color565(160, 174, 192), tft.color565(20, 26, 36));
    tft.drawString(descBuf, 20, y + 26, 2);

    // Determine status badge color and text
    uint16_t statusColor;
    char statusBuf[24];

    if (WiFi.status() == WL_CONNECTED) {
        if (node.isOnline) {
            statusColor = TFT_GREEN;
            snprintf(statusBuf, sizeof(statusBuf), "ONLINE %dms", node.latencyMs);
        } else {
            statusColor = tft.color565(239, 68, 68); // Soft Red
            snprintf(statusBuf, sizeof(statusBuf), "OFFLINE");
        }
    } else {
        statusColor = tft.color565(56, 189, 248); // Cyan Standby
        snprintf(statusBuf, sizeof(statusBuf), "STANDBY");
    }

    // Status Pill
    tft.fillRoundRect(352, y + 9, 110, 24, 12, statusColor);
    tft.setTextColor(TFT_BLACK, statusColor);
    tft.drawCentreString(statusBuf, 407, y + 13, 2);
}

void drawDashboard() {
    tft.fillScreen(tft.color565(10, 14, 23));

    // Header Banner
    tft.fillRect(0, 0, SCREEN_W, 38, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 38, SCREEN_W, tft.color565(56, 189, 248));

    // Title
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(15, 23, 42));
    tft.drawString(NOC_TITLE, 14, 8, 4);

    // WiFi Indicator in Header
    if (WiFi.status() == WL_CONNECTED) {
        tft.fillCircle(250, 19, 4, TFT_GREEN);
        tft.setTextColor(TFT_GREEN, tft.color565(15, 23, 42));
        tft.drawString("WiFi Live", 260, 11, 2);
    } else if (strlen(WIFI_SSID) > 0) {
        tft.fillCircle(250, 19, 4, TFT_YELLOW);
        tft.setTextColor(TFT_YELLOW, tft.color565(15, 23, 42));
        tft.drawString("WiFi Connecting", 260, 11, 2);
    } else {
        tft.fillCircle(250, 19, 4, tft.color565(100, 116, 139));
        tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
        tft.drawString("WiFi Standby", 260, 11, 2);
    }

    // Switch Screen Button in Top Right
    tft.fillRoundRect(365, 6, 105, 26, 4, tft.color565(30, 41, 59));
    tft.drawRoundRect(365, 6, 105, 26, 4, tft.color565(56, 189, 248));
    tft.setTextColor(TFT_WHITE, tft.color565(30, 41, 59));
    tft.drawCentreString("System Info >", 417, 11, 2);

    // 4 Server Cards
    for (int i = 0; i < 4; i++) {
        drawServerCard(46 + (i * 50), nodes[i]);
    }

    // Bottom Telemetry Bar
    tft.fillRect(0, 252, SCREEN_W, 68, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 252, SCREEN_W, tft.color565(45, 55, 72));

    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
    tft.drawString("Touch Screen to Test LED", 14, 262, 2);
    tft.drawString("LDR Light: ---", 14, 288, 2);

    char domBuf[36];
    snprintf(domBuf, sizeof(domBuf), "Domain: %s", DOMAIN_NAME);
    tft.drawString(domBuf, 230, 262, 2);
    tft.drawString("Uptime: 00:00:00", 230, 288, 2);

    // Bottom Right: LED Touch Target Button
    tft.fillRoundRect(385, 260, 85, 50, 6, tft.color565(24, 32, 47));
    tft.drawRoundRect(385, 260, 85, 50, 6, tft.color565(56, 189, 248));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(24, 32, 47));
    tft.drawCentreString("LED", 427, 268, 2);
    tft.drawCentreString("TAP", 427, 286, 2);
}

// -----------------------------------------------------------------------------
// UI RENDERING: SCREEN 1 - HARDWARE & NETWORK DIAGNOSTICS
// -----------------------------------------------------------------------------
void drawDiagnostics() {
    tft.fillScreen(tft.color565(10, 14, 23));

    // Header Banner
    tft.fillRect(0, 0, SCREEN_W, 38, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 38, SCREEN_W, tft.color565(56, 189, 248));

    // Back Button in Top Left
    tft.fillRoundRect(10, 6, 90, 26, 4, tft.color565(30, 41, 59));
    tft.drawRoundRect(10, 6, 90, 26, 4, tft.color565(56, 189, 248));
    tft.setTextColor(TFT_WHITE, tft.color565(30, 41, 59));
    tft.drawCentreString("< Back", 55, 11, 2);

    // Title
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(15, 23, 42));
    tft.drawString("SYSTEM & NETWORK DIAGNOSTICS", 115, 8, 4);

    // Card 1: Board & Chipset Information
    tft.fillRoundRect(8, 46, 226, 96, 6, tft.color565(20, 26, 36));
    tft.drawRoundRect(8, 46, 226, 96, 6, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("BOARD & HARDWARE", 16, 52, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));
    tft.drawString("Model: ESP32-035 (3.5\" CYD)", 16, 70, 1);
    tft.drawString("MCU: ESP32 Dual-Core 240MHz", 16, 84, 1);
    tft.drawString("Display: 3.5\" 480x320 ST7796", 16, 98, 1);
    tft.drawString("Backlight: GPIO 27 (Transistor)", 16, 112, 1);

    // Card 2: Live Network & WiFi Telemetry
    tft.fillRoundRect(244, 46, 228, 96, 6, tft.color565(20, 26, 36));
    tft.drawRoundRect(244, 46, 228, 96, 6, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("NETWORK STATUS", 252, 52, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));

    if (WiFi.status() == WL_CONNECTED) {
        char ipBuf[40];
        snprintf(ipBuf, sizeof(ipBuf), "IP: %s", WiFi.localIP().toString().c_str());
        tft.drawString(ipBuf, 252, 70, 1);

        char rssiBuf[40];
        snprintf(rssiBuf, sizeof(rssiBuf), "SSID: %s (%d dBm)", WIFI_SSID, WiFi.RSSI());
        tft.drawString(rssiBuf, 252, 84, 1);

        char gwBuf[40];
        snprintf(gwBuf, sizeof(gwBuf), "Gateway: %s", WiFi.gatewayIP().toString().c_str());
        tft.drawString(gwBuf, 252, 98, 1);

        if (internetOnline) {
            char netBuf[40];
            snprintf(netBuf, sizeof(netBuf), "Internet: ONLINE (%dms)", internetLatencyMs);
            tft.setTextColor(TFT_GREEN, tft.color565(20, 26, 36));
            tft.drawString(netBuf, 252, 112, 1);
        } else {
            tft.setTextColor(TFT_YELLOW, tft.color565(20, 26, 36));
            tft.drawString("Internet: Connecting...", 252, 112, 1);
        }
    } else {
        tft.drawString("WiFi: Not Connected", 252, 70, 1);
        char ssidBuf[40];
        snprintf(ssidBuf, sizeof(ssidBuf), "Target SSID: %s", WIFI_SSID);
        tft.drawString(ssidBuf, 252, 84, 1);
        tft.drawString("Status: Connecting / Standby", 252, 98, 1);
        tft.drawString("Mode: Offline Telemetry", 252, 112, 1);
    }

    // Card 3: Memory & Runtime Status
    tft.fillRoundRect(8, 150, 226, 96, 6, tft.color565(20, 26, 36));
    tft.drawRoundRect(8, 150, 226, 96, 6, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("SYSTEM HEALTH", 16, 156, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));

    char heapBuf[48];
    snprintf(heapBuf, sizeof(heapBuf), "Free Heap: %u KB", ESP.getFreeHeap() / 1024);
    tft.drawString(heapBuf, 16, 174, 1);

    char minHeapBuf[48];
    snprintf(minHeapBuf, sizeof(minHeapBuf), "Min Free Heap: %u KB", ESP.getMinFreeHeap() / 1024);
    tft.drawString(minHeapBuf, 16, 188, 1);

    tft.drawString("SPI Bus: HSPI @ 40MHz", 16, 202, 1);
    tft.drawString("Dual-Core Task: Core 0 Net", 16, 216, 1);

    // Card 4: Touch & Peripherals Subsystem
    tft.fillRoundRect(244, 150, 228, 96, 6, tft.color565(20, 26, 36));
    tft.drawRoundRect(244, 150, 228, 96, 6, tft.color565(45, 55, 72));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(20, 26, 36));
    tft.drawString("TOUCH & SENSORS", 252, 156, 2);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));
    tft.drawString("Touch Controller: XPT2046", 252, 174, 1);
    tft.drawString("Touch Pins: CS 33 | IRQ 36", 252, 188, 1);
    tft.drawString("RGB LED Pins: R:4 | G:16 | B:17", 252, 202, 1);
    tft.drawString("Ambient LDR Pin: GPIO 34", 252, 216, 1);

    // Bottom Interactive Diagnostic Pad
    tft.fillRect(0, 254, SCREEN_W, 66, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 254, SCREEN_W, tft.color565(45, 55, 72));

    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
    tft.drawString("Tap Screen to Test Touch Coordinates & LED Color Pulse", 16, 264, 2);

    char touchInfo[64];
    if (lastTouchX >= 0) {
        snprintf(touchInfo, sizeof(touchInfo), "Last Touch: X:%d Y:%d | Active Color: %s", lastTouchX, lastTouchY, colorNames[touchColorIndex]);
    } else {
        snprintf(touchInfo, sizeof(touchInfo), "Touch Screen: Ready  |  Active Color: %s", colorNames[touchColorIndex]);
    }
    tft.setTextColor(TFT_YELLOW, tft.color565(15, 23, 42));
    tft.drawString(touchInfo, 16, 288, 2);
}

// -----------------------------------------------------------------------------
// SETUP & INITIALIZATION
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Starting Multi-Screen Live Dashboard...");

    // 1. Backlight: GPIO 27 on ESP32-035 (Drives Transistor Q2)
    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);
    Serial.println("[ESP32-035] Backlight GPIO 27 set HIGH.");

    // 2. Configure Onboard RGB LED (Turn OFF initially)
    pinMode(CYD_LED_RED,   OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE,  OUTPUT);
    setRGB(false, false, false); // All OFF

    // 3. Configure LDR (Light Sensor)
    pinMode(CYD_LDR_PIN, INPUT);

    // 4. Ensure Touch CS is deselected before initializing TFT
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    // 5. Initialize TFT Display (ST7796 480x320)
    tft.init();
    tft.setRotation(1); // Landscape 480x320
    Serial.println("[ESP32-035] TFT Display Initialized.");

    // 6. Initialize Touch Screen (XPT2046 on shared HSPI)
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);
    Serial.println("[ESP32-035] Touch Screen Initialized.");

    // 7. Render Initial Dashboard Screen
    drawDashboard();
    Serial.println("[ESP32-035] Dashboard Rendered.");

    // 8. Launch Background Network Task on Core 0
    xTaskCreatePinnedToCore(
        networkTask,    // Task function
        "NetworkTask",  // Name of task
        4096,           // Stack size in words
        NULL,           // Task input parameter
        1,              // Priority of the task
        NULL,           // Task handle
        0               // Pin to Core 0 (UI runs on Core 1)
    );
    Serial.println("[ESP32-035] Background Network Task Launched on Core 0.");
}

// -----------------------------------------------------------------------------
// MAIN LOOP (Runs on Core 1)
// -----------------------------------------------------------------------------
void loop() {
    unsigned long now = millis();

    // 1. Automatically turn off RGB LED after tap duration expires
    if (ledIsActive && now > ledTurnOffTime) {
        setRGB(false, false, false);
        ledIsActive = false;
    }

    // 2. Update Live Sensor Telemetry (every 1 second)
    if (now - lastSensorRead > 1000) {
        lastSensorRead = now;

        // Read Ambient Room Light Sensor (0 - 4095)
        int ldrRaw = analogRead(CYD_LDR_PIN);
        int lightPct = map(constrain(ldrRaw, 100, 3800), 100, 3800, 0, 100);

        if (currentScreen == SCREEN_DASHBOARD) {
            char ldrBuf[36];
            snprintf(ldrBuf, sizeof(ldrBuf), "LDR Light: %d%% (%d)    ", lightPct, ldrRaw);
            tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
            tft.drawString(ldrBuf, 14, 288, 2);

            // Uptime Clock
            unsigned long sec = now / 1000;
            unsigned long s = sec % 60;
            unsigned long m = (sec / 60) % 60;
            unsigned long h = (sec / 3600);
            char upBuf[32];
            snprintf(upBuf, sizeof(upBuf), "Uptime: %02lu:%02lu:%02lu  ", h, m, s);
            tft.drawString(upBuf, 230, 288, 2);

            // Re-render server cards to update live ONLINE/OFFLINE status
            for (int i = 0; i < 4; i++) {
                drawServerCard(46 + (i * 50), nodes[i]);
            }
        }
    }

    // 3. Touch Screen Interaction Handling
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        touchCount++;

        // Map touch raw coordinates (~200 to 3800) to 480x320 landscape
        int touchX = map(p.x, 200, 3800, 0, SCREEN_W);
        int touchY = map(p.y, 200, 3800, 0, SCREEN_H);
        touchX = constrain(touchX, 0, SCREEN_W - 1);
        touchY = constrain(touchY, 0, SCREEN_H - 1);

        lastTouchX = touchX;
        lastTouchY = touchY;

        // Trigger RGB LED pulse on tap
        triggerLEDFlash();

        // SCREEN NAVIGATION:
        if (currentScreen == SCREEN_DASHBOARD) {
            // Check if top-right button "System Info >" was tapped
            if (touchX >= 340 && touchY <= 45) {
                currentScreen = SCREEN_DIAGNOSTICS;
                drawDiagnostics();
                delay(200); // Debounce
                return;
            }

            // Visual touch ripple marker on the screen
            uint16_t activeCol = tftColors[touchColorIndex % 7];
            tft.drawCircle(touchX, touchY, 7, activeCol);
            tft.drawCircle(touchX, touchY, 8, activeCol);

            // Update touch banner
            char touchBuf[48];
            snprintf(touchBuf, sizeof(touchBuf), "Touch #%d: X:%d Y:%d (LED: %s)   ", touchCount, touchX, touchY, colorNames[touchColorIndex]);
            tft.setTextColor(TFT_YELLOW, tft.color565(15, 23, 42));
            tft.drawString(touchBuf, 14, 262, 2);
        }
        else if (currentScreen == SCREEN_DIAGNOSTICS) {
            // Check if top-left button "< Back" was tapped
            if (touchX <= 120 && touchY <= 45) {
                currentScreen = SCREEN_DASHBOARD;
                drawDashboard();
                delay(200); // Debounce
                return;
            }

            // Visual touch ripple marker
            uint16_t activeCol = tftColors[touchColorIndex % 7];
            tft.drawCircle(touchX, touchY, 7, activeCol);
            tft.drawCircle(touchX, touchY, 8, activeCol);

            // Update bottom diagnostic pad info
            char touchInfo[64];
            snprintf(touchInfo, sizeof(touchInfo), "Touch #%d: X:%d Y:%d | Active Color: %s    ", touchCount, touchX, touchY, colorNames[touchColorIndex]);
            tft.setTextColor(TFT_YELLOW, tft.color565(15, 23, 42));
            tft.drawString(touchInfo, 16, 288, 2);
        }

        delay(80); // Debounce
    }
}
