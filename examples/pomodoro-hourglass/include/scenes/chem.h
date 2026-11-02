// ==============================================================================
//  Titration - a mad scientist's clock. Glowing reagent crawls in the funnel, drips
//  through a stopcock on a long viscous thread and blooms into the analyte below.
//
//  Both liquids are low-res scalar fields (4 px cells) moved by semi-Lagrangian
//  advection in integer maths: a curl of scrolling smooth noise, plus a descending
//  vortex pair for every landed drop, which is what rolls each cloud into a mushroom.
//  Pixels: bilinear field + scrolling detail -> dye level, position -> brightness,
//  both index one prebuilt palette. Nothing per pixel but adds, shifts and loads.
// ==============================================================================
#pragma once
#include "../hg_core.h"
#include "../art/chem.h"

namespace chem {

enum { NX = 38, NY = 36, NCELL = NX * NY, GX0 = 44, CELL = 4 };
enum { TIP_Y = 177, MAX_BUB = 20, MAX_SPL = 8, MAX_PLUME = 3, WAVE_W = 148 };
static const float FILL = 0.74f, ANALYTE = 0.22f;   // same constants in tools/art_chem.py

struct Bubble { float x, y, r, vy, ph; uint8_t on, up; };
struct Splash { float x, y, vx, vy; uint8_t on; };
struct Plume { float x, y, vy, w, gamma; uint8_t on; }; // cells

struct State {
    GlassVolume vol;
    uint16_t fields[3][NCELL];
    uint8_t idUp, idLo, idScr;
    int8_t vu[NCELL], vv[NCELL];
    uint8_t sm[4096];               // smooth tiling noise
    uint16_t palU[512], palL[512], palE[512]; // [brightness 0..15][level 0..31]
    uint16_t vapU[16], vapL[16];
    uint16_t glowU, glowL;
    uint16_t lutV[256];             // 1/(d2+1) for the vortices
    uint8_t ell[64];                // sqrt(1-u2)
    uint8_t isq[256];               // sqrt(d2*32)
    int8_t rip[80];
    int16_t back[WAVE_W], frontC[WAVE_W];
    uint16_t rv[NX + 1];
    uint8_t rowB[17];

    Bubble bub[MAX_BUB];
    Splash spl[MAX_SPL];
    Plume plume[MAX_PLUME];

    float brk, palP, lastP, levelU, levelL;
    bool palEValid;
    float ripT, ripAmp;
    float ripTU, ripAmpU;
    float flash;
    float closeAnim;         // stopcock 0 open .. 1 closed
    float rewind;            // 0..1 charge rushing back up
    // drop machine
    uint8_t stage;           // 0 wait, 1 swell, 2 stretch, 3 fall
    int16_t wait;
    float dropY, dropV, dropR, thread, stub, tail;
    int16_t srcTimer, bubTimer, wispPhase;
};
static_assert(sizeof(State) <= SCENE_ARENA_BYTES, "");

// ---------------------------------------------------------------- palettes
struct RGBf { float r, g, b; };
static inline RGBf mixf(RGBf a, RGBf b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline uint16_t to565(RGBf c) {
    int r = (int)c.r, g = (int)c.g, b = (int)c.b;
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
    return C565(r, g, b);
}
static RGBf rampf(const RGBf* s, int n, float t) {
    if (t <= 0) return s[0];
    float f = t * (n - 1);
    int i = (int)f;
    if (i >= n - 1) return s[n - 1];
    return mixf(s[i], s[i + 1], f - i);
}

// Lit colour -> palette row: dark edge of the body up to a hot, slightly washed highlight.
static inline RGBf bright(RGBf c, int b, RGBf hot, float wash) {
    float k = 0.30f + 0.74f * b / 15.0f;
    RGBf o = {c.r * k, c.g * k, c.b * k};
    if (b >= 13) o = mixf(o, hot, (b - 12) * wash);
    return o;
}

static const RGBf REAG_F[6] = {{10, 62, 34}, {22, 122, 40}, {62, 196, 40}, {132, 240, 48}, {214, 255, 92}, {246, 255, 196}};
static const RGBf REAG_B[6] = {{8, 26, 96}, {14, 54, 168}, {30, 96, 232}, {70, 150, 255}, {140, 205, 255}, {225, 242, 255}};

static void buildUpper(State& st, float brk) {
    RGBf hotF = {240, 255, 200}, hotB = {220, 240, 255};
    RGBf hot = mixf(hotF, hotB, brk);
    for (int b = 0; b < 16; b++)
        for (int l = 0; l < 32; l++) {
            float t = l / 31.0f;
            RGBf c = mixf(rampf(REAG_F, 6, t), rampf(REAG_B, 6, t), brk);
            st.palU[(b << 5) | l] = to565(bright(c, b, hot, 0.06f));
        }
    for (int i = 0; i < 16; i++) {
        RGBf c = mixf(RGBf{60, 104, 54}, RGBf{44, 74, 120}, brk);
        float k = i / 15.0f;
        st.vapU[i] = to565({c.r * k, c.g * k, c.b * k});
    }
    st.glowU = to565(mixf(RGBf{70, 190, 50}, RGBf{40, 90, 220}, brk));
}

// Titration colour of the bulk with progress: violet, indigo, teal, green. Never through grey.
static const RGBf BULK_F[5] = {{124, 78, 222}, {84, 76, 226}, {30, 118, 214}, {20, 168, 136}, {66, 204, 58}};
static const RGBf BULK_B[5] = {{150, 150, 214}, {120, 136, 220}, {90, 120, 226}, {64, 104, 232}, {44, 92, 226}};

static void buildLowerInto(uint16_t* pal, RGBf bulk, RGBf edge, RGBf cloud, RGBf hotc) {
    for (int b = 0; b < 16; b++)
        for (int l = 0; l < 32; l++) {
            float t = l / 31.0f;
            RGBf c;
            if (t < 0.20f) c = mixf(bulk, edge, t / 0.20f);
            else if (t < 0.55f) c = mixf(edge, cloud, (t - 0.20f) / 0.35f);
            else c = mixf(cloud, hotc, (t - 0.55f) / 0.45f);
            pal[(b << 5) | l] = to565(bright(c, b, RGBf{235, 240, 255}, 0.02f));
        }
}

static void buildLower(State& st, float p, float brk) {
    float t = p > 1 ? 1 : p;
    RGBf bulk = mixf(rampf(BULK_F, 5, t), rampf(BULK_B, 5, t), brk);
    bulk = {bulk.r * 0.80f, bulk.g * 0.80f, bulk.b * 0.80f};
    // clouds are always well above the bulk: cyan edge early on, yellow-green once the bulk is green
    RGBf edge = mixf(mixf(RGBf{44, 186, 196}, RGBf{120, 230, 110}, t * t), RGBf{120, 190, 250}, brk);
    RGBf cloud = mixf(mixf(RGBf{96, 232, 76}, RGBf{180, 250, 80}, t), RGBf{70, 150, 255}, brk);
    RGBf hotc = mixf(mixf(RGBf{216, 255, 110}, RGBf{240, 255, 150}, t), RGBf{200, 232, 255}, brk);
    buildLowerInto(st.palL, bulk, edge, cloud, hotc);
    st.palP = p;
    st.brk = brk;
    if (p > 0.955f) {
        // the endpoint colour: the bulb turns hot magenta. On a break: pale frost.
        RGBf eb = mixf(RGBf{176, 28, 122}, RGBf{84, 150, 216}, brk);
        RGBf ee = mixf(RGBf{250, 90, 170}, RGBf{120, 190, 240}, brk);
        RGBf ec = mixf(RGBf{255, 130, 190}, RGBf{170, 220, 250}, brk);
        RGBf eh = mixf(RGBf{255, 214, 236}, RGBf{220, 240, 255}, brk);
        buildLowerInto(st.palE, eb, ee, ec, eh);
        st.palEValid = true;
    } else st.palEValid = false;
    for (int i = 0; i < 16; i++) {
        float k = i / 15.0f;
        RGBf c = mixf(RGBf{bulk.r * 0.42f + 14, bulk.g * 0.42f + 18, bulk.b * 0.42f + 14}, RGBf{44, 74, 120}, brk * 0.5f);
        st.vapL[i] = to565({c.r * k, c.g * k, c.b * k});
    }
    st.glowL = to565(mixf(RGBf{70, 210, 60}, RGBf{50, 110, 240}, brk));
}

// ---------------------------------------------------------------- fields
static void buildSmooth(State& st) {
    static_assert(sizeof(st.sm) == 4096, "");
    uint8_t* d = st.sm;
    // Two box blurs of the white noise, in place via a small line buffer.
    memcpy(d, HG_NOISE, 4096);
    uint8_t line[64];
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < 64; y++) {
            uint8_t* r = d + y * 64;
            int sum = 0;
            for (int k = -3; k <= 3; k++) sum += r[k & 63];
            for (int x = 0; x < 64; x++) { line[x] = sum / 7; sum += r[(x + 4) & 63] - r[(x - 3) & 63]; }
            memcpy(r, line, 64);
        }
        for (int x = 0; x < 64; x++) {
            int sum = 0;
            for (int k = -3; k <= 3; k++) sum += d[(k & 63) * 64 + x];
            for (int y = 0; y < 64; y++) { line[y] = sum / 7; sum += d[((y + 4) & 63) * 64 + x] - d[((y - 3) & 63) * 64 + x]; }
            for (int y = 0; y < 64; y++) d[y * 64 + x] = line[y];
        }
    }
    int lo = 255, hi = 0;
    for (int i = 0; i < 4096; i++) { if (d[i] < lo) lo = d[i]; if (d[i] > hi) hi = d[i]; }
    if (hi <= lo) hi = lo + 1;
    for (int i = 0; i < 4096; i++) d[i] = (uint8_t)((d[i] - lo) * 255 / (hi - lo));
}

