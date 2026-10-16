// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    Unified Firmware & Example Repository Launcher (480x320 Landscape)
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch, RGB LED, Speaker, LDR
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include "mario_sprites.h"

// --- PIN DEFINITIONS FOR CYD 3.5" (ESP32-035) ---
#define CYD_BACKLIGHT   27   // Backlight is GPIO 27 (Active HIGH)
#define CYD_LED_RED     4    // Active LOW (0 = ON, 1 = OFF)
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_LDR_PIN     34   // Ambient Light Sensor
#define CYD_SPEAKER_PIN 26   // Audio Amplifier

// Touch Screen SPI (Shared HSPI bus: CLK=14, MISO=12, MOSI=13, CS=33)
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

// Display Dimensions (Landscape 480x320)
#define SCREEN_W        480
#define SCREEN_H        320

// Hardware instances
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);

// -----------------------------------------------------------------------------
// APP STATES / REPOSITORIES
// -----------------------------------------------------------------------------
enum AppState {
    APP_HUB           = 0, // Main Repository Selector Screen
    APP_MARIO_ARCADE  = 1, // Super Mario Bros Autonomous Arcade
    APP_MANGA_AURA    = 2, // Manga Ultra Instinct & Speedlines
    APP_SYNTHWAVE     = 3, // Neo-Tokyo 2088 3D Grid Horizon
    APP_RASENGAN      = 4, // 3D Rasengan Ki Energy Vortex
    APP_MATRIX_RAIN   = 5, // Cyber Katakana Matrix Rain
    APP_NOC_DASHBOARD = 6  // NOC IT Server Diagnostics
};

AppState currentApp = APP_HUB;
unsigned long appStartTime = 0;
unsigned long lastTouchTime = 0;
int animTick = 0;

// -----------------------------------------------------------------------------
// RGB LED & AUDIO HELPERS
// -----------------------------------------------------------------------------
void setRGB(bool r, bool g, bool b) {
    digitalWrite(CYD_LED_RED,   r ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE,  b ? LOW : HIGH);
}

unsigned long ledOffTime = 0;
bool ledActive = false;
void flashLED(bool r, bool g, bool b, int durationMs = 120) {
    setRGB(r, g, b);
    ledOffTime = millis() + durationMs;
    ledActive = true;
}

void updateLED() {
    if (ledActive && millis() > ledOffTime) {
        setRGB(false, false, false);
        ledActive = false;
    }
}

unsigned long toneEndTime = 0;
bool toneActive = false;
int nextToneFreq = 0;
int nextToneDuration = 0;
unsigned long nextToneStartTime = 0;

void playToneAsync(int freq, int durationMs) {
    tone(CYD_SPEAKER_PIN, freq, durationMs);
    toneEndTime = millis() + durationMs;
    toneActive = true;
}

void updateAudio() {
    unsigned long now = millis();
    if (nextToneFreq > 0 && now >= nextToneStartTime) {
        tone(CYD_SPEAKER_PIN, nextToneFreq, nextToneDuration);
        toneEndTime = now + nextToneDuration;
        toneActive = true;
        nextToneFreq = 0;
    }
    if (toneActive && now > toneEndTime) {
        toneActive = false;
    }
}

void soundJump()     { playToneAsync(360, 90); }
void soundStomp()    { playToneAsync(130, 80); }
void soundFireball() { playToneAsync(620, 60); }
void soundCoin() {
    playToneAsync(988, 80); // B5
    nextToneFreq = 1319;    // E6
    nextToneDuration = 200;
    nextToneStartTime = millis() + 85;
}

// -----------------------------------------------------------------------------
// PRE-RENDERED MARIO SPRITES (Stored in RAM for instant 0.4ms DMA push)
// -----------------------------------------------------------------------------
uint16_t bufMarioStand[1024];
uint16_t bufMarioRun1[1024];
uint16_t bufMarioRun2[1024];
uint16_t bufMarioJump[1024];
uint16_t bufGoomba1[1024];
uint16_t bufGoomba2[1024];
uint16_t bufGoombaFlat[1024];
uint16_t bufBlockQ[1024];
uint16_t bufBlockBrick[1024];
uint16_t bufBlockEmpty[1024];
uint16_t bufBlockGround[1024];
uint16_t bufCoin[3][1024];
uint16_t bufFireball16[2][256];

void expandSprite32(const uint8_t src[16][16], const uint16_t* palette, uint16_t* dest) {
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {
            uint16_t color = palette[src[r][c]];
            int dR = r * 2;
            int dC = c * 2;
            dest[dR * 32 + dC]             = color;
            dest[dR * 32 + dC + 1]         = color;
            dest[(dR + 1) * 32 + dC]       = color;
            dest[(dR + 1) * 32 + dC + 1]   = color;
        }
    }
}

void expandSprite16(const uint8_t src[8][8], const uint16_t* palette, uint16_t* dest) {
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            uint16_t color = palette[src[r][c]];
            int dR = r * 2;
            int dC = c * 2;
            dest[dR * 16 + dC]             = color;
            dest[dR * 16 + dC + 1]         = color;
            dest[(dR + 1) * 16 + dC]       = color;
            dest[(dR + 1) * 16 + dC + 1]   = color;
        }
    }
}

// -----------------------------------------------------------------------------
// TOP HUB RETURN BUTTON (Rendered inside all demo projects)
// -----------------------------------------------------------------------------
void drawHubButton() {
    tft.fillRoundRect(408, 6, 66, 22, 5, tft.color565(20, 26, 40));
    tft.drawRoundRect(408, 6, 66, 22, 5, TFT_CYAN);
    tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 40));
    tft.drawString("< Hub", 418, 9, 2);
}

