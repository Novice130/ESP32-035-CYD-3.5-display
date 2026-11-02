// ==============================================================================
//  Clepsydra - a blown-glass water clock at a rainy window.
//
//  Water is seen through, not painted: each water pixel samples the background
//  flipped and minified the way a filled flask does, tints it and adds real
//  caustics (two photon-traced tables scrolled against each other). Every 1.2 s a
//  drop swells at the spout, necks, pinches off with a satellite, falls wobbling
//  and acts as its own tiny lens. On impact: crown, Worthington jet, entrained
//  bubbles and a radial 1D wave whose rings run out and reflect from the glass.
// ==============================================================================
#pragma once
#include "../hg_core.h"
#include "../art/water.h"

namespace water {

enum { CELLS = 74, COLS = 148, PARTS = 28, TRICKLES = 3, ROWS = 274, TIP = 163, SQ_LEN = 1370 };

struct Wave { int16_t h[CELLS], v[CELLS]; int n; };
enum { P_OFF, P_DROPLET, P_BUBBLE, P_SAT };
struct Part { float x, y, vx, vy, baseY; uint8_t kind, age, upper; float r; };
struct Falling { float y, vy, r, age; bool on; };
struct Trickle { float x, y, v; int16_t len; uint8_t phase; };

struct State {
    uint8_t caustic[128 * 128];         // copied from flash: random reads are too slow there
    uint8_t sq[SQ_LEN];                 // sq[d2 >> 3] = distance
    uint8_t ell[65];                    // sqrt(1-u^2) * 255
    uint8_t glint[COLS];
    GlassVolume vol;
    Wave up, lo;
    Part parts[PARTS];
    Falling drop[2];
    Trickle tr[TRICKLES];
    int32_t edgeB[COLS], edgeF[COLS];   // surface back / front edge per column, y * 256
    uint16_t stepU[ROWS], lensK[ROWS];  // per row: 16 shade columns across, source x step (8.8)
    int16_t srcY[ROWS];
    int16_t inv[CELLS];
    uint32_t tint[8][16];               // spread565 * (32 - trans)
    uint8_t trans[16];
    uint16_t ca[4][16];                 // caustic ramp per depth group
    uint16_t surf[32];
    uint16_t dropTint, dropGlow, dropHot, bubble;
    int dropMix;
    float brk, lastP, upLevel, loLevel, upR, loR;
    float t;                            // growth of the hanging drop 0..1
    float period, finalR, recoil;
    int jetAge, jetLife;
    float jetH;
    bool jetShot;
    int energy;                         // 0..255, caustics and wobble follow the impact
    int rush;
    int sinceBubble;
    int impactFlash;
    int crownAge;                       // sheet thrown up around the impact
    uint8_t crownSeed;
};
static_assert(sizeof(State) <= SCENE_ARENA_BYTES, "");

static inline uint16_t rgb(const uint8_t* c) { return C565(c[0], c[1], c[2]); }
static uint16_t lerp3(const uint8_t* a, const uint8_t* b, int t) { return mix565(rgb(a), rgb(b), t); }

// focus: moonlit clear water. break: warm dawn rose-gold.
static const uint8_t F_TOP[3] = {34, 168, 186}, F_DEEP[3] = {10, 84, 128}, F_EDGE[3] = {4, 34, 70};
static const uint8_t B_TOP[3] = {255, 178, 128}, B_DEEP[3] = {232, 112, 100}, B_EDGE[3] = {128, 38, 58};
static const uint8_t F_CA[3] = {170, 240, 250}, B_CA[3] = {255, 214, 160};
static const uint8_t F_S0[3] = {6, 44, 66}, F_S1[3] = {56, 158, 178}, F_S2[3] = {240, 255, 255};
static const uint8_t B_S0[3] = {96, 34, 40}, B_S1[3] = {236, 142, 108}, B_S2[3] = {255, 246, 224};
static const uint8_t F_DROP[3] = {120, 226, 240}, B_DROP[3] = {255, 196, 150};

static void buildPalette(State& st, float brk) {
    int b = (int)(brk * 255);
    uint16_t top = lerp3(F_TOP, B_TOP, b), deep = lerp3(F_DEEP, B_DEEP, b), edge = lerp3(F_EDGE, B_EDGE, b);
    uint16_t cac = lerp3(F_CA, B_CA, b);
    // transmission across the bulb: clear in the middle, absorbing at the rim
    static const uint8_t TR[16] = {6, 9, 13, 16, 18, 19, 20, 20, 20, 20, 19, 18, 16, 13, 9, 6};
    static const uint8_t SH[16] = {255, 190, 120, 70, 30, 8, 0, 0, 6, 16, 34, 64, 104, 150, 200, 255}; // to edge colour
    // rose water is denser: the teal night behind must not muddy it
    int trk = 256 - (b * 150 >> 8);
    for (int u = 0; u < 16; u++) st.trans[u] = (uint8_t)((TR[u] * trk) >> 8);
    for (int d = 0; d < 8; d++) {
        uint16_t base = mix565(top, deep, d * 36);
        for (int u = 0; u < 16; u++) {
            uint16_t col = mix565(base, edge, SH[u]);
            st.tint[d][u] = spread565(col) * (32 - st.trans[u]);
        }
    }
    for (int g = 0; g < 4; g++)
        for (int i = 0; i < 16; i++) {
            // eased: the faintest steps of 5-bit colour would read as flat patches
            int k = (i + 1) * (i + 1) * (44 + g * 18) >> 8;
            int r = (((cac >> 11) & 31) * k + 128) >> 8, gr = (((cac >> 5) & 63) * k + 128) >> 8, bl = ((cac & 31) * k + 128) >> 8;
            st.ca[g][i] = (uint16_t)((r > 31 ? 31 : r) << 11 | (gr > 63 ? 63 : gr) << 5 | (bl > 31 ? 31 : bl));
        }
    uint16_t s0 = lerp3(F_S0, B_S0, b), s1 = lerp3(F_S1, B_S1, b), s2 = lerp3(F_S2, B_S2, b);
    for (int i = 0; i < 32; i++)
        st.surf[i] = i < 13 ? mix565(s0, s1, i * 255 / 13) : mix565(s1, s2, (i - 13) * 255 / 18);
    st.dropTint = lerp3(F_DROP, B_DROP, b);
    st.dropMix = 60 + (b * 110 >> 8);
    st.dropGlow = scale565(st.dropTint, 150);
    st.dropHot = mix565(st.dropTint, 0xFFFF, 200);
    st.bubble = mix565(st.dropTint, 0xFFFF, 110);
    st.brk = brk;
}

static void buildTables(State& st) {
    const SceneArt& a = ART_water;
    memcpy(st.caustic, ART_water_caustic, sizeof(st.caustic));
    for (int i = 0; i < SQ_LEN; i++) { int v = (int)sqrtf((float)(i * 8 + 4)); st.sq[i] = (uint8_t)(v > 255 ? 255 : v); }
    for (int i = 0; i <= 64; i++) st.ell[i] = (uint8_t)(sqrtf(1.0f - (i * i) / 4096.0f) * 255);
    for (int i = 0; i < CELLS; i++) st.inv[i] = i ? (int16_t)(128 / i) : 0;
    const float upperC = (a.top + a.neck) * 0.5f, lowerC = (a.neck + a.bot) * 0.5f;
    for (int i = 0; i < ROWS; i++) {
        int y = a.dy + i;
        float r = art_radius(a, y);
        if (r < 1) r = 1;
        st.stepU[i] = (uint16_t)((16 << 8) / (2 * r + 1));
        float k = 94.0f / r;
        if (k > 3.0f) k = 3.0f;
        st.lensK[i] = (uint16_t)(k * 256);
        float c = y <= a.neck ? upperC : lowerC;
        st.srcY[i] = (int16_t)(c - (y - c) * 0.9f);
    }
}

// ------------------------------------------------------------------ waves
static inline void kick(Wave& w, int at, int width, int amp) {
    for (int i = at - width; i <= at + width; i++) {
        int j = i < 0 ? -i : i;
        if (j >= w.n) continue;
        int k = i - at, f = width * width + 1 - k * k;
        w.v[j] -= (int16_t)(amp * f / (width * width + 1));
    }
}

static void stepWave(const State& st, Wave& w, int substeps, int c2, int damp) {
    const int n = w.n;
    for (int s = 0; s < substeps; s++) {
        for (int i = 1; i < n - 1; i++) {
            int d = w.h[i + 1] - w.h[i - 1];
            int lap = w.h[i - 1] + w.h[i + 1] - 2 * w.h[i] + ((d * st.inv[i]) >> 8);
            int v = w.v[i] + ((lap * c2) >> 8);
            v -= v >> damp;
            w.v[i] = (int16_t)v;
        }
        w.v[0] += (int16_t)((2 * (w.h[1] - w.h[0]) * c2) >> 8);
        w.v[0] -= w.v[0] >> damp;
        // the glass wall: free reflecting end
        w.v[n - 1] += (int16_t)(((w.h[n - 2] - w.h[n - 1]) * c2) >> 8);
        w.v[n - 1] -= w.v[n - 1] >> damp;
        for (int i = 0; i < n; i++) {
            int h = w.h[i] + w.v[i];
            h -= h >> 9;
            if (h > 900) h = 900;
            if (h < -900) h = -900;
            w.h[i] = (int16_t)h;
        }
    }
}

// ------------------------------------------------------------------ particles
static Part* newPart(State& st) {
    for (int i = 0; i < PARTS; i++) if (!st.parts[i].kind) return &st.parts[i];
    return nullptr;
}

static void addBubble(State& st, float x, float y, bool upper, float r) {
    Part* p = newPart(st);
    if (!p) return;
    *p = Part{x, y, 0, -0.32f - hg_randf() * 0.3f, y, P_BUBBLE, (uint8_t)(hg_rand() & 255), (uint8_t)upper, r};
}

static void impact(SceneCtx& c, State& st, float r) {
    const SceneArt& a = ART_water;
    float y = st.loLevel;
    if (y > a.bot - 2) y = a.bot - 2;
    kick(st.lo, 0, 5, (int)(r * 26));
    float ry = st.loR * 0.14f, kz = ry / (st.loR + 1);
    int crown = 10 + (hg_rand() & 3);
    int off = hg_rand() & 255;
    for (int i = 0; i < crown; i++) {
        Part* p = newPart(st);
        if (!p) break;
        int ph = off + i * 256 / crown + (hg_rand() & 7);
        float cs = hg_icos(ph) / 127.0f, sn = hg_isin(ph) / 127.0f;
        float rc = 3.5f + hg_randf();
        float sp = 0.7f + hg_randf() * 1.1f;
        float rise = 1.9f + hg_randf() * 2.3f;
        float pr = 0.55f + hg_randf() * hg_randf() * 1.15f;
        float x = a.cx + cs * rc, base = y + sn * rc * kz * 2.0f;
        *p = Part{x, base, cs * sp, -rise, base, P_DROPLET, 0, 0, pr};
    }
    // air dragged under by the drop
    float room = a.bot - y;
    if (room > 16) {
        int n = 1 + (hg_rand() & 1) + ((hg_rand() & 3) == 0);
        for (int i = 0; i < n; i++)
            addBubble(st, a.cx + hg_rands() * 6, y + 8 + hg_randf() * (room > 34 ? 18 : room - 12), false, 0.9f + hg_randf() * 1.1f);
    }
    // the cavity rebounds into a jet, sometimes tall enough to throw a bead
    st.crownAge = 0;
    st.crownSeed = (uint8_t)off;
    st.jetAge = -7;
    st.jetLife = 15 + (hg_rand() & 7);
    st.jetH = room > 8 ? 7 + hg_randf() * hg_randf() * 21 : 0;
    st.jetShot = false;
    st.energy = 255;
    st.impactFlash = 6;
    if (st.lastP < 0.998f && !c.paused) {
        // the fuller the vessel, the shorter the air column and the higher the note
        hg_drip((uint16_t)(900 + st.lastP * 800));
    }
}

static void init(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_water;
    st.vol.build(a);
    buildTables(st);
    buildPalette(st, c.breakMix);
    st.lastP = c.progress;
    st.upLevel = st.vol.upperLevel(1 - c.progress);
    st.loLevel = st.vol.lowerLevel(c.progress);
    st.t = 0.45f;
    st.period = 36;
    st.finalR = 6.2f;
    st.jetAge = 1000;
    st.crownAge = 100;
    st.up.n = st.lo.n = 40;
    for (int i = 0; i < TRICKLES; i++) {
        st.tr[i] = Trickle{(float)(a.dx + 8 + (hg_rand() % (a.dw - 16))), (float)(a.dy + (hg_rand() % a.dh)), 0, (int16_t)(16 + (hg_rand() & 31)), (uint8_t)(hg_rand() & 255)};
    }
}

static inline float upperSurf(const State& st) { return st.upLevel; }
static inline float lowerSurf(const State& st) { const SceneArt& a = ART_water; return st.loLevel > a.bot - 2 ? a.bot - 2 : st.loLevel; }

// The hanging drop's shape as a function of its growth.
struct Pendant { float cy, r, stemTip, stemMin; };
static inline Pendant pendant(const State& st, float recoilWob) {
    float t = st.t, g = t / 0.74f;
    if (g > 1) g = 1;
    g = g * g * (3 - 2 * g);
    float r = 2.0f + (st.finalR - 2.0f) * g;
    float n = t < 0.74f ? 0 : (t - 0.74f) / 0.26f;
    Pendant p;
    p.r = r + recoilWob * 0.5f;
    p.cy = TIP + r * 0.74f + 1.0f + n * n * 11.0f + recoilWob;
    p.stemTip = 2.9f;
    p.stemMin = (1.0f - n) * (1.0f - n) * 2.4f;
    return p;
}

static void tick(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_water;
    if (fabsf(c.breakMix - st.brk) > 0.01f || (c.breakMix != st.brk && (c.breakMix == 0 || c.breakMix == 1))) buildPalette(st, c.breakMix);

    float p = c.progress;
    float dp = p - st.lastP;
    if (dp != 0) {
        st.upLevel = st.vol.upperLevel(1 - p);
        st.loLevel = st.vol.lowerLevel(p);
    }
    st.lastP = p;
    if (dp < -0.0015f) st.rush = 12;
    else if (st.rush > 0) st.rush--;

    int ly = (int)st.loLevel;
    if (ly > a.bot - 1) ly = a.bot - 1;
    st.loR = art_radius(a, ly);
    int uy = (int)st.upLevel + 1;
    if (uy > a.neck) uy = a.neck;
    st.upR = art_radius(a, uy);
    int nl = (int)st.loR + 1, nu = (int)st.upR + 1;
    st.lo.n = nl < 3 ? 3 : nl > CELLS ? CELLS : nl;
    st.up.n = nu < 3 ? 3 : nu > CELLS ? CELLS : nu;

    // ---- the charge rushes back at a restart
    if (st.rush) {
        kick(st.up, hg_rand() % st.up.n, 4, 7);
        kick(st.lo, hg_rand() % st.lo.n, 4, 10);
        st.energy = 200;
        if (Part* q = newPart(st)) *q = Part{a.cx + hg_rands() * 2, st.loLevel - 2, hg_rands() * 0.2f, -9 - hg_randf() * 3, 0, P_SAT, 0, 2, 0.8f + hg_randf()};
    }

    // ---- hanging drop
    bool flowing = !c.paused && p < 0.998f && !st.rush;
    if (st.recoil > 0.01f) st.recoil *= 0.90f;
    if (flowing) {
        st.t += 1.0f / st.period;
        if (st.t >= 1.0f) {
            Pendant pd = pendant(st, 0);
            for (int i = 0; i < 2; i++) {
                if (!st.drop[i].on) { st.drop[i] = Falling{pd.cy, 1.1f, pd.r, 0, true}; break; }
            }
            if (Part* q = newPart(st)) *q = Part{(float)a.cx + hg_rands() * 0.2f, pd.cy - pd.r - 3.5f, 0, 0.75f, 0, P_SAT, 0, 0, 1.15f + hg_randf() * 0.45f};
            st.t = 0;
            st.recoil = 3.6f;
            st.period = 33.0f + hg_randf() * 7.0f;
            st.finalR = 5.7f + hg_randf() * 1.0f;
            kick(st.up, 0, 6, 9);
            // the gulp: now and then a bubble climbs through the upper bulb
            if (++st.sinceBubble >= 2 + (int)(hg_rand() & 1) && st.upLevel < a.neck - 16) {
                st.sinceBubble = 0;
                addBubble(st, a.cx + hg_rands(), a.neck - 13, true, 0.9f + hg_randf() * 1.0f);
            }
        }
    }

    // ---- falling drops
    float surf = lowerSurf(st);
    for (int i = 0; i < 2; i++) {
        Falling& d = st.drop[i];
        if (!d.on) continue;
        d.vy += 0.235f;
        d.y += d.vy;
        d.age += 1;
        if (d.y + d.r * 0.8f >= surf) {
            d.on = false;
            impact(c, st, d.r);
        }
    }

    // ---- crown, satellites, bubbles
    for (int i = 0; i < PARTS; i++) {
        Part& q = st.parts[i];
        if (!q.kind) continue;
        if (q.age < 255) q.age++;
        if (q.kind == P_BUBBLE) {
            q.vy -= 0.006f;
            if (q.vy < -1.1f) q.vy = -1.1f;
            q.y += q.vy;
            q.x += hg_isin(q.age * 11) * 0.0028f;
            float top = q.upper ? upperSurf(st) : surf;
            float rr = art_radius(a, (int)q.y) - q.r - 1.5f;
            if (fabsf(q.x - a.cx) > rr && rr > 0) q.x = a.cx + (q.x > a.cx ? rr : -rr);
            if (q.y <= top + q.r) {
                Wave& w = q.upper ? st.up : st.lo;
                int at = (int)fabsf(q.x - a.cx);
                kick(w, at, 2, 12);
                q.kind = P_OFF;
            }
            continue;
        }
        q.vy += 0.235f;
        q.x += q.vx;
        q.y += q.vy;
        if (q.upper == 2) {  // rush back: rising drops die in the neck
            if (q.y < a.neck) q.kind = P_OFF;
            continue;
        }
        float rr = art_radius(a, (int)q.y) - 1;
        if (rr > 0 && fabsf(q.x - a.cx) > rr) { q.vx *= -0.5f; q.x = a.cx + (q.x > a.cx ? rr : -rr); }
        if (q.vy > 0 && q.y >= (q.kind == P_DROPLET ? q.baseY : surf)) {
            int at = (int)fabsf(q.x - a.cx);
            kick(st.lo, at, 2, (int)(q.r * (q.kind == P_SAT ? 22 : 9)));
            q.kind = P_OFF;
        }
    }

    if (st.crownAge < 100) st.crownAge++;
    // ---- jet
    if (st.jetAge < st.jetLife) {
        st.jetAge++;
        if (st.jetH > 16 && !st.jetShot && st.jetAge > st.jetLife * 2 / 5) {
            st.jetShot = true;
            if (Part* q = newPart(st)) *q = Part{(float)a.cx, surf - st.jetH * 0.95f, hg_rands() * 0.12f, -1.0f - hg_randf() * 0.8f, surf, P_DROPLET, 40, 0, 1.3f + hg_randf() * 0.7f};
        }
    }

    // ---- surfaces: always a little alive
    if ((c.tick & 15) == 0) kick(st.up, hg_rand() % st.up.n, 5, 3);
    if ((c.tick & 31) == 7) kick(st.lo, hg_rand() % st.lo.n, 5, 3);
    stepWave(st, st.lo, 3, 88, 8);
    stepWave(st, st.up, 2, 80, 7);
    if (st.energy > 0) { st.energy -= 8; if (st.energy < 0) st.energy = 0; }
    if (st.impactFlash > 0) st.impactFlash--;

    // ---- rain on the pane behind the glass: stop, go, stop
    for (int i = 0; i < TRICKLES; i++) {
        Trickle& t = st.tr[i];
        t.phase++;
        int m = hg_isin(t.phase * 3) + hg_isin(t.phase * 7 + 40) / 2;
        float target = m > 40 ? 0.9f : 0.03f;
        t.v += (target - t.v) * 0.12f;
        t.y += t.v;
        t.x += hg_isin(t.phase * 2) * 0.0009f * t.v * 8;
        if (t.y > a.dy + a.dh + t.len) {
            t.y = a.dy - 4;
            t.x = a.dx + 8 + (hg_rand() % (a.dw - 16));
            t.len = (int16_t)(14 + (hg_rand() & 31));
        }
    }
}

// ------------------------------------------------------------------ water body
// Row loop. Each pixel: flipped background * transmission + tint, plus thresholded caustics.
static void bodyRows(const SceneCtx& c, const State& st, int y0, int y1, float level, int checkUntil, int wobAmp) {
    const SceneArt& a = ART_water;
    const Surf& s = c.s;
    const uint8_t* T = st.caustic;
    const int bx0 = a.cx - COLS / 2;
    const int tk = (int)c.tick;
    const int ox1 = (tk * 3) >> 2, oy1 = (tk * 5) >> 3;
    const int ox2 = 37 - ((tk * 5) >> 3), oy2 = 91 + ((tk * 3) >> 3);
    const int boost = st.energy >> 3;
    const int levI = (int)level;
    for (int y = y0; y <= y1; y++) {
        int xl, xr;
        if (!art_inner(a, y, xl, xr)) continue;
        int row = y - a.dy;
        if ((unsigned)row >= ROWS) continue;
        int dp = (y - levI) >> 4;
        if (dp < 0) dp = 0;
        if (dp > 7) dp = 7;
        const uint32_t* ti = st.tint[dp];
        const uint16_t* ca = st.ca[dp >> 1];
        const int K = st.lensK[row] * 256;  // 16.16
        int sx = (a.cx << 16) - (xl - a.cx) * K + (hg_isin(y * 7 + tk * 4) * wobAmp);
        const uint16_t* src = art_bg_row(a, st.srcY[row]);
        int ss = 0;
        const int su = st.stepU[row];
        const int wob = HG_SIN[(y * 4 + tk * 2) & 255] >> 4;
        int x1 = xl + ox1 + wob, x2 = -xl + ox2 + wob / 2;
        const int y1w = ((y + oy1) & 127) << 7, y2w = ((((y * 3) >> 2) + oy2) & 127) << 7;
        uint16_t* o = s.at(xl, y);
        const bool check = y <= checkUntil;
        const int yFix = (y + 1) << 8;
        uint16_t base = 0;
        // where the lens squeezes the view hard, neighbours differ too much to share a sample
        const bool fine = check || st.lensK[row] > 400;
        if (!fine) {
            base = pack565((spread565(src[sx >> 16]) * st.trans[0] + ti[0]) >> 5);
        }
        for (int x = xl; x <= xr; x++, o++, x1++, x2--, ss += su, sx -= K) {
            if (check && yFix <= st.edgeF[x - bx0]) continue;
            if (fine || !((x ^ y) & 1)) {  // the backdrop is soft: half the flash reads, checkerboarded
                int u = ss >> 8;
                base = pack565((spread565(src[sx >> 16]) * st.trans[u] + ti[u]) >> 5);
            }
            uint16_t px = base;
            int lv = T[y1w | (x1 & 127)] + T[y2w | (x2 & 127)] + boost - 60 + (((x ^ y) & 1) << 3);
            if (lv > 0) {
                lv >>= 5;
                px = add565(px, ca[lv > 15 ? 15 : lv]);
            }
            *o = px;
        }
    }
}

// Glass above the waterline catches the dancing light thrown off the surface.
static void wallCaustics(const SceneCtx& c, const State& st, int yBottom) {
    const SceneArt& a = ART_water;
    const Surf& s = c.s;
    const uint8_t* T = st.caustic;
    const int tk = (int)c.tick;
    const int ox1 = (tk * 3) >> 2, oy1 = (tk * 5) >> 3, ox2 = 37 - ((tk * 5) >> 3), oy2 = 91 + ((tk * 3) >> 3);
    for (int y = yBottom - 34; y < yBottom; y++) {
        int xl, xr;
        if (y <= a.neck + 10 || !art_inner(a, y, xl, xr)) continue;
        int width = (xr - xl) >> 3;
        if (width < 2) continue;
        int fade = (y - (yBottom - 34)) * 6;
        const int es = 256 / width;
        for (int side = 0; side < 2; side++) {
            int x0 = side ? xr - width : xl, x1 = side ? xr : xl + width;
            for (int x = x0; x <= x1; x++) {
                int edge = (side ? x - x0 : x1 - x) * es;    // 0 away from the wall ... 256 at the wall
                int l1 = T[(((y + oy1) & 127) << 7) | ((x * 2 + ox1) & 127)], l2 = T[((((y * 2) + oy2) & 127) << 7) | ((ox2 - x * 2) & 127)];
                int l = ((l1 + l2) >> 1) - 22;
                if (l <= 0) continue;
                int amp = (((l * edge) >> 8) * fade) >> 8;
                if (amp > 3) *s.at(x, y) = add565(*s.at(x, y), scale565(st.ca[1][15], amp > 255 ? 255 : amp * 2));
            }
        }
    }
}

// Surface of a bulb: squashed ellipse, meniscus, ripple rings driven by the wave. isUpper: seen from below.
static void surfaceEdges(const SceneCtx& c, State& st, const Wave& w, float level, float R, bool isUpper, float ry, float slosh) {
    const SceneArt& a = ART_water;
    const int bx0 = a.cx - COLS / 2;
    static const uint8_t MEN[6] = {200, 132, 80, 44, 18, 5};
    int levF = (int)(level * 256);
    int ri = (int)R;
    int ry8 = (int)(ry * 256);
    int k = ri > 0 ? (64 << 8) / (ri + 1) : 0;
    int tiltPh = (int)(c.tick * 5) >> 1, tilt2 = (int)(c.tick * 7) >> 1;
    int sl = (int)(slosh * 256);
    int s1 = (hg_isin(tiltPh) * sl) >> 7, s2 = (hg_isin(tilt2 + 60) * sl) >> 8;
    for (int i = 0; i < COLS; i++) { st.edgeB[i] = levF; st.edgeF[i] = levF; st.glint[i] = 0; }
    int gc = isUpper ? (int)(R * 0.25f) : -(int)(R * 0.32f);
    int gw = (int)(R * 0.16f) + 2;
    for (int dx = -ri; dx <= ri; dx++) {
        int ix = dx + a.cx - bx0;
        if ((unsigned)ix >= COLS) continue;
        int ad = dx < 0 ? -dx : dx;
        int e = st.ell[(ad * k) >> 8 > 64 ? 64 : (ad * k) >> 8];
        int dist = ri - ad;
        int men = dist < 6 ? (MEN[dist] * 3) : 0;          // up to ~2.3 px climb at the wall
        int h = (ad < w.n ? w.h[ad] : 0) * 2;              // h in 1/64 px, positive = up
        int su = (dx * ((k * 4) >> 2)) >> 6;               // -256..256 approx
        int sl1 = (s1 * su) >> 8;
        int sl2 = (s2 * ((su * su >> 8) - 128)) >> 8;
        int cen = levF - h + sl1 + sl2;
        int span = (ry8 * e) >> 8;
        st.edgeB[ix] = cen - span - men;
        st.edgeF[ix] = cen + span - men;
        int gd = dx - gc;
        if (gd < 0) gd = -gd;
        st.glint[ix] = gd < gw ? (uint8_t)(255 - gd * 255 / gw) : 0;
    }
}

static void surfaceDraw(const SceneCtx& c, const State& st, const Wave& w, float level, float R, bool isUpper, float ry) {
    const SceneArt& a = ART_water;
    const Surf& s = c.s;
    const int bx0 = a.cx - COLS / 2;
    int ri = (int)R;
    if (ri < 1) return;
    int levF = (int)(level * 256);
    int inv = (int)(R * 256 / (ry > 0.5f ? ry : 0.5f));   // screen dy -> real radius, 8.8
    int baseI = isUpper ? 15 : 12;
    int n1 = w.n - 1;
    // columns this far in can never touch the glass
    float rA = art_radius(a, (int)(level - ry) - 3), rB = art_radius(a, (int)(level + ry) + 3);
    int safe = (int)(rA < rB ? rA : rB) - 2;
    for (int dx = -ri; dx <= ri; dx++) {
        int x = a.cx + dx;
        const bool near = (dx < 0 ? -dx : dx) > safe;
        int ix = x - bx0;
        int b = st.edgeB[ix], f = st.edgeF[ix];
        int yb = b >> 8, yf = f >> 8;
        int g = st.glint[ix];
        int fx = (x << 4) + 8;
        for (int y = yb; y <= yf; y++) {
            if (near && (y < a.top || y > a.bot || fx < a.gl[y] || fx > a.gr[y])) continue;
            int dyf = (y << 8) + 128 - levF;
            int dyr = (dyf * inv) >> 16;
            int d2 = dx * dx + dyr * dyr;
            int di = d2 >> 3;
            int d = st.sq[di >= SQ_LEN ? SQ_LEN - 1 : di];
            if (d >= n1) d = n1 - 1;
            if (d < 1) d = 1;
            int slope = w.h[d + 1] - w.h[d - 1] + w.h[d + 2 < w.n ? d + 2 : d + 1] - w.h[d >= 2 ? d - 2 : 0];
            int sgn = dyf < 0 ? slope : -slope;     // the side of each ring that faces the moon
            int i = baseI + (sgn >> 3) + (dyf < 0 ? 2 : 0);
            int spark = (slope < 0 ? -slope : slope) >> 2;
            {
                int ev = dyf < 0 ? -dyf : dyf;
                int fall = 256 - ((ev >> 2) * (inv >> 4) >> 8);
                if (fall > 0) i += (g * ((14 + spark) * fall >> 8)) >> 8;
            }
            if (i < 0) i = 0;
            if (i > 31) i = 31;
            int al = isUpper ? 175 : 190;
            if (y == yb) al = (al * (256 - (b & 255))) >> 8;
            else if (y == yf) al = (al * (f & 255)) >> 8;
            uint16_t* p = s.at(x, y);
            *p = mix565(*p, st.surf[i], al);
        }
        // the lit lines where the surface meets the glass, front and back
        // (split over two rows by the sub-pixel position, so a passing wave stays smooth)
        int rimF = st.surf[isUpper ? 22 : 27], fr = f & 255;
        if (yf >= a.top && yf < a.bot && fx >= a.gl[yf] && fx <= a.gr[yf]) {
            uint16_t* p = s.at(x, yf);
            *p = mix565(*p, rimF, ((256 - fr) * 170) >> 8);
            if (fx >= a.gl[yf + 1] && fx <= a.gr[yf + 1]) { p = s.at(x, yf + 1); *p = mix565(*p, rimF, (fr * 170) >> 8); }
        }
        int rimB = st.surf[isUpper ? 29 : 22], br = b & 255;
        if (yb > a.top && yb <= a.bot && fx >= a.gl[yb] && fx <= a.gr[yb]) {
            uint16_t* p = s.at(x, yb);
            *p = mix565(*p, rimB, ((256 - br) * 110) >> 8);
        }
    }
}

static void drawBulb(const SceneCtx& c, State& st, Wave& w, float level, float R, bool isUpper, int yEnd) {
    const SceneArt& a = ART_water;
    if (level >= yEnd - 0.5f || R < 1) return;
    float dist = isUpper ? a.neck - level : level - a.neck;
    float ry = R * dist * 0.00175f;
    if (ry > R * 0.17f) ry = R * 0.17f;
    if (ry < 0.6f) ry = 0.6f;
    float slosh = isUpper ? 1.5f + st.rush * 0.3f : 0.35f + st.energy * 0.002f;
    surfaceEdges(c, st, w, level, R, isUpper, ry, slosh);
    int wob = isUpper ? 180 : 180 + st.energy * 3;
    int top = (int)(level - ry) - 4;
    int check = (int)(level + ry) + 5;
    bodyRows(c, st, top, yEnd, level, check, wob);
    surfaceDraw(c, st, w, level, R, isUpper, ry);
}

// ------------------------------------------------------------------ the hero: a lens of water
// One pixel of the drop. nx, ny in -1..1 over the sphere. Colour comes from the whole window, upside down.
static inline uint16_t lensPixel(const State& st, float nx, float ny, float d2) {
    const SceneArt& a = ART_water;
    float bulge = 1.0f + 0.35f * d2;
    int sx = a.cx - (int)(nx * 92 * bulge);
    int sy = 150 - (int)(ny * 118 * bulge);
    if (sx < 0) sx = 0;
    if (sx > SCR_W - 1) sx = SCR_W - 1;
    if (sy < 2) sy = 2;
    if (sy > 288) sy = 288;
    uint16_t px = art_bg_row(a, sy)[sx];
    px = mix565(px, st.dropTint, st.dropMix);
    // a real sphere: dark band just inside the rim, lit crescent where light collects at the far side
    if (d2 > 0.5f) {
        int k = (int)((d2 - 0.5f) * 2.0f * 225);
        px = scale565(px, 255 - k);
    }
    if (ny > 0.15f) {
        float cres = ny * (1.0f - fabsf(d2 - 0.62f) * 3.2f);
        if (cres > 0) px = add565(px, scale565(st.dropTint, (int)(cres * 330) > 255 ? 255 : (int)(cres * 330)));
    }
    // the moon, a hot point upper left
    float hx = nx + 0.36f, hy = ny + 0.40f;
    float h2 = hx * hx + hy * hy * 1.25f;
    if (h2 < 0.09f) px = mix565(px, 0xFFFF, (int)((1.0f - h2 / 0.09f) * 255));
    return px;
}

static void drawLensDrop(const SceneCtx& c, const State& st, float cx, float cy, float rx, float ry) {
    const Surf& s = c.s;
    int x0 = (int)floorf(cx - rx - 1), x1 = (int)ceilf(cx + rx);
    int y0 = (int)floorf(cy - ry - 1), y1 = (int)ceilf(cy + ry);
    float rm = rx < ry ? rx : ry;
    for (int y = y0; y <= y1; y++) {
        float ny = (y + 0.5f - cy) / ry;
        for (int x = x0; x <= x1; x++) {
            if (!s.in(x, y)) continue;
            float nx = (x + 0.5f - cx) / rx;
            float d2 = nx * nx + ny * ny;
            if (d2 >= 1.25f) continue;
            float cov = 1.0f;
            if (d2 > 0.6f) {
                cov = (1.0f - sqrtf(d2)) * rm + 0.5f;
                if (cov <= 0) continue;
                if (cov > 1) cov = 1;
            }
            uint16_t px = lensPixel(st, nx, ny, d2 > 1 ? 1 : d2);
            uint16_t* o = s.at(x, y);
            *o = cov >= 1 ? px : mix565(*o, px, (int)(cov * 255));
        }
    }
}

// The hanging drop: stem from the spout swelling into a sphere that necks and lets go.
static void drawPendant(const SceneCtx& c, const State& st, const Pendant& p) {
    const SceneArt& a = ART_water;
    const Surf& s = c.s;
    float cx = a.cx;
    int yTop = a.neck + 1;
    int yEnd = (int)ceilf(p.cy + p.r);
    float stemLen = p.cy - TIP;
    if (stemLen < 1) stemLen = 1;
    for (int y = yTop; y <= yEnd; y++) {
        float yy = y + 0.5f;
        float hw = 0;
        // tube inside the spout
        if (y <= TIP) hw = 3.0f;
        else {
            float t = (yy - TIP) / stemLen;
            if (t < 1) {
                float q = 1 - t;
                hw = p.stemMin + (p.stemTip - p.stemMin) * q * q * q;
            }
        }
        float dy = (yy - p.cy) / p.r;
        float sph = 0;
        if (dy * dy < 1) sph = p.r * sqrtf(1 - dy * dy);
        if (sph > hw) hw = sph;
        if (hw <= 0.05f) continue;
        float bodyA = hw < 0.5f ? hw * 2 : 1;
        int x0 = (int)floorf(cx - hw - 1), x1 = (int)ceilf(cx + hw);
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx;
            float cov = hw + 0.5f - fabsf(dx);
            if (cov <= 0) continue;
            if (cov > 1) cov = 1;
            cov *= bodyA;
            float nu = dx / (hw > 0.5f ? hw : 0.5f);
            float ny = dy > 1 ? 1 : dy < -1 ? -1 : dy;
            float nx = nu * (dy * dy < 1 ? sqrtf(1 - dy * dy) : 0.4f);
            float d2 = yy >= p.cy ? nx * nx + ny * ny : nu * nu;
            uint16_t px = lensPixel(st, nu * 0.9f, ny, d2 > 1 ? 1 : d2);
            uint16_t* o = s.at(x, y);
            *o = cov >= 1 ? px : mix565(*o, px, (int)(cov * 255));
        }
    }
}