// Soft stamp into a field. Coordinates in cells.
static void stamp(uint16_t* f, float cx, float cy, float rad, int amount) {
    int r2 = (int)(rad * rad * 64);
    if (r2 < 1) r2 = 1;
    int i0 = (int)(cx - rad), i1 = (int)(cx + rad) + 1, j0 = (int)(cy - rad), j1 = (int)(cy + rad) + 1;
    int cx8 = (int)(cx * 8), cy8 = (int)(cy * 8);
    if (i0 < 0) i0 = 0;
    if (j0 < 0) j0 = 0;
    if (i1 >= NX) i1 = NX - 1;
    if (j1 >= NY) j1 = NY - 1;
    for (int j = j0; j <= j1; j++)
        for (int i = i0; i <= i1; i++) {
            int dx = i * 8 - cx8, dy = j * 8 - cy8, d2 = dx * dx + dy * dy;
            if (d2 >= r2) continue;
            int v = f[j * NX + i] + ((HG_GLOW[d2 * 255 / r2] * amount) >> 8);
            f[j * NX + i] = v > 65535 ? 65535 : (uint16_t)v;
        }
}

static inline int clamp8(int v) { return v > 127 ? 127 : v < -127 ? -127 : v; }

// Curl of the scrolling noise, in 1/64 cell per step.
static void buildVelocity(State& st, uint32_t tick, int ampShift, int phaseA, int phaseB, int freq) {
    const uint8_t* sm = st.sm;
    int a = (tick >> 2) * phaseA >> 2, b = (tick >> 2) * phaseB >> 2;
    int8_t* vu = st.vu, *vv = st.vv;
    for (int j = 0; j < NY; j++) {
        int y0 = ((j - 1) * freq + b) & 63, y1 = ((j + 1) * freq + b) & 63, ym = (j * freq + b) & 63;
        const uint8_t* r0 = sm + y0 * 64, *r1 = sm + y1 * 64, *rm = sm + ym * 64;
        for (int i = 0; i < NX; i++) {
            int xm = (i * freq + a) & 63, xl = ((i - 1) * freq + a) & 63, xr = ((i + 1) * freq + a) & 63;
            int u = ((int)r1[xm] - r0[xm]) >> ampShift;
            int v = ((int)rm[xl] - rm[xr]) >> ampShift;
            *vu++ = (int8_t)clamp8(u);
            *vv++ = (int8_t)clamp8(v);
        }
    }
}