bool checkHubButtonTap(int tx, int ty) {
    return (tx >= 390 && tx <= 478 && ty >= 2 && ty <= 34);
}

// -----------------------------------------------------------------------------
// FORWARD DECLARATIONS FOR DEMO APPS
// -----------------------------------------------------------------------------
void initAppHub();
void updateAppHub();
void initAppMario();
void updateAppMario();
void initAppManga();
void updateAppManga();
void initAppSynthwave();
void updateAppSynthwave();
void initAppRasengan();
void updateAppRasengan();
void initAppMatrix();
void updateAppMatrix();
void initAppNoc();
void updateAppNoc();

void switchApp(AppState newApp) {
    currentApp = newApp;
    appStartTime = millis();
    tft.fillScreen(TFT_BLACK);

    flashLED(true, true, true, 80);

    switch (currentApp) {
        case APP_HUB:           initAppHub();       break;
        case APP_MARIO_ARCADE:  initAppMario();     break;
        case APP_MANGA_AURA:    initAppManga();     break;
        case APP_SYNTHWAVE:     initAppSynthwave(); break;
        case APP_RASENGAN:      initAppRasengan();  break;
        case APP_MATRIX_RAIN:   initAppMatrix();    break;
        case APP_NOC_DASHBOARD: initAppNoc();       break;
    }
    Serial.printf("[App Switch] Running App ID: %d\n", (int)currentApp);
}

// -----------------------------------------------------------------------------
// 1. REPOSITORY HUB / MAIN LAUNCHER (Screen 0)
// -----------------------------------------------------------------------------
struct RepoCard {
    int x, y, w, h;
    const char* title;
    const char* tag;
    const char* desc;
    uint16_t tagColor;
    uint16_t accentColor;
    AppState targetApp;
};

RepoCard repoCards[6] = {
    {14,  46, 220, 78, "mario-arcade",         "[GAME]",       "Autonomous NES Mario runner",       TFT_GREEN,   TFT_RED,     APP_MARIO_ARCADE},
    {246, 46, 220, 78, "manga-aura",           "[MANGA]",      "Ultra Instinct speedlines & SFX",  TFT_CYAN,    TFT_CYAN,    APP_MANGA_AURA},
    {14,  134, 220, 78, "neo-tokyo-synthwave", "[3D SCENE]",   "3D horizon grid & retro sunset",    TFT_MAGENTA, TFT_MAGENTA, APP_SYNTHWAVE},
    {246, 134, 220, 78, "rasengan-vortex",     "[PARTICLES]",  "3D Ki energy sphere & shockwaves", TFT_CYAN,    TFT_BLUE,    APP_RASENGAN},
    {14,  222, 220, 78, "matrix-rain",         "[CYBERPUNK]",  "Digital Katakana phosphor rain",   TFT_GREEN,   TFT_GREEN,   APP_MATRIX_RAIN},
    {246, 222, 220, 78, "noc-dashboard",       "[TELEMETRY]",  "IT server diagnostics & monitor",  TFT_YELLOW,  TFT_YELLOW,  APP_NOC_DASHBOARD}
};

void drawRepoCard(const RepoCard& c) {
    // Card background & border
    tft.fillRoundRect(c.x, c.y, c.w, c.h, 6, tft.color565(16, 22, 34));
    tft.drawRoundRect(c.x, c.y, c.w, c.h, 6, tft.color565(40, 50, 70));

    // Accent line on left
    tft.fillRoundRect(c.x + 2, c.y + 4, 4, c.h - 8, 2, c.accentColor);

    // Repo Title (/examples/...)
    tft.setTextColor(TFT_WHITE, tft.color565(16, 22, 34));
    char titleBuf[32];
    snprintf(titleBuf, sizeof(titleBuf), "/examples/%s", c.title);
    tft.drawString(titleBuf, c.x + 12, c.y + 8, 2);

    // Tag Pill
    tft.setTextColor(c.tagColor, tft.color565(16, 22, 34));
    tft.drawRightString(c.tag, c.x + c.w - 8, c.y + 8, 2);

    // Description
    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(16, 22, 34));
    tft.drawString(c.desc, c.x + 12, c.y + 32, 1);

    // Direct launch indicator
    tft.setTextColor(c.accentColor, tft.color565(16, 22, 34));
    tft.drawString("Tap to Launch Repository >", c.x + 12, c.y + 54, 2);
}

void initAppHub() {
    tft.fillScreen(tft.color565(10, 14, 24));

    // Header Bar
    tft.fillRect(0, 0, SCREEN_W, 38, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 38, SCREEN_W, tft.color565(56, 189, 248));

    tft.setTextColor(TFT_WHITE, tft.color565(15, 23, 42));
    tft.drawString("ESP32-035 FIRMWARE LAB // EXAMPLE REPOSITORIES", 16, 6, 2);
    tft.setTextColor(tft.color565(148, 163, 184), tft.color565(15, 23, 42));
    tft.drawString("Select any project repository below to launch live on display:", 16, 22, 1);

    // Top Right Hardware Badge
    tft.fillRoundRect(360, 8, 108, 22, 11, tft.color565(30, 41, 59));
    tft.setTextColor(TFT_GREEN, tft.color565(30, 41, 59));
    tft.drawCentreString("ST7796 480x320", 414, 11, 2);

    // Render all 6 Repository Cards
    for (int i = 0; i < 6; i++) {
        drawRepoCard(repoCards[i]);
    }

    // Footer
    tft.setTextColor(tft.color565(100, 116, 139), tft.color565(10, 14, 24));
    tft.drawCentreString("Touch screen to select | In any demo, tap < Hub to return", SCREEN_W / 2, 306, 1);

    setRGB(false, true, false); // Green standby
}