static void drawBubble(const SceneCtx& c, const State& st, const Part& q) {
    const Surf& s = c.s;
    float r = q.r;
    int x0 = (int)floorf(q.x - r - 1), x1 = (int)ceilf(q.x + r + 1);
    int y0 = (int)floorf(q.y - r - 1), y1 = (int)ceilf(q.y + r + 1);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (!s.in(x, y)) continue;
            float dx = x + 0.5f - q.x, dy = y + 0.5f - q.y, d = sqrtf(dx * dx + dy * dy);
            float ring = 1.0f - fabsf(d - r) * 1.5f;
            uint16_t* o = s.at(x, y);
            if (d < r) *o = mix565(*o, st.dropTint, 34);
            if (ring > 0) *o = mix565(*o, st.bubble, (int)(ring * (dy > 0 ? 130 : 210)));
        }
    }
    s.mix((int)floorf(q.x - r * 0.4f), (int)floorf(q.y - r * 0.45f), 0xFFFF, 235);
}

static void drawDroplet(const Surf& s, const State& st, float x, float y, float r) {
    if (r < 1.0f) {  // bilinear splat keeps tiny beads gliding smoothly
        int ix = (int)floorf(x), iy = (int)floorf(y);
        int fx = (int)((x - ix) * 255), fy = (int)((y - iy) * 255);
        int amp = 170 + (int)(r * 85);
        s.mix(ix, iy, st.dropHot, (((255 - fx) * (255 - fy)) >> 8) * amp >> 8);
        s.mix(ix + 1, iy, st.dropHot, ((fx * (255 - fy)) >> 8) * amp >> 8);
        s.mix(ix, iy + 1, st.dropHot, (((255 - fx) * fy) >> 8) * amp >> 8);
        s.mix(ix + 1, iy + 1, st.dropHot, ((fx * fy) >> 8) * amp >> 8);
        return;
    }
    s.disc(x - 0.5f, y - 0.5f, r, st.dropTint, 225);
    s.mix((int)floorf(x - r * 0.35f), (int)floorf(y - r * 0.4f), 0xFFFF, 240);
    if (r > 1.5f) s.mix((int)floorf(x), (int)floorf(y + r * 0.55f), st.dropGlow, 200);
}

