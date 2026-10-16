// ==============================================================================
//  game.h - Kalos Route + battle engine (pure C++, runs on ESP32 and desktop)
//
//  Two modes:
//    DEMO  - the game plays itself forever (display / screensaver)
//    PLAY  - first tap takes over. Overworld: 1 tap = walk, 2 taps = search
//            grass for a wild POKeMON. Battle: 1 tap = attack, 2 taps = throw
//            a POKe BALL. 30 s without a tap drops back to DEMO.
//
//  The platform layer calls game_tick() at 30 Hz and game_render() whenever it
//  wants a fresh frame in `fb`, and implements plat_sfx() / plat_led().
// ==============================================================================
#pragma once
#include <math.h>
#include <stdio.h>
#include "gfx.h"

void plat_sfx(uint8_t id, uint16_t param);
void plat_led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);

enum Tap : uint8_t { TAP_NONE, TAP_SINGLE, TAP_DOUBLE };

enum Sfx : uint8_t {
    SFX_TAP, SFX_RUSTLE, SFX_ENCOUNTER, SFX_LEGEND, SFX_CRY, SFX_HIT, SFX_SUPER, SFX_WEAK,
    SFX_BOLT, SFX_FIRE, SFX_WATER, SFX_LEAF, SFX_WIND, SFX_MOON, SFX_DARK, SFX_QUAKE,
    SFX_THROW, SFX_POP, SFX_SHAKE, SFX_CATCH, SFX_BREAK, SFX_FAINT, SFX_LEVELUP, SFX_EXP,
    SFX_MODE
};

#define TICK_HZ 30
#define IDLE_TO_DEMO_TICKS (30 * TICK_HZ)

// ------------------------------------------------------------------ rng
static uint32_t rngState = 0x6B616C6F;  // "kalo"
static uint32_t rnd() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}
static int rndInt(int n) { return n > 0 ? (int)(rnd() % (uint32_t)n) : 0; }
static float rndf() { return (rnd() & 0xFFFF) / 65535.0f; }

// ------------------------------------------------------------------ data
enum Type : uint8_t { T_NORMAL, T_FIRE, T_WATER, T_GRASS, T_ELECTRIC, T_FLYING, T_FAIRY, T_DARK, T_DRAGON };

enum Fx : uint8_t { FX_TACKLE, FX_QUICK, FX_BOLT, FX_FIRE, FX_WATER, FX_LEAF, FX_WIND, FX_MUD, FX_MOON, FX_DARKWING, FX_ARROWS };

struct Move { const char* name; uint8_t type; uint8_t power; uint8_t fx; };
enum {
    MV_TACKLE, MV_QUICK, MV_THUNDERBOLT, MV_EMBER, MV_WATERGUN, MV_VINEWHIP, MV_GUST, MV_MUDSHOT,
    MV_MOONBLAST, MV_OBLIVION, MV_ARROWS, MV_SCRATCH
};
static const Move MOVES[] = {
    {"TACKLE", T_NORMAL, 40, FX_TACKLE},
    {"QUICK ATTACK", T_NORMAL, 40, FX_QUICK},
    {"THUNDERBOLT", T_ELECTRIC, 90, FX_BOLT},
    {"EMBER", T_FIRE, 70, FX_FIRE},
    {"WATER GUN", T_WATER, 70, FX_WATER},
    {"VINE WHIP", T_GRASS, 70, FX_LEAF},
    {"GUST", T_FLYING, 60, FX_WIND},
    {"MUD SHOT", T_NORMAL, 55, FX_MUD},
    {"MOONBLAST", T_FAIRY, 95, FX_MOON},
    {"OBLIVION WING", T_DARK, 80, FX_DARKWING},
    {"THOUSAND ARROWS", T_DRAGON, 90, FX_ARROWS},
    {"SCRATCH", T_NORMAL, 40, FX_TACKLE},
};

struct Species {
    const char* name; uint8_t type; const Sprite* front; const Sprite* back;
    uint8_t moves[2]; uint8_t catchPct; bool legend; uint16_t cryHz; uint32_t aura;
};
enum { SP_PIKACHU, SP_FENNEKIN, SP_FROAKIE, SP_CHESPIN, SP_FLETCHLING, SP_BUNNELBY, SP_XERNEAS, SP_YVELTAL, SP_ZYGARDE, SP_COUNT };
static const Species SPECIES[SP_COUNT] = {
    {"PIKACHU", T_ELECTRIC, &SPR_PIKACHU_FRONT, &SPR_PIKACHU_BACK, {MV_THUNDERBOLT, MV_QUICK}, 70, false, 1500, 0xF8D030},
    {"FENNEKIN", T_FIRE, &SPR_FENNEKIN_FRONT, &SPR_FENNEKIN_BACK, {MV_EMBER, MV_SCRATCH}, 65, false, 1250, 0xF06030},
    {"FROAKIE", T_WATER, &SPR_FROAKIE_FRONT, &SPR_FROAKIE_BACK, {MV_WATERGUN, MV_QUICK}, 65, false, 700, 0x60B8F0},
    {"CHESPIN", T_GRASS, &SPR_CHESPIN_FRONT, &SPR_CHESPIN_BACK, {MV_VINEWHIP, MV_TACKLE}, 65, false, 900, 0x60B040},
    {"FLETCHLING", T_FLYING, &SPR_FLETCHLING_FRONT, nullptr, {MV_GUST, MV_QUICK}, 80, false, 1900, 0xE86040},
    {"BUNNELBY", T_NORMAL, &SPR_BUNNELBY_FRONT, nullptr, {MV_MUDSHOT, MV_TACKLE}, 80, false, 1100, 0xA89070},
    {"XERNEAS", T_FAIRY, &SPR_XERNEAS_FRONT, nullptr, {MV_MOONBLAST, MV_TACKLE}, 35, true, 600, 0x40A0F0},
    {"YVELTAL", T_DARK, &SPR_YVELTAL_FRONT, nullptr, {MV_OBLIVION, MV_TACKLE}, 35, true, 350, 0xE02838},
    {"ZYGARDE", T_DRAGON, &SPR_ZYGARDE_FRONT, nullptr, {MV_ARROWS, MV_TACKLE}, 35, true, 450, 0x58E050},
};

// effectiveness in percent
static int typeEff(uint8_t mv, uint8_t def) {
    switch (mv) {
        case T_FIRE: return def == T_GRASS ? 200 : (def == T_WATER || def == T_FIRE || def == T_DRAGON) ? 50 : 100;
        case T_WATER: return def == T_FIRE ? 200 : (def == T_GRASS || def == T_WATER || def == T_DRAGON) ? 50 : 100;
        case T_GRASS: return def == T_WATER ? 200 : (def == T_FIRE || def == T_GRASS || def == T_FLYING || def == T_DRAGON) ? 50 : 100;
        case T_ELECTRIC: return (def == T_WATER || def == T_FLYING) ? 200 : (def == T_GRASS || def == T_ELECTRIC || def == T_DRAGON) ? 50 : 100;
        case T_FLYING: return def == T_GRASS ? 200 : def == T_ELECTRIC ? 50 : 100;
        case T_FAIRY: return (def == T_DRAGON || def == T_DARK) ? 200 : def == T_FIRE ? 50 : 100;
        case T_DARK: return (def == T_FAIRY || def == T_DARK) ? 50 : 100;
        case T_DRAGON: return def == T_DRAGON ? 200 : def == T_FAIRY ? 50 : 100;
    }
    return 100;
}

static uint32_t lerpRGB(uint32_t a, uint32_t b, int t) {  // t 0..256
    int r = (a >> 16) + ((((int)(b >> 16)) - (int)(a >> 16)) * t >> 8);
    int g = ((a >> 8) & 255) + ((((int)((b >> 8) & 255)) - (int)((a >> 8) & 255)) * t >> 8);
    int bl = (a & 255) + ((((int)(b & 255)) - (int)(a & 255)) * t >> 8);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}
static uint16_t C24(uint32_t c) { return RGB(c >> 16, (c >> 8) & 255, c & 255); }

// ------------------------------------------------------------------ global state
enum Scene : uint8_t { SC_WORLD, SC_TRANS, SC_BATTLE };
static Scene scene = SC_WORLD;
static bool playMode = false;
static uint32_t tick = 0;
static uint32_t lastInputTick = 0;
static int caughtCount = 0;
static int fadeIn = 0;  // ticks of fade-from-black remaining

static char toastMsg[32];
static int toastT = 0;
static void toast(const char* s) {
    snprintf(toastMsg, sizeof(toastMsg), "%s", s);
    toastT = 75;
}