void updateAppHub() {
    // LED gentle breathing
    float b = sinf(millis() * 0.003f);
    setRGB(false, b > 0.3f, b > 0.7f);
}

// -----------------------------------------------------------------------------
// 2. SUPER MARIO BROS ARCADE (PERFECTED GRAPHICS & ZERO ARTIFACTS)
// -----------------------------------------------------------------------------
#define MARIO_GROUND_Y  272
#define MARIO_RUN_X     95

float marioY = 240.0f;
float marioVY = 0.0f;
float oldMarioY = 240.0f;
bool isJumping = false;
int marioRunFrame = 0;
unsigned long marioScore = 2450;
int coinCount = 12;
int worldTimer = 380;
unsigned long lastTimerTick = 0;
float scrollSpeed = 3.0f;
float worldX = 0.0f;

struct Pipe { float x; int h; bool cleared; };
Pipe pipes[2] = { {340.0f, 54, false}, {620.0f, 62, false} };

struct Block { float x, y; bool isQuestion; bool isHit; float bounceY; };
Block blocks[4] = {
    {210.0f, 168.0f, true,  false, 0},
    {242.0f, 168.0f, false, false, 0},
    {274.0f, 168.0f, true,  false, 0},
    {490.0f, 168.0f, true,  false, 0}
};

struct Goomba { float x, y; bool alive; bool stomped; int squishTimer; int walkFrame; };
Goomba goombas[2] = {
    {310.0f, 240.0f, true, false, 0, 0},
    {560.0f, 240.0f, true, false, 0, 0}
};

struct Fireball { float x, y, vx, vy; int frame; bool active; };
Fireball fireballs[2];

void marioJump(float power = -12.4f) {
    marioVY = power;
    isJumping = true;
    soundJump();
    flashLED(false, true, false, 80);
}

void marioShoot() {
    for (int i = 0; i < 2; i++) {
        if (!fireballs[i].active) {
            fireballs[i].x = MARIO_RUN_X + 26;
            fireballs[i].y = marioY + 12;
            fireballs[i].vx = 8.5f;
            fireballs[i].vy = 2.0f;
            fireballs[i].frame = 0;
            fireballs[i].active = true;
            soundFireball();
            flashLED(true, true, false, 90);
            break;
        }
    }
}

// Clean cloud in upper sky (y: 35..85)
void drawMarioCloud(int x, int y) {
    if (x < -70 || x > SCREEN_W) return;
    tft.fillRoundRect(x + 10, y, 42, 18, 9, NES_WHITE);
    tft.fillCircle(x + 18, y + 6, 12, NES_WHITE);
    tft.fillCircle(x + 36, y + 4, 15, NES_WHITE);
    tft.fillCircle(x + 50, y + 8, 11, NES_WHITE);
    tft.fillRoundRect(x + 4, y + 10, 56, 14, 6, NES_WHITE);
}

void drawMarioPipe(int x, int h) {
    if (x < -50 || x > SCREEN_W) return;
    int topY = MARIO_GROUND_Y - h;
    // Rim
    tft.fillRect(x - 3, topY, 46, 18, PIPE_GREEN_MD);
    tft.drawRect(x - 3, topY, 46, 18, NES_BLACK);
    tft.fillRect(x + 2, topY + 1, 5, 16, PIPE_GREEN_LT);
    tft.fillRect(x + 34, topY + 1, 6, 16, PIPE_GREEN_DK);
    // Body
    int bodyH = h - 18;
    tft.fillRect(x + 1, topY + 18, 38, bodyH, PIPE_GREEN_MD);
    tft.drawFastVLine(x, topY + 18, bodyH, NES_BLACK);
    tft.drawFastVLine(x + 39, topY + 18, bodyH, NES_BLACK);
    tft.fillRect(x + 4, topY + 18, 5, bodyH, PIPE_GREEN_LT);
    tft.fillRect(x + 30, topY + 18, 6, bodyH, PIPE_GREEN_DK);
}

void drawMarioHUD() {
    tft.fillRect(0, 0, 400, 30, NES_SKY);
    tft.setTextColor(NES_WHITE, NES_SKY);
    tft.drawString("MARIO", 30, 4, 2);
    char sBuf[16];
    snprintf(sBuf, sizeof(sBuf), "%06lu", marioScore);
    tft.drawString(sBuf, 30, 18, 2);

    tft.fillCircle(170, 23, 4, NES_YELLOW);
    tft.drawCircle(170, 23, 4, NES_BLACK);
    char cBuf[12];
    snprintf(cBuf, sizeof(cBuf), "x%02d", coinCount);
    tft.drawString(cBuf, 182, 18, 2);

    tft.drawString("WORLD", 260, 4, 2);
    tft.drawString(" 1-1 ", 262, 18, 2);

    tft.drawString("TIME", 340, 4, 2);
    char tBuf[12];
    snprintf(tBuf, sizeof(tBuf), "%03d", worldTimer);
    tft.drawString(tBuf, 344, 18, 2);

    drawHubButton();
}