static void lineAlpha(const Surf& s, float x0, float y0, float x1, float y1, uint16_t c, int a0, int a1) {
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)(fabsf(dy) + 1);
    if (n > 40) n = 40;
    float ix = dx / n, iy = dy / n;
    for (int i = 0; i <= n; i++) {
        float xf = x0 - 0.5f, yf = y0;
        int xi = (int)floorf(xf);
        int f = (int)((xf - xi) * 255);
        int al = a0 + (a1 - a0) * i / n;
        s.mix(xi, (int)yf, c, ((255 - f) * al) >> 8);
        s.mix(xi + 1, (int)yf, c, (f * al) >> 8);
        x0 += ix; y0 += iy;
    }
}

static void draw(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_water;
    const Surf& s = c.s;
    float surf = lowerSurf(st);

    // ---- rain running down the pane (the water, painted next, hides what passes behind it)
    for (int i = 0; i < TRICKLES; i++) {
        const Trickle& t = st.tr[i];
        int xi = (int)t.x, yi = (int)t.y;
        for (int k = 0; k < t.len; k++) {
            int yy = yi - k;
            if (!s.in(xi, yy)) continue;
            uint16_t* o = s.at(xi, yy);
            *o = add565(*o, scale565(C565(70, 110, 120), 120 - k * 110 / t.len));
        }
        s.disc(t.x - 0.5f, t.y, 1.4f, C565(150, 205, 215), 170);
        s.mix(xi - 1, yi - 1, 0xFFFF, 200);
    }

    // ---- light thrown on the lower glass
    wallCaustics(c, st, (int)surf);

    // ---- both bodies of water
    drawBulb(c, st, st.up, st.upLevel, st.upR, true, a.neck);
    drawBulb(c, st, st.lo, st.loLevel, st.loR, false, a.bot);

    // ---- bubbles live in the water
    for (int i = 0; i < PARTS; i++) if (st.parts[i].kind == P_BUBBLE) drawBubble(c, st, st.parts[i]);

    // ---- the jet: a tapered column with a bead on top, shaded like a little glass rod
    if (st.jetAge >= 0 && st.jetAge < st.jetLife && st.jetH > 2) {
        float u = (float)st.jetAge / st.jetLife;
        float hgt = st.jetH * hg_isin((int)(u * 128)) / 127.0f;
        int yt = (int)(surf - hgt);
        for (int y = yt; y <= (int)surf; y++) {
            float k = (surf - y) / (hgt + 1);
            float hw = 1.5f - 0.9f * k + (1 - k) * (1 - k) * (1 - k) * 3.0f;
            int xl = (int)floorf(a.cx - hw), xr = (int)ceilf(a.cx + hw) - 1;
            for (int x = xl; x <= xr; x++) {
                float dx = (x + 0.5f - a.cx) / hw;
                uint16_t col = dx < -0.45f ? st.dropHot : dx < 0.3f ? st.dropTint : st.dropGlow;
                float cov = hw + 0.5f - fabsf(x + 0.5f - a.cx);
                if (cov > 1) cov = 1;
                if (cov > 0) s.mix(x, y, col, (int)(cov * 190));
            }
        }
        drawDroplet(s, st, a.cx, surf - hgt, 1.6f + st.jetH * 0.03f);
    }

    // ---- the crown: a thin scalloped sheet that climbs, opens and falls back
    if (st.crownAge < 11 && st.loLevel < a.bot - 3) {
        float kz = (st.loR * 0.14f) / (st.loR + 1);
        float age = st.crownAge;
        float rc = 4.0f + age * 1.35f;
        float hc = age < 4 ? age * 2.3f : 9.2f - (age - 4) * 1.25f;
        for (int i = 0; i < 30; i++) {
            int ph = st.crownSeed + i * 256 / 30;
            float cs = hg_icos(ph) / 127.0f, sn = hg_isin(ph) / 127.0f;
            float sc = 0.62f + 0.38f * hg_isin(ph * 6 + st.crownSeed) / 127.0f;
            float bx = a.cx + cs * rc, by = surf + sn * rc * kz * 2.0f;
            float tx = a.cx + cs * (rc + hc * 0.28f), ty = by - hc * sc;
            int al = (sn > 0 ? 150 : 85) - st.crownAge * 12;
            if (al < 10) continue;
            lineAlpha(s, bx, by, tx, ty, sn > 0 ? st.dropHot : st.dropTint, al / 2, al);
        }
    }

    // ---- crown beads: tied to the sheet by a short thinning stalk while young
    for (int i = 0; i < PARTS; i++) {
        const Part& q = st.parts[i];
        if (q.kind == P_DROPLET) {
            if (q.age < 5) lineAlpha(s, q.x - q.vx * 2.0f, q.y + 5.0f, q.x, q.y, st.dropTint, 40, 140);
            drawDroplet(s, st, q.x, q.y, q.r);
        } else if (q.kind == P_SAT) {
            drawDroplet(s, st, q.x, q.y, q.r);
        }
    }

    // ---- hanging drop
    if (c.progress < 0.998f) {
        float wobR = st.recoil * hg_icos((int)(c.tick * 26)) / 127.0f;
        if (c.paused) wobR = hg_isin((int)(c.tick * 4)) * 0.0022f;
        drawPendant(c, st, pendant(st, wobR * 0.4f));
    }

    // ---- the falling drop
    for (int i = 0; i < 2; i++) {
        const Falling& d = st.drop[i];
        if (!d.on) continue;
        // prolate at pinch-off, then ringing around a sphere as surface tension wins
        float dec = 1.0f - d.age * 0.03f;
        if (dec < 0.15f) dec = 0.15f;
        float osc = hg_icos((int)(d.age * 20)) / 127.0f * 0.17f * dec;
        float rx = d.r * (1 - osc), ry = d.r * (1 + osc) * (1 + d.vy * 0.018f);
        // faint trail: the eye's persistence, never a solid bar
        for (int k = 1; k <= 3; k++) {
            float ty = d.y - ry - d.vy * k * 0.5f;
            int al = 60 - k * 16;
            s.mix((int)a.cx - 1, (int)ty, st.dropGlow, al);
            s.mix((int)a.cx, (int)ty, st.dropGlow, al);
        }
        s.glow(a.cx, (int)d.y, (int)(d.r * 1.9f), st.dropGlow, 46);
        drawLensDrop(c, st, a.cx, d.y, rx, ry);
        // the drop's own reflection rises to meet it
        float gap = surf - d.y;
        if (gap < 34 && st.loLevel < a.bot - 3) {
            int am = (int)((34 - gap) * 4);
            s.glow(a.cx, (int)(surf + gap * 0.09f), 6, st.dropTint, am);
        }
    }

    // ---- the flash of the impact
    if (st.impactFlash) s.glow(a.cx, (int)surf, 12, st.dropGlow, st.impactFlash * 22);

    // ---- thin rising thread when the charge rushes back
    if (st.rush > 0) {
        for (int y = a.neck + 1; y < (int)surf; y++) {
            int n = hg_noise(3, y + c.tick * 9);
            if (n > 70) { s.mix(a.cx - 1, y, st.dropTint, n >> 1); s.mix(a.cx, y, st.dropHot, n >> 1); }
        }
    }
}

}  // namespace water

static const Scene SCENE_water = {
    "Clepsydra", "Rain outside. One drop at a time.", &ART_water,
    {C565(232, 248, 250), C565(150, 196, 204), C565(70, 206, 224), C565(246, 170, 124), C565(28, 66, 80), C565(2, 22, 32), C565(255, 120, 100)},
    {40, 190, 220},
    water::init, water::tick, water::draw,
};