static void addVortices(State& st) {
    for (int k = 0; k < MAX_PLUME; k++) {
        const Plume& pl = st.plume[k];
        if (!pl.on) continue;
        int gam = (int)pl.gamma;
        int px8 = (int)(pl.x * 8), py8 = (int)(pl.y * 8), w8 = (int)(pl.w * 8);
        int i0 = (int)pl.x - 11, i1 = (int)pl.x + 11, j0 = (int)pl.y - 8, j1 = (int)pl.y + 11;
        if (i0 < 0) i0 = 0;
        if (j0 < 0) j0 = 0;
        if (i1 >= NX) i1 = NX - 1;
        if (j1 >= NY) j1 = NY - 1;
        for (int j = j0; j <= j1; j++) {
            for (int i = i0; i <= i1; i++) {
                int dy = j * 8 - py8;
                int dx1 = i * 8 - (px8 - w8), dx2 = i * 8 - (px8 + w8);
                int d1 = (dx1 * dx1 + dy * dy) >> 4, d2 = (dx2 * dx2 + dy * dy) >> 4;
                if (d1 > 255) d1 = 255;
                if (d2 > 255) d2 = 255;
                int l1 = st.lutV[d1], l2 = st.lutV[d2];
                // left vortex s=+1, right s=-1: fluid between them moves down
                int u = (gam * (-dy * l1 + dy * l2)) >> 15;
                int v = (gam * (dx1 * l1 - dx2 * l2)) >> 15;
                int idx = j * NX + i;
                st.vu[idx] = (int8_t)clamp8(st.vu[idx] + u);
                st.vv[idx] = (int8_t)clamp8(st.vv[idx] + v);
            }
        }
    }
}

// One semi-Lagrangian step. Velocities in 1/64 cell. decayShift: v -= v >> shift.
static void advect(State& st, uint8_t& idF, int meanV, const int8_t* colMean, int decayShift) {
    const uint16_t* src = st.fields[idF];
    uint16_t* dst = st.fields[st.idScr];
    const int8_t* vu = st.vu, *vv = st.vv;
    const int maxX = ((NX - 1) << 6) - 1, maxY = ((NY - 1) << 6) - 1;
    for (int j = 0; j < NY; j++) {
        for (int i = 0; i < NX; i++) {
            int sx = (i << 6) - *vu++;
            int sy = (j << 6) - *vv++ - meanV - (colMean ? colMean[i] : 0);
            if (sx < 0) sx = 0; else if (sx > maxX) sx = maxX;
            if (sy < 0) sy = 0; else if (sy > maxY) sy = maxY;
            int ix = sx >> 6, iy = sy >> 6, fx = sx & 63, fy = sy & 63;
            const uint16_t* q = src + iy * NX + ix;
            uint32_t t = (uint32_t)q[0] * (64 - fx) + (uint32_t)q[1] * fx;
            uint32_t b = (uint32_t)q[NX] * (64 - fx) + (uint32_t)q[NX + 1] * fx;
            uint32_t v = (t * (64 - fy) + b * fy) >> 12;
            *dst++ = (uint16_t)(v - (v >> decayShift));
        }
    }
    uint8_t tmp = idF;
    idF = st.idScr;
    st.idScr = tmp;
}

static inline float cellX(float x) { return (x - GX0) / CELL; }

static void spawnBubble(State& st, float x, float y, float r, float vy, bool up) {
    for (int i = 0; i < MAX_BUB; i++) {
        Bubble& b = st.bub[i];
        if (!b.on) { b = {x, y, r, vy, hg_randf() * 6.28f, 1, (uint8_t)(up ? 1 : 0)}; return; }
    }
}

static void impact(SceneCtx& c, State& st, float x, float y) {
    const SceneArt& a = ART_chem;
    float gy = (y + 3 - a.neck) / CELL;
    uint16_t* f = st.fields[st.idLo];
    stamp(f, cellX(x), gy, 3.2f, 44000);
    for (int k = 0; k < MAX_PLUME; k++) {
        if (!st.plume[k].on) {
            st.plume[k] = {cellX(x), gy + 1.0f, 0.13f, 1.9f, 66.0f, 1};
            break;
        }
    }
    st.ripT = 0;
    st.ripAmp = 1.0f;
    st.flash = 1.0f;
    int nb = 4 + (hg_rand() & 3);
    for (int i = 0; i < nb; i++)
        spawnBubble(st, x + hg_rands() * 7, y + 4 + hg_randf() * 12, 0.7f + hg_randf() * 1.3f, -0.45f - hg_randf() * 0.6f, false);
    for (int i = 0; i < MAX_SPL; i++) {
        if (i >= 4) break;
        Splash& s = st.spl[i];
        s = {x, y - 1, hg_rands() * 0.75f, -0.9f - hg_randf() * 1.1f, 1};
    }
    if (!c.paused) hg_drip((uint16_t)(210 + (hg_rand() & 63)));
}

static void seedUpper(State& st) {
    const SceneArt& a = ART_chem;
    uint16_t* f = st.fields[st.idUp];
    for (int i = 0; i < NCELL; i++) f[i] = 7000;
    for (int k = 0; k < 16; k++) {
        float y = st.levelU + hg_randf() * (a.neck - st.levelU);
        float r = art_radius(a, (int)y);
        stamp(f, cellX(a.cx + hg_rands() * r * 0.8f), (y - a.top) / CELL, 3.5f + hg_randf() * 3, 15000);
    }
}

static void init(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_chem;
    st.vol.build(a, FILL);
    st.idUp = 0; st.idLo = 1; st.idScr = 2;
    buildSmooth(st);
    for (int i = 0; i < 256; i++) st.lutV[i] = (uint16_t)(4096.0f / (1.0f + i / 4.0f));
    for (int i = 0; i < 64; i++) st.ell[i] = (uint8_t)(sqrtf(1.0f - (i + 0.5f) / 64.0f) * 255.0f);
    for (int i = 0; i < 256; i++) { int v = (int)sqrtf(i * 32.0f); st.isq[i] = (uint8_t)(v > 79 ? 79 : v); }
    buildUpper(st, c.breakMix);
    buildLower(st, c.progress, c.breakMix);
    st.levelU = st.vol.upperLevel(1.0f - c.progress);
    st.levelL = st.vol.lowerLevel(ANALYTE + c.progress);
    seedUpper(st);
    st.lastP = c.progress;
    st.stage = 0;
    st.wait = 20;
    st.closeAnim = (c.paused || c.progress >= 0.998f) ? 1.0f : 0.0f;
    st.srcTimer = 10;
    st.bubTimer = 60;
    // a cloud already hanging in the bulk, so a scene switch never shows a blank bulb
    if (c.progress > 0.01f)
        for (int k = 0; k < 3; k++)
            stamp(st.fields[st.idLo], cellX(a.cx + hg_rands() * 18), (st.levelL + 14 + k * 16 - a.neck) / CELL, 5.0f, 20000);
}