void initAppMario() {
    tft.fillScreen(NES_SKY);
    marioY = 240.0f;
    marioVY = 0.0f;
    isJumping = false;
    fireballs[0].active = false;
    fireballs[1].active = false;

    // Static clouds in safe high sky band (y: 35..65)
    drawMarioCloud(50, 42);
    drawMarioCloud(260, 52);
    drawMarioCloud(410, 38);

    // Draw initial ground
    for (int i = 0; i < 16; i++) {
        tft.pushImage(i * 32, MARIO_GROUND_Y, 32, 32, bufBlockGround);
        tft.pushImage(i * 32, MARIO_GROUND_Y + 32, 32, 16, bufBlockGround);
    }
    drawMarioHUD();
    soundCoin();
}

void updateAppMario() {
    unsigned long now = millis();
    animTick++;
    updateAudio();
    updateLED();

    // Timer tick
    if (now - lastTimerTick >= 1000) {
        lastTimerTick = now;
        if (worldTimer > 0) worldTimer--;
        drawMarioHUD();
    }

    worldX += scrollSpeed;

    // Scrolling Ground
    int groundOffset = (int)worldX % 32;
    for (int i = 0; i < 16; i++) {
        int gx = -groundOffset + (i * 32);
        if (gx > -32 && gx < SCREEN_W) {
            tft.pushImage(gx, MARIO_GROUND_Y, 32, 32, bufBlockGround);
            tft.pushImage(gx, MARIO_GROUND_Y + 32, 32, 16, bufBlockGround);
        }
    }

    // Warp Pipes (y: 216..272)
    for (int i = 0; i < 2; i++) {
        pipes[i].x -= scrollSpeed;
        int px = (int)pipes[i].x;

        // Clean erase behind pipe
        int topY = MARIO_GROUND_Y - pipes[i].h;
        tft.fillRect(px + 43, topY, 8, pipes[i].h, NES_SKY);
        drawMarioPipe(px, pipes[i].h);

        if (pipes[i].x < -50) {
            pipes[i].x = SCREEN_W + random(60, 180);
            pipes[i].h = random(48, 64);
            pipes[i].cleared = false;
        }

        // Mario Auto-Jump over Pipe
        if (!pipes[i].cleared && pipes[i].x > MARIO_RUN_X && pipes[i].x < MARIO_RUN_X + 65) {
            if (!isJumping) {
                marioJump(-12.8f);
                pipes[i].cleared = true;
            }
        }
    }

    // Floating Blocks (y: 168)
    for (int i = 0; i < 4; i++) {
        blocks[i].x -= scrollSpeed;
        int bx = (int)blocks[i].x;

        // Clean erase behind block
        tft.fillRect(bx + 32, (int)blocks[i].y, 6, 32, NES_SKY);

        int drawBY = (int)blocks[i].y;
        if (blocks[i].bounceY > 0) {
            drawBY -= (int)blocks[i].bounceY;
            blocks[i].bounceY -= 1.5f;
            if (blocks[i].bounceY < 0) blocks[i].bounceY = 0;
        }

        uint16_t* bBuf = blocks[i].isQuestion ? (blocks[i].isHit ? bufBlockEmpty : bufBlockQ) : bufBlockBrick;
        tft.pushImage(bx, drawBY, 32, 32, bBuf);

        if (blocks[i].x < -36) {
            blocks[i].x = SCREEN_W + (i * 34) + random(30, 90);
            blocks[i].isHit = false;
            blocks[i].bounceY = 0;
        }

        // Mario Bonks Question Block
        if (blocks[i].isQuestion && !blocks[i].isHit) {
            if (blocks[i].x > MARIO_RUN_X - 10 && blocks[i].x < MARIO_RUN_X + 25) {
                if (!isJumping && random(100) > 40) marioJump(-11.5f);
                if (isJumping && marioY <= blocks[i].y + 36 && marioY >= blocks[i].y + 20) {
                    blocks[i].isHit = true;
                    blocks[i].bounceY = 8.0f;
                    marioScore += 200;
                    coinCount++;
                    soundCoin();
                    flashLED(false, true, true, 140);
                    drawMarioHUD();
                }
            }
        }
    }

    // Goombas (y: 240)
    for (int i = 0; i < 2; i++) {
        if (!goombas[i].alive) continue;
        float gMove = goombas[i].stomped ? scrollSpeed : (scrollSpeed + 1.2f);
        goombas[i].x -= gMove;
        int gx = (int)goombas[i].x;

        // Clean erase behind Goomba
        tft.fillRect(gx + 32, (int)goombas[i].y, 8, 32, NES_SKY);

        if (goombas[i].stomped) {
            tft.pushImage(gx, (int)goombas[i].y, 32, 32, bufGoombaFlat);
            goombas[i].squishTimer--;
            if (goombas[i].squishTimer <= 0) {
                tft.fillRect(gx, (int)goombas[i].y, 32, 32, NES_SKY);
                goombas[i].x = SCREEN_W + random(80, 200);
                goombas[i].stomped = false;
            }
        } else {
            if (animTick % 8 == 0) goombas[i].walkFrame = !goombas[i].walkFrame;
            tft.pushImage(gx, (int)goombas[i].y, 32, 32, goombas[i].walkFrame ? bufGoomba1 : bufGoomba2);

            // Autonomous Mario AI response
            if (goombas[i].x > MARIO_RUN_X + 25 && goombas[i].x < MARIO_RUN_X + 110) {
                if (!isJumping) {
                    if (random(100) > 40) marioJump(-11.5f);
                    else marioShoot();
                }
            }

            // Stomp Detection
            if (isJumping && marioVY > 0) {
                if (abs(gx - MARIO_RUN_X) < 24 && abs((int)marioY + 28 - (int)goombas[i].y) < 14) {
                    goombas[i].stomped = true;
                    goombas[i].squishTimer = 22;
                    marioScore += 100;
                    marioVY = -6.5f; // Bounce
                    soundStomp();
                    flashLED(false, true, false, 90);
                    drawMarioHUD();
                }
            }
        }

        if (goombas[i].x < -36) {
            goombas[i].x = SCREEN_W + random(60, 220);
            goombas[i].stomped = false;
        }
    }

    // Fireballs
    for (int i = 0; i < 2; i++) {
        if (!fireballs[i].active) continue;
        tft.fillRect((int)fireballs[i].x, (int)fireballs[i].y, 18, 18, NES_SKY);
        fireballs[i].x += fireballs[i].vx;
        fireballs[i].y += fireballs[i].vy;
        fireballs[i].vy += 0.75f;

        if (fireballs[i].y >= MARIO_GROUND_Y - 16) {
            fireballs[i].y = MARIO_GROUND_Y - 16;
            fireballs[i].vy = -5.5f;
        }
        fireballs[i].frame = (fireballs[i].frame + 1) % 2;

        // Collision with Goombas
        for (int g = 0; g < 2; g++) {
            if (!goombas[g].stomped && abs((int)fireballs[i].x - (int)goombas[g].x) < 22 && abs((int)fireballs[i].y - (int)goombas[g].y) < 26) {
                goombas[g].stomped = true;
                goombas[g].squishTimer = 18;
                fireballs[i].active = false;
                marioScore += 100;
                soundFireball();
                flashLED(true, true, false, 120);
                drawMarioHUD();
                break;
            }
        }

        if (fireballs[i].x > SCREEN_W || fireballs[i].x < 0) {
            fireballs[i].active = false;
        } else if (fireballs[i].active) {
            tft.pushImage((int)fireballs[i].x, (int)fireballs[i].y, 16, 16, bufFireball16[fireballs[i].frame]);
        }
    }

    // Mario Physics & Clean Redraw
    oldMarioY = marioY;
    if (isJumping) {
        marioY += marioVY;
        marioVY += 0.82f;
        if (marioY >= 240.0f) {
            marioY = 240.0f;
            marioVY = 0.0f;
            isJumping = false;
        }
    } else {
        if (animTick % 4 == 0) marioRunFrame = (marioRunFrame + 1) % 3;
    }

    int curY = (int)marioY;
    int prevY = (int)oldMarioY;
    if (curY != prevY) {
        if (curY > prevY) tft.fillRect(MARIO_RUN_X, prevY, 32, curY - prevY + 1, NES_SKY);
        else tft.fillRect(MARIO_RUN_X, curY + 32, 32, prevY - curY + 1, NES_SKY);
    }

    uint16_t* mBuf = bufMarioStand;
    if (isJumping) mBuf = bufMarioJump;
    else if (marioRunFrame == 0) mBuf = bufMarioRun1;
    else if (marioRunFrame == 1) mBuf = bufMarioStand;
    else mBuf = bufMarioRun2;

    tft.pushImage(MARIO_RUN_X, curY, 32, 32, mBuf);
}