static void sfx(uint8_t id, uint16_t p = 0) {
    if (playMode) plat_sfx(id, p);  // DEMO is silent - it's a display
}
static void led(uint32_t rgb, uint16_t ms) { plat_led(rgb >> 16, (rgb >> 8) & 255, rgb & 255, ms); }

// ------------------------------------------------------------------ particles
enum PKind : uint8_t { PK_DOT, PK_SPARK, PK_EMBER, PK_BUBBLE, PK_LEAF, PK_STAR, PK_FEATHER, PK_CELL, PK_DUST };
struct Particle { float x, y, vx, vy, g; int16_t life, max; uint16_t col; uint8_t kind, size; };
#define MAX_PARTICLES 72
static Particle parts[MAX_PARTICLES];

static void spawn(PKind k, float x, float y, float vx, float vy, int life, uint16_t col, uint8_t size = 1, float g = 0) {
    for (auto& p : parts) {
        if (p.life <= 0) {
            p = Particle{x, y, vx, vy, g, (int16_t)life, (int16_t)life, col, (uint8_t)k, size};
            return;
        }
    }
}

static void updateParticles() {
    for (auto& p : parts) {
        if (p.life <= 0) continue;
        p.x += p.vx;
        p.y += p.vy;
        p.vy += p.g;
        p.life--;
    }
}

static void drawParticles(int ox, int oy) {
    for (auto& p : parts) {
        if (p.life <= 0) continue;
        int x = (int)p.x + ox, y = (int)p.y + oy;
        int fade = p.life * 256 / p.max;
        switch (p.kind) {
            case PK_DOT: pset(x, y, p.col); break;
            case PK_SPARK:
                pset(x, y, 0xFFFF);
                pset(x - 1, y, p.col); pset(x + 1, y, p.col); pset(x, y - 1, p.col); pset(x, y + 1, p.col);
                break;
            case PK_EMBER: {
                int s = p.size * fade / 256 + 1;
                fillCircle(x, y, s, p.col);
                if (s > 1) fillCircle(x, y, s - 1, blend565(p.col, RGB(255, 250, 200), 160));
                break;
            }
            case PK_BUBBLE:
                circleOutline(x, y, p.size, p.col);
                pset(x - 1, y - 1, 0xFFFF);
                break;
            case PK_LEAF:
                pset(x, y, p.col); pset(x + 1, y, p.col); pset(x - 1, y + 1, p.col); pset(x, y + 1, blend565(p.col, 0, 80));
                break;
            case PK_STAR:
                blit(SPR_STAR, x - 3, y - 3);
                break;
            case PK_FEATHER:
                line(x, y, x + 3, y - 2, p.col);
                pset(x + 1, y, RGB(220, 40, 56));
                break;
            case PK_CELL:
                pset(x, y, p.col); pset(x + 1, y, p.col); pset(x, y + 1, p.col); pset(x + 1, y + 1, RGB(200, 255, 200));
                break;
            case PK_DUST:
                blendEllipse(x, y, p.size, p.size * 2 / 3 + 1, p.col, 140 * fade / 256);
                break;
        }
    }
}

static void clearParticles() {
    for (auto& p : parts) p.life = 0;
}

// ==============================================================================
//  OVERWORLD
// ==============================================================================
enum Dir : uint8_t { D_DOWN, D_UP, D_LEFT, D_RIGHT };
static int trX, trY;  // pixel position (top-left of 16x16 tile)
static uint8_t trDir = D_RIGHT;
static int trMove = 0;  // pixels left in current step
static int trParity = 0;
static int routeIdx = 1;
static int stepsQueued = 0;
static int stepsSinceBattle = 0;
static int idlePause = 0;
static int rustleT = 0, rustleX = 0, rustleY = 0;
static int exclaimT = 0;  // "!" bubble before an encounter
static int bannerT = 0;   // ROUTE 4 location sign
static int pendingSpecies = -1;
static uint8_t objOrder[MAP_OBJ_COUNT];

static char tileAt(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return 'h';
    return MAP[ty][tx];
}

static int pickWildSpecies() {
    int legendOdds = playMode ? 7 : 9;
    if (rndInt(legendOdds) == 0) return SP_XERNEAS + rndInt(3);
    static const uint8_t pool[] = {SP_FLETCHLING, SP_FLETCHLING, SP_BUNNELBY, SP_BUNNELBY, SP_PIKACHU,
                                   SP_FENNEKIN, SP_FROAKIE, SP_CHESPIN};
    return pool[rndInt(sizeof(pool))];
}

static void beginEncounter(int species);

static void world_reset() {
    trX = ROUTE[0][0] * 16;
    trY = ROUTE[0][1] * 16;
    routeIdx = 1;
    trMove = 0;
    // depth-sort the static objects once
    for (int i = 0; i < MAP_OBJ_COUNT; i++) objOrder[i] = i;
    for (int i = 1; i < MAP_OBJ_COUNT; i++)
        for (int j = i; j > 0 && MAP_OBJS[objOrder[j]].y < MAP_OBJS[objOrder[j - 1]].y; j--) {
            uint8_t t = objOrder[j]; objOrder[j] = objOrder[j - 1]; objOrder[j - 1] = t;
        }
}

static void startStep() {
    int tx = trX / 16, ty = trY / 16;
    if (tx == ROUTE[routeIdx][0] && ty == ROUTE[routeIdx][1]) routeIdx = (routeIdx + 1) % ROUTE_LEN;
    int gx = ROUTE[routeIdx][0], gy = ROUTE[routeIdx][1];
    if (gx > tx) trDir = D_RIGHT;
    else if (gx < tx) trDir = D_LEFT;
    else if (gy > ty) trDir = D_DOWN;
    else trDir = D_UP;
    trMove = 16;
    trParity ^= 1;
}

static void onStepLanded() {
    int tx = trX / 16, ty = trY / 16;
    stepsSinceBattle++;
    if (tileAt(tx, ty) == 'T') {
        rustleT = 12; rustleX = tx; rustleY = ty;
        for (int i = 0; i < 4; i++)
            spawn(PK_LEAF, tx * 16 + 4 + rndInt(8), ty * 16 + 6, (rndf() - 0.5f) * 1.6f, -1.2f - rndf(), 14, RGB(100, 200, 90), 1, 0.15f);
        sfx(SFX_RUSTLE);
        int need = playMode ? 3 : 4;
        if (stepsSinceBattle >= need && rndInt(playMode ? 6 : 5) == 0) {
            exclaimT = 18;
            pendingSpecies = pickWildSpecies();
            stepsQueued = 0;
        }
    }
}

static void world_tick(Tap tap) {
    if (rustleT) rustleT--;
    if (bannerT) bannerT--;
    if (exclaimT) {
        if (--exclaimT == 0) beginEncounter(pendingSpecies);
        return;
    }
    if (playMode) {
        if (tap == TAP_SINGLE) {
            stepsQueued += 4;
            if (stepsQueued > 12) stepsQueued = 12;
            sfx(SFX_TAP);
        } else if (tap == TAP_DOUBLE) {
            exclaimT = 18;
            pendingSpecies = pickWildSpecies();
            stepsQueued = 0;
            sfx(SFX_TAP);
            return;
        }
    }
    if (trMove > 0) {
        int sp = 2;
        switch (trDir) {
            case D_RIGHT: trX += sp; break;
            case D_LEFT: trX -= sp; break;
            case D_DOWN: trY += sp; break;
            case D_UP: trY -= sp; break;
        }
        trMove -= sp;
        if (trMove == 0) {
            onStepLanded();
            if (playMode && stepsQueued > 0) stepsQueued--;
        }
        return;
    }
    if (exclaimT) return;
    if (playMode) {
        if (stepsQueued > 0) startStep();
    } else {
        if (idlePause > 0) { idlePause--; return; }
        if (rndInt(60) == 0) { idlePause = 20 + rndInt(30); return; }
        startStep();
    }
}

static const uint16_t RIM_LT = 0xE71A;  // stone rim light
static const uint16_t RIM_DK = 0x7B4F;
static const uint16_t WATER_SHADOW = 0x3376;

static void drawWaterRim(int tx, int ty, int sx, int sy) {
    bool up = tileAt(tx, ty - 1) != 'w', dn = tileAt(tx, ty + 1) != 'w';
    bool lf = tileAt(tx - 1, ty) != 'w', rt = tileAt(tx + 1, ty) != 'w';
    if (up) { fillRect(sx, sy, 16, 3, RIM_LT); hline(sx, sx + 15, sy + 3, RIM_DK); fillRect(sx, sy + 4, 16, 2, WATER_SHADOW); }
    if (dn) { fillRect(sx, sy + 13, 16, 3, RIM_LT); hline(sx, sx + 15, sy + 15, RIM_DK); }
    if (lf) { fillRect(sx, sy, 3, 16, RIM_LT); for (int y = 0; y < 16; y++) pset(sx + 3, sy + y, RIM_DK); }
    if (rt) { fillRect(sx + 13, sy, 3, 16, RIM_LT); for (int y = 0; y < 16; y++) pset(sx + 15, sy + y, RIM_DK); }
}