static void tick(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_chem;
    float p = c.progress;
    float dp = p - st.lastP;
    st.lastP = p;

    if (fabsf(c.breakMix - st.brk) > 0.004f || (c.breakMix != st.brk && (c.breakMix == 0 || c.breakMix == 1))) {
        buildUpper(st, c.breakMix);
        buildLower(st, p, c.breakMix);
    } else if (fabsf(p - st.palP) > 0.004f) {
        buildLower(st, p, c.breakMix);
    }
    st.levelU = st.vol.upperLevel(1.0f - p);
    st.levelL = st.vol.lowerLevel(ANALYTE + p);

    bool empty = p >= 0.998f;
    if (dp < -0.0015f) {  // restart: the charge rushes back up
        st.rewind = 1.0f;
        st.stage = 0;
        st.wait = 40;
        st.flash = 0;
        for (int k = 0; k < MAX_PLUME; k++) st.plume[k].on = 0;
        for (int i = 0; i < MAX_BUB; i++) if (!st.bub[i].up) st.bub[i].on = 0;
    } else if (st.rewind > 0) {
        st.rewind -= 0.06f;
        if (st.rewind < 0) st.rewind = 0;
    }
    bool hold = c.paused || empty;
    float tgt = hold ? 1.0f : 0.0f;
    st.closeAnim += (tgt - st.closeAnim) * 0.12f;

    // ---- the drop
    float surf = st.levelL;
    switch (st.stage) {
        case 0:
            if (st.dropR > 0.2f) st.dropR *= 0.9f;
            if (!hold && st.rewind <= 0 && --st.wait <= 0) { st.stage = 1; st.dropR = 0.8f; st.thread = 0; st.dropV = 0; }
            st.dropY = TIP_Y + 2;
            break;
        case 1:
            if (!hold) {
                st.dropR += 0.07f;
                if (st.dropR >= 3.5f) { st.stage = 2; st.dropV = 0.12f; }
            } else if (empty) { st.dropR -= 0.03f; if (st.dropR <= 0.8f) st.stage = 0; }
            st.dropY = TIP_Y + 1 + st.dropR * 0.85f;
            break;
        case 2: {
            if (!hold) {
                st.dropV += 0.012f;
                st.dropY += st.dropV;
                float lim = surf - TIP_Y;
                lim = lim * 0.62f;
                if (lim > 52) lim = 52;
                if (lim < 10) lim = 10;
                st.dropR += (2.9f - st.dropR) * 0.02f;
                if (st.dropY - TIP_Y >= lim) {
                    st.stage = 3;
                    st.stub = (st.dropY - TIP_Y) * 0.45f;
                    st.tail = (st.dropY - TIP_Y) * 0.48f;
                    st.dropV += 0.15f;
                }
            }
            break;
        }
        case 3:
            st.dropV += 0.085f;
            if (st.dropV > 3.4f) st.dropV = 3.4f;
            st.dropY += st.dropV;
            st.stub *= 0.84f;          // the upper thread snaps back to the tip
            st.tail *= 0.80f;          // the lower one is swallowed by the drop
            break;
    }
    if (st.stage >= 2 && st.dropY + st.dropR >= surf) {
        if (st.stage == 3 || !c.paused) {
            impact(c, st, a.cx + 0.0f, surf);
            st.stage = 0;
            st.wait = 14 + (hg_rand() & 15);
            st.dropR = 0;
            st.stub = st.tail = 0;
        }
    }

    // ---- ripples
    if (st.ripAmp > 0.01f) { st.ripT += 1.0f; st.ripAmp *= 0.972f; } else st.ripAmp = 0;
    if (st.ripAmpU > 0.01f) { st.ripTU += 1.0f; st.ripAmpU *= 0.96f; } else st.ripAmpU = 0;
    if (st.flash > 0.01f) st.flash *= 0.90f; else st.flash = 0;

    // ---- splash droplets
    for (int i = 0; i < MAX_SPL; i++) {
        Splash& s = st.spl[i];
        if (!s.on) continue;
        s.vy += 0.09f;
        s.x += s.vx;
        s.y += s.vy;
        if (s.y >= surf && s.vy > 0) s.on = 0;
    }

    // ---- bubbles
    if (--st.bubTimer <= 0) {
        st.bubTimer = 70 + (hg_rand() & 127);
        if (st.levelU < a.neck - 24) {
            float y = a.neck - 12 - hg_randf() * (a.neck - st.levelU - 30);
            float r = art_radius(a, (int)y);
            spawnBubble(st, a.cx + hg_rands() * r * 0.5f, y, 1.4f + hg_randf() * 2.2f, -0.12f - hg_randf() * 0.10f, true);
        }
        // the analyte also gives the odd slow fizz
        spawnBubble(st, a.cx + hg_rands() * 30, a.bot - 12 - hg_randf() * 10, 0.6f + hg_randf() * 0.8f, -0.42f, false);
    }
    for (int i = 0; i < MAX_BUB; i++) {
        Bubble& b = st.bub[i];
        if (!b.on) continue;
        b.ph += 0.09f;
        b.y += b.vy;
        float top = b.up ? st.levelU : st.levelL;
        float rr = art_radius(a, (int)b.y) - b.r - 2;
        b.x += (hg_isin((int)(b.ph * 40.7f)) / 127.0f) * 0.11f;
        if (rr < 1) rr = 1;
        if (b.x < a.cx - rr) b.x = a.cx - rr;
        if (b.x > a.cx + rr) b.x = a.cx + rr;
        if (b.y <= top + 1) {
            b.on = 0;
            if (b.up) { st.ripTU = 0; st.ripAmpU = 0.8f; }
        }
    }

    // ---- fluid
    if (c.tick & 1) {
        // upper: convection roll: hot core rising, the rim sinking
        int8_t col[NX];
        for (int i = 0; i < NX; i++) {
            int dx = i * 256 / NX - 128;   // -128..128
            col[i] = (int8_t)(-(hg_icos(dx) * 6) >> 7);
        }
        buildVelocity(st, c.tick, 2, 3, 5, 2);
        advect(st, st.idUp, 0, col, 7);
        if (--st.srcTimer <= 0) {
            st.srcTimer = 12 + (hg_rand() & 15);
            float y = st.levelU + 6 + hg_randf() * (a.neck - st.levelU - 6);
            float r = art_radius(a, (int)y);
            uint32_t pick = hg_rand();
            stamp(st.fields[st.idUp], cellX(a.cx + hg_rands() * r * 0.75f), (y - a.top) / CELL, 3.5f + hg_randf() * 3.0f, (pick & 1) ? 30000 : 14000);
        }
        // a hot stream drawn into the neck while it flows
        if (!hold) stamp(st.fields[st.idUp], cellX(a.cx), (a.neck - 12 - a.top) / CELL, 2.2f, 2200);
    } else {
        buildVelocity(st, c.tick + 900, 3, -2, 3, 2);
        for (int k = 0; k < MAX_PLUME; k++) {
            Plume& pl = st.plume[k];
            if (!pl.on) continue;
            pl.y += pl.vy;
            pl.vy *= 0.982f;
            // near the floor of the bulb the ring flattens and spreads, like real ink
            float floorY = (art_radius(a, a.neck + (int)pl.y * CELL + 8) < 20) ? 1.0f : 0.0f;
            pl.w += 0.014f + floorY * 0.10f;
            if (floorY > 0) pl.vy *= 0.9f;
            pl.gamma *= 0.986f;
            // the vortex cores keep their ink: this is what stays crisp and curls
            if (pl.gamma > 34) {
                int amt = (int)(pl.gamma * 80);
                stamp(st.fields[st.idLo], pl.x - pl.w, pl.y, 1.9f, amt);
                stamp(st.fields[st.idLo], pl.x + pl.w, pl.y, 1.9f, amt);
            }
            if (pl.gamma < 8) pl.on = 0;
        }
        addVortices(st);
        advect(st, st.idLo, 2, nullptr, 7);
    }
    // nothing is falling: keep the bulk faintly turning so the bulb never freezes
    if (hold && (c.tick & 63) == 0)
        stamp(st.fields[st.idLo], cellX(a.cx + hg_rands() * 40), (st.levelL + 10 + hg_randf() * (a.bot - st.levelL - 14) - a.neck) / CELL, 4.0f, 9000);
    if (st.rewind > 0) {  // pale wash pulled out of the bulk
        uint16_t* f = st.fields[st.idLo];
        for (int i = 0; i < NCELL; i++) f[i] -= f[i] >> 3;
    }
}

