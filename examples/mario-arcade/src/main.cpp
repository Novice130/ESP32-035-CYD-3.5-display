// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    Autonomous Super Mario Bros Arcade & Interactive Runner (480x320)
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch, RGB LED, Speaker (GPIO 26)
// ==============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
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

// Landscape 480x320
#define SCREEN_W        480
#define SCREEN_H        320
#define MARIO_GROUND_Y  272
#define MARIO_X         95

// Hardware instances
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);

// -----------------------------------------------------------------------------
// PRE-RENDERED 32x32 SPRITE BUFFERS (Stored in RAM for instant 0.4ms DMA push)
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
uint16_t bufFireball[2][256];

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
// AUDIO & LED HELPERS
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
// GAME STATE & OBJECTS
// -----------------------------------------------------------------------------
float marioY = 240.0f;
float marioVY = 0.0f;
float oldMarioY = 240.0f;
bool isJumping = false;
int marioRunFrame = 0;
int animTick = 0;
unsigned long marioScore = 2450;
int coinCount = 12;
int worldTimer = 380;
unsigned long lastTimerTick = 0;
unsigned long lastTouchTime = 0;

float scrollSpeed = 3.0f;
float worldX = 0.0f;

// Pipes
struct Pipe {
    float x;
    int prevX;
    int h;
    bool cleared;
};
Pipe pipes[2] = {
    {340.0f, 340, 54, false},
    {620.0f, 620, 62, false}
};

// Floating Blocks (y: 168)
struct Block {
    float x;
    int prevX;
    float y;
    bool isQuestion;
    bool isHit;
    float bounceY;
};
Block blocks[4] = {
    {210.0f, 210, 168.0f, true,  false, 0},
    {242.0f, 242, 168.0f, false, false, 0},
    {274.0f, 274, 168.0f, true,  false, 0},
    {490.0f, 490, 168.0f, true,  false, 0}
};

// Goombas
struct Goomba {
    float x;
    int prevX;
    float y;
    bool alive;
    bool stomped;
    int squishTimer;
    int walkFrame;
};
Goomba goombas[2] = {
    {310.0f, 310, 240.0f, true, false, 0, 0},
    {560.0f, 560, 240.0f, true, false, 0, 0}
};

// Fireballs
struct Fireball {
    float x, y;
    int prevX, prevY;
    float vx, vy;
    int frame;
    bool active;
};
Fireball fireballs[2];

// Pop Coins from hit blocks
struct PopCoin {
    float x, y;
    int prevX, prevY;
    float vy;
    int frame;
    bool active;
};
PopCoin popCoins[2];

// -----------------------------------------------------------------------------
// MARIO ACTIONS: JUMP & FIREBALL
// -----------------------------------------------------------------------------
void marioJump(float power = -12.4f) {
    marioVY = power;
    isJumping = true;
    soundJump();
    flashLED(false, true, false, 80);
}