// -----------------------------------------------------------------------------
// 3. MANGA ULTRA INSTINCT & SPEEDLINES
// -----------------------------------------------------------------------------
struct MangaRay { float angle; int rInner, rOuter; uint16_t color; };
MangaRay rays[26];
MangaRay oldRays[26];

void drawAnimeEye(int cx, int cy, bool isLeft, float pulse) {
    int dir = isLeft ? -1 : 1;
    tft.drawLine(cx - dir * 42, cy - 26, cx + dir * 36, cy - 14, TFT_WHITE);
    tft.drawLine(cx - dir * 42, cy - 25, cx + dir * 36, cy - 13, TFT_CYAN);
    tft.drawLine(cx - dir * 38, cy - 10, cx - dir * 10, cy - 15, TFT_WHITE);
    tft.drawLine(cx - dir * 10, cy - 15, cx + dir * 34, cy - 6, TFT_WHITE);
    tft.drawLine(cx - dir * 20, cy + 14, cx + dir * 24, cy + 12, TFT_WHITE);

    int irisRadius = 14 + (int)(pulse * 2.5f);
    tft.fillCircle(cx, cy, irisRadius, tft.color565(0, 180, 255));
    tft.drawCircle(cx, cy, irisRadius + 1, TFT_WHITE);
    tft.fillCircle(cx, cy, irisRadius - 4, tft.color565(120, 235, 255));
    tft.fillCircle(cx, cy, 5 + (int)(pulse * 1.5f), TFT_BLACK);
    tft.fillCircle(cx + dir * 4, cy - 4, 3, TFT_WHITE);
}

void initAppManga() {
    tft.fillScreen(TFT_BLACK);
    drawHubButton();
    for (int i = 0; i < 26; i++) {
        rays[i].angle = (float)i * (6.28318f / 26.0f);
        rays[i].rInner = random(80, 120);
        rays[i].rOuter = random(200, 310);
        rays[i].color = (random(100) > 40) ? TFT_WHITE : TFT_CYAN;
        oldRays[i] = rays[i];
    }
}