// ---------------------------------------------------------------- pixels
static const uint8_t BAY[4][4] = {{0, 4, 1, 5}, {6, 2, 7, 3}, {1, 5, 0, 4}, {7, 3, 6, 2}};

// Column positions of the liquid surface as a thin ellipse seen from slightly above. 1/16 px.
static void surfaceCurves(State& st, const SceneArt& a, float level, float ryK, bool lower, uint32_t tick) {
    float r = art_radius(a, (int)level + 1);
    if (r < 1) r = 1;
    float ry = r * ryK;
    int ry16 = (int)(ry * 16);
    int L16 = (int)(level * 16);
    int k = (int)(64.0f * 256.0f / (r * r));
    const int8_t* rip = st.rip;
    int ph1 = tick, ph2 = tick >> 1;
    for (int i = 0; i < WAVE_W; i++) {
        int dx = i + GX0 - a.cx;
        int wave = (hg_isin(i * 3 + ph1) + hg_isin(i * 5 - ph2)) >> 5;   // +-half a px (in 1/16)
        if (lower) {
            int adx = dx < 0 ? -dx : dx;
            if (adx < 80) wave += rip[adx] * 4;
        }
        int idx = (dx * dx * k) >> 8;
        if (idx < 64) {
            int e = (st.ell[idx] * ry16) >> 8;
            st.back[i] = (int16_t)(L16 - e + wave);
            st.frontC[i] = (int16_t)(L16 + e + wave);
        } else {
            st.back[i] = (int16_t)(L16 + wave);
            st.frontC[i] = (int16_t)(L16 + wave);
        }
    }
}

struct LiquidP {
    const uint16_t* f;
    const uint16_t* pal;
    const uint16_t* palE;   // endpoint palette for the wave, or null
    int gy0, bias, mulShift, detX, detY;
    int level;              // int surface row
    int waveFront;          // endpoint wave distance
    int kry;                // ellipse aspect, for ripple rings
};