static const Sprite* objSprite(uint8_t kind) {
    switch (kind) {
        case OBJ_TREE: return &SPR_TREE;
        case OBJ_LAMP: return &SPR_LAMP;
        case OBJ_SIGN: return &SPR_SIGN;
        default: return &SPR_CENTER;
    }
}

static void drawPill(int cx, int y, const char* s, uint16_t fill, uint16_t ink) {
    int w = textWidth(s) + 14;
    int x = cx - w / 2;
    panel(x, y, w, 14, RGB(30, 30, 44), blend565(fill, 0xFFFF, 90), fill);
    text(x + 7, y + 3, s, ink, blend565(fill, 0, 120));
}

static void drawBall(int x, int y) { blit(SPR_BALL, x, y); }

static void world_render() {
    int camX = trX + 8 - FB_W / 2, camY = trY + 8 - FB_H / 2;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > MAP_W * 16 - FB_W) camX = MAP_W * 16 - FB_W;
    if (camY > MAP_H * 16 - FB_H) camY = MAP_H * 16 - FB_H;

    int water = (tick / 8) % 4, flower = (tick / 20) % 2;
    for (int ty = camY / 16; ty <= (camY + FB_H) / 16; ty++) {
        for (int tx = camX / 16; tx <= (camX + FB_W) / 16; tx++) {
            int sx = tx * 16 - camX, sy = ty * 16 - camY;
            char ch = tileAt(tx, ty);
            int id = TILE_GRASS;
            switch (ch) {
                case ',': id = TILE_GRASS2; break;
                case 'T': id = (rustleT && tx == rustleX && ty == rustleY && (rustleT / 3) % 2) ? TILE_TALL1 : TILE_TALL0; break;
                case '=': id = TILE_PATH; break;
                case 'r': id = TILE_FLOWERR0 + flower; break;
                case 'y': id = TILE_FLOWERY0 + flower; break;
                case 'o': id = TILE_FLOWERW0 + flower; break;
                case 'w': id = TILE_WATER0 + water; break;
                case 'h': id = TILE_HEDGE; break;
                case 'F': id = TILE_FENCE; break;
            }
            blitTile(*TILESET[id], sx, sy);
            if (ch == 'w') drawWaterRim(tx, ty, sx, sy);
        }
    }
    // sun glints dancing on the pond
    for (int i = 0; i < 6; i++) {
        int gx = 11 * 16 + ((i * 37 + tick / 3) % 120), gy = 10 * 16 + (i * 13) % 64;
        if (((tick / 4) + i * 5) % 12 < 3) {
            pset(gx - camX, gy - camY, 0xFFFF);
            pset(gx - camX + 1, gy - camY, RGB(200, 230, 255));
        }
    }

    // depth-sorted objects + trainer
    int feet = trY + 16;
    bool trainerDrawn = false;
    auto drawTrainer = [&]() {
        int sx = trX - camX, sy = trY - camY;
        blendEllipse(sx + 8, sy + 14, 6, 2, RGB(20, 40, 20), 110);
        int f = 0;
        if (trMove > 8) f = trParity ? 1 : 2;
        else if (trMove > 0) f = 0;
        const Sprite& s = *TRAINER[trDir * 3 + f];
        blit(s, sx, sy + 16 - s.h - 1);
        // tall grass covers the legs
        int tx = (trX + 8) / 16, ty = (trY + 8) / 16;
        if (tileAt(tx, ty) == 'T') blit(SPR_TALLOVER, tx * 16 - camX, ty * 16 - camY);
        if (exclaimT) {
            int bx = sx + 3, by = sy - 14 - (exclaimT > 14 ? (exclaimT - 14) : 0);
            panel(bx, by, 10, 12, RGB(40, 40, 50), RGB(255, 255, 255), RGB(255, 255, 255));
            fillRect(bx + 4, by + 2, 2, 5, RGB(220, 40, 40));
            fillRect(bx + 4, by + 8, 2, 2, RGB(220, 40, 40));
        }
    };
    for (int k = 0; k < MAP_OBJ_COUNT; k++) {
        const MapObj& o = MAP_OBJS[objOrder[k]];
        if (!trainerDrawn && o.y > feet) { drawTrainer(); trainerDrawn = true; }
        const Sprite* s = objSprite(o.kind);
        int sx = o.x - camX, sy = o.y - s->h - camY;
        if (sx > FB_W || sy > FB_H || sx + s->w < 0 || sy + s->h < 0) continue;
        blit(*s, sx, sy);
    }
    if (!trainerDrawn) drawTrainer();
    drawParticles(-camX, -camY);

    // soft vignette-ish top light: subtle warm haze at the top rows
    for (int y = 0; y < 6; y++) blendRect(0, y, FB_W, 1, RGB(255, 250, 220), 40 - y * 6);

    // ROUTE banner (wooden sign that slides down)
    if (bannerT) {
        int t = 90 - bannerT;
        int y = t < 10 ? -24 + t * 3 : (bannerT < 10 ? -24 + bannerT * 3 : 6);
        panel(6, y, 92, 20, RGB(70, 44, 30), RGB(230, 184, 120), RGB(196, 140, 84));
        text(14, y + 6, "ROUTE 4", RGB(255, 250, 235), RGB(110, 70, 40));
        text(62, y + 6, "KALOS", RGB(255, 228, 160), RGB(110, 70, 40));
    }

    if (scene != SC_WORLD) return;  // no HUD during the encounter wipe
    if (!playMode) {
        if ((tick / 15) % 3 != 0) drawPill(FB_W / 2, FB_H - 20, "\x02 TAP TO PLAY", RGB(226, 56, 56), 0xFFFF);
    } else {
        // caught counter chip
        panel(FB_W - 46, 4, 42, 16, RGB(30, 30, 44), RGB(255, 255, 255), RGB(244, 244, 236));
        drawBall(FB_W - 43, 6);
        char b[8];
        snprintf(b, sizeof(b), "x%d", caughtCount);
        text(FB_W - 29, 8, b, RGB(60, 60, 70), RGB(200, 200, 200));
        if (trMove == 0 && stepsQueued == 0 && !exclaimT)
            drawPill(FB_W / 2, FB_H - 20, "TAP walk   2x TAP find POK\x01MON", RGB(52, 76, 140), 0xFFFF);
    }
}

// ==============================================================================
//  ENCOUNTER TRANSITION
// ==============================================================================
static int transT = 0;
static int transSpecies = 0;
static void battle_begin(int species);

static void beginEncounter(int species) {
    scene = SC_TRANS;
    transT = 0;
    transSpecies = species;
    stepsSinceBattle = 0;
    const Species& s = SPECIES[species];
    sfx(s.legend ? SFX_LEGEND : SFX_ENCOUNTER);
    led(s.legend ? s.aura : 0xFFFFFF, 400);
}

static void trans_tick() {
    if (++transT >= 52) battle_begin(transSpecies);
}

static void trans_render() {
    const Species& s = SPECIES[transSpecies];
    world_render();
    int t = transT;
    if (t < 18) {
        uint16_t c = s.legend ? C24(s.aura) : 0xFFFF;
        if ((t / 3) % 2 == 0) blendRect(0, 0, FB_W, FB_H, c, s.legend ? 150 : 190);
        else if (s.legend) blendRect(0, 0, FB_W, FB_H, 0, 120);
        return;
    }
    // Emerald-style closing bars, alternating directions
    int k = t - 18;
    for (int b = 0; b < 10; b++) {
        int w = (k - b) * 240 / 20;
        if (w <= 0) continue;
        if (w > FB_W) w = FB_W;
        int y = b * 16;
        if (b % 2) fillRect(FB_W - w, y, w, 16, 0);
        else fillRect(0, y, w, 16, 0);
        if (w < FB_W && s.legend) fillRect(b % 2 ? FB_W - w - 2 : w, y, 2, 16, C24(s.aura));
    }
}

// ==============================================================================
//  BATTLE
// ==============================================================================
struct Mon { uint8_t sp; uint8_t lvl; int hp, maxHp; int shownHp; };

struct PartyMon { uint8_t sp; uint8_t lvl; int xp; int hp; };
static PartyMon party[4] = {
    {SP_PIKACHU, 14, 0, 0}, {SP_FENNEKIN, 13, 0, 0}, {SP_FROAKIE, 13, 0, 0}, {SP_CHESPIN, 13, 0, 0}};

