// ==============================================================================
//  Event Horizon - an hourglass holding the universe.
//
//  One catalogue of stars, sorted by radius, is shared by two discs: the top one
//  shows the innermost N*(1-p) stars, the bottom one (mirrored, spinning the other
//  way) shows the innermost N*p. Matter is conserved: every star lost above is born
//  below. Star positions are stateless functions of tick, so nothing ever drifts:
//   - free stars keep true Keplerian orbits (uniform in angle, cannot wind up).
//   - arm stars live in a density wave: born on the arm, sheared by their own orbit
//     speed, fading before the shear could wind the spiral away.
//  The neck is a black hole: baked ring map + scrolling hot spots. Stream particles
//  are the only simulated things: they fall in above and spiral out below.
// ==============================================================================
#pragma once
#include "../hg_core.h"
#include "../art/cosmos.h"

namespace cosmos {

enum { UP_N = 56, DN_N = 40, SIN_N = 2048, LVLS = 32 };
enum { COL_BLUE, COL_GOLD, COL_VIOLET, COL_ROSE, NCOL };

struct Flow { float t, v; uint16_t ph; uint8_t lvl, on; };

struct State {
    GlassVolume vol;
    int16_t sn[SIN_N];         // sin * 32767, angle in 1/2048 turn
    uint16_t omega[256];       // orbit speed per radius, 1/65536 turn per tick
    int16_t dOmega[256];       // the same relative to the spiral pattern
    uint8_t env[256];          // life of an arm star
    uint8_t lump[256];         // hot spots going round the ring
    uint8_t lut[256];          // nebula reveal, d -> 0..31
    uint16_t pal[NCOL][LVLS];  // additive star colours
    uint16_t gas[8 * LVLS];    // additive nebula colours by hue
    uint16_t palRing[LVLS];    // ring ramp, ember to white
    uint16_t palCore[LVLS], palCoreHot[LVLS], palHalo[LVLS];
    Flow up[UP_N], dn[DN_N];
    float brk, lastP, act;
    float gy, gScale;          // top galaxy centre row and scale
    int nTop, nBot;
    int lutR;
    int rewind;                // >0 while the charge rushes back up
    uint8_t spawn;
};
static_assert(sizeof(State) <= SCENE_ARENA_BYTES, "cosmos state too large");

static uint16_t rampAt(const uint8_t (*r)[3], int n, int i, int m) {
    int f = i * (n - 1) * 255 / (m - 1);
    int k = f >> 8 > n - 2 ? n - 2 : f >> 8;
    int t = f - (k * 255);
    if (t > 255) t = 255;
    return mix565(C565(r[k][0], r[k][1], r[k][2]), C565(r[k + 1][0], r[k + 1][1], r[k + 1][2]), t);
}

// Each palette is {focus, break}, level 0 = black.
static const uint8_t R_BLUE[2][3] = {{196, 218, 255}, {176, 255, 232}};
static const uint8_t R_GOLD[2][3] = {{255, 214, 150}, {214, 255, 190}};
static const uint8_t R_VIOLET[2][3] = {{136, 78, 255}, {30, 170, 150}};
static const uint8_t R_ROSE[2][3] = {{255, 74, 150}, {50, 230, 130}};

static const uint8_t R_RING_F[5][3] = {{0, 0, 0}, {140, 34, 66}, {255, 128, 56}, {255, 216, 140}, {255, 255, 248}};
static const uint8_t R_RING_B[5][3] = {{0, 0, 0}, {8, 90, 96}, {30, 200, 150}, {150, 255, 210}, {248, 255, 255}};
static const uint8_t R_CORE_F[4][3] = {{0, 0, 0}, {120, 62, 20}, {255, 170, 70}, {255, 226, 170}};
static const uint8_t R_CORE_B[4][3] = {{0, 0, 0}, {10, 92, 82}, {40, 214, 160}, {190, 255, 226}};
static const uint8_t R_HALO_F[3][3] = {{0, 0, 0}, {70, 30, 90}, {190, 90, 140}};
static const uint8_t R_HALO_B[3][3] = {{0, 0, 0}, {6, 62, 70}, {30, 170, 140}};
// nebula hue 0 (outer, cold) .. 7 (inner, hot)
static const uint8_t R_GAS_F[8][3] = {{50, 70, 210}, {84, 66, 230}, {124, 60, 230}, {170, 56, 210}, {214, 56, 170}, {246, 70, 140}, {255, 110, 120}, {255, 170, 120}};
static const uint8_t R_GAS_B[8][3] = {{10, 90, 130}, {10, 120, 130}, {14, 150, 130}, {20, 176, 124}, {28, 200, 120}, {50, 220, 120}, {90, 236, 130}, {150, 250, 160}};

static void buildPalettes(State& st, float brk) {
    int t = (int)(brk * 255);
    const uint8_t* cols[NCOL][2] = {{R_BLUE[0], R_BLUE[1]}, {R_GOLD[0], R_GOLD[1]}, {R_VIOLET[0], R_VIOLET[1]}, {R_ROSE[0], R_ROSE[1]}};
    for (int c = 0; c < NCOL; c++) {
        uint16_t tint = mix565(C565(cols[c][0][0], cols[c][0][1], cols[c][0][2]), C565(cols[c][1][0], cols[c][1][1], cols[c][1][2]), t);
        for (int l = 0; l < LVLS; l++) {
            // gentle curve; the brightest stars burn out to white
            int k = (int)(255.0f * powf((float)l / (LVLS - 1), 0.72f));
            uint16_t v = scale565(tint, k);
            if (c < COL_VIOLET && l > 21) v = mix565(v, 0xFFFF, (l - 21) * 24);
            st.pal[c][l] = v;
        }
    }
    for (int h = 0; h < 8; h++) {
        uint16_t tint = mix565(C565(R_GAS_F[h][0], R_GAS_F[h][1], R_GAS_F[h][2]), C565(R_GAS_B[h][0], R_GAS_B[h][1], R_GAS_B[h][2]), t);
        for (int l = 0; l < LVLS; l++) st.gas[h * LVLS + l] = scale565(tint, l * 132 / (LVLS - 1));
    }
    for (int l = 0; l < LVLS; l++) {
        st.palRing[l] = mix565(rampAt(R_RING_F, 5, l, LVLS), rampAt(R_RING_B, 5, l, LVLS), t);
        uint16_t core = mix565(rampAt(R_CORE_F, 4, l, LVLS), rampAt(R_CORE_B, 4, l, LVLS), t);
        st.palCore[l] = core;
        st.palCoreHot[l] = scale565(0xFFFF, l * 255 / (LVLS - 1));
        st.palHalo[l] = mix565(rampAt(R_HALO_F, 3, l, LVLS), rampAt(R_HALO_B, 3, l, LVLS), t);
    }
    st.brk = brk;
}

// How the charge sits in the top bulb: the galaxy slides down the funnel as it drains.
static void computeLayout(State& st, float p) {
    const SceneArt& a = ART_cosmos;
    float remain = 1.0f - p;
    float level = st.vol.upperLevel(remain);
    st.gy = level + (a.neck - level) * 0.22f;
    float fit = (art_radius(a, (int)st.gy + 6) + 3.0f) / 80.0f;
    if (fit > 1) fit = 1;
    if (fit < 0.02f) fit = 0.02f;
    st.gScale = fit;
    st.nTop = (int)(COSMOS_NSTARS * remain + 0.5f);
    st.nBot = (int)(COSMOS_NSTARS * p + 0.5f);
    if (st.nTop > COSMOS_NSTARS) st.nTop = COSMOS_NSTARS;
    if (st.nBot > COSMOS_NSTARS) st.nBot = COSMOS_NSTARS;
}

static void init(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    st.vol.build(ART_cosmos);
    for (int i = 0; i < SIN_N; i++) st.sn[i] = (int16_t)(sinf(i * 6.2831853f / SIN_N) * 32767.0f);
    const int patternSpeed = 42;
    for (int r = 0; r < 256; r++) {
        float px = r / 3.0f + 10.0f;
        float w = 20750.0f / (px * sqrtf(px));
        st.omega[r] = (uint16_t)w;
        st.dOmega[r] = (int16_t)(w - patternSpeed);
    }
    for (int i = 0; i < 256; i++) {
        float e = sinf(i * 3.14159265f / 256.0f);
        st.env[i] = (uint8_t)(e * sqrtf(e) * 255.0f);
        float l = 0.84f + 0.10f * sinf(i * 6.2831853f / 256.0f * 2) + 0.06f * sinf(i * 6.2831853f / 256.0f * 5 + 1.3f)
                  + 0.04f * sinf(i * 6.2831853f / 256.0f * 9 + 4.0f);
        st.lump[i] = (uint8_t)(l > 1 ? 255 : l < 0.2f ? 51 : l * 255);
    }
    buildPalettes(st, c.breakMix);
    computeLayout(st, c.progress);
    st.lastP = c.progress;
    st.act = (c.progress < 0.998f && !c.paused) ? 1.0f : 0.4f;
    st.lutR = -1;
    // start with the streams already running, so the scene never opens cold
    if (c.progress < 0.998f && !c.paused) {
        for (int i = 0; i < UP_N; i++) {
            float t = (float)i / UP_N;
            st.up[i] = {t, 1.0f, (uint16_t)hg_rand(), (uint8_t)(18 + (hg_rand() & 13)), 1};
        }
        for (int i = 0; i < DN_N; i++) {
            float t = (float)i / DN_N;
            st.dn[i] = {t, 1.0f, (uint16_t)hg_rand(), (uint8_t)(16 + (hg_rand() & 15)), 1};
        }
    }
}

static inline float upSpeed(float t) { return 0.0052f + t * t * 0.026f; }
static inline float dnSpeed(float t) { return 0.0028f + (1.0f - t) * (1.0f - t) * 0.024f; }

static void tick(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    if (fabsf(c.breakMix - st.brk) > 0.01f || (c.breakMix != st.brk && (c.breakMix == 0 || c.breakMix == 1))) buildPalettes(st, c.breakMix);
    float p = c.progress;
    computeLayout(st, p);

    float dp = p - st.lastP;
    st.lastP = p;
    if (dp < -0.0006f) st.rewind = 8;
    else if (st.rewind) st.rewind--;
    bool back = st.rewind > 0;
    bool flowing = !c.paused && (p < 0.998f || back);

    float target = flowing ? 1.0f : 0.4f;
    st.act += (target - st.act) * 0.05f;

    st.spawn++;
    if (flowing) {
        if (back || (st.spawn & 1) == 0) {
            for (int i = 0; i < UP_N; i++) {
                if (!st.up[i].on) {
                    st.up[i] = {back ? 1.0f : 0.0f, back ? -3.5f : 1.0f, (uint16_t)hg_rand(), (uint8_t)(18 + (hg_rand() & 13)), 1};
                    break;
                }
            }
        }
        if (back || (st.spawn & 3) == 0 || (st.spawn & 3) == 2) {
            for (int i = 0; i < DN_N; i++) {
                if (!st.dn[i].on) {
                    st.dn[i] = {back ? 1.0f : 0.0f, back ? -3.5f : 1.0f, (uint16_t)hg_rand(), (uint8_t)(16 + (hg_rand() & 15)), 1};
                    break;
                }
            }
        }
    }
    for (int i = 0; i < UP_N; i++) {
        Flow& f = st.up[i];
        if (!f.on) continue;
        f.t += upSpeed(f.t) * f.v;
        if (f.t > 1 || f.t < 0) f.on = 0;
    }
    for (int i = 0; i < DN_N; i++) {
        Flow& f = st.dn[i];
        if (!f.on) continue;
        f.t += dnSpeed(f.t) * f.v;
        if (f.t > 1 || f.t < 0) f.on = 0;
    }
}

// ------------------------------------------------------------------ plotting
static inline void addPx(const Surf& s, int x, int y, uint16_t c) {
    if ((unsigned)(x - s.x0) < (unsigned)s.w && (unsigned)(y - s.y0) < (unsigned)s.h) {
        uint16_t* p = s.at(x, y);
        *p = add565(*p, c);
    }
}

// Point in 1/16 pixel, shared between the neighbours so slow motion stays smooth.
static inline void plot(const Surf& s, const uint16_t* pal, int x16, int y16, int lvl) {
    int tx = x16 >> 4, ty = y16 >> 4, fx = x16 & 15, fy = y16 & 15;
    if (lvl >= 23) {
        int l0 = (lvl * (16 - fy)) >> 4, l1 = lvl - l0;
        int a = (l0 * (16 - fx)) >> 4, b = l0 - a, d = (l1 * (16 - fx)) >> 4, e = l1 - d;
        if (a) addPx(s, tx, ty, pal[a]);
        if (b) addPx(s, tx + 1, ty, pal[b]);
        if (d) addPx(s, tx, ty + 1, pal[d]);
        if (e) addPx(s, tx + 1, ty + 1, pal[e]);
    } else {
        ty = (y16 + 8) >> 4;
        int a = (lvl * (16 - fx)) >> 4, b = lvl - a;
        if (a) addPx(s, tx, ty, pal[a]);
        if (b) addPx(s, tx + 1, ty, pal[b]);
    }
}

// Tilted soft glow without a division per pixel. rx, ry in pixels, shear in 1/256 rows per column.
static void softGlow(const Surf& s, const uint16_t* pal, int cx, int cy, int rx, int ry, int shear, int amp) {
    if (rx < 1 || ry < 1 || amp <= 0) return;
    int ax = (255 << 12) / (rx * rx), ay = (255 << 12) / (ry * ry);
    int reach = ry + ((rx * (shear < 0 ? -shear : shear)) >> 8);
    for (int dy = -reach; dy <= reach; dy++) {
        int y = cy + dy;
        if ((unsigned)(y - s.y0) >= (unsigned)s.h) continue;
        for (int dx = -rx; dx <= rx; dx++) {
            int x = cx + dx;
            if ((unsigned)(x - s.x0) >= (unsigned)s.w) continue;
            int v = dy - ((dx * shear) >> 8);
            int i = (dx * dx * ax + v * v * ay) >> 12;
            if (i >= 255) continue;
            int lvl = (HG_GLOW[i] * amp + (((x ^ y) & 1) << 12)) >> 13;
            if (lvl <= 0) continue;
            if (lvl > LVLS - 1) lvl = LVLS - 1;
            uint16_t* q = s.at(x, y);
            *q = add565(*q, pal[lvl]);
        }
    }
}

// Draw the innermost n stars of the catalogue as a tilted disc.
// (A,B)/(C,D) are the projection in 1/256, K is pixels per radius unit in Q12.
static void drawDisc(const SceneCtx& c, const State& st, int n, float cx, float cy, float rollDeg, float squash, float scale, bool mirror) {
    const SceneArt& a = ART_cosmos;
    const Surf& s = c.s;
    float rad = rollDeg * 0.0174533f;
    float cr = cosf(rad), sr = sinf(rad);
    int A = (int)(cr * 256), B = (int)(-sr * squash * 256), C = (int)(sr * 256), D = (int)(cr * squash * 256);
    int K = (int)(scale * 4096.0f / 3.0f);
    int cx16 = (int)(cx * 16), cy16 = (int)(cy * 16);
    uint32_t tick = c.tick;
    const CosmosStar* cat = ART_cosmos_stars;
    const int16_t* sn = st.sn;
    const int16_t* gl = a.gl;
    const int16_t* gr = a.gr;
    uint16_t pattern = (uint16_t)(tick * 42u);
    for (int i = 0; i < n; i++) {
        const CosmosStar& e = cat[i];
        int r = e.r;
        uint16_t ang = e.ang;
        int lvl = e.lc & 31;
        int kind = e.kind;
        if (kind == 0) {
            ang = (uint16_t)(ang + (uint32_t)st.omega[r] * tick);
        } else {
            // born on the arm, sheared by its own orbit, gone before the arm could wind up
            int shift = r < 66 ? 7 : r < 120 ? 8 : 9;
            if (kind == 2) shift = r < 66 ? 8 : 9;
            uint32_t L = 1u << shift;
            int age = (int)((tick + ((uint32_t)e.seed << (shift - 6))) & (L - 1));
            lvl = (lvl * st.env[(age << 8) >> shift]) >> 8;
            ang = (uint16_t)(ang + pattern + st.dOmega[r] * (age - (int)(L >> 1)));
            if (lvl <= 0) continue;
        }
        // the rim of the disc fades as stars leave
        int left = n - i;
        if (left < 64) lvl = (lvl * left) >> 6;
        if (lvl <= 0) continue;

        if (mirror) ang = (uint16_t)(0 - ang);
        int idx = ang >> 5;
        int cs = sn[(idx + SIN_N / 4) & (SIN_N - 1)], sv = sn[idx];
        int rs = r * K;  // Q12 pixels
        int xp = (cs * A + sv * B) >> 15, yp = (cs * C + sv * D) >> 15;  // Q8
        int x16 = cx16 + ((xp * (rs >> 4)) >> 12);
        int y16 = cy16 + ((yp * (rs >> 4)) >> 12);
        int ty = y16 >> 4;
        if (ty < a.top || ty > a.bot) continue;
        if (x16 < gl[ty] || x16 >= gr[ty]) continue;
        const uint16_t* pal = st.pal[(e.lc >> 5) & 3];
        if (kind == 2) {
            int tx = x16 >> 4;
            int side = lvl >> 1;
            addPx(s, tx, ty, pal[lvl]);
            if (side) {
                addPx(s, tx + 1, ty, pal[side]);
                addPx(s, tx - 1, ty, pal[side]);
                addPx(s, tx, ty + 1, pal[side]);
                addPx(s, tx, ty - 1, pal[side]);
            }
        } else {
            plot(s, pal, x16, y16, lvl);
        }
    }
}

static void drawTwinkle(const SceneCtx& c, const State& st) {
    const Surf& s = c.s;
    for (int i = 0; i < COSMOS_NTW; i++) {
        const CosmosTw& t = ART_cosmos_tw[i];
        // two slow incommensurate waves: never a visible loop
        int w = hg_isin((c.tick * t.sp >> 1) + t.ph) + (hg_isin((c.tick * (t.sp + 3) >> 2) + t.ph * 3) >> 1);
        int lvl = (t.lvl * (340 + w)) >> 9;
        if (lvl < 1) continue;
        if (lvl > LVLS - 1) lvl = LVLS - 1;
        addPx(s, t.x, t.y, st.pal[(t.col & 1) ? COL_GOLD : COL_BLUE][lvl]);
    }
}

// Newborn nebula: every pixel has a moment d when the growing cloud reaches it.
static void drawNebula(const SceneCtx& c, State& st) {
    const Surf& s = c.s;
    float p = c.progress;
    if (p < 0.002f) return;
    int R = (int)(sqrtf(p) * 300.0f) + ((hg_isin((int)(c.tick / 3)) * 3) >> 7);
    if (R != st.lutR) {
        st.lutR = R;
        const int soft = 64;
        for (int d = 0; d < 256; d++) {
            int v = (R - d) * 31 / soft;
            st.lut[d] = (uint8_t)(v < 0 ? 0 : v > 31 ? 31 : v);
        }
    }
    int j0 = COSMOS_NEB_CY - COSMOS_NEB_Y - (R * 58 >> 8) - 2, j1 = COSMOS_NEB_CY - COSMOS_NEB_Y + (R * 58 >> 8) + 2;
    if (j0 < 0) j0 = 0;
    if (j1 > COSMOS_NEB_H) j1 = COSMOS_NEB_H;
    for (int j = j0; j < j1; j++) {
        const uint16_t* span = ART_cosmos_nebspan + j * 3;
        const uint8_t* src = ART_cosmos_neb + span[0] * 2;
        int i0 = span[1], len = span[2];
        uint16_t* o = s.at(COSMOS_NEB_X + i0, COSMOS_NEB_Y + j);
        int odd = (i0 ^ j) & 1;
        for (int i = 0; i < len; i++, o++, src += 2, odd ^= 1) {
            int b = src[0];
            if (!b) continue;
            int a = st.lut[src[1]];
            if (!a) continue;
            int lvl = ((b >> 3) * a + (odd << 4)) >> 5;
            if (lvl) *o = add565(*o, st.gas[(b & 7) * LVLS + lvl]);
        }
    }
}

// Stream into the hole: leaves the core in a thin thread, swings out into the ring's plane, dives.
static inline void upPos(const State& st, float cx, float t, uint16_t ph, int& x16, int& y16) {
    const SceneArt& a = ART_cosmos;
    float k = t / 0.74f;
    if (k > 1) k = 1;
    float yt = st.gy + (a.neck - st.gy) * k * k;
    float R;
    if (t < 0.70f) R = 1.2f + t * 5.0f;
    else {
        float u = (t - 0.70f) / 0.30f;
        R = u < 0.28f ? 4.7f + u / 0.28f * 19.0f : 23.7f - (u - 0.28f) / 0.72f * 17.0f;
    }
    int ang = (ph + (int)(t * t * t * 15.0f * 2048.0f)) & (SIN_N - 1);
    float cs = st.sn[(ang + SIN_N / 4) & (SIN_N - 1)] / 32767.0f, sv = st.sn[ang] / 32767.0f;
    float x = cs * R, y = sv * R * 0.20f;
    x16 = (int)((cx + x + y * 0.22f) * 16);
    y16 = (int)((yt + y - x * 0.225f) * 16);
}

static inline void dnPos(const State& st, float cx, float t, uint16_t ph, int& x16, int& y16) {
    const SceneArt& a = ART_cosmos;
    float yt = a.neck + 6 + (COSMOS_NEB_CY - a.neck - 6) * t;
    float R = 1.0f + 5.0f * t * t;
    int ang = (ph + (int)((1.0f - t) * (1.0f - t) * 5.0f * 2048.0f)) & (SIN_N - 1);
    float cs = st.sn[(ang + SIN_N / 4) & (SIN_N - 1)] / 32767.0f, sv = st.sn[ang] / 32767.0f;
    x16 = (int)((cx + cs * R) * 16);
    y16 = (int)((yt + sv * R * 0.30f) * 16);
}

static void drawStreams(const SceneCtx& c, const State& st) {
    const SceneArt& a = ART_cosmos;
    const Surf& s = c.s;
    float cx = a.cx;
    for (int i = 0; i < UP_N; i++) {
        const Flow& f = st.up[i];
        if (!f.on) continue;
        float fade = f.t < 0.08f ? f.t / 0.08f : 1.0f;
        int base = (int)(f.lvl * fade * (0.55f + 0.75f * f.t));
        const uint16_t* pal = f.t > 0.72f ? st.pal[COL_GOLD] : st.pal[COL_BLUE];
        float dt = upSpeed(f.t) * (f.v < 0 ? -1.0f : 1.0f);
        int prevX = 0, prevY = 0;
        for (int g = 0; g < 3; g++) {
            float t = f.t - dt * g * 0.9f;
            if (t < 0) break;
            int x16, y16;
            upPos(st, cx, t, f.ph, x16, y16);
            int lvl = base >> g;
            if (lvl > LVLS - 1) lvl = LVLS - 1;
            if (lvl > 0 && !(g && (x16 >> 4) == prevX && (y16 >> 4) == prevY)) plot(s, pal, x16, y16, lvl);
            prevX = x16 >> 4; prevY = y16 >> 4;
        }
    }
    for (int i = 0; i < DN_N; i++) {
        const Flow& f = st.dn[i];
        if (!f.on) continue;
        float fade = f.t < 0.06f ? f.t / 0.06f : f.t > 0.82f ? (1.0f - f.t) / 0.18f : 1.0f;
        int base = (int)(f.lvl * fade);
        const uint16_t* pal = (i & 3) == 0 ? st.pal[COL_ROSE] : st.pal[COL_BLUE];
        float dt = dnSpeed(f.t) * (f.v < 0 ? -1.0f : 1.0f);
        int prevX = 0, prevY = 0;
        for (int g = 0; g < 2; g++) {
            float t = f.t - dt * g * 1.2f;
            if (t < 0) break;
            int x16, y16;
            dnPos(st, cx, t, f.ph, x16, y16);
            int lvl = base >> g;
            if (lvl > LVLS - 1) lvl = LVLS - 1;
            if (lvl > 0 && !(g && (x16 >> 4) == prevX && (y16 >> 4) == prevY)) plot(s, pal, x16, y16, lvl);
            prevX = x16 >> 4; prevY = y16 >> 4;
        }
    }
}

static void drawHole(const SceneCtx& c, const State& st) {
    const SceneArt& a = ART_cosmos;
    const Surf& s = c.s;
    int cx = a.cx, cy = a.neck;
    int breathe = hg_isin((int)(c.tick / 2));
    int amp = (int)((46 + (breathe >> 4)) * st.act);
    softGlow(s, st.palHalo, cx, cy, 36, 15, -59, amp);
    s.disc((float)cx, (float)cy, 7.2f, 0);

    int gain = (int)(st.act * 256);
    int phase = (int)(c.tick * 5);
    const uint8_t* src = ART_cosmos_ring;
    int x0 = cx - COSMOS_RW / 2, y0 = cy - COSMOS_RH / 2;
    for (int j = 0; j < COSMOS_RH; j++) {
        uint16_t* o = s.at(x0, y0 + j);
        for (int i = 0; i < COSMOS_RW; i++, o++, src += 2) {
            int e = src[1];
            if (e < 3) continue;
            int lvl = (e * st.lump[(src[0] + phase) & 255] * gain) >> 19;
            if (lvl <= 0) continue;
            if (lvl > LVLS - 1) lvl = LVLS - 1;
            *o = add565(*o, st.palRing[lvl]);
        }
    }
}

static void draw(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_cosmos;

    drawTwinkle(c, st);
    drawNebula(c, st);

    // bottom: the universe being born, spinning the other way
    if (st.nBot > 0) {
        float m = (float)st.nBot / COSMOS_NSTARS;
        int amp = 70 + (int)(sqrtf(m) * 130);
        int rx = 4 + (int)(sqrtf(m) * 22), ry = 2 + (int)(sqrtf(m) * 10);
        softGlow(c.s, st.palCore, a.cx, COSMOS_NEB_CY, rx, ry, 40, amp);
        softGlow(c.s, st.palCoreHot, a.cx, COSMOS_NEB_CY, rx / 3 + 1, ry / 3 + 1, 40, amp / 2);
        drawDisc(c, st, st.nBot, a.cx, COSMOS_NEB_CY, 9.0f, 0.42f, 0.92f, true);
    }
    // top: the old galaxy, draining
    if (st.nTop > 0) {
        float m = (float)st.nTop / COSMOS_NSTARS;
        float sm = sqrtf(m);
        int gy = (int)st.gy;
        int amp = (int)(sm * 230);
        int rx = 3 + (int)(sm * 26 * st.gScale), ry = 2 + (int)(sm * 12 * st.gScale);
        softGlow(c.s, st.palCore, a.cx, gy, rx, ry, -74, amp);
        softGlow(c.s, st.palCoreHot, a.cx, gy, rx / 3 + 1, ry / 3 + 1, -74, amp);
        drawDisc(c, st, st.nTop, (float)a.cx, st.gy, -16.0f, 0.46f, st.gScale, false);
    }
    drawStreams(c, st);
    drawHole(c, st);
}

}  // namespace cosmos

static const Scene SCENE_cosmos = {
    "Event Horizon", "The universe, falling through the neck of time.", &ART_cosmos,
    {C565(240, 242, 255), C565(164, 170, 208), C565(246, 198, 112), C565(92, 228, 184), C565(48, 48, 84), C565(14, 12, 28), C565(255, 112, 134)},
    {150, 90, 255},
    cosmos::init, cosmos::tick, cosmos::draw,
};