void updateAppManga() {
    int eyeCY = 165;
    // Erase old rays
    for (int i = 0; i < 26; i++) {
        int x0 = 240 + (int)(cosf(oldRays[i].angle) * oldRays[i].rInner);
        int y0 = eyeCY + (int)(sinf(oldRays[i].angle) * oldRays[i].rInner);
        int x1 = 240 + (int)(cosf(oldRays[i].angle) * oldRays[i].rOuter);
        int y1 = eyeCY + (int)(sinf(oldRays[i].angle) * oldRays[i].rOuter);
        tft.drawLine(x0, y0, x1, y1, TFT_BLACK);
    }
    // Draw new rays
    for (int i = 0; i < 26; i++) {
        oldRays[i] = rays[i];
        rays[i].rInner = random(80, 120);
        rays[i].rOuter = random(200, 310);
        rays[i].color = (random(100) > 30) ? TFT_WHITE : TFT_CYAN;
        int x0 = 240 + (int)(cosf(rays[i].angle) * rays[i].rInner);
        int y0 = eyeCY + (int)(sinf(rays[i].angle) * rays[i].rInner);
        int x1 = 240 + (int)(cosf(rays[i].angle) * rays[i].rOuter);
        int y1 = eyeCY + (int)(sinf(rays[i].angle) * rays[i].rOuter);
        tft.drawLine(x0, y0, x1, y1, rays[i].color);
    }

    float pulse = sinf(millis() * 0.007f);
    drawAnimeEye(185, eyeCY, true, pulse);
    drawAnimeEye(295, eyeCY, false, pulse);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("GOGOGO (ゴゴゴ)", 20, 280, 2);
    tft.drawString("DODODO (ドドド)", 340, 280, 2);
    drawHubButton();
    setRGB(false, pulse > 0.1f, true);
}

// -----------------------------------------------------------------------------
// 4. NEO-TOKYO 2088 SYNTHWAVE
// -----------------------------------------------------------------------------
float gridScroll = 0.0f;
#define SYNTH_HORIZON 180

void initAppSynthwave() {
    tft.fillScreen(tft.color565(10, 6, 24));
    // Mt. Fuji & Synthwave Sun
    tft.fillTriangle(240, 95, 110, SYNTH_HORIZON, 370, SYNTH_HORIZON, tft.color565(25, 12, 50));
    tft.fillTriangle(240, 95, 210, 125, 270, 125, tft.color565(255, 160, 230));

    // Giant Sun
    for (int r = 48; r > 0; r--) {
        tft.drawCircle(240, 105, r, tft.color565(255, map(r, 0, 48, 240, 20), 80));
    }
    for (int sl = 0; sl < 5; sl++) {
        tft.fillRect(180, 108 + (sl * 7), 120, 2 + sl / 2, tft.color565(10, 6, 24));
    }
    tft.drawFastHLine(0, SYNTH_HORIZON, SCREEN_W, tft.color565(255, 40, 180));
    drawHubButton();
}

void updateAppSynthwave() {
    gridScroll += 0.06f;
    if (gridScroll >= 1.0f) gridScroll -= 1.0f;

    // Erase lower floor
    tft.fillRect(0, SYNTH_HORIZON + 1, SCREEN_W, SCREEN_H - SYNTH_HORIZON - 1, tft.color565(8, 4, 18));

    // Radial perspective lines
    int vpX = 240, vpY = SYNTH_HORIZON;
    for (int x = -100; x <= SCREEN_W + 100; x += 40) {
        tft.drawLine(vpX, vpY, x, SCREEN_H, tft.color565(240, 20, 160));
    }

    // Exponential horizontal lines
    for (int i = 0; i < 7; i++) {
        float norm = ((float)i + gridScroll) / 7.0f;
        int gy = SYNTH_HORIZON + (int)(powf(norm, 2.2f) * (SCREEN_H - SYNTH_HORIZON));
        if (gy > SYNTH_HORIZON && gy < SCREEN_H) {
            tft.drawFastHLine(0, gy, SCREEN_W, tft.color565((uint8_t)(norm * 255), 255 - (uint8_t)(norm * 255), 255));
        }
    }

    tft.setTextColor(tft.color565(255, 40, 180), tft.color565(10, 6, 24));
    tft.drawString("NEO TOKYO 2088 // SYNTHWAVE", 20, 10, 2);
    drawHubButton();
    setRGB(true, false, sinf(millis() * 0.005f) > 0);
}

// -----------------------------------------------------------------------------
// 5. RASENGAN KI ENERGY VORTEX
// -----------------------------------------------------------------------------
struct KiPart { float angle, rad, speed, phi, dPhi; int oldX, oldY; uint16_t col; };
KiPart kiParts[90];
float shockR = 15.0f;

void initAppRasengan() {
    tft.fillScreen(TFT_BLACK);
    drawHubButton();
    for (int i = 0; i < 90; i++) {
        kiParts[i].rad = random(18, 90);
        kiParts[i].angle = random(0, 360) * 0.0174533f;
        kiParts[i].phi = random(-60, 60) * 0.0174533f;
        kiParts[i].speed = (random(20, 60) * 0.0018f) * ((i % 2 == 0) ? 1.0f : -1.0f);
        kiParts[i].dPhi = random(-20, 20) * 0.001f;
        kiParts[i].oldX = 240;
        kiParts[i].oldY = 160;
        kiParts[i].col = (random(100) > 40) ? TFT_CYAN : TFT_WHITE;
    }
}