static int partnerMaxHp(int lvl) { return 22 + lvl * 3; }

enum BState : uint8_t {
    BS_SLIDE, BS_APPEAR, BS_SENDOUT, BS_MENU, BS_ATK_TEXT, BS_ATK_FX, BS_HIT, BS_EFF_TEXT,
    BS_FAINT, BS_EXP, BS_LEVELUP, BS_THROW_TEXT, BS_THROW, BS_ABSORB, BS_DROP, BS_SHAKE,
    BS_CAUGHT, BS_BREAK, BS_PFAINT, BS_WHITEOUT, BS_END
};

static Mon enemy, partner;
static int partyIdx = 0;
static BState bs;
static int bsT = 0;
static bool atkByPartner = true;
static uint8_t curMove = 0;
static int lastEff = 100;
static int pendingTap = TAP_NONE;
static int shakeT = 0;
static int blinkEnemy = 0, blinkPartner = 0;
static int lungeT = 0;
static int ballShakes = 0, ballShakeIdx = 0;
static bool willCatch = false;
static int enemyScale = 256, partnerScale = 256;
static bool enemyVisible = true, partnerVisible = false;
static int flashT = 0;
static uint16_t flashCol = 0xFFFF;
static int xpShown = 0;
static float ballX, ballY;
static int ballOpen = 0;
static bool ballDim = false;

// geometry
#define EN_CX 176
#define EN_BASE 76
#define PL_CX 58
#define PL_BASE 118
#define TEXT_Y 112

// --------------------------------------------------- text box with typewriter
static char msg[96];
static int msgLen = 0, msgShown = 0;
static int msgWrap = 222;