template <bool SLOW, bool WAVE>
static inline void liquidRow(State& st, const Surf& s, const SceneArt& a, int y, int xl, int xr, const LiquidP& L, const int8_t* ringRip) {
    const int warp = hg_isin(y * 5 + (int)(L.detY * 3)) >> 6;
    int X0 = xl + warp - GX0, X1 = xr + warp - GX0;
    if (X0 < 0) { xl -= X0; X0 = 0; }
    if (X1 > (NX - 1) * 4 - 1) { xr -= X1 - ((NX - 1) * 4 - 1); X1 = (NX - 1) * 4 - 1; }
    if (xr < xl) return;
    int gy = y - L.gy0;
    int fy = gy & 3;
    const uint16_t* f0 = L.f + (gy >> 2) * NX;
    const uint16_t* f1 = f0 + NX;
    uint16_t* rv = st.rv;
    int i0 = X0 >> 2, i1 = (X1 >> 2) + 1;
    for (int i = i0; i <= i1; i++) rv[i] = (uint16_t)(((uint32_t)f0[i] * (4 - fy) + (uint32_t)f1[i] * fy) >> 2);

    int width = xr - xl + 1;
    int stepS = (16 << 12) / (width > 0 ? width : 1);
    int ss = 0;
    uint16_t* o = s.at(xl, y);
    const uint8_t* smRow = st.sm + (((y + L.detY) & 63) << 6);
    const uint8_t* bay = BAY[y & 3];
    const uint8_t* rowB = st.rowB;
    const uint16_t* pal = L.pal;
    const int detX = L.detX, bias = L.bias, ms = L.mulShift;
    const int Y16 = y << 4;
    int x = xl, X = X0;
    while (x <= xr) {
        int i = X >> 2, fx = X & 3;
        int step = (int)rv[i + 1] - (int)rv[i];
        int acc = (int)rv[i] * 4 + step * fx;
        int n = 4 - fx;
        if (n > xr - x + 1) n = xr - x + 1;
        x += n;
        X += n;
        int xp = x - n;
        for (; n > 0; n--, xp++, o++, ss += stepS, acc += step) {
            int d = acc >> 10;
            int dt = (int)smRow[(xp + detX) & 63] - 128;
            int l = d + bias + bay[xp & 3] + (((d + 8) * dt) >> ms);
            int b = rowB[(ss + (bay[xp & 3] << 9)) >> 12];
            const uint16_t* use = pal;
            if (WAVE) {
                int adx = xp - a.cx;
                if (adx < 0) adx = -adx;
                int dd = (y - L.level) + (adx >> 1) + (dt >> 4) + (bay[xp & 3] >> 1);
                int edge = dd - L.waveFront;
                if (edge < 0) {
                    use = L.palE;
                    if (edge > -9) { l += 80 + edge * 8; b += 2; if (b > 15) b = 15; }  // hot front, fading behind it
                } else if (edge < 4) {
                    b -= 2; if (b < 0) b = 0;   // a dark halo just ahead of the front
                }
            }
            if (SLOW) {
                int col = xp - GX0;
                int back = st.back[col], fr = st.frontC[col];
                if (Y16 + 16 <= back) continue;  // air
                if (Y16 < fr) {
                    // the lit top of the liquid, ripple rings on it
                    int adx = xp - a.cx, ady = (y - L.level) * L.kry;
                    int d2 = (adx * adx + ady * ady) >> 5;
                    if (d2 > 255) d2 = 255;
                    b += 2 + ringRip[st.isq[d2]];
                    l += 6;
                    if (fr - Y16 <= 16) { b += 4; l += 18; }
                    if (b > 15) b = 15; else if (b < 0) b = 0;
                    if (l > 255) l = 255; else if (l < 0) l = 0;
                    uint16_t c = use[(b << 5) | (l >> 3)];
                    if (Y16 < back) *o = mix565(*o, c, (Y16 + 16 - back) * 16);
                    else *o = c;
                    continue;
                }
            }
            if (l > 255) l = 255; else if (l < 0) l = 0;
            *o = use[(b << 5) | (l >> 3)];
        }
    }
}

static void setRowB(State& st, const uint8_t* shape, int add) {
    for (int i = 0; i < 16; i++) {
        int v = shape[i] + add;
        st.rowB[i] = (uint8_t)(v < 0 ? 0 : v > 15 ? 15 : v);
    }
    st.rowB[16] = st.rowB[15];
}

// Sparse vapour: anisotropic noise (narrow streaks), thresholded, added on the air.
static void vapour(const State& st, const Surf& s, const SceneArt& a, int y0, int y1, const uint16_t* lut, uint32_t tick, int seed, uint16_t spill) {
    int h = y1 - y0;
    if (h <= 0) return;
    int scroll = (tick >> 1) + seed * 8;
    for (int y = y0; y < y1; y++) {
        int xl, xr;
        if (!art_inner(a, y, xl, xr)) continue;
        int fromSurf = y1 - y;
        int fade = 255 - fromSurf * 255 / h;   // strongest near the liquid
        if (fromSurf < 5) fade = fade * fromSurf / 5;
        int wob = (hg_isin(y * 6 + (tick >> 2) + seed) >> 3) + (hg_isin(y * 13 - (tick >> 1)) >> 4);
        // soft plumes stretched 4:1; rows interpolated so nothing looks stepped
        int ys = y * 4 + scroll;
        int r0 = (ys >> 4) & 63, r1 = (r0 + 1) & 63, fy = ys & 15;
        const uint8_t* ra = st.sm + (r0 << 6), *rb = st.sm + (r1 << 6);
        uint16_t* o = s.at(xl, y);
        uint16_t sp = 0;
        if (spill && fromSurf < 14) sp = scale565(spill, (14 - fromSurf) * (14 - fromSurf) / 3);
        for (int x = xl; x <= xr; x++, o++) {
            int xx = (x + wob) & 63;
            int n = (ra[xx] * (16 - fy) + rb[xx] * fy) >> 4;
            if (sp) *o = add565(*o, sp);
            if (n > 140) {
                int v = ((n - 140) * fade) >> 12;
                if (v > 15) v = 15;
                if (v) *o = add565(*o, lut[v]);
            }
        }
    }
}

// Thread segment, tapered and anti-aliased.
static void threadSeg(const Surf& s, int cx, float y0, float y1, float w0, float w1, uint16_t core, uint16_t edge, uint16_t halo) {
    if (y1 <= y0) return;
    int iy0 = (int)y0, iy1 = (int)y1;
    float inv = 1.0f / (y1 - y0);
    for (int y = iy0; y <= iy1; y++) {
        float t = (y - y0) * inv;
        float w = w0 + (w1 - w0) * t;   // half width beyond the centre pixel
        int wi = (int)w;
        int frac = (int)((w - wi) * 255);
        s.hspan(cx - wi, cx + wi, y, core);
        s.mix(cx - wi - 1, y, edge, frac);
        s.mix(cx + wi + 1, y, edge, frac);
        s.add(cx - wi - 1 - (frac > 128), y, halo);
        s.add(cx + wi + 1 + (frac > 128), y, halo);
    }
}