void updateAppRasengan() {
    int cx = 240, cy = 160;

    // Erase old shockwave
    if (shockR > 15 && shockR < 130) tft.drawCircle(cx, cy, (int)shockR, TFT_BLACK);
    shockR += 4.5f;
    if (shockR > 130.0f) shockR = 16.0f;
    tft.drawCircle(cx, cy, (int)shockR, tft.color565(0, map((int)shockR, 15, 130, 255, 40), 255));

    for (int i = 0; i < 90; i++) {
        tft.drawPixel(kiParts[i].oldX, kiParts[i].oldY, TFT_BLACK);
        kiParts[i].angle += kiParts[i].speed;
        kiParts[i].phi += kiParts[i].dPhi;
        if (kiParts[i].phi > 1.2f || kiParts[i].phi < -1.2f) kiParts[i].dPhi = -kiParts[i].dPhi;

        int px = cx + (int)(kiParts[i].rad * cosf(kiParts[i].angle) * cosf(kiParts[i].phi));
        int py = cy + (int)(kiParts[i].rad * sinf(kiParts[i].angle) * 0.55f + kiParts[i].rad * sinf(kiParts[i].phi) * 0.75f);
        kiParts[i].oldX = px;
        kiParts[i].oldY = py;
        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) tft.drawPixel(px, py, kiParts[i].col);
    }

    tft.fillCircle(cx, cy, 18, tft.color565(0, 200, 255));
    tft.fillCircle(cx, cy, 10, TFT_WHITE);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("RASENGAN KI SPHERE (螺旋丸)", 20, 10, 2);
    drawHubButton();
    setRGB(false, true, random(100) > 40);
}

// -----------------------------------------------------------------------------
// 6. DIGITAL KATAKANA MATRIX RAIN
// -----------------------------------------------------------------------------
struct MatCol { float headY, speed; int trailLen; uint8_t idx; };
MatCol mCols[28];

void initAppMatrix() {
    tft.fillScreen(TFT_BLACK);
    drawHubButton();
    for (int c = 0; c < 28; c++) {
        mCols[c].headY = random(-280, 0);
        mCols[c].speed = random(5, 13) * 0.75f;
        mCols[c].trailLen = random(100, 220);
        mCols[c].idx = random(16);
    }
}

void updateAppMatrix() {
    for (int c = 0; c < 28; c++) {
        int colX = c * 17;
        int curY = (int)mCols[c].headY;
        int tailY = curY - mCols[c].trailLen;
        if (tailY >= 0 && tailY < SCREEN_H) tft.fillRect(colX, tailY, 16, 24, TFT_BLACK);

        mCols[c].headY += mCols[c].speed;
        int newY = (int)mCols[c].headY;
        if (newY >= 0 && newY < SCREEN_H) {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawChar(65 + (mCols[c].idx % 26), colX, newY, 2);
        }
        if (curY >= 0 && curY < SCREEN_H) {
            tft.setTextColor(tft.color565(0, 240, 80), TFT_BLACK);
            tft.drawChar(65 + ((mCols[c].idx + 1) % 26), colX, curY, 2);
        }
        if (mCols[c].headY - mCols[c].trailLen > SCREEN_H) {
            mCols[c].headY = random(-100, -20);
            mCols[c].speed = random(5, 13) * 0.75f;
        }
    }
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("CYBERSTREAM MATRIX v4.2", 20, 10, 2);
    drawHubButton();
    setRGB(false, (animTick % 6 < 4), false);
}

// -----------------------------------------------------------------------------
// 7. NOC IT INFRASTRUCTURE MONITOR
// -----------------------------------------------------------------------------
void initAppNoc() {
    tft.fillScreen(tft.color565(10, 14, 23));

    // Header
    tft.fillRect(0, 0, SCREEN_W, 36, tft.color565(15, 23, 42));
    tft.drawFastHLine(0, 36, SCREEN_W, tft.color565(56, 189, 248));
    tft.setTextColor(TFT_WHITE, tft.color565(15, 23, 42));
    tft.drawString("NOC IT INFRASTRUCTURE MONITOR", 16, 10, 2);

    // 4 Server Cards
    const char* sNames[4] = {"Gateway Router", "Core Switch", "Edge Firewall", "Production Server"};
    const char* sIps[4]   = {"192.168.1.1", "192.168.1.2", "192.168.1.5", "10.0.0.10"};
    for (int i = 0; i < 4; i++) {
        int y = 46 + (i * 48);
        tft.fillRoundRect(14, y, 452, 42, 5, tft.color565(20, 26, 36));
        tft.drawRoundRect(14, y, 452, 42, 5, tft.color565(45, 55, 72));
        tft.setTextColor(TFT_WHITE, tft.color565(20, 26, 36));
        tft.drawString(sNames[i], 24, y + 6, 2);
        tft.setTextColor(tft.color565(148, 163, 184), tft.color565(20, 26, 36));
        tft.drawString(sIps[i], 24, y + 24, 1);
        tft.fillRoundRect(360, y + 9, 90, 24, 12, TFT_GREEN);
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
        tft.drawCentreString("ONLINE 12ms", 405, y + 13, 2);
    }

    // Bottom Telemetry
    tft.fillRoundRect(14, 244, 452, 64, 5, tft.color565(18, 28, 45));
    tft.setTextColor(tft.color565(56, 189, 248), tft.color565(18, 28, 45));
    tft.drawString("SYSTEM TELEMETRY & HARDWARE HEALTH", 24, 252, 2);

    char heapBuf[48];
    snprintf(heapBuf, sizeof(heapBuf), "Free Heap: %u KB | SoC: ESP32-D0WD-V3 @ 240MHz", ESP.getFreeHeap() / 1024);
    tft.setTextColor(TFT_WHITE, tft.color565(18, 28, 45));
    tft.drawString(heapBuf, 24, 274, 1);

    drawHubButton();
}