void marioShoot() {
    for (int i = 0; i < 2; i++) {
        if (!fireballs[i].active) {
            fireballs[i].x = MARIO_X + 26;
            fireballs[i].y = marioY + 12;
            fireballs[i].prevX = (int)fireballs[i].x;
            fireballs[i].prevY = (int)fireballs[i].y;
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

// -----------------------------------------------------------------------------
// RENDERERS (CLEAN PIXEL-PERFECT GRAPHICS WITHOUT ANY ARTIFACTS)
// -----------------------------------------------------------------------------

// NES Puffy Cloud in safe high sky band (y: 35..65)
void drawCloud(int x, int y) {
    tft.fillRoundRect(x + 10, y, 42, 18, 9, NES_WHITE);
    tft.fillCircle(x + 18, y + 6, 12, NES_WHITE);
    tft.fillCircle(x + 36, y + 4, 15, NES_WHITE);
    tft.fillCircle(x + 50, y + 8, 11, NES_WHITE);
    tft.fillRoundRect(x + 4, y + 10, 56, 14, 6, NES_WHITE);
}

// NES Green Warp Pipe
void drawPipe(int x, int h) {
    if (x < -50 || x > SCREEN_W) return;
    int topY = MARIO_GROUND_Y - h;

    // Pipe Top Rim (46px wide, 18px high)
    tft.fillRect(x - 3, topY, 46, 18, PIPE_GREEN_MD);
    tft.drawRect(x - 3, topY, 46, 18, NES_BLACK);
    tft.fillRect(x + 2, topY + 1, 5, 16, PIPE_GREEN_LT);
    tft.fillRect(x + 34, topY + 1, 6, 16, PIPE_GREEN_DK);

    // Pipe Body (38px wide)
    int bodyH = h - 18;
    tft.fillRect(x + 1, topY + 18, 38, bodyH, PIPE_GREEN_MD);
    tft.drawFastVLine(x, topY + 18, bodyH, NES_BLACK);
    tft.drawFastVLine(x + 39, topY + 18, bodyH, NES_BLACK);
    tft.fillRect(x + 4, topY + 18, 5, bodyH, PIPE_GREEN_LT);
    tft.fillRect(x + 30, topY + 18, 6, bodyH, PIPE_GREEN_DK);
}

// Ground Bricks Row
void drawGround() {
    int groundOffset = (int)worldX % 32;
    for (int i = 0; i < 16; i++) {
        int gx = -groundOffset + (i * 32);
        if (gx > -32 && gx < SCREEN_W) {
            tft.pushImage(gx, MARIO_GROUND_Y, 32, 32, bufBlockGround);
            tft.pushImage(gx, MARIO_GROUND_Y + 32, 32, 16, bufBlockGround);
        }
    }
}

// Upper NES Status HUD
void drawHUD() {
    tft.fillRect(0, 0, SCREEN_W, 30, NES_SKY);

    // MARIO
    tft.setTextColor(NES_WHITE, NES_SKY);
    tft.drawString("MARIO", 30, 4, 2);
    char sBuf[16];
    snprintf(sBuf, sizeof(sBuf), "%06lu", marioScore);
    tft.drawString(sBuf, 30, 18, 2);

    // Coins
    tft.fillCircle(175, 23, 4, NES_YELLOW);
    tft.drawCircle(175, 23, 4, NES_BLACK);
    char cBuf[12];
    snprintf(cBuf, sizeof(cBuf), "x%02d", coinCount);
    tft.drawString(cBuf, 186, 18, 2);

    // WORLD 1-1
    tft.drawString("WORLD", 270, 4, 2);
    tft.drawString(" 1-1 ", 272, 18, 2);

    // TIME
    tft.drawString("TIME", 390, 4, 2);
    char tBuf[12];
    snprintf(tBuf, sizeof(tBuf), "%03d", worldTimer);
    tft.drawString(tBuf, 396, 18, 2);
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Starting Super Mario Bros Arcade (480x320)...");

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

    // 5. TFT Display Initialisation (Standard Working Landscape Rotation 1)
    tft.init();
    tft.setRotation(1); // Rotation 1 is the verified working 480x320 landscape orientation
    tft.setSwapBytes(true); // sprite buffers hold normal RGB565; without this pushImage shows swapped colours
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

    expandSprite16(fireball_f0, fireball_palette, bufFireball[0]);
    expandSprite16(fireball_f1, fireball_palette, bufFireball[1]);

    // 8. Paint Background Screen
    tft.fillScreen(NES_SKY);

    // Static NES clouds in safe upper sky band (y: 35..65)
    drawCloud(50, 42);
    drawCloud(240, 52);
    drawCloud(390, 38);

    drawGround();
    drawHUD();

    soundCoin();
    Serial.println("[ESP32-035] Mario Game is Live!");
}

// -----------------------------------------------------------------------------
// MAIN GAME LOOP (Smooth 50 FPS Autonomous Arcade Runner)
// -----------------------------------------------------------------------------
void loop() {
    unsigned long now = millis();
    animTick++;

    // Audio & LED updates
    updateAudio();
    updateLED();

    // World timer tick
    if (now - lastTimerTick >= 1000) {
        lastTimerTick = now;
        if (worldTimer > 0) worldTimer--;
        drawHUD();
    }

    worldX += scrollSpeed;

    // 1. SCROLLING GROUND
    drawGround();

    // 2. PIPES (y: 216..272)
    for (int i = 0; i < 2; i++) {
        int oldPX = (int)pipes[i].x;
        pipes[i].x -= scrollSpeed;
        int newPX = (int)pipes[i].x;

        // Clean erase: erase only the exact strip vacated between frames
        int topY = MARIO_GROUND_Y - pipes[i].h;
        if (oldPX > newPX) {
            tft.fillRect(newPX + 43, topY, oldPX - newPX + 2, pipes[i].h, NES_SKY);
        }
        drawPipe(newPX, pipes[i].h);

        if (pipes[i].x < -50) {
            pipes[i].x = SCREEN_W + random(60, 180);
            pipes[i].h = random(48, 64);
            pipes[i].cleared = false;
        }

        // Autonomous AI: Mario leaps cleanly over approaching pipe!
        if (!pipes[i].cleared && pipes[i].x > MARIO_X && pipes[i].x < MARIO_X + 65) {
            if (!isJumping) {
                marioJump(-12.8f);
                pipes[i].cleared = true;
            }
        }
    }

    // 3. FLOATING BLOCKS (y: 168)
    for (int i = 0; i < 4; i++) {
        int oldBX = (int)blocks[i].x;
        blocks[i].x -= scrollSpeed;
        int newBX = (int)blocks[i].x;

        // Clean erase vacated pixels
        if (oldBX > newBX) {
            tft.fillRect(newBX + 32, (int)blocks[i].y, oldBX - newBX + 2, 32, NES_SKY);
        }

        int drawBY = (int)blocks[i].y;
        if (blocks[i].bounceY > 0) {
            drawBY -= (int)blocks[i].bounceY;
            blocks[i].bounceY -= 1.5f;
            if (blocks[i].bounceY < 0) blocks[i].bounceY = 0;
        }

        uint16_t* bBuf = blocks[i].isQuestion ? (blocks[i].isHit ? bufBlockEmpty : bufBlockQ) : bufBlockBrick;
        tft.pushImage(newBX, drawBY, 32, 32, bBuf);

        if (blocks[i].x < -36) {
            blocks[i].x = SCREEN_W + (i * 34) + random(30, 90);
            blocks[i].isHit = false;
            blocks[i].bounceY = 0;
        }

        // Autonomous AI: Mario leaps to bonk question block overhead!
        if (blocks[i].isQuestion && !blocks[i].isHit) {
            if (blocks[i].x > MARIO_X - 10 && blocks[i].x < MARIO_X + 25) {
                if (!isJumping && random(100) > 40) marioJump(-11.5f);
                if (isJumping && marioY <= blocks[i].y + 36 && marioY >= blocks[i].y + 20) {
                    blocks[i].isHit = true;
                    blocks[i].bounceY = 8.0f;
                    marioScore += 200;
                    coinCount++;
                    soundCoin();
                    flashLED(false, true, true, 140);
                    drawHUD();

                    // Spawn pop coin
                    for (int c = 0; c < 2; c++) {
                        if (!popCoins[c].active) {
                            popCoins[c].x = blocks[i].x + 8;
                            popCoins[c].y = blocks[i].y - 24;
                            popCoins[c].prevX = (int)popCoins[c].x;
                            popCoins[c].prevY = (int)popCoins[c].y;
                            popCoins[c].vy = -6.0f;
                            popCoins[c].frame = 0;
                            popCoins[c].active = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    // 4. POPPING COINS ANIMATION
    for (int c = 0; c < 2; c++) {
        if (!popCoins[c].active) continue;
        tft.fillRect(popCoins[c].prevX, popCoins[c].prevY, 16, 16, NES_SKY);

        popCoins[c].y += popCoins[c].vy;
        popCoins[c].vy += 0.8f;
        popCoins[c].frame = (popCoins[c].frame + 1) % 3;

        popCoins[c].prevX = (int)popCoins[c].x;
        popCoins[c].prevY = (int)popCoins[c].y;

        if (popCoins[c].vy > 3.0f) {
            popCoins[c].active = false;
            tft.fillRect(popCoins[c].prevX, popCoins[c].prevY, 16, 16, NES_SKY);
        } else {
            tft.pushImage((int)popCoins[c].x, (int)popCoins[c].y, 16, 16, bufCoin[popCoins[c].frame]);
        }
    }

    // 5. GOOMBAS (y: 240)
    for (int i = 0; i < 2; i++) {
        int oldGX = (int)goombas[i].x;
        float gMove = goombas[i].stomped ? scrollSpeed : (scrollSpeed + 1.2f);
        goombas[i].x -= gMove;
        int newGX = (int)goombas[i].x;

        if (oldGX > newGX) {
            tft.fillRect(newGX + 32, (int)goombas[i].y, oldGX - newGX + 2, 32, NES_SKY);
        }

        if (goombas[i].stomped) {
            tft.pushImage(newGX, (int)goombas[i].y, 32, 32, bufGoombaFlat);
            goombas[i].squishTimer--;
            if (goombas[i].squishTimer <= 0) {
                tft.fillRect(newGX, (int)goombas[i].y, 32, 32, NES_SKY);
                goombas[i].x = SCREEN_W + random(80, 200);
                goombas[i].stomped = false;
            }
        } else {
            if (animTick % 8 == 0) goombas[i].walkFrame = !goombas[i].walkFrame;
            tft.pushImage(newGX, (int)goombas[i].y, 32, 32, goombas[i].walkFrame ? bufGoomba1 : bufGoomba2);

            // Autonomous AI: Stomp or Shoot approaching Goomba
            if (goombas[i].x > MARIO_X + 25 && goombas[i].x < MARIO_X + 110) {
                if (!isJumping) {
                    if (random(100) > 40) marioJump(-11.5f);
                    else marioShoot();
                }
            }

            // Stomp Detection
            if (isJumping && marioVY > 0) {
                if (abs(newGX - MARIO_X) < 24 && abs((int)marioY + 28 - (int)goombas[i].y) < 14) {
                    goombas[i].stomped = true;
                    goombas[i].squishTimer = 22;
                    marioScore += 100;
                    marioVY = -6.5f; // Bounce
                    soundStomp();
                    flashLED(false, true, false, 90);
                    drawHUD();
                }
            }
        }

        if (goombas[i].x < -36) {
            goombas[i].x = SCREEN_W + random(60, 220);
            goombas[i].stomped = false;
        }
    }

    // 6. FIREBALLS
    for (int i = 0; i < 2; i++) {
        if (!fireballs[i].active) continue;
        tft.fillRect(fireballs[i].prevX, fireballs[i].prevY, 16, 16, NES_SKY);

        fireballs[i].x += fireballs[i].vx;
        fireballs[i].y += fireballs[i].vy;
        fireballs[i].vy += 0.75f;

        if (fireballs[i].y >= MARIO_GROUND_Y - 16) {
            fireballs[i].y = MARIO_GROUND_Y - 16;
            fireballs[i].vy = -5.5f; // Bounce!
        }
        fireballs[i].frame = (fireballs[i].frame + 1) % 2;

        fireballs[i].prevX = (int)fireballs[i].x;
        fireballs[i].prevY = (int)fireballs[i].y;

        // Collision with Goombas
        for (int g = 0; g < 2; g++) {
            if (!goombas[g].stomped && abs((int)fireballs[i].x - (int)goombas[g].x) < 22 && abs((int)fireballs[i].y - (int)goombas[g].y) < 26) {
                goombas[g].stomped = true;
                goombas[g].squishTimer = 18;
                fireballs[i].active = false;
                tft.fillRect(fireballs[i].prevX, fireballs[i].prevY, 16, 16, NES_SKY);
                marioScore += 100;
                soundFireball();
                flashLED(true, true, false, 120);
                drawHUD();
                break;
            }
        }

        if (fireballs[i].x > SCREEN_W || fireballs[i].x < 0) {
            fireballs[i].active = false;
            tft.fillRect(fireballs[i].prevX, fireballs[i].prevY, 16, 16, NES_SKY);
        } else if (fireballs[i].active) {
            tft.pushImage((int)fireballs[i].x, (int)fireballs[i].y, 16, 16, bufFireball[fireballs[i].frame]);
        }
    }

    // 7. MARIO PHYSICS & ZERO-ARTIFACT RENDERING
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

    // Erase only vertical slice vacated by Mario's jump/fall
    int curY = (int)marioY;
    int prevY = (int)oldMarioY;
    if (curY != prevY) {
        if (curY > prevY) tft.fillRect(MARIO_X, prevY, 32, curY - prevY + 1, NES_SKY);
        else tft.fillRect(MARIO_X, curY + 32, 32, prevY - curY + 1, NES_SKY);
    }

    uint16_t* mBuf = bufMarioStand;
    if (isJumping) mBuf = bufMarioJump;
    else if (marioRunFrame == 0) mBuf = bufMarioRun1;
    else if (marioRunFrame == 1) mBuf = bufMarioStand;
    else mBuf = bufMarioRun2;

    // Mario is drawn onto the clean NES sky: ZERO box, ZERO cutouts!
    tft.pushImage(MARIO_X, curY, 32, 32, mBuf);

    // 8. INTERACTIVE INPUTS: TOUCH TAP & OPTICAL LDR SENSOR
    // Optical Tap via LDR: Cover corner sensor with finger to trigger Jump & Fireball!
    static bool ldrWasCovered = true;
    int ldrVal = analogRead(CYD_LDR_PIN);
    if (ldrVal < 400 && !ldrWasCovered && (now - lastTouchTime > 250)) {
        ldrWasCovered = true;
        lastTouchTime = now;
        Serial.printf("[Optical Tap] LDR covered -> Mario Jump & Fireball!\n");
        marioJump(-13.2f);
        marioShoot();
    } else if (ldrVal > 650) {
        ldrWasCovered = false;
    }

    // Touch Screen Tap: Tap anywhere on screen to make Mario jump & shoot!
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        if (p.z >= 500 && p.z < 3900) {
            if (now - lastTouchTime > 200) {
                lastTouchTime = now;
                Serial.printf("[Touch Tap] Mario Jump & Shoot!\n");
                marioJump(-13.2f);
                marioShoot();
            }
        }
    }

    delay(18); // Silky smooth 50 FPS arcade timing
}
