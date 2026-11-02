// ==============================================================================
//  Desk Bronze - fine terracotta sand. Every grain on screen is a pixel: a shaded
//  palette plus tiled noise, with a slow crawl toward the funnel, a mottled falling
//  thread, and individual grains that bounce and roll down the pile.
// ==============================================================================
#pragma once
#include "../hg_core.h"
#include "../art/bronze.h"

namespace bronze {

struct Grain { float x, y, vx, vy; uint8_t mode, life; }; // mode 0 off, 1 falling, 2 rolling
#define BRONZE_GRAINS 48
struct State {
    GlassVolume vol;
    Grain g[BRONZE_GRAINS];
    uint16_t pal[8 * 8];
    int16_t pileTop[160];
    float brk;
    float pileX, pileY;  // tip of the pile
    float halfBase, coneH, lowLevel;
};

static const uint8_t F_LIGHT[3] = {240, 150, 100}, F_MID[3] = {198, 104, 62}, F_DEEP[3] = {112, 50, 26};
static const uint8_t B_LIGHT[3] = {190, 208, 150}, B_MID[3] = {128, 156, 104}, B_DEEP[3] = {64, 88, 56};

static void buildPalette(State& st, float brk) {
    int t = (int)(brk * 255);
    for (int s = 0; s < 8; s++) {
        // s 0 = lit side, 7 = shadow side
        int k = s * 255 / 7;
        uint16_t fa = mix565(C565(F_LIGHT[0], F_LIGHT[1], F_LIGHT[2]), C565(F_MID[0], F_MID[1], F_MID[2]), k < 128 ? k * 2 : 255);
        uint16_t fb = mix565(fa, C565(F_DEEP[0], F_DEEP[1], F_DEEP[2]), k < 128 ? 0 : (k - 128) * 2);
        uint16_t ba = mix565(C565(B_LIGHT[0], B_LIGHT[1], B_LIGHT[2]), C565(B_MID[0], B_MID[1], B_MID[2]), k < 128 ? k * 2 : 255);
        uint16_t bb = mix565(ba, C565(B_DEEP[0], B_DEEP[1], B_DEEP[2]), k < 128 ? 0 : (k - 128) * 2);
        uint16_t base = mix565(fb, bb, t);
        for (int g = 0; g < 8; g++) {
            // g spreads each shade into dark and bright grains
            uint16_t c = g < 4 ? mix565(base, 0, (4 - g) * 16) : mix565(base, C565(255, 235, 200), (g - 3) * 12);
            st.pal[s * 8 + g] = c;
        }
    }
    st.brk = brk;
}

static void init(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    st.vol.build(ART_bronze);
    buildPalette(st, c.breakMix);
}

static inline float pileSurface(const State& st, float x) {
    float d = fabsf(x - ART_bronze.cx) / st.halfBase;
    if (d > 1) d = 1;
    return st.pileY + st.coneH * d;
}

static void computePile(State& st, float p) {
    const SceneArt& a = ART_bronze;
    st.lowLevel = st.vol.lowerLevel(p);
    float base = art_radius(a, (int)st.lowLevel);
    if (base < 10) base = 10;
    float h = base * 0.42f;
    float room = a.bot - st.lowLevel + 2;
    if (h > room * 2.4f) h = room * 2.4f;
    st.coneH = h;
    st.halfBase = base;
    st.pileX = a.cx;
    st.pileY = st.lowLevel + h * 0.34f - h;
}

static void tick(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_bronze;
    if (fabsf(c.breakMix - st.brk) > 0.01f || (c.breakMix != st.brk && (c.breakMix == 0 || c.breakMix == 1))) buildPalette(st, c.breakMix);
    computePile(st, c.progress);
    bool flowing = !c.paused && c.progress < 0.998f;
    if (flowing) {
        for (int n = 0; n < 2; n++) {
            for (int i = 0; i < BRONZE_GRAINS; i++) {
                if (!st.g[i].mode) {
                    st.g[i] = {(float)a.cx + hg_rands() * 0.9f, (float)a.neck + 2, hg_rands() * 0.06f, 0.6f + hg_randf(), 1, 0};
                    break;
                }
            }
        }
    }
    for (int i = 0; i < BRONZE_GRAINS; i++) {
        Grain& g = st.g[i];
        if (g.mode == 1) {
            g.vy += 0.42f;
            g.x += g.vx;
            g.y += g.vy;
            float s = pileSurface(st, g.x);
            if (g.y >= s) {
                g.y = s;
                if (hg_rand() & 1) {
                    g.mode = 2;
                    g.vx = hg_rands() * 1.1f;
                    if (fabsf(g.vx) < 0.3f) g.vx = g.vx < 0 ? -0.3f : 0.3f;
                    g.life = 16 + (hg_rand() & 31);
                } else {
                    g.mode = 0;
                }
            }
        } else if (g.mode == 2) {
            g.x += g.vx;
            g.vx *= 0.975f;
            g.y = pileSurface(st, g.x) - 1;
            if (!g.life-- || fabsf(g.x - a.cx) > st.halfBase - 2) g.mode = 0;
        }
    }
}

static void draw(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_bronze;
    const Surf& s = c.s;
    float p = c.progress;
    int crawl = (int)(c.tick >> 2);

    // ---- upper bulb: bowl with a crater that deepens as it drains
    float level = st.vol.upperLevel(1.0f - p);
    if (p < 0.998f) {
        float rl = art_radius(a, (int)level + 1);
        float crater = rl * 0.34f;
        if (crater > a.neck - level - 2) crater = a.neck - level - 2;
        if (crater < 0) crater = 0;
        int wob = (hg_isin(c.tick * 2) * 3) >> 7;
        for (int y = (int)level; y <= a.neck; y++) {
            int xl, xr;
            if (!art_inner(a, y, xl, xr)) continue;
            int width = xr - xl + 1;
            int stepS = (8 << 12) / (width > 0 ? width : 1);
            int ss = 0;
            uint16_t* o = s.at(xl, y);
            float surfaceBand = level + crater;
            bool checkTop = y < surfaceBand + 1;
            for (int x = xl; x <= xr; x++, o++, ss += stepS) {
                int dx = x - a.cx;
                if (checkTop) {
                    float u = (float)dx / rl;
                    float top = level + crater * (1 - u * u) + ((hg_noise(x, 11) >> 6) & 1) * 0.5f;
                    if (y < top) continue;
                    if (y < top + 1) {
                        *o = st.pal[(ss >> 12) * 8 + 7];
                        continue;
                    }
                }
                int adx = dx < 0 ? -dx : dx;
                // grains near the axis crawl down toward the neck, faster the lower they are
                int flow = 0;
                if (adx < (a.neck - y) / 2 + 6 && !c.paused) flow = crawl * (3 + (y - (int)level) / 16) >> 1;
                int n = hg_noise(x + (dx >> 2) * wob, y - flow);
                *o = st.pal[(ss >> 12) * 8 + (n >> 5)];
            }
        }
    }

    // ---- lower pile
    if (p > 0.0015f) {
        int xlo = a.cx - (int)st.halfBase - 2, xhi = a.cx + (int)st.halfBase + 2;
        int bot = a.bot;
        for (int x = xlo; x <= xhi; x++) {
            int i = x - xlo;
            if (i >= 0 && i < 160) {
                float d = fabsf((float)(x - a.cx)) / st.halfBase;
                if (d > 1) d = 1;
                st.pileTop[i] = (int16_t)(st.pileY + st.coneH * d);
            }
        }
        int ymin = (int)st.pileY;
        if (ymin < a.neck) ymin = a.neck;
        for (int y = ymin; y <= bot; y++) {
            int xl, xr;
            if (!art_inner(a, y, xl, xr)) continue;
            int width = xr - xl + 1;
            int stepS = (8 << 12) / (width > 0 ? width : 1);
            int ss = 0;
            uint16_t* o = s.at(xl, y);
            for (int x = xl; x <= xr; x++, o++, ss += stepS) {
                int i = x - xlo;
                int top = (i >= 0 && i < 160) ? st.pileTop[i] : (int)(st.lowLevel + st.coneH * 0.34f);
                int yy = y - top;
                if (yy < 0) continue;
                int n = hg_noise(x, y) >> 5;
                int shade = ss >> 12;
                if (yy == 0) n = 7;
                *o = st.pal[shade * 8 + n];
            }
        }
    }

    // ---- the falling thread: mottled, never a solid bar
    if (p < 0.998f && !c.paused) {
        int yEnd = (int)st.pileY;
        if (yEnd > a.bot) yEnd = a.bot;
        for (int y = a.neck - 2; y < yEnd; y++) {
            int fall = c.tick * 7;
            int n = hg_noise(y - fall, 5);
            int spread = 1 + ((y - a.neck) >> 5);
            if (n < 200) s.put(a.cx - 1 + (n >> 6) % (spread + 1), y, st.pal[3 * 8 + (n >> 5)]);
            if (n > 60) s.put(a.cx + (n >> 7) - (spread >> 1), y, st.pal[1 * 8 + ((n * 3) & 7)]);
        }
        s.glow(a.cx, yEnd, 6, st.pal[1 * 8 + 6], 42);
    }
    // ---- free grains
    for (int i = 0; i < BRONZE_GRAINS; i++) {
        const Grain& g = st.g[i];
        if (g.mode) s.put((int)g.x, (int)g.y, st.pal[(i & 3) * 8 + 6]);
    }

    // ---- OLED readout on the base
    char buf[20];
    snprintf(buf, sizeof(buf), "%02u:%02u  %s", (unsigned)(c.remainSec / 60), (unsigned)(c.remainSec % 60),
             c.breakMix > 0.5f ? "BREAK" : "FOCUS");
    int w = textWidth(FONT_SMALL, buf);
    drawText(s, a.cx - w / 2, 247, FONT_SMALL, buf, C565(236, 240, 230));
}

}  // namespace bronze

static const Scene SCENE_bronze = {
    "Desk Bronze", "Terracotta sand, one grain at a time.", &ART_bronze,
    {C565(250, 238, 220), C565(205, 180, 150), C565(232, 140, 88), C565(160, 190, 130), C565(90, 68, 50), C565(32, 20, 12), C565(255, 92, 76)},
    {255, 120, 40},
    bronze::init, bronze::tick, bronze::draw,
};