static void setMsg(const char* s, int wrap = 222) {
    // greedy word wrap into at most 2 lines
    msgWrap = wrap;
    char line[96];
    int ln = 0, out = 0;
    msg[0] = 0;
    line[0] = 0;
    const char* p = s;
    while (*p && out < 90) {
        const char* w = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        char word[40];
        int wl = (int)(p - w) < 39 ? (int)(p - w) : 39;
        memcpy(word, w, wl);
        word[wl] = 0;
        char test[96];
        snprintf(test, sizeof(test), "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && (textWidth(test) > wrap || ln == -1)) {
            out += snprintf(msg + out, sizeof(msg) - out, "%s\n", line);
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
        if (*p == '\n') {
            out += snprintf(msg + out, sizeof(msg) - out, "%s\n", line);
            line[0] = 0;
        }
        if (*p) p++;
    }
    if (line[0]) out += snprintf(msg + out, sizeof(msg) - out, "%s", line);
    msgLen = (int)strlen(msg);
    msgShown = 0;
}

static bool msgDone() { return msgShown >= msgLen; }

static void go(BState s) {
    bs = s;
    bsT = 0;
}

static const char* nameOf(const Mon& m) { return SPECIES[m.sp].name; }

static int choosePartner(uint8_t enemyType) {
    int best = -1, bestScore = -1;
    for (int i = 0; i < 4; i++) {
        if (party[i].hp <= 0) continue;
        const Species& s = SPECIES[party[i].sp];
        int sc = typeEff(MOVES[s.moves[0]].type, enemyType) * 10 + rndInt(9) - typeEff(enemyType, s.type) * 3;
        if (sc > bestScore) { bestScore = sc; best = i; }
    }
    return best;
}

static void loadPartner(int i) {
    partyIdx = i;
    partner.sp = party[i].sp;
    partner.lvl = party[i].lvl;
    partner.maxHp = partnerMaxHp(partner.lvl);
    partner.hp = party[i].hp;
    partner.shownHp = partner.hp;
    xpShown = party[i].xp;
}

static void battle_begin(int species) {
    scene = SC_BATTLE;
    clearParticles();
    const Species& s = SPECIES[species];
    enemy.sp = species;
    enemy.lvl = s.legend ? 50 : 4 + rndInt(8);
    enemy.maxHp = s.legend ? 96 : 14 + enemy.lvl * 3;
    enemy.hp = enemy.shownHp = enemy.maxHp;
    for (auto& p : party) p.hp = partnerMaxHp(p.lvl);  // healed at the POKeMON Center
    loadPartner(choosePartner(s.type));
    enemyScale = partnerScale = 256;
    enemyVisible = true;
    partnerVisible = false;
    pendingTap = TAP_NONE;
    ballDim = false;
    go(BS_SLIDE);
    msg[0] = 0;
    msgLen = msgShown = 0;
}

static int calcDamage(const Mon& atk, const Move& mv, int eff) {
    int d = mv.power * (2 * atk.lvl / 5 + 2) / 25 + 2;
    if (SPECIES[atk.sp].legend) d = d / 5 + 6;  // legends hit hard but fair
    d = d * eff / 100;
    d = d * (85 + rndInt(16)) / 100;
    return d < 1 ? 1 : d;
}

static uint8_t pickMove(const Mon& atk, const Mon& def) {
    const Species& s = SPECIES[atk.sp];
    int a = s.moves[0], b = s.moves[1];
    int sa = MOVES[a].power * typeEff(MOVES[a].type, SPECIES[def.sp].type);
    int sb = MOVES[b].power * typeEff(MOVES[b].type, SPECIES[def.sp].type);
    int best = sa >= sb ? a : b, other = sa >= sb ? b : a;
    return rndInt(4) == 0 ? other : best;
}

static void startAttack(bool byPartner) {
    atkByPartner = byPartner;
    Mon& atk = byPartner ? partner : enemy;
    Mon& def = byPartner ? enemy : partner;
    curMove = pickMove(atk, def);
    char b[64];
    if (byPartner) snprintf(b, sizeof(b), "%s used %s!", nameOf(atk), MOVES[curMove].name);
    else snprintf(b, sizeof(b), "%s%s used %s!", SPECIES[atk.sp].legend ? "" : "Wild ", nameOf(atk), MOVES[curMove].name);
    setMsg(b);
    go(BS_ATK_TEXT);
}

static void startThrow() {
    setMsg("You threw a POK\x01 BALL!");
    go(BS_THROW_TEXT);
    const Species& s = SPECIES[enemy.sp];
    int hpPct = enemy.hp * 100 / enemy.maxHp;
    int chance = s.catchPct + (100 - hpPct) * (s.legend ? 50 : 40) / 100;
    if (chance > 97) chance = 97;
    willCatch = rndInt(100) < chance;
    ballShakes = willCatch ? 3 : rndInt(3);
    ballShakeIdx = 0;
}

static void decide(int tap) {
    if (tap == TAP_DOUBLE) startThrow();
    else startAttack(true);
}

// ------------------------------------------------------------- fx runtime
static int fxDur(uint8_t fx) {
    switch (fx) {
        case FX_TACKLE: return 20;
        case FX_QUICK: return 18;
        case FX_MOON: case FX_DARKWING: case FX_ARROWS: return 52;
        default: return 36;
    }
}

static void fxTick(int t) {
    const Move& mv = MOVES[curMove];
    float sx = atkByPartner ? PL_CX : EN_CX, sy = atkByPartner ? 88 : 44;
    float tx = atkByPartner ? EN_CX : PL_CX, ty = atkByPartner ? 44 : 88;
    float dx = tx - sx, dy = ty - sy;
    switch (mv.fx) {
        case FX_TACKLE:
        case FX_QUICK:
            if (t == 7) {
                for (int i = 0; i < 6; i++) {
                    float a = i * 1.047f;
                    spawn(PK_SPARK, tx, ty, cosf(a) * 2.2f, sinf(a) * 2.2f, 10, RGB(255, 240, 160));
                }
                spawn(PK_STAR, tx, ty, 0, 0, 8, 0);
                sfx(SFX_HIT);
            }
            if (mv.fx == FX_QUICK && t < 8)
                for (int i = 0; i < 2; i++)
                    spawn(PK_DOT, sx + dx * t / 8 + rndInt(20) - 10, sy + dy * t / 8 + rndInt(30) - 15, -dx / 30, -dy / 30, 8, 0xFFFF);
            break;
        case FX_BOLT:
            if (t < 12) {
                float a = t * 0.9f;
                spawn(PK_SPARK, sx + cosf(a) * 18, sy + sinf(a) * 14, 0, -0.4f, 8, RGB(255, 230, 60));
            }
            if (t == 12) { sfx(SFX_BOLT); led(0xFFE040, 500); }
            if (t >= 12 && t < 30) {
                if (t % 2 == 0) { flashT = 2; flashCol = RGB(255, 250, 180); }
                for (int i = 0; i < 2; i++)
                    spawn(PK_SPARK, tx + rndInt(36) - 18, ty + rndInt(30) - 15, (rndf() - .5f) * 3, (rndf() - .5f) * 3, 8, RGB(255, 230, 60));
            }
            break;
        case FX_FIRE:
            if (t == 0) { sfx(SFX_FIRE); led(0xFF6020, 600); }
            if (t < 20)
                for (int i = 0; i < 2; i++) {
                    float sp = 0.075f + rndf() * 0.02f;
                    spawn(PK_EMBER, sx + rndInt(8) - 4, sy + rndInt(8) - 4, dx * sp + (rndf() - .5f), dy * sp + (rndf() - .5f), 14,
                          rndInt(2) ? RGB(255, 120, 40) : RGB(255, 200, 60), 2 + rndInt(2));
                }
            if (t >= 16 && t < 32)
                for (int i = 0; i < 2; i++)
                    spawn(PK_EMBER, tx + rndInt(40) - 20, ty + 16, (rndf() - .5f) * .5f, -1.2f - rndf(), 16,
                          rndInt(3) ? RGB(255, 100, 30) : RGB(255, 220, 90), 3);
            break;
        case FX_WATER:
            if (t == 0) { sfx(SFX_WATER); led(0x3090FF, 600); }
            if (t < 22) {
                float sp = 0.07f;
                spawn(PK_BUBBLE, sx, sy, dx * sp + (rndf() - .5f) * .6f, dy * sp + (rndf() - .5f) * .6f, 15,
                      RGB(120, 200, 255), 2 + rndInt(2));
            }
            if (t >= 16 && t < 30)
                spawn(PK_DOT, tx + rndInt(30) - 15, ty, (rndf() - .5f) * 3, -2 - rndf() * 2, 16, RGB(160, 220, 255), 1, 0.25f);
            break;
        case FX_LEAF:
            if (t == 0) { sfx(SFX_LEAF); led(0x40D040, 600); }
            if (t < 18)
                for (int i = 0; i < 2; i++)
                    spawn(PK_LEAF, sx + rndInt(10) - 5, sy, dx / 16 + (rndf() - .5f) * 1.5f, dy / 16 - 1.2f, 18,
                          rndInt(2) ? RGB(90, 200, 70) : RGB(150, 230, 90), 1, 0.08f);
            break;
        case FX_WIND:
            if (t == 0) { sfx(SFX_WIND); led(0xC0E0FF, 600); }
            if (t < 28) {
                float a = t * 0.6f;
                spawn(PK_DOT, tx + cosf(a) * 22, ty + sinf(a) * 10, -sinf(a) * 2, cosf(a), 10, 0xFFFF);
                spawn(PK_LEAF, tx + cosf(a + 3) * 18, ty + sinf(a + 3) * 8 + 10, -sinf(a + 3) * 2, -0.6f, 12, RGB(130, 200, 90));
            }
            break;
        case FX_MUD:
            if (t == 0) { sfx(SFX_QUAKE); led(0xA06830, 600); }
            if (t < 16 && t % 2 == 0) {
                float flight = 18;
                spawn(PK_EMBER, sx, sy, dx / flight, dy / flight - 2.4f, 18, RGB(140, 96, 52), 3, 2.4f / (flight / 2));
            }
            if (t >= 18 && t < 26)
                spawn(PK_DUST, tx + rndInt(30) - 15, ty + 10, (rndf() - .5f), -0.5f, 14, RGB(170, 130, 90), 4);
            break;
        case FX_MOON:
            if (t == 0) { sfx(SFX_MOON); led(0xFF80E0, 1200); }
            if (t > 24 && t < 40)
                for (int i = 0; i < 2; i++)
                    spawn(PK_SPARK, tx + rndInt(40) - 20, ty + rndInt(40) - 20, 0, -0.5f, 12, RGB(255, 150, 230));
            break;
        case FX_DARKWING:
            if (t == 0) { sfx(SFX_DARK); led(0xFF1020, 1200); }
            if (t % 2 == 0)
                spawn(PK_FEATHER, rndInt(FB_W), -4, -0.5f + rndf(), 1.5f + rndf() * 2, 40, RGB(40, 30, 50));
            if (t > 26 && t < 40 && t % 3 == 0) { flashT = 2; flashCol = RGB(255, 40, 60); }
            break;
        case FX_ARROWS:
            if (t == 0) { sfx(SFX_QUAKE); led(0x40FF40, 1200); }
            if (t < 34)
                for (int i = 0; i < 2; i++)
                    spawn(PK_CELL, tx + rndInt(60) - 30, -4, 0, 5 + rndf() * 2, 22, RGB(90, 240, 80));
            if (t >= 24 && t < 44) { shakeT = 2; if (t % 2) spawn(PK_DUST, tx + rndInt(50) - 25, ty + 18, 0, -0.3f, 14, RGB(150, 140, 110), 5); }
            break;
    }
}

static void fxRender(int t, int ox, int oy) {
    const Move& mv = MOVES[curMove];
    int sx = atkByPartner ? PL_CX : EN_CX, sy = atkByPartner ? 88 : 44;
    int tx = (atkByPartner ? EN_CX : PL_CX) + ox, ty = (atkByPartner ? 44 : 88) + oy;
    switch (mv.fx) {
        case FX_BOLT:
            if (t >= 12 && t < 30) {
                // jagged bolts from the sky
                for (int b = 0; b < 3; b++) {
                    int x = tx - 14 + b * 14 + rndInt(9) - 4, y = -2;
                    while (y < ty) {
                        int nx = x + rndInt(13) - 6, ny = y + 6 + rndInt(6);
                        thickLine(x, y, nx, ny, RGB(255, 245, 120));
                        line(x, y, nx, ny, 0xFFFF);
                        x = nx; y = ny;
                    }
                }
                blendEllipse(tx, ty, 26, 20, RGB(255, 250, 160), 90);
            }
            break;
        case FX_LEAF:
            if (t >= 14 && t < 26) {  // whip lash
                int midx = (sx + tx) / 2, midy = (sy + ty) / 2 - 30 + (t - 14) * 3;
                for (int i = 0; i <= 16; i++) {
                    float u = i / 16.0f;
                    int x = (int)((1 - u) * (1 - u) * sx + 2 * u * (1 - u) * midx + u * u * tx);
                    int y = (int)((1 - u) * (1 - u) * sy + 2 * u * (1 - u) * midy + u * u * ty);
                    fillCircle(x, y, 1, RGB(60, 150, 50));
                }
            }
            break;
        case FX_WIND:
            for (int r = 0; r < 3; r++) {
                int rr = 10 + r * 8 + (t % 6);
                for (int a = 0; a < 10; a++) {
                    float ang = a * 0.3f + t * 0.35f + r * 2;
                    pblend(tx + (int)(cosf(ang) * rr), ty + (int)(sinf(ang) * rr * 0.5f), 0xFFFF, 180);
                }
            }
            break;
        case FX_MOON: {
            blendRect(0, 0, FB_W, TEXT_Y, RGB(20, 10, 40), t < 8 ? t * 18 : 140);
            int r = t < 24 ? t * 14 / 24 : 14;
            int mx = tx, my = 14;
            blendEllipse(mx, my, r + 6, r + 6, RGB(255, 140, 220), 70);
            fillCircle(mx, my, r, RGB(255, 190, 235));
            if (r > 4) fillCircle(mx - r / 3, my - r / 3, r / 3, RGB(255, 235, 250));
            if (t >= 24 && t < 42) {
                int w = (t < 30 ? (t - 24) * 2 : 12) - (t > 36 ? (t - 36) * 2 : 0);
                blendRect(mx - w, my, w * 2, ty - my + 10, RGB(255, 170, 235), 170);
                fillRect(mx - w / 3, my, w * 2 / 3 + 1, ty - my + 10, RGB(255, 240, 252));
            }
            break;
        }
        case FX_DARKWING: {
            blendRect(0, 0, FB_W, TEXT_Y, RGB(60, 0, 10), t < 10 ? t * 16 : 160);
            if (t > 18 && t < 44) {
                int k = t - 18;
                for (int i = -2; i <= 2; i++) {
                    thickLine(tx - 60 + k * 2, ty - 50 + i, tx + 4 * i, ty + i, RGB(240, 30, 50));
                    thickLine(tx + 60 - k * 2, ty - 50 + i, tx + 4 * i, ty + i, RGB(240, 30, 50));
                }
                if (k > 8) blendEllipse(tx, ty, 18 + (k - 8), 14 + (k - 8) / 2, RGB(255, 60, 80), 120);
            }
            break;
        }
        case FX_ARROWS:
            blendRect(0, 0, FB_W, TEXT_Y, RGB(0, 30, 0), t < 10 ? t * 14 : 140);
            break;
        default: break;
    }
}

// ------------------------------------------------------------- battle tick
static void startNextTurnAfterHit() {
    Mon& def = atkByPartner ? enemy : partner;
    if (def.hp <= 0) {
        go(atkByPartner ? BS_FAINT : BS_PFAINT);
        return;
    }
    if (atkByPartner) startAttack(false);
    else go(BS_MENU);
}

static bool textWait(int extra) { return msgDone() && bsT > extra; }

static void battle_tick(Tap tap) {
    bsT++;
    if (msgShown < msgLen) msgShown++;
    if (shakeT) shakeT--;
    if (blinkEnemy) blinkEnemy--;
    if (blinkPartner) blinkPartner--;
    if (lungeT) lungeT--;
    if (flashT) flashT--;
    if (playMode && tap != TAP_NONE && bs != BS_MENU) pendingTap = tap;
    updateParticles();

    // HP bars drain smoothly
    Mon* mons[2] = {&enemy, &partner};
    for (Mon* m : mons) {
        if (m->shownHp > m->hp) m->shownHp -= (m->maxHp > 60 ? 2 : 1);
        if (m->shownHp < m->hp) m->shownHp = m->hp;
    }

    const Species& es = SPECIES[enemy.sp];
    switch (bs) {
        case BS_SLIDE:
            if (bsT == 30) {
                char b[64];
                if (es.legend) snprintf(b, sizeof(b), "The legendary %s appeared!", es.name);
                else snprintf(b, sizeof(b), "Wild %s appeared!", es.name);
                setMsg(b);
                sfx(SFX_CRY, es.cryHz);
                go(BS_APPEAR);
            }
            break;
        case BS_APPEAR:
            if (textWait(40)) {
                char b[48];
                snprintf(b, sizeof(b), "Go! %s!", nameOf(partner));
                setMsg(b);
                go(BS_SENDOUT);
            }
            break;
        case BS_SENDOUT:
            if (bsT == 4) sfx(SFX_THROW);
            if (bsT == 20) {
                partnerVisible = true;
                partnerScale = 0;
                sfx(SFX_POP);
                for (int i = 0; i < 10; i++) {
                    float a = i * 0.628f;
                    spawn(PK_SPARK, PL_CX, 92, cosf(a) * 2.5f, sinf(a) * 2, 12, RGB(255, 255, 200));
                }
            }
            if (partnerVisible && partnerScale < 256) partnerScale += 32;
            if (bsT == 30) sfx(SFX_CRY, SPECIES[partner.sp].cryHz);
            if (textWait(44)) go(BS_MENU);
            break;
        case BS_MENU: {
            if (bsT == 1) {
                char b[48];
                snprintf(b, sizeof(b), "What will %s do?", nameOf(partner));
                setMsg(b, 130);
            }
            if (playMode) {
                int t = tap != TAP_NONE ? tap : pendingTap;
                if (t != TAP_NONE && bsT > 2) {
                    pendingTap = TAP_NONE;
                    sfx(SFX_TAP);
                    decide(t);
                }
            } else if (bsT > 50) {
                int hpPct = enemy.hp * 100 / enemy.maxHp;
                bool tryCatch = hpPct < (es.legend ? 35 : 55) && rndInt(3) != 0;
                decide(tryCatch ? TAP_DOUBLE : TAP_SINGLE);
            }
            break;
        }
        case BS_ATK_TEXT:
            if (textWait(18)) { go(BS_ATK_FX); lungeT = 12; }
            break;
        case BS_ATK_FX:
            fxTick(bsT - 1);
            if (bsT >= fxDur(MOVES[curMove].fx)) {
                Mon& atk = atkByPartner ? partner : enemy;
                Mon& def = atkByPartner ? enemy : partner;
                lastEff = typeEff(MOVES[curMove].type, SPECIES[def.sp].type);
                int d = calcDamage(atk, MOVES[curMove], lastEff);
                def.hp -= d;
                if (def.hp < 0) def.hp = 0;
                if (atkByPartner) blinkEnemy = 18; else blinkPartner = 18;
                shakeT = 8;
                sfx(lastEff > 100 ? SFX_SUPER : lastEff < 100 ? SFX_WEAK : SFX_HIT);
                led(lastEff > 100 ? 0xFFFFFF : 0xFF4020, 150);
                go(BS_HIT);
            }
            break;
        case BS_HIT: {
            Mon& def = atkByPartner ? enemy : partner;
            if (bsT > 18 && def.shownHp == def.hp) {
                if (lastEff != 100) {
                    setMsg(lastEff > 100 ? "It's super effective!" : "It's not very effective...");
                    go(BS_EFF_TEXT);
                } else {
                    startNextTurnAfterHit();
                }
            }
            break;
        }
        case BS_EFF_TEXT:
            if (textWait(26)) startNextTurnAfterHit();
            break;
        case BS_FAINT:
            if (bsT == 1) sfx(SFX_CRY, es.cryHz / 2);
            if (bsT == 18) {
                enemyVisible = false;
                char b[48];
                snprintf(b, sizeof(b), "%s%s fainted!", es.legend ? "" : "Wild ", es.name);
                setMsg(b);
                sfx(SFX_FAINT);
            }
            if (bsT > 18 && textWait(60)) {
                PartyMon& pm = party[partyIdx];
                int gain = enemy.lvl * (es.legend ? 3 : 7);
                char b[64];
                snprintf(b, sizeof(b), "%s gained %d EXP. Points!", nameOf(partner), gain);
                setMsg(b);
                pm.xp += gain;
                sfx(SFX_EXP);
                go(BS_EXP);
            }
            break;
        case BS_EXP: {
            PartyMon& pm = party[partyIdx];
            int need = pm.lvl * 12;
            if (xpShown < pm.xp) xpShown += 2;
            if (xpShown >= need) {
                pm.xp -= need;
                xpShown = 0;
                pm.lvl++;
                partner.lvl = pm.lvl;
                char b[48];
                snprintf(b, sizeof(b), "%s grew to Lv. %d!", nameOf(partner), pm.lvl);
                setMsg(b);
                sfx(SFX_LEVELUP);
                led(0x40FFFF, 500);
                go(BS_LEVELUP);
                break;
            }
            if (xpShown >= pm.xp && textWait(40)) go(BS_END);
            break;
        }
        case BS_LEVELUP:
            if (bsT % 3 == 0) spawn(PK_SPARK, PL_CX + rndInt(40) - 20, 110, 0, -1.5f, 20, RGB(120, 240, 255));
            if (textWait(50)) go(xpShown < party[partyIdx].xp ? BS_EXP : BS_END);
            break;
        case BS_THROW_TEXT:
            if (textWait(10)) {
                sfx(SFX_THROW);
                go(BS_THROW);
            }
            break;
        case BS_THROW: {
            float u = bsT / 22.0f;
            ballX = 10 + (EN_CX - 6 - 10) * u;
            ballY = 110 + (30 - 110) * u - sinf(u * 3.14159f) * 50;
            if (bsT >= 22) {
                ballOpen = 1;
                sfx(SFX_POP);
                led(0xFF3030, 300);
                go(BS_ABSORB);
            }
            break;
        }
        case BS_ABSORB:
            if (bsT < 18) {
                enemyScale = 256 - bsT * 15;
                if (bsT % 2 == 0) spawn(PK_SPARK, EN_CX + rndInt(40) - 20, 50 + rndInt(30) - 15, 0, -1, 8, RGB(255, 120, 120));
            } else {
                enemyVisible = false;
                enemyScale = 256;
                ballOpen = 0;
                go(BS_DROP);
            }
            break;
        case BS_DROP: {
            float target = EN_BASE - 14;
            if (bsT < 10) ballY = 30 + (target - 30) * (bsT / 10.0f) * (bsT / 10.0f);
            else if (bsT < 16) ballY = target - sinf((bsT - 10) / 6.0f * 3.14159f) * 6;
            else ballY = target;
            if (bsT == 10) sfx(SFX_SHAKE);
            if (bsT > 28) go(BS_SHAKE);
            break;
        }
        case BS_SHAKE:
            if (bsT == 1 && ballShakeIdx < ballShakes) sfx(SFX_SHAKE);
            if (bsT >= 26) {
                if (ballShakeIdx < ballShakes) {
                    ballShakeIdx++;
                    if (ballShakeIdx >= 3 && willCatch) {
                        go(BS_CAUGHT);
                        break;
                    }
                    bsT = 0;
                    if (ballShakeIdx >= ballShakes && !willCatch) go(BS_BREAK);
                } else {
                    go(BS_BREAK);
                }
            }
            break;
        case BS_CAUGHT:
            if (bsT == 1) {
                char b[48];
                snprintf(b, sizeof(b), "Gotcha! %s was caught!", es.name);
                setMsg(b);
                sfx(SFX_CATCH);
                led(0x40FF60, 900);
                caughtCount++;
                ballDim = true;
                for (int i = 0; i < 3; i++) spawn(PK_STAR, ballX + 6, ballY + 2, -1.2f + i * 1.2f, -2.2f, 26, 0, 1, 0.12f);
            }
            if (textWait(70)) go(BS_END);
            break;
        case BS_BREAK:
            if (bsT == 1) {
                enemyVisible = true;
                enemyScale = 64;
                sfx(SFX_BREAK);
                for (int i = 0; i < 10; i++) {
                    float a = i * 0.628f;
                    spawn(PK_SPARK, EN_CX, 56, cosf(a) * 2.6f, sinf(a) * 2, 12, 0xFFFF);
                }
                setMsg(es.legend ? "Argh! It's too strong!" : "Oh no! It broke free!");
            }
            if (enemyScale < 256) enemyScale += 24;
            if (enemyScale > 256) enemyScale = 256;
            if (textWait(36)) startAttack(false);
            break;
        case BS_PFAINT:
            if (bsT == 1) sfx(SFX_CRY, SPECIES[partner.sp].cryHz / 2);
            if (bsT == 18) {
                partnerVisible = false;
                party[partyIdx].hp = 0;
                char b[48];
                snprintf(b, sizeof(b), "%s fainted!", nameOf(partner));
                setMsg(b);
                sfx(SFX_FAINT);
            }
            if (bsT > 18 && textWait(50)) {
                int next = choosePartner(es.type);
                if (next >= 0) {
                    loadPartner(next);
                    char b[48];
                    snprintf(b, sizeof(b), "Go! %s!", nameOf(partner));
                    setMsg(b);
                    partnerScale = 256;
                    go(BS_SENDOUT);
                } else {
                    setMsg("Your team needs a rest... Back to the POK\x01MON Center!");
                    go(BS_WHITEOUT);
                }
            }
            break;
        case BS_WHITEOUT:
            if (textWait(70)) go(BS_END);
            break;
        case BS_END:
            if (bsT >= 18) {
                scene = SC_WORLD;
                fadeIn = 16;
                clearParticles();
            }
            break;
    }
}

// ------------------------------------------------------------- battle render
static void drawBackground(int ox, int oy) {
    const Species& es = SPECIES[enemy.sp];
    if (enemy.sp == SP_XERNEAS) {
        gradientV(0, TEXT_Y, 0x0A1440, 0x1E6A8A);
        for (int i = 0; i < 26; i++) {  // twinkling fairy lights
            int x = (i * 97 + 13) % FB_W, y = (i * 53 + 7) % 100;
            int ph = (tick + i * 7) % 40;
            if (ph < 20) {
                uint16_t c = (i % 3 == 0) ? RGB(255, 200, 240) : (i % 3 == 1) ? RGB(160, 230, 255) : RGB(255, 240, 160);
                pset(x, y, c);
                if (ph > 6 && ph < 14) { pset(x - 1, y, c); pset(x + 1, y, c); pset(x, y - 1, c); pset(x, y + 1, c); }
            }
        }
    } else if (enemy.sp == SP_YVELTAL) {
        gradientV(0, TEXT_Y, 0x1A0408, 0x6A0E18);
        for (int i = 0; i < 6; i++) {  // drifting ash
            int x = (i * 61 + tick) % FB_W, y = (i * 29 + tick / 2) % TEXT_Y;
            pset(x, y, RGB(200, 60, 70));
        }
    } else if (enemy.sp == SP_ZYGARDE) {
        gradientV(0, TEXT_Y, 0x02140A, 0x0C3A1C);
        uint16_t g = RGB(30, 90, 50);
        for (int y = 6; y < TEXT_Y; y += 14)  // hex lattice
            for (int x = ((y / 14) % 2) * 12; x < FB_W; x += 24) {
                line(x, y, x + 6, y - 4, g); line(x + 6, y - 4, x + 12, y, g);
                line(x, y, x, y + 6, g);
            }
        for (int i = 0; i < 8; i++) {
            int x = (i * 71) % FB_W, y = TEXT_Y - ((tick + i * 23) % TEXT_Y);
            pset(x, y, RGB(110, 240, 100));
            pset(x + 1, y, RGB(60, 180, 70));
        }
    } else {
        // classic daytime meadow: sky, distant tree line, field
        gradientV(0, 46, 0x78C8F8, 0xD8F0F8);
        for (int i = 0; i < 3; i++) {  // slow clouds
            int cx = ((i * 90 + tick / 6) % (FB_W + 60)) - 30, cy = 12 + i * 9;
            fillEllipse(cx, cy, 14, 4, RGB(255, 255, 255));
            fillEllipse(cx + 8, cy - 2, 8, 4, RGB(255, 255, 255));
        }
        for (int x = 0; x < FB_W; x++) {
            int h = 6 + (int)(4 * sinf(x * 0.21f) + 3 * sinf(x * 0.067f + 1));
            for (int y = 46 - h; y < 48; y++) pset(x, y, y < 46 - h + 2 ? RGB(80, 160, 90) : RGB(56, 128, 76));
        }
        gradientV(48, TEXT_Y, 0xA8E080, 0x70C060);
        for (int y = 52; y < TEXT_Y; y += 6)
            for (int x = (y * 7) % 11; x < FB_W; x += 11) { pset(x, y, RGB(96, 176, 80)); pset(x + 1, y - 1, RGB(96, 176, 80)); }
    }
    (void)es; (void)ox; (void)oy;  // backgrounds stay put while sprites shake
}

static void drawPlatform(int cx, int cy, int rx, int ry) {
    bool dark = SPECIES[enemy.sp].legend;
    uint16_t rim = dark ? RGB(30, 30, 40) : RGB(80, 140, 70);
    uint16_t top = dark ? RGB(70, 70, 90) : RGB(150, 210, 110);
    uint16_t hi = dark ? RGB(100, 100, 124) : RGB(190, 232, 140);
    if (enemy.sp == SP_XERNEAS) { top = RGB(50, 110, 150); hi = RGB(90, 160, 200); }
    if (enemy.sp == SP_YVELTAL) { top = RGB(80, 30, 40); hi = RGB(120, 50, 60); }
    if (enemy.sp == SP_ZYGARDE) { top = RGB(30, 80, 50); hi = RGB(50, 120, 70); }
    fillEllipse(cx, cy + 2, rx, ry, rim);
    fillEllipse(cx, cy, rx - 1, ry - 1, top);
    fillEllipse(cx - rx / 6, cy - ry / 4, rx * 2 / 3, ry / 2, hi);
}

static void drawHpBar(int x, int y, int w, int hp, int maxHp) {
    fillRect(x, y, w + 2, 5, RGB(56, 64, 64));
    int fw = maxHp > 0 ? hp * w / maxHp : 0;
    int pct = maxHp > 0 ? hp * 100 / maxHp : 0;
    uint16_t c = pct > 50 ? RGB(88, 208, 128) : pct > 20 ? RGB(248, 200, 56) : RGB(240, 72, 56);
    uint16_t cl = blend565(c, 0xFFFF, 110);
    fillRect(x + 1, y + 1, fw, 3, c);
    hline(x + 1, x + fw, y + 1, cl);
}

static void hpBox(int x, int y, const Mon& m, bool player) {
    int h = player ? 38 : 28;
    uint16_t ink = RGB(64, 64, 64), sh = RGB(216, 208, 176);
    panel(x, y, 104, h, RGB(56, 64, 72), RGB(208, 208, 176), RGB(248, 248, 216));
    text(x + 7, y + 4, SPECIES[m.sp].name, ink, sh);
    char b[16];
    snprintf(b, sizeof(b), "Lv%d", m.lvl);
    text(x + 97 - textWidth(b), y + 4, b, ink, sh);
    text(x + 7, y + 15, "HP", RGB(240, 160, 48), RGB(160, 96, 32));
    drawHpBar(x + 22, y + 17, 74, m.shownHp, m.maxHp);
    if (player) {
        snprintf(b, sizeof(b), "%d/%d", m.shownHp, m.maxHp);
        text(x + 97 - textWidth(b), y + 24, b, ink, sh);
        PartyMon& pm = party[partyIdx];
        int need = pm.lvl * 12;
        int xw = need ? (xpShown > need ? need : xpShown) * 60 / need : 0;
        fillRect(x + 7, y + 34, 62, 3, RGB(56, 64, 64));
        fillRect(x + 8, y + 35, xw, 1, RGB(64, 200, 248));
    }
}

static void drawTextBox() {
    panel(0, TEXT_Y, FB_W, FB_H - TEXT_Y, RGB(24, 30, 44), RGB(200, 168, 72), RGB(40, 72, 104));
    blendRect(3, TEXT_Y + 3, FB_W - 6, 2, 0xFFFF, 30);
    // typewriter text, line by line
    int x = 10, y = TEXT_Y + 10, n = msgShown;
    const char* p = msg;
    while (*p && n > 0) {
        const char* e = strchr(p, '\n');
        int len = e ? (int)(e - p) : (int)strlen(p);
        char ln[64];
        int cl = len < 63 ? len : 63;
        memcpy(ln, p, cl);
        ln[cl] = 0;
        text(x, y, ln, RGB(248, 248, 248), RGB(80, 88, 104), n < cl ? n : cl);
        n -= len + 1;
        y += 14;
        if (!e) break;
        p = e + 1;
    }
    if (msgDone() && msgLen && bs != BS_MENU && (tick / 8) % 2) text(FB_W - 16, FB_H - 14, "\x03", RGB(248, 200, 80), RGB(80, 60, 30));

    if (bs == BS_MENU) {
        int px = 146;
        panel(px, TEXT_Y + 2, FB_W - px - 2, FB_H - TEXT_Y - 4, RGB(72, 72, 88), RGB(208, 208, 216), RGB(248, 248, 248));
        uint16_t ink = RGB(64, 64, 72), sh = RGB(208, 208, 208);
        if (playMode) {
            text(px + 8, TEXT_Y + 10, "\x02", RGB(226, 56, 56), sh);
            text(px + 16, TEXT_Y + 10, "TAP", ink, sh);
            text(px + 44, TEXT_Y + 10, "FIGHT", RGB(200, 60, 40), sh);
            text(px + 8, TEXT_Y + 26, "2x TAP", ink, sh);
            blit(SPR_BALL, px + 44, TEXT_Y + 24);
            text(px + 58, TEXT_Y + 26, "BALL", RGB(40, 90, 180), sh);
        } else if ((tick / 15) % 3 != 0) {
            text(px + 12, TEXT_Y + 10, "TAP TO", RGB(226, 56, 56), sh);
            text(px + 12, TEXT_Y + 26, "PLAY!", RGB(226, 56, 56), sh);
        }
    }
}

static void battle_render() {
    int ox = 0, oy = 0;
    if (shakeT) { ox = (shakeT % 2 ? 3 : -3); oy = (shakeT % 4 < 2 ? 1 : -1); }
    drawBackground(ox, oy);

    // slide-in offsets
    int slide = bs == BS_SLIDE ? (30 - bsT) * 8 : 0;
    int enOff = -slide, plOff = slide;

    drawPlatform(EN_CX + enOff + ox, EN_BASE - 4 + oy, 48, 10);
    drawPlatform(PL_CX + plOff + ox, PL_BASE - 4 + oy, 60, 12);

    const Species& es = SPECIES[enemy.sp];
    int bob = ((tick / 12) % 2);

    // enemy
    int atkLunge = 0;
    if (bs == BS_ATK_FX && (MOVES[curMove].fx == FX_TACKLE || MOVES[curMove].fx == FX_QUICK)) {
        int t = bsT;
        atkLunge = t < 7 ? t * 4 : (t < 14 ? (14 - t) * 4 : 0);
    }
    if (enemyVisible && !(blinkEnemy && (blinkEnemy / 2) % 2)) {
        BlitOpt o;
        o.scale = enemyScale;
        int ex = EN_CX - 32 + enOff + ox, ey = EN_BASE - 64 + oy + (bs == BS_SLIDE ? 0 : bob);
        if (!atkByPartner) { ex -= atkLunge; ey += atkLunge / 3; }
        if (bs == BS_SLIDE) { o.mode = BM_BLACK; o.alpha = 200; }
        if (bs == BS_ABSORB) { o.mode = BM_TINT; o.tint = RGB(255, 60, 60); o.alpha = 180; }
        if (bs == BS_FAINT) { ey += bsT * 4; o.clipBottom = EN_BASE + oy; }
        if (es.legend && bs != BS_SLIDE) {  // pulsing aura
            int a = 40 + (int)(30 * sinf(tick * 0.15f));
            blendEllipse(EN_CX + ox, EN_BASE - 30 + oy, 38, 34, C24(es.aura), a);
        }
        blitEx(*es.front, ex, ey, o);
    }

    // partner (back sprite)
    if (partnerVisible && !(blinkPartner && (blinkPartner / 2) % 2)) {
        BlitOpt o;
        o.scale = partnerScale;
        if (partnerScale < 256) { o.mode = BM_WHITE; o.alpha = 256 - partnerScale; }
        int px = PL_CX - 32 + plOff + ox, py = PL_BASE - 66 + oy + (bob ^ 1);
        if (atkByPartner) { px += atkLunge; py -= atkLunge / 3; }
        if (bs == BS_PFAINT) { py += bsT * 4; o.clipBottom = TEXT_Y; }
        blitEx(*SPECIES[partner.sp].back, px, py, o);
    }

    // send-out ball arc
    if (bs == BS_SENDOUT && bsT < 20 && !partnerVisible) {
        float u = bsT / 20.0f;
        int bx = (int)(-10 + (PL_CX - (-10)) * u), by = (int)(100 - sinf(u * 3.14159f) * 40);
        blit(SPR_BALL, bx - 6, by - 6);
    }
    // capture ball
    if (bs == BS_THROW || bs == BS_ABSORB || bs == BS_DROP || bs == BS_SHAKE || bs == BS_CAUGHT) {
        int bx = (int)ballX + ox, by = (int)ballY + oy;
        if (bs == BS_SHAKE && ballShakeIdx < ballShakes && bsT < 16) bx += (bsT / 4 % 2 ? 2 : -2);
        if (bs == BS_ABSORB) {
            blendEllipse(bx + 6, by + 6, 10 + bsT / 2, 8 + bsT / 3, RGB(255, 230, 230), 120);
            line(bx + 6, by + 6, EN_CX, EN_BASE - 30, RGB(255, 200, 200));
        }
        BlitOpt o;
        if (ballDim) { o.mode = BM_TINT; o.tint = RGB(40, 40, 60); o.alpha = 90; }
        blitEx(SPR_BALL, bx, by, o);
    }

    if (bs == BS_ATK_FX) fxRender(bsT, ox, oy);
    drawParticles(ox, oy);

    bool ballHoldsEnemy = bs == BS_ABSORB || bs == BS_DROP || bs == BS_SHAKE || bs == BS_CAUGHT;
    if (bs != BS_SLIDE && (enemyVisible || ballHoldsEnemy)) hpBox(6, 8, enemy, false);
    if (partnerVisible || bs == BS_PFAINT) hpBox(130, 70, partner, true);

    if (flashT) blendRect(0, 0, FB_W, TEXT_Y, flashCol, 120);
    drawTextBox();

    if (bs == BS_END) blendRect(0, 0, FB_W, FB_H, 0, bsT * 15);
}

// ==============================================================================
//  TOP LEVEL
// ==============================================================================
static void game_init(uint32_t seed) {
    if (seed) rngState = seed;
    world_reset();
    bannerT = 90;
    fadeIn = 16;
}

static void setPlayMode(bool on) {
    if (playMode == on) return;
    playMode = on;
    if (on) {
        toast("YOUR TURN! LET'S GO!");
        plat_sfx(SFX_MODE, 1);
        led(0x40A0FF, 300);
        stepsQueued = 0;
    } else {
        toast("DEMO MODE");
    }
}

static void game_tick(Tap tap) {
    tick++;
    if (toastT) toastT--;
    if (fadeIn) fadeIn--;
    if (tap != TAP_NONE) {
        lastInputTick = tick;
        if (!playMode) {
            setPlayMode(true);
            tap = TAP_NONE;  // the wake-up tap is consumed
        }
    } else if (playMode && tick - lastInputTick > IDLE_TO_DEMO_TICKS) {
        setPlayMode(false);
    }
    switch (scene) {
        case SC_WORLD: world_tick(tap); updateParticles(); break;
        case SC_TRANS: trans_tick(); break;
        case SC_BATTLE: battle_tick(tap); break;
    }
}

static void game_render() {
    switch (scene) {
        case SC_WORLD: world_render(); break;
        case SC_TRANS: trans_render(); break;
        case SC_BATTLE: battle_render(); break;
    }
    if (fadeIn) blendRect(0, 0, FB_W, FB_H, 0, fadeIn * 16);
    if (toastT) {
        int y = toastT > 65 ? (75 - toastT) * 2 - 14 : (toastT < 10 ? toastT * 2 - 14 : 6);
        if (scene == SC_BATTLE) y += 30;
        drawPill(FB_W / 2, y, toastMsg, RGB(248, 200, 64), RGB(60, 40, 20));
    }
}