void updateAppNoc() {
    setRGB(false, true, false);
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Starting Unified Firmware & Multi-Repository Hub...");

    // 1. Backlight on GPIO 27 (Active HIGH)
    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);

    // 2. RGB LED (Active LOW)
    pinMode(CYD_LED_RED,   OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE,  OUTPUT);
    setRGB(false, false, false);

    // 3. Audio & Sensors
    pinMode(CYD_SPEAKER_PIN, OUTPUT);
    digitalWrite(CYD_SPEAKER_PIN, LOW);
    pinMode(CYD_LDR_PIN, INPUT);

    // 4. Touch CS setup
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);

    // 5. TFT Display Initialisation in Un-Mirrored Landscape (Rotation 7)
    tft.init();
    tft.setRotation(7); // Rotation 7 corrects horizontal mirroring for ST7796
    Serial.printf("[ESP32-035] TFT Initialized (%dx%d).\n", tft.width(), tft.height());

    // 6. Touch Controller Initialisation
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    // 7. Pre-render 16x16 Sprites into 32x32 Buffers in RAM
    Serial.println("[ESP32-035] Baking Mario sprites into 32x32 DMA buffers...");
    expandSprite32(mario_stand, mario_palette, bufMarioStand);
    expandSprite32(mario_run1,  mario_palette, bufMarioRun1);
    expandSprite32(mario_run2,  mario_palette, bufMarioRun2);
    expandSprite32(mario_jump,  mario_palette, bufMarioJump);

    expandSprite32(goomba_walk1, goomba_palette, bufGoomba1);
    expandSprite32(goomba_walk2, goomba_palette, bufGoomba2);
    expandSprite32(goomba_flat,  goomba_palette, bufGoombaFlat);

    expandSprite32(block_q1,     block_palette, bufBlockQ);
    expandSprite32(block_brick,  block_palette, bufBlockBrick);
    expandSprite32(block_empty,  block_palette, bufBlockEmpty);
    expandSprite32(block_ground, block_palette, bufBlockGround);

    expandSprite32(coin_f0, block_palette, bufCoin[0]);
    expandSprite32(coin_f1, block_palette, bufCoin[1]);
    expandSprite32(coin_f2, block_palette, bufCoin[2]);

    expandSprite16(fireball_f0, fireball_palette, bufFireball16[0]);
    expandSprite16(fireball_f1, fireball_palette, bufFireball16[1]);

    // 8. Start with Repository Hub Screen
    switchApp(APP_HUB);
    soundCoin();
}

// -----------------------------------------------------------------------------
// MAIN LOOP
// -----------------------------------------------------------------------------
void loop() {
    unsigned long now = millis();

    // Render active app
    switch (currentApp) {
        case APP_HUB:           updateAppHub();       break;
        case APP_MARIO_ARCADE:  updateAppMario();     break;
        case APP_MANGA_AURA:    updateAppManga();     break;
        case APP_SYNTHWAVE:     updateAppSynthwave(); break;
        case APP_RASENGAN:      updateAppRasengan();  break;
        case APP_MATRIX_RAIN:   updateAppMatrix();    break;
        case APP_NOC_DASHBOARD: updateAppNoc();       break;
    }

    // Optical Tap via LDR: Cover corner sensor to return to Hub!
    static bool ldrWasCovered = true;
    int ldrVal = analogRead(CYD_LDR_PIN);
    if (ldrVal < 400 && !ldrWasCovered && (now - lastTouchTime > 400)) {
        ldrWasCovered = true;
        lastTouchTime = now;
        Serial.printf("[Optical Tap] LDR covered -> Return to Hub!\n");
        if (currentApp != APP_HUB) switchApp(APP_HUB);
        else switchApp(APP_MARIO_ARCADE);
        return;
    } else if (ldrVal > 650) {
        ldrWasCovered = false;
    }

    // Touch Screen Handling
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        if (p.z >= 500 && p.z < 3900) {
            // Map coordinates with X-flip to match TFT Rotation 7
            int rawX = map(p.x, 200, 3800, 0, SCREEN_W);
            int rawY = map(p.y, 200, 3800, 0, SCREEN_H);
            int touchX = (SCREEN_W - 1) - constrain(rawX, 0, SCREEN_W - 1);
            int touchY = constrain(rawY, 0, SCREEN_H - 1);

            if (now - lastTouchTime > 250) {
                lastTouchTime = now;
                Serial.printf("[Touch] X:%d Y:%d (App: %d)\n", touchX, touchY, (int)currentApp);

                // Check "< Hub" button if currently inside a demo app
                if (currentApp != APP_HUB) {
                    if (checkHubButtonTap(touchX, touchY)) {
                        switchApp(APP_HUB);
                        return;
                    }

                    // Inside Mario: Tap jumps & shoots!
                    if (currentApp == APP_MARIO_ARCADE) {
                        marioJump(-13.0f);
                        marioShoot();
                        return;
                    }
                }
                // Inside Repository Hub: Check which card was tapped!
                else {
                    for (int i = 0; i < 6; i++) {
                        const RepoCard& c = repoCards[i];
                        if (touchX >= c.x && touchX <= c.x + c.w && touchY >= c.y && touchY <= c.y + c.h) {
                            flashLED(true, true, false, 100);
                            soundCoin();
                            switchApp(c.targetApp);
                            return;
                        }
                    }
                }
            }
        }
    }

    delay(18); // Smooth 45-50 FPS timing
}