static void dropBody(const Surf& s, float cx, float cy, float rx, float ry, uint16_t rim, uint16_t core, uint16_t hot) {
    int y0 = (int)floorf(cy - ry), y1 = (int)ceilf(cy + ry);
    for (int y = y0; y <= y1; y++) {
        float v = (y + 0.5f - cy) / ry;
        if (v <= -1 || v >= 1) continue;
        float half = rx * sqrtf(1 - v * v);
        int x0 = (int)floorf(cx - half), x1 = (int)ceilf(cx + half);
        for (int x = x0; x <= x1; x++) {
            float dx = fabsf(x + 0.5f - cx);
            float cov = half + 0.5f - dx;
            if (cov <= 0) continue;
            float u = half > 0.5f ? dx / half : 0;
            uint16_t c = u < 0.55f ? core : rim;
            s.mix(x, y, c, cov >= 1 ? 255 : (int)(cov * 255));
        }
    }
    s.mix((int)(cx - rx * 0.35f), (int)(cy - ry * 0.35f), hot, 235);
}

static void bubbleDraw(const Surf& s, const Bubble& b, uint16_t rim, uint16_t core, uint16_t spec) {
    if (b.r < 1.2f) {
        s.mix((int)b.x, (int)b.y, spec, 170);
        return;
    }
    s.disc(b.x, b.y, b.r, rim, 150);
    if (b.r > 1.6f) s.disc(b.x, b.y, b.r - 1.0f, core, 150);
    s.mix((int)(b.x - b.r * 0.4f), (int)(b.y - b.r * 0.45f), spec, 240);
}

