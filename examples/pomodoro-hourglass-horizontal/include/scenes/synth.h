// ==============================================================================
//  Neon Flow - hot plasma in violet neon glass. The charge is a per-row palette
//  stirred by two slow sine fields, the stream is glowing dashes that accelerate,
//  and every landing makes a small flare. The glass halo is baked; live bloom is
//  a table-driven band next to the walls plus a handful of soft glows.
// ==============================================================================
#pragma once
#include "../hg_core.h"
#include "../art/synth.h"

namespace synth {

#define SYNTH_GRADS 64
#define SYNTH_SHADES 16
#define SYNTH_DASHES 20
#define SYNTH_MOTES 14
#define SYNTH_FLARES 5
#define SYNTH_SPARKS 14
#define SYNTH_COLS 162
#define SYNTH_HALO 9

struct Dash { float x, y, vy; uint8_t on, dot; int8_t dir; };  // dir +1 falling, -1 rushing back up
struct Mote { float x, y, vy; uint8_t age, life, zone; int8_t phase; }; // life 0 = free
struct Flare { int16_t x, y; uint8_t age, on; };
struct Spark { float x, y, vx, vy; uint8_t life; };

struct State {
    GlassVolume vol;
    uint16_t pal[2][SYNTH_GRADS][SYNTH_SHADES];  // 0 = upper charge, 1 = lower charge; shade 0 edge .. 15 core
    uint16_t rim[2][4];                          // hot surface line
    uint16_t stream[32];                         // colour down the stream
    uint16_t hazeU, hazeL;
    uint8_t  bayer[16];
    uint8_t  shade[33];
    uint8_t  profile[66];                        // mound silhouette, 0 = peak, 255 = skirt
    int32_t  topU[SYNTH_COLS], topL[SYNTH_COLS]; // surface per column, y * 256
    int32_t  minU, maxU, minL, maxL;
    Dash dash[SYNTH_DASHES];
    Mote mote[SYNTH_MOTES];
    Flare flare[SYNTH_FLARES];
    Spark spark[SYNTH_SPARKS];
    float brk, level, lowLevel, peak, edge, hb, moundH, lastP, neckGlow, hotGlow, neckBase, hotBase;
    int spawnIn;
    uint8_t pattern;
    bool rewinding;
};
static_assert(sizeof(State) <= SCENE_ARENA_BYTES, "");

struct Stop { uint8_t pos, r, g, b; };

// position 0..63; 8 levels at each end are headroom for the plasma wobble
static const Stop F_UP[] = {{0, 255, 70, 190}, {12, 255, 34, 158}, {30, 255, 22, 128}, {38, 255, 56, 92}, {47, 255, 104, 44}, {55, 255, 146, 24}, {63, 255, 190, 60}};
static const Stop F_LO[] = {{0, 255, 176, 56}, {9, 255, 136, 36}, {19, 255, 72, 66}, {29, 245, 28, 132}, {39, 186, 26, 186}, {50, 116, 30, 186}, {63, 70, 24, 150}};
static const Stop B_UP[] = {{0, 130, 250, 255}, {12, 44, 216, 255}, {30, 20, 180, 245}, {40, 24, 208, 232}, {50, 50, 234, 214}, {58, 110, 250, 214}, {63, 180, 255, 226}};
static const Stop B_LO[] = {{0, 190, 255, 228}, {10, 110, 250, 220}, {22, 34, 214, 232}, {34, 26, 150, 240}, {46, 70, 80, 225}, {56, 84, 40, 190}, {63, 60, 26, 140}};

static uint16_t rampColor(const Stop* st, int n, int pos) {
    int i = 0;
    while (i < n - 2 && pos > st[i + 1].pos) i++;
    const Stop& a = st[i]; const Stop& b = st[i + 1];
    int span = b.pos - a.pos, t = pos - a.pos;
    if (t < 0) t = 0;
    if (t > span) t = span;
    return C565(a.r + (b.r - a.r) * t / span, a.g + (b.g - a.g) * t / span, a.b + (b.b - a.b) * t / span);
}

static void buildPalette(State& st, float brk) {
    int t = (int)(brk * 255);
    for (int z = 0; z < 2; z++) {
        const Stop* f = z ? F_LO : F_UP; const Stop* b = z ? B_LO : B_UP;
        for (int g = 0; g < SYNTH_GRADS; g++) {
            uint16_t base = mix565(rampColor(f, 7, g), rampColor(b, 7, g), t);
            uint16_t deep = scale565(base, 168);
            uint16_t hot = mix565(base, C565(255, 240, 235), 52);
            for (int s = 0; s < SYNTH_SHADES; s++) {
                uint16_t c = s < 8 ? mix565(deep, base, s * 32) : mix565(base, hot, (s - 8) * 36);
                st.pal[z][g][s] = c;
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        st.rim[0][i] = mix565(st.pal[0][8][15], C565(255, 236, 244), 10 + i * 34);
        st.rim[1][i] = mix565(st.pal[1][6][15], C565(255, 240, 215), 10 + i * 40);
    }
    uint16_t head = mix565(C565(255, 170, 50), C565(120, 250, 240), t);
    uint16_t tail = mix565(C565(255, 40, 150), C565(40, 170, 255), t);
    for (int i = 0; i < 32; i++) st.stream[i] = mix565(head, tail, i * 8);
    st.hazeU = scale565(st.pal[0][14][8], 92);
    st.hazeL = scale565(st.pal[1][10][8], 80);
    st.brk = brk;
}

// ---------------------------------------------------------------- additive helpers
// Sub-pixel soft glow; x16,y16 in 1/16 pixel, no division in the loop.
static void glowF(const Surf& s, int x16, int y16, int r, uint16_t c, int amp) {
    int r16 = r << 4, r2 = r16 * r16;
    int inv = (255 << 12) / (r2 >> 4);
    int yA = (y16 - r16) >> 4, yB = (y16 + r16) >> 4, xA = (x16 - r16) >> 4, xB = (x16 + r16) >> 4;
    if (yA < s.y0) yA = s.y0;
    if (yB >= s.y0 + s.h) yB = s.y0 + s.h - 1;
    if (xA < s.x0) xA = s.x0;
    if (xB >= s.x0 + s.w) xB = s.x0 + s.w - 1;
    for (int y = yA; y <= yB; y++) {
        int dy = (y << 4) + 8 - y16, dy2 = dy * dy;
        uint16_t* p = s.at(xA, y);
        for (int x = xA; x <= xB; x++, p++) {
            int dx = (x << 4) + 8 - x16, d2 = dx * dx + dy2;
            if (d2 >= r2) continue;
            int a = (HG_GLOW[((d2 >> 4) * inv) >> 12] * amp) >> 8;
            if (a > 7) *p = add565(*p, scale565(c, a));
        }
    }
}

// ---------------------------------------------------------------- charge geometry
static void computeLevels(State& st, float p, uint32_t tick) {
    const SceneArt& a = ART_synth;
    st.level = st.vol.upperLevel(1.0f - p);
    st.lowLevel = st.vol.lowerLevel(p);

    // the mound: a free heap on the floor first, once it reaches the walls it rides on the level
    float rFloor = art_radius(a, a.bot - 1);
    float v = p * st.vol.v0;
    float hb = cbrtf(v * 6.0f);
    float rAt = art_radius(a, (int)st.lowLevel);
    if (hb > rFloor) hb = rAt > rFloor ? rAt : rFloor;
    if (hb < 2.0f) hb = 2.0f;
    float H = hb * 0.5f;
    float edge = st.lowLevel + 0.4f * H;
    if (edge > a.bot + 1) edge = a.bot + 1;
    st.hb = hb; st.moundH = H; st.edge = edge; st.peak = edge - H;

    // upper surface per column: two slow sines, amplitude follows the width of the bulb
    float rl = art_radius(a, (int)st.level + 1);
    int amp = (int)(rl * 0.10f * 256.0f);
    int lvl8 = (int)(st.level * 256.0f);
    int mn = 32767, mx = -32768;
    int ph1 = (int)(tick / 2), ph2 = (int)(tick * 3 / 4);
    int stepK = rl > 2 ? (int)(150.0f / rl) : 40;
    if (stepK < 1) stepK = 1;
    for (int i = 0; i < SYNTH_COLS; i++) {
        int dx = i + a.dx - a.cx;
        int w = hg_isin(dx * stepK / 2 + ph1) * 5 + hg_isin(dx * stepK + 90 - ph2) * 3 + hg_isin(dx * stepK * 2 + ph1 * 3) ;
        int t = lvl8 + (amp * w >> 10);
        st.topU[i] = t;
        if (t < mn) mn = t;
        if (t > mx) mx = t;
    }
    st.minU = mn; st.maxU = mx;

    // lower surface per column
    mn = 32767; mx = -32768;
    int peak8 = (int)(st.peak * 256.0f), h8 = (int)(H * 256.0f);
    int kd = (int)(65536.0f / hb * 64.0f);
    int wobAmp = (int)(H * 0.04f * 16.0f);
    for (int i = 0; i < SYNTH_COLS; i++) {
        int dx = i + a.dx - a.cx, ad = dx < 0 ? -dx : dx;
        int q = (ad * kd) >> 16;
        if (q > 65) q = 65;
        int t = peak8 + ((h8 * st.profile[q]) >> 8);
        int fade = q < 8 ? q : 8;
        t += (hg_isin(ad * 5 - (int)tick * 2) * wobAmp * fade) >> 7;
        st.topL[i] = t;
        if (t < mn) mn = t;
        if (q < 64 && t > mx) mx = t;
    }
    st.minL = mn; st.maxL = mx;
}

static inline float lowerSurface(const State& st, float x) {
    int i = (int)x - ART_synth.dx;
    if (i < 0) i = 0;
    if (i >= SYNTH_COLS) i = SYNTH_COLS - 1;
    float t = st.topL[i] / 256.0f;
    return t > ART_synth.bot ? ART_synth.bot : t;
}

// ---------------------------------------------------------------- particles
static void spawnDash(State& st, int dir) {
    const SceneArt& a = ART_synth;
    for (int i = 0; i < SYNTH_DASHES; i++) {
        Dash& d = st.dash[i];
        if (d.on) continue;
        d.on = 1;
        d.dir = (int8_t)dir;
        d.dot = dir > 0 && (st.pattern = (uint8_t)(st.pattern * 5 + 3), (st.pattern >> 5) < 3);
        d.x = (float)a.cx + hg_rands() * 0.7f;
        if (dir > 0) { d.y = (float)a.neck + 1; d.vy = 0.5f + hg_randf() * 0.4f; }
        else         { d.y = st.peak; d.vy = -1.5f; d.dot = 0; }
        return;
    }
}

static void landing(State& st, float x, float y) {
    for (int i = 0; i < SYNTH_FLARES; i++) {
        Flare& f = st.flare[i];
        if (f.on) continue;
        f.on = 1; f.age = 0; f.x = (int16_t)(x * 16); f.y = (int16_t)(y * 16);
        break;
    }
    int n = 1 + (hg_rand() & 1);
    for (int i = 0; i < SYNTH_SPARKS && n > 0; i++) {
        Spark& k = st.spark[i];
        if (k.life) continue;
        k.x = x; k.y = y - 1;
        k.vx = hg_rands() * 1.1f;
        k.vy = -0.5f - hg_randf() * 1.0f;
        k.life = (uint8_t)(10 + (hg_rand() & 7));
        n--;
    }
    st.hotGlow += 26.0f;
}

static void respawnMote(State& st, Mote& m, bool anyAge) {
    const SceneArt& a = ART_synth;
    bool wantUpper = (hg_rand() & 7) < 2;
    m.life = 0;
    if (wantUpper) {
        if (st.level > a.neck - 22) return;
        float y = st.level + 6 + hg_randf() * (a.neck - 16 - st.level);
        float r = art_radius(a, (int)y) - 7;
        if (r < 3) return;
        m.x = a.cx + hg_rands() * r; m.y = y; m.zone = 0;
        m.vy = -0.12f - hg_randf() * 0.16f;
        m.life = (uint8_t)(70 + (hg_rand() & 63));
    } else {
        if (st.hb < 8) return;
        float x = a.cx + hg_rands() * (st.hb - 4);
        m.x = x; m.y = lowerSurface(st, x) + 3 + hg_randf() * 8; m.zone = 1;
        m.vy = -0.16f - hg_randf() * 0.26f;
        m.life = (uint8_t)(110 + (hg_rand() & 127));
    }
    m.phase = (int8_t)(hg_rand() & 127);
    m.age = anyAge ? (uint8_t)(hg_rand() % m.life) : 0;
}

static void stepParticles(State& st, const SceneCtx& c, bool flowing) {
    const SceneArt& a = ART_synth;
    // ---- dashes
    if (flowing && --st.spawnIn <= 0) {
        spawnDash(st, 1);
        st.spawnIn = 3 + (int)(hg_rand() % 8);
        st.neckGlow += 16.0f;
    }
    if (st.rewinding && (c.tick & 1)) spawnDash(st, -1);
    for (int i = 0; i < SYNTH_DASHES; i++) {
        Dash& d = st.dash[i];
        if (!d.on) continue;
        if (d.dir > 0) {
            d.vy += 0.17f;
            d.y += d.vy;
            float top = lowerSurface(st, d.x);
            if (d.y >= top) {
                d.on = 0;
                if (!st.rewinding) landing(st, d.x, top);
            }
        } else {
            d.vy -= 0.5f;
            d.y += d.vy;
            if (d.y < a.neck) d.on = 0;
        }
    }
    // ---- flares and sparks
    for (int i = 0; i < SYNTH_FLARES; i++) {
        Flare& f = st.flare[i];
        if (f.on && ++f.age > 13) f.on = 0;
    }
    for (int i = 0; i < SYNTH_SPARKS; i++) {
        Spark& k = st.spark[i];
        if (!k.life) continue;
        k.life--;
        k.vy += 0.05f;
        k.x += k.vx; k.y += k.vy;
        k.vx *= 0.96f;
        if (k.y > lowerSurface(st, k.x)) k.life = 0;
    }
    // ---- motes
    for (int i = 0; i < SYNTH_MOTES; i++) {
        Mote& m = st.mote[i];
        if (!m.life) {
            if ((hg_rand() & 7) == 0) respawnMote(st, m, false);
            continue;
        }
        m.y += m.vy;
        if (++m.age >= m.life) { m.life = 0; continue; }
        if (m.zone == 0) {
            if (m.y < st.minU / 256.0f + 3) m.life = 0;  // reaches the surface and pops
        } else if (m.y < a.neck + 12 || m.y > a.bot) {
            m.life = 0;
        }
    }
    st.neckGlow *= 0.86f;
    st.hotGlow *= 0.88f;
    // hot spots ease in and out, so pausing or finishing never pops
    bool live = c.progress < 0.998f;
    st.neckBase += ((live ? (c.paused ? 60.0f : 110.0f) : 0.0f) - st.neckBase) * 0.08f;
    st.hotBase += ((c.progress > 0.002f && !c.paused && live ? 40.0f : 14.0f) - st.hotBase) * 0.08f;
}

// ---------------------------------------------------------------- scene hooks
static void init(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    st.vol.build(ART_synth, 0.60f);
    buildPalette(st, c.breakMix);
    static const uint8_t B4[16] = {0, 4, 1, 5, 6, 2, 7, 3, 1, 5, 0, 4, 7, 3, 6, 2};
    memcpy(st.bayer, B4, 16);
    for (int i = 0; i < 33; i++) {
        // bright core, deeper at the very edge of the bulb
        int d = i < 16 ? i : 32 - i;
        float u = d / 16.0f;                         // 0 at the wall, 1 on the axis
        st.shade[i] = (uint8_t)((1.0f - (1.0f - u) * (1.0f - u)) * 15.0f * 8.0f);
    }
    for (int i = 0; i < 66; i++) {
        // rounded tip, flaring skirt
        float d = i / 64.0f;
        if (d > 1) d = 1;
        float f = 1 - (1 - d) * (1 - d);
        float tip = sqrtf(d * d + 0.0036f) - 0.06f;
        float v = f * 0.72f + tip * 0.28f;
        st.profile[i] = (uint8_t)(v * 255.0f);
    }
    st.lastP = c.progress;
    computeLevels(st, c.progress, c.tick);
    st.neckBase = c.progress < 0.998f ? 100.0f : 0.0f;
    st.hotBase = 30.0f;
    st.spawnIn = 1;
    st.pattern = 17;
    for (int i = 0; i < SYNTH_MOTES; i++) respawnMote(st, st.mote[i], true);
    // settle the stream so it is already running when the scene appears
    bool flowing = !c.paused && c.progress < 0.998f;
    if (flowing) for (int i = 0; i < 44; i++) stepParticles(st, c, true);
}

static void tick(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    if (fabsf(c.breakMix - st.brk) > 0.01f || (c.breakMix != st.brk && (c.breakMix == 0 || c.breakMix == 1))) buildPalette(st, c.breakMix);
    computeLevels(st, c.progress, c.tick);
    st.rewinding = c.progress < st.lastP - 0.0015f;
    st.lastP = c.progress;
    stepParticles(st, c, !c.paused && !st.rewinding && c.progress < 0.998f);
}

// ---- upper plasma
static void drawUpper(SceneCtx& c, State& st) {
    const SceneArt& a = ART_synth;
    const Surf& s = c.s;
    int yA = st.minU >> 8;
    if (yA < a.top) yA = a.top;
    if (yA < s.y0) yA = s.y0;
    int yEnd = a.neck + 3;
    float span = a.neck - st.level;
    if (span < 8) span = 8;
    int k = (int)(47.0f * 8.0f * 256.0f / span);  // row -> gradient position, 8 steps per colour
    int lvl = st.minU >> 8;
    int bandEnd = (st.maxU >> 8) + 3;
    int t = (int)(c.tick & ~1u);
    int ph1T = t, ph2T = t * 3 / 2;
    const uint16_t(*pal)[SYNTH_SHADES] = st.pal[0];
    for (int y = yA; y <= yEnd; y++) {
        int xl, xr;
        if (!art_inner(a, y, xl, xr)) continue;
        int width = xr - xl + 1;
        int stepS = (32 << 10) / width, ss = 0;
        int g0 = ((y - lvl) * k >> 8) + 64 + (hg_isin(y * 4 - t * 2) >> 3);   // slow bands sliding to the neck
        if (g0 < 64) g0 = 64;   // leaves 8 colours headroom each side
        if (g0 > 440) g0 = 440;
        int warp = hg_isin(y * 3 - ph1T) >> 3;
        int ph1 = xl * 3 + y * 2 + warp - ph2T, ph2 = xl * 5 - y * 4 - ph1T - warp;
        int yb = (y & 3) << 2;
        uint16_t* o = s.at(xl, y);
        if (y > a.neck) {   // the column of plasma tapers out just past the neck
            int fade = 180 - (y - a.neck) * 50;
            for (int x = xl; x <= xr; x++, o++) *o = mix565(*o, pal[g0 >> 3][12], fade);
        } else if (y > bandEnd) {
            // the sine field is smooth: sampled every 4th pixel, the shade every 2nd
            const uint8_t* bay = st.bayer + yb;
            const uint8_t* shT = st.shade;
            int stepS2 = stepS * 2, g = g0;
            for (int x = xl, n = 0; x <= xr; x += 2, n++, ss += stepS2) {
                if (!(n & 1)) { g = g0 + (HG_SIN[ph1 & 255] >> 2) + (HG_SIN[ph2 & 255] >> 2); ph1 += 12; ph2 += 20; }
                int sv = shT[ss >> 10];
                int b = bay[x & 3];
                *o++ = pal[(g + b) >> 3][(sv + b) >> 3];
                if (x < xr) { b = bay[(x + 1) & 3]; *o++ = pal[(g + b) >> 3][(sv + b) >> 3]; }
            }
        } else {
            int yy = y << 8;
            const int32_t* top = st.topU + (xl - a.dx);
            for (int x = xl; x <= xr; x++, o++, ss += stepS, ph1 += 3, ph2 += 5, top++) {
                int d = yy - *top;
                if (d < -256) continue;
                int b = st.bayer[(x & 3) | yb];
                int sh = (st.shade[ss >> 10] + b) >> 3;
                if (d < 0) { *o = mix565(*o, st.rim[0][sh >> 2], d + 256); continue; }
                if (d < 256) { *o = st.rim[0][sh >> 2]; continue; }
                if (d < 768) { *o = mix565(st.rim[0][sh >> 2], pal[g0 >> 3][sh], (d - 256) >> 1); continue; }
                int g = g0 + (HG_SIN[ph1 & 255] >> 2) + (HG_SIN[ph2 & 255] >> 2) + b;
                *o = pal[g >> 3][sh];
            }
        }
    }
}

// ---- lower plasma: colour follows the depth under the mound's surface and the row
static void drawLower(SceneCtx& c, State& st) {
    const SceneArt& a = ART_synth;
    const Surf& s = c.s;
    int yA = st.minL >> 8;
    if (yA < a.neck + 2) yA = a.neck + 2;
    float span = a.bot - st.peak;
    if (span < 4) span = 4;
    int k = (int)(47.0f * 8.0f / span * 256.0f);
    int t = (int)(c.tick & ~1u);
    const uint16_t(*pal)[SYNTH_SHADES] = st.pal[1];
    int peakI = (int)st.peak;
    int xMin = a.cx - (int)st.hb - 1, xMax = a.cx + (int)st.hb + 1;
    int bandEnd = (st.maxL >> 8) + 4;
    for (int y = yA; y <= a.bot; y++) {
        int xl, xr;
        if (!art_inner(a, y, xl, xr)) continue;
        int stepS = (32 << 10) / (xr - xl + 1), ss = 0;
        int warp = hg_isin(y * 4 + t) >> 3;
        int ph1 = xl * 4 + y * 3 + warp + t, ph2 = xl * 2 - y * 5 + t - warp;
        const uint8_t* bay = st.bayer + ((y & 3) << 2);
        int yy = y << 8;
        int yP = y - peakI;
        if (yP < 0) yP = 0;
        int rowG = yP << 3;
        uint16_t* o = s.at(xl, y);
        const int32_t* top = st.topL + (xl - a.dx);
        if (xl < xMin) {
            int n = xMin - xl;
            o += n; top += n; ss += stepS * n; ph1 += 3 * n; ph2 += 5 * n; xl = xMin;
        }
        if (xr > xMax) xr = xMax;
        int g = 0, sv = 0;
        if (y > bandEnd) {
            // fully below the surface: sample the slow fields every 4th pixel, no edge tests
            int stepS4 = stepS * 4;
            for (int x = xl; x <= xr; x += 4, top += 4, ss += stepS4, ph1 += 12, ph2 += 20) {
                int g0 = ((((yy - *top) >> 5) + rowG) * k >> 12) + 64;
                if (g0 > 440) g0 = 440;
                g = g0 + (HG_SIN[ph1 & 255] >> 2) + (HG_SIN[ph2 & 255] >> 2);
                sv = st.shade[ss >> 10];
                int sv2 = st.shade[(ss + stepS * 2) >> 10];
                int end = x + 3 > xr ? xr : x + 3;
                for (int xx = x; xx <= end; xx++) {
                    int b = bay[xx & 3];
                    *o++ = pal[(g + b) >> 3][((xx - x < 2 ? sv : sv2) + b) >> 3];
                }
            }
            continue;
        }
        for (int x = xl, n = 0; x <= xr; x++, o++, n++, ss += stepS, top++) {
            int d = yy - *top;
            if (d < -256) continue;
            if (!(n & 3)) {
                int g0 = (((d >> 5) + rowG) * k >> 12) + 64;
                if (g0 > 440) g0 = 440;
                int pn = n * 3;
                g = g0;
                sv = (HG_SIN[(ph1 + pn) & 255] >> 2) + (HG_SIN[(ph2 + pn * 5 / 3) & 255] >> 2);
            }
            int b = bay[x & 3];
            int sh = (st.shade[ss >> 10] + b) >> 3;
            if (d < 0) { *o = mix565(*o, st.rim[1][sh >> 2], d + 256); continue; }
            if (d < 256) { *o = st.rim[1][sh >> 2]; continue; }
            if (d < 768) { *o = mix565(st.rim[1][sh >> 2], pal[g >> 3][sh], (d - 256) >> 1); continue; }
            *o = pal[(g + sv + b) >> 3][sh];
        }
    }
}

// ---- bloom outside the wall, row by row, plus haze in the air above each surface
static const uint8_t HALO_FALL[SYNTH_HALO] = {176, 146, 118, 94, 72, 52, 34, 19, 8};

static void haloRow(const Surf& s, int y, uint16_t c, int amp) {
    const SceneArt& a = ART_synth;
    int gl = (a.gl[y] >> 4) - 3, gr = (a.gr[y] >> 4) + 3;
    uint16_t lv[SYNTH_HALO];
    uint32_t sc = spread565(c);
    for (int k = 0; k < SYNTH_HALO; k++) lv[k] = pack565((sc * ((((HALO_FALL[k] * amp) >> 8) + 4) >> 3)) >> 5);
    int x0 = s.x0, x1 = s.x0 + s.w - 1;
    uint16_t* row = s.at(x0, y);
    for (int k = 0; k < SYNTH_HALO; k++) {
        int xa = gl - k, xb = gr + 1 + k;
        if (xa >= x0) { uint16_t* p = row + (xa - x0); *p = add565(*p, lv[k]); }
        if (xb <= x1) { uint16_t* p = row + (xb - x0); *p = add565(*p, lv[k]); }
    }
}

static void drawBloom(SceneCtx& c, State& st) {
    const SceneArt& a = ART_synth;
    const Surf& s = c.s;
    int pulse = 208 + (hg_isin((int)c.tick) >> 3) + (hg_isin((int)(c.tick * 5 / 2)) >> 4);
    // upper bulb
    if (c.progress < 0.997f) {
        int lvl = (int)st.level;
        float span = a.neck - st.level;
        if (span < 8) span = 8;
        int k = (int)(47.0f * 256.0f / span);
        for (int y = lvl - 16; y <= a.neck + 6; y++) {
            if (y < a.top || y < s.y0) continue;
            int d = y - lvl;
            int e = d + 16;                                   // soft in over 40 rows around the surface
            e = e >= 40 ? 256 : (e * e * 256) / 1600;
            int nk = a.neck + 7 - y;                          // and out at the neck
            if (nk < 12) e = (e * nk) / 12;
            int amp = (pulse * e) >> 8;
            int g = d <= 0 ? 8 : 8 + ((d * k) >> 8);
            if (g > 55) g = 55;
            haloRow(s, y, st.pal[0][g][8], amp);
        }
        // air above the surface glows
        int yH0 = (st.minU >> 8) - 14;
        if (yH0 < s.y0) yH0 = s.y0;
        int yH1 = (st.maxU >> 8);
        for (int y = yH0; y <= yH1; y++) {
            int xl, xr;
            if (!art_inner(a, y, xl, xr)) continue;
            int d = y - yH0;
            uint16_t col = scale565(st.hazeU, ((d * d * 255) >> 8) * pulse >> 8 > 255 ? 255 : (((d * d * 255) >> 8) * pulse >> 8));
            if (!col) continue;
            uint16_t col2 = scale565(col, 228);
            uint16_t* o = s.at(xl, y);
            for (int x = xl; x <= xr; x++, o++) *o = add565(*o, ((x ^ y) & 1) ? col : col2);
        }
    }
    // lower bulb
    if (c.progress > 0.002f) {
        int yTop = (int)st.peak;
        int k = (int)(47.0f * 256.0f / (a.bot + 1 - st.peak));
        for (int y = yTop; y <= a.bot; y++) {
            if (y < a.neck + 1) continue;
            int d = y - yTop;
            // bloom fades in along the wall as the mound's skirt comes near it
            int e = y - (int)st.edge + 26;
            if (e <= 0) continue;
            e = e >= 40 ? 256 : (e * e * 256) / 1600;
            int amp = (pulse * e) >> 8;
            int g = 8 + ((d * k) >> 8);
            if (g > 55) g = 55;
            haloRow(s, y, st.pal[1][g][8], amp);
        }
        // air over the mound, only where the mound will not paint over it
        int yH0 = (int)st.peak - 13;
        if (yH0 < a.neck + 8) yH0 = a.neck + 8;
        int yH1 = (int)st.edge;
        const int cxI = a.cx - s.x0;
        int hw = 0;
        for (int y = yH0; y <= yH1; y++) {
            int xl, xr;
            if (!art_inner(a, y, xl, xr)) continue;
            int d = y - yH0, n = yH1 - yH0 + 1;
            int amp = (((d * 255) / n) * pulse) >> 8;
            uint16_t col = scale565(st.hazeL, amp);
            if (!col) continue;
            uint16_t col2 = scale565(col, 228);
            int yy = y << 8;
            while (cxI + hw + 1 < SYNTH_COLS && st.topL[cxI + hw + 1] < yy) hw++;
            bool covered = st.topL[cxI] < yy;
            int e1 = covered ? a.cx - hw - 1 : xr;
            uint16_t* o = s.at(xl, y);
            for (int x = xl; x <= e1; x++, o++) *o = add565(*o, ((x ^ y) & 1) ? col : col2);
            if (covered) {
                int x2 = a.cx + hw + 1;
                if (x2 < xl) x2 = xl;
                o = s.at(x2, y);
                for (int x = x2; x <= xr; x++, o++) *o = add565(*o, ((x ^ y) & 1) ? col : col2);
            }
        }
    }
}

// ---- one dash: hot head, long fading trail, soft shoulders
static void drawDash(const Surf& s, const State& st, const Dash& d) {
    const SceneArt& a = ART_synth;
    float v = d.vy < 0 ? -d.vy : d.vy;
    int len = d.dot ? 3 : (int)(6.0f + v * 1.7f);
    if (len > 22) len = 22;
    int dir = d.dir;
    int yHead = (int)d.y;
    int fracY = (int)((d.y - yHead) * 255.0f);
    int x0 = (int)d.x;
    int fx = (int)((d.x - x0) * 255.0f);
    for (int n = 0; n <= len; n++) {
        int y = yHead - n * dir;
        if (y <= a.neck) break;
        if ((unsigned)(y - s.y0) >= (unsigned)s.h) continue;
        int si = (y - a.neck) >> 2;
        if (si > 31) si = 31;
        uint16_t base = st.stream[si];
        int q = (len - n) * 255 / len;   // 255 at the head, 0 at the tail
        int amp = 255 - (((255 - q) * (255 - q)) >> 8);
        if (d.dot) amp = q;
        if (n == 0) amp = (amp * (dir > 0 ? fracY : 255 - fracY)) >> 8;
        uint16_t core = mix565(base, C565(255, 244, 232), n < len / 4 ? 90 : 0);
        uint16_t* p = s.at(x0 - 2, y);
        uint16_t a1 = scale565(core, amp);
        uint16_t side = scale565(base, (amp * 96) >> 8), far = scale565(base, (amp * 30) >> 8);
        p[0] = add565(p[0], far);
        p[1] = add565(p[1], side);
        p[2] = add565(p[2], fx < 128 ? a1 : scale565(a1, 511 - fx * 2));
        p[3] = add565(p[3], a1);
        p[4] = add565(p[4], fx < 128 ? scale565(a1, fx * 2) : a1);
        p[5] = add565(p[5], side);
        p[6] = add565(p[6], far);
    }
}

static void draw(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_synth;
    const Surf& s = c.s;
    float p = c.progress;

    drawBloom(c, st);
    if (p < 0.998f) drawUpper(c, st);
    if (p > 0.0015f) drawLower(c, st);

    // ---- motes: small soft lights, gently swaying
    for (int i = 0; i < SYNTH_MOTES; i++) {
        const Mote& m = st.mote[i];
        if (!m.life) continue;
        int env = m.age < 24 ? m.age * 10 : (m.life - m.age < 40 ? (m.life - m.age) * 6 : 240);
        int x16 = (int)(m.x * 16) + hg_isin(m.age * 2 + m.phase);
        int y16 = (int)(m.y * 16);
        if (m.zone == 0) {
            if (m.y > a.neck - 6) continue;
            glowF(s, x16, y16, 4, st.pal[0][4][15], (env * 110) >> 8);
        } else {
            float r = art_radius(a, (int)m.y) - 2.0f;
            float ax = fabsf(x16 / 16.0f - a.cx);
            if (ax > r) continue;
            int q = ((int)m.y - a.neck) >> 2;
            glowF(s, x16, y16, 4, st.stream[q > 31 ? 31 : q], (env * 180) >> 8);
        }
    }

    // ---- stream
    for (int i = 0; i < SYNTH_DASHES; i++)
        if (st.dash[i].on) drawDash(s, st, st.dash[i]);

    // ---- neck: a steady hot spot that swells when a dash leaves
    if (st.neckBase > 3.0f) {
        int neckAmp = (int)(st.neckBase + st.neckGlow) + (hg_isin((int)c.tick * 2) >> 4);
        if (neckAmp > 255) neckAmp = 255;
        if (neckAmp > 0) glowF(s, a.cx * 16 + 8, a.neck * 16, 11, st.pal[0][52][12], neckAmp);
    }

    // ---- flares at the landing point
    for (int i = 0; i < SYNTH_FLARES; i++) {
        const Flare& f = st.flare[i];
        if (!f.on) continue;
        int amp = 170 - f.age * 12;
        int r = 6 + f.age / 2;
        glowF(s, f.x, f.y, r, st.pal[1][10][7], amp);
        int y = f.y >> 4;
        if ((unsigned)(y - s.y0) < (unsigned)s.h) {
            int reach = 5 + f.age * 2;
            for (int dx = -reach; dx <= reach; dx++) {
                int ax = dx < 0 ? -dx : dx;
                int q = ((reach - ax) * amp) / reach;
                s.add((f.x >> 4) + dx, y, scale565(st.stream[10], q));
            }
        }
    }
    // ---- steady glow on the mound's tip, kicked by landings
    if (p > 0.002f) {
        int amp = (int)(st.hotBase + st.hotGlow);
        if (amp > 180) amp = 180;
        glowF(s, a.cx * 16 + 8, (int)(st.peak * 16), 10, st.pal[1][4][12], amp);
    }
    // ---- sparks
    for (int i = 0; i < SYNTH_SPARKS; i++) {
        const Spark& k = st.spark[i];
        if (k.life) s.add((int)k.x, (int)k.y, scale565(C565(255, 225, 180), k.life * 16));
    }
}

}  // namespace synth

static const Scene SCENE_synth = {
    "Neon Flow", "Hot plasma, falling light.", &ART_synth,
    {C565(246, 242, 255), C565(170, 160, 222), C565(255, 40, 150), C565(50, 220, 240), C565(46, 38, 92), C565(14, 6, 26), C565(255, 150, 40)},
    {255, 30, 140},
    synth::init, synth::tick, synth::draw,
};