static void draw(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_chem;
    Surf& s = c.s;
    float p = c.progress;
    bool empty = p >= 0.998f;
    uint32_t t = c.tick;

    // cylinder shading: a dark edge, a broad lit belly, slightly offset to the left
    static const uint8_t SHAPE_U[16] = {9, 11, 13, 14, 15, 15, 15, 15, 15, 14, 14, 13, 13, 12, 11, 9};
    static const uint8_t SHAPE_L[16] = {8, 10, 12, 13, 14, 15, 15, 15, 14, 14, 13, 13, 12, 11, 10, 8};

    int levelU = (int)st.levelU, levelL = (int)st.levelL;

    // ---- wet film left on the funnel wall as it drains
    if (p > 0.004f) {
        int y0 = (int)st.vol.upperLevel(1.0f);
        uint16_t film = st.palU[(9 << 5) | 10];
        for (int y = y0; y < levelU + 4 && y < a.neck - 7; y++) {
            int xl, xr;
            if (!art_inner(a, y, xl, xr)) continue;
            int nl = 1 + (st.sm[(y * 2 & 63) * 64 + 5] >> 5), nr = 1 + (st.sm[(y * 2 & 63) * 64 + 37] >> 5);
            int am = 26 + (y - y0) * 80 / (levelU + 4 - y0 + 1);
            for (int k = 0; k < nl; k++) s.mix(xl + k, y, film, am - k * am / nl);
            for (int k = 0; k < nr; k++) s.mix(xr - k, y, film, am - k * am / nr);
        }
    }

    // ---- air above the reagent, with the glow spilling up
    if (!empty) {
        int h = levelU - a.top;
        if (h > 34) h = 34;
        vapour(st, s, a, levelU - h, levelU - 1, st.vapU, t, 0, st.glowU);
    }

    // ---- upper reagent
    if (!empty) {
        surfaceCurves(st, a, st.levelU, 0.07f, false, t);
        LiquidP L;
        L.f = st.fields[st.idUp];
        L.pal = st.palU;
        L.palE = nullptr;
        L.gy0 = a.top;
        L.bias = 48;
        L.mulShift = 8;
        L.detX = t >> 4;
        L.detY = -(int)(t >> 3);
        L.level = levelU;
        L.waveFront = 0;
        L.kry = 14;
        static const int8_t ZERO[80] = {0};
        float rr = art_radius(a, levelU + 1);
        int rowsS = (int)(rr * 0.07f) + 3;
        setRowB(st, SHAPE_U, 0);
        for (int y = levelU - rowsS; y <= a.neck - 7; y++) {
            int xl, xr;
            if (!art_inner(a, y, xl, xr)) continue;
            if (y <= levelU + rowsS) liquidRow<true, false>(st, s, a, y, xl, xr, L, ZERO);
            else liquidRow<false, false>(st, s, a, y, xl, xr, L, ZERO);
        }
    }

    // ---- stem and drip tip
    if (!empty && st.rewind <= 0) {
        uint16_t hot = st.palU[(15 << 5) | 25], mid = st.palU[(13 << 5) | 17];
        for (int y = a.neck - 8; y <= TIP_Y; y++) {
            int w = y <= a.neck + 7 ? 2 : 1;
            int ph = c.paused ? 0 : (int)(t >> 1);
            uint16_t m = st.sm[((y - ph) & 63) * 64 + 11] > 128 ? hot : mid;
            s.hspan(a.cx - w, a.cx + w, y, mid);
            s.put(a.cx, y, m);
        }
    }

    // ---- air and vapour over the analyte
    {
        int h = levelL - (TIP_Y + 2);
        if (h > 42) h = 42;
        vapour(st, s, a, levelL - h, levelL - 1, st.vapL, t, 21, 0);
    }

    // ---- ripple profile
    {
        int amp = (int)(st.ripAmp * 40);
        int front = (int)(st.ripT * 1.35f);
        for (int r = 0; r < 80; r++) {
            int v = 0;
            if (amp && r < front) {
                int env = 256 - (front - r) * 6;
                if (env > 0) v = (hg_isin(r * 22 - (int)(st.ripT * 20)) * amp * env) >> 19;
            }
            st.rip[r] = (int8_t)v;
        }
    }

    // ---- lower liquid
    {
        surfaceCurves(st, a, st.levelL, 0.095f, true, t);
        float eT = (p - 0.97f) / 0.03f;
        int dTotal = a.bot - levelL + 48;
        LiquidP L;
        L.f = st.fields[st.idLo];
        L.pal = st.palL;
        L.palE = st.palE;
        L.gy0 = a.neck;
        L.bias = 0;
        L.mulShift = 6;
        L.detX = t >> 3;
        L.detY = (int)(t >> 2);
        L.level = levelL;
        L.waveFront = (int)(eT * dTotal) - 10;
        L.kry = 10;
        bool wave = eT > 0 && eT < 1 && st.palEValid;
        if (eT >= 1 && st.palEValid) L.pal = st.palE;

        // rings in level units, for the lit surface
        int8_t ring[80];
        for (int r = 0; r < 80; r++) ring[r] = (int8_t)(st.rip[r] >> 2);

        float rr = art_radius(a, levelL + 1);
        int rowsS = (int)(rr * 0.095f) + 3;
        int y = levelL - rowsS;
        if (y < TIP_Y) y = TIP_Y;
        for (; y <= a.bot; y++) {
            int xl, xr;
            if (!art_inner(a, y, xl, xr)) continue;
            int depth = y - levelL;
            // a glow near the surface, darker deep in the bulb, lifted again by the caustic at the bottom
            int add = depth < 22 ? 0 : depth < 70 ? -((depth - 22) >> 5) : -1;
            setRowB(st, SHAPE_L, add);
            bool sl = y <= levelL + rowsS;
            if (wave) { if (sl) liquidRow<true, true>(st, s, a, y, xl, xr, L, ring); else liquidRow<false, true>(st, s, a, y, xl, xr, L, ring); }
            else { if (sl) liquidRow<true, false>(st, s, a, y, xl, xr, L, ring); else liquidRow<false, false>(st, s, a, y, xl, xr, L, ring); }
        }
    }

    // ---- bubbles
    {
        uint16_t rimU = st.palU[(15 << 5) | 28], coreU = st.palU[(6 << 5) | 6], specU = C565(250, 255, 235);
        uint16_t rimL = C565(226, 236, 255), coreL = st.palL[(11 << 5) | 2];
        for (int i = 0; i < MAX_BUB; i++) {
            const Bubble& b = st.bub[i];
            if (!b.on) continue;
            if (b.up) bubbleDraw(s, b, rimU, coreU, specU);
            else bubbleDraw(s, b, rimL, coreL, C565(255, 255, 255));
        }
    }

    // ---- drop and thread
    {
        uint16_t core = st.palU[(15 << 5) | 25], edge = st.palU[(14 << 5) | 16], rim = st.palU[(12 << 5) | 15];
        uint16_t hot = C565(250, 255, 232), halo = scale565(st.glowU, 90);
        int cx = a.cx;
        float T = TIP_Y + 1;
        if (st.stage == 1) {
            float r = st.dropR;
            threadSeg(s, cx, T - 1, st.dropY - r * 0.4f, 1.8f, r * 0.72f, core, edge, halo);
            dropBody(s, cx + 0.5f, st.dropY, r, r * 1.1f, rim, core, hot);
            s.glow(cx, (int)st.dropY, 8, st.glowU, (int)(r * 22));
        } else if (st.stage == 2) {
            float len = st.dropY - T;
            float wTop = 1.6f - len * 0.022f;
            if (wTop < 0.55f) wTop = 0.55f;
            float wMid = 1.0f - len * 0.016f;
            if (wMid < 0.28f) wMid = 0.28f;
            float mid = T + len * 0.55f;
            threadSeg(s, cx, T - 1, mid, wTop, wMid, core, edge, halo);
            threadSeg(s, cx, mid, st.dropY - st.dropR * 0.6f, wMid, st.dropR * 0.55f, core, edge, halo);
            dropBody(s, cx + 0.5f, st.dropY, st.dropR, st.dropR * 1.22f, rim, core, hot);
            s.glow(cx, (int)st.dropY, 9, st.glowU, 70);
        } else if (st.stage == 3) {
            if (st.stub > 0.6f) threadSeg(s, cx, T - 1, T + st.stub, 1.4f, 0.3f, core, edge, halo);
            if (st.tail > 0.6f) threadSeg(s, cx, st.dropY - st.dropR - st.tail, st.dropY - st.dropR * 0.5f, 0.3f, st.dropR * 0.5f, core, edge, halo);
            float stretch = 1.1f + st.dropV * 0.10f;
            dropBody(s, cx + 0.5f, st.dropY, st.dropR * (1.08f - st.dropV * 0.03f), st.dropR * stretch, rim, core, hot);
            s.glow(cx, (int)st.dropY, 9, st.glowU, 70);
        }
        // rewinding: a solid bright column racing back up
        if (st.rewind > 0) {
            float top = T - 1;
            threadSeg(s, cx, top, st.levelL, 0.4f, 0.4f + st.rewind, edge, rim, halo);
            // beads of reagent climbing it
            for (int k = 0; k < 4; k++) {
                float by = st.levelL - fmodf(c.tick * 9.0f + k * 37.0f, st.levelL - top);
                dropBody(s, cx + 0.5f, by, 1.6f, 2.6f, rim, core, hot);
            }
        }
        for (int i = 0; i < MAX_SPL; i++) {
            const Splash& sp = st.spl[i];
            if (sp.on) { s.disc(sp.x, sp.y, 0.9f, core, 230); }
        }
        if (st.flash > 0.02f) s.glow(cx, levelL + 3, 20, st.glowL, (int)(st.flash * 100));
    }

    // ---- the stopcock key: a grip that turns about the barrel when the flow is shut off
    {
        int gx = a.cx + 19;
        int hh = 11 - (int)(st.closeAnim * 8.0f + 0.5f);
        static const uint8_t SH[5] = {40, 170, 245, 150, 44};
        uint16_t tint = C565(214, 240, 232), edgeC = C565(10, 20, 18);
        for (int y = a.neck - hh; y <= a.neck + hh; y++) {
            int ad = y < a.neck ? a.neck - y : y - a.neck;
            int cap = ad == hh ? 90 : ad == hh - 1 ? 220 : 255;
            int tall = ad > 3 ? 1 : 0;   // the stalk is thin, the wings flare
            for (int k = 0; k < 5; k++) {
                if (tall && (k == 0)) continue;
                uint16_t c = mix565(edgeC, tint, SH[k]);
                s.mix(gx + k, y, c, cap);
            }
        }
    }
}

}  // namespace chem

static const Scene SCENE_chem = {
    "Titration", "One drop from the endpoint.", &ART_chem,
    {C565(236, 252, 236), C565(164, 196, 172), C565(150, 240, 74), C565(96, 160, 255), C565(36, 60, 48), C565(8, 22, 10), C565(255, 92, 120)},
    {120, 255, 60},
    chem::init, chem::tick, chem::draw,
};
