// ==============================================================================
//  Deep Focus - the charge is a swarm of glowing embers, not a fill. About two
//  thousand individually twinkling dots on a soft additive bed: the top swarm flows
//  with the funnel in (u, s) coordinates, true particles pour through the neck and
//  scatter, and they settle into a mound that keeps glowing. Integers and tables in
//  all the hot paths; floats only for the few hundred free particles.
// ==============================================================================
#pragma once
#include "../hg_core.h"
#include "../art/ember.h"

namespace ember {

#define EMB_UP 1740
#define EMB_LOW 1500
#define EMB_PARTS 190
#define EMB_SPARKS 16
#define EMB_COLS 144
#define EMB_HALO 9
#define EMB_BED 7
#define EMB_GLOW_N 33

struct Ember { int8_t u; uint8_t s, ph, k; };   // k: 0..3 threshold nibble, 4..5 speed, 6..7 size class
struct Coal { uint8_t x, y, ph, k; };           // x from dx, y from neck
struct Part { float x, y, vx, vy; uint8_t mode, ph, life, max; };  // 1 falling, 2 bounce, 3 rushing back up
struct Spark { float x, y, vy; uint8_t ph, life, max, on; };

struct State {
    GlassVolume vol;
    Ember up[EMB_UP];
    Coal low[EMB_LOW];
    Part p[EMB_PARTS];
    Spark sp[EMB_SPARKS];
    int16_t yTab[257];       // area coordinate s -> y (1/16 px)
    int16_t rowQ[SCR_H];     // half width of the glass on each row, 1/16 px
    int16_t topU[EMB_COLS];  // surface row of the top swarm per column
    int16_t topL[EMB_COLS];  // surface row of the mound per column
    uint16_t hot[128];       // ember ramp (additive), 64.. clamped: dot() overdrives the weights
    uint16_t glo[48];        // bed and halo ramp (additive)
    uint16_t solid[48];      // glo over the plain glass tint: deep inside the bed nothing needs blending
    uint8_t twk[256];        // twinkle curve
    uint8_t heart[64];       // surface shape of the top swarm
    uint8_t bloom[EMB_GLOW_N * EMB_GLOW_N];  // soft round sprite, values 0..255
    uint8_t dmU[EMB_HALO + EMB_BED + 1], dmL[EMB_HALO + EMB_BED + 1];
    uint16_t flow;           // area-coordinate scroll
    float brk, flowAmt, lastP, rush;
    int topUmin, topUmax, topLmin, topLmax;
    int apexY;
    bool hasU, hasL;
};
static_assert(sizeof(State) <= SCENE_ARENA_BYTES, "");

struct Stop { uint8_t i, r, g, b; };
static const Stop HOT_F[] = {{0, 0, 0, 0}, {10, 66, 22, 1}, {22, 196, 92, 4}, {36, 255, 150, 14}, {52, 255, 182, 40}, {63, 255, 226, 130}};
static const Stop HOT_B[] = {{0, 0, 0, 0}, {10, 2, 28, 42}, {22, 8, 104, 146}, {36, 34, 182, 214}, {52, 90, 216, 236}, {63, 190, 244, 252}};
static const Stop GLO_F[] = {{0, 0, 0, 0}, {14, 30, 10, 0}, {30, 88, 36, 2}, {47, 150, 76, 6}};
static const Stop GLO_B[] = {{0, 0, 0, 0}, {14, 2, 16, 26}, {30, 6, 46, 70}, {47, 12, 80, 112}};

static void rampAt(const Stop* st, int n, int i, int out[3]) {
    int k = 0;
    while (k < n - 2 && i > st[k + 1].i) k++;
    const Stop& a = st[k]; const Stop& b = st[k + 1];
    int span = b.i - a.i, t = i - a.i;
    out[0] = a.r + (b.r - a.r) * t / span;
    out[1] = a.g + (b.g - a.g) * t / span;
    out[2] = a.b + (b.b - a.b) * t / span;
}

static void buildPalette(State& st, float brk) {
    int t = (int)(brk * 256);
    int f[3], b[3];
    for (int i = 0; i < 64; i++) {
        rampAt(HOT_F, 6, i, f); rampAt(HOT_B, 6, i, b);
        st.hot[i] = C565(f[0] + ((b[0] - f[0]) * t >> 8), f[1] + ((b[1] - f[1]) * t >> 8), f[2] + ((b[2] - f[2]) * t >> 8));
    }
    for (int i = 64; i < 128; i++) st.hot[i] = st.hot[63];
    for (int i = 0; i < 48; i++) {
        rampAt(GLO_F, 4, i, f); rampAt(GLO_B, 4, i, b);
        st.glo[i] = C565(f[0] + ((b[0] - f[0]) * t >> 8), f[1] + ((b[1] - f[1]) * t >> 8), f[2] + ((b[2] - f[2]) * t >> 8));
        st.solid[i] = add565(ART_ember.bg[(ART_ember.neck - 40) * SCR_W + ART_ember.cx], st.glo[i]);
    }
    st.brk = brk;
}

// d from -HALO (far outside) to BED (deep inside) -> bed index, scaled by how much mass there is
static void buildBed(uint8_t* dm, float amp) {
    for (int i = 0; i <= EMB_HALO + EMB_BED; i++) {
        int d = i - EMB_HALO;
        float v;
        if (d < 0) { float k = 1.0f + d / (float)(EMB_HALO + 1); v = 19.0f * k * k; }
        else v = 19.0f + 8.0f * d / EMB_BED;
        dm[i] = (uint8_t)(v * amp);
    }
}

static void updateShapes(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_ember;
    float p = c.progress;
    int t = (int)c.tick;

    // ---- top swarm: flat volume level turned into a soft two-lobed heart
    st.hasU = p < 0.998f;
    int umin = 9999, umax = -1;
    if (st.hasU) {
        float level = st.vol.upperLevel(1.0f - p);
        float rL = art_radius(a, (int)level + 1);
        if (rL < 3) rL = 3;
        float H = rL * 0.46f;
        float room = a.neck - level;
        if (H > room * 0.8f) H = room * 0.8f;
        float base = level - H * 0.28f;
        if (base < a.top + 2) base = a.top + 2;
        float toU = 63.0f / rL;
        for (int i = 0; i < EMB_COLS; i++) {
            int x = a.dx + i, dx = x - a.cx;
            float au = (dx < 0 ? -dx : dx) * toU;
            int ui = au > 63 ? 63 : (int)au;
            int w = (hg_isin(x * 3 + (t >> 1)) + hg_isin(x * 7 - (t >> 2) + 90)) >> 6; // -3..3, drifting
            int y = (int)(base + H * st.heart[ui] / 255.0f) + (room > 8 ? w : 0);
            if (dx > 0) y += (int)(H * 0.05f);    // a little lopsided, like a real heap
            if (y > a.neck) y = a.neck;
            st.topU[i] = (int16_t)y;
            if (ui < 63) { if (y < umin) umin = y; if (y > umax) umax = y; }
        }
    }
    st.topUmin = umin; st.topUmax = umax;
    float ampU = (1.0f - p) * 14.0f;
    buildBed(st.dmU, ampU > 1 ? 1 : ampU);

    // ---- mound: rounded cone, base grows quickly at first, then fills the bulb
    st.hasL = p > 0.0015f;
    int lmin = 9999, lmax = -1;
    if (st.hasL) {
        float L = st.vol.lowerLevel(p);
        float grow = cbrtf(p > 0.16f ? 1.0f : p / 0.16f);
        float rL = art_radius(a, (int)L);
        float b = rL * grow;
        if (b < 6) b = 6;
        float h = rL * 0.44f * grow;
        if (h < 2) h = 2;
        float apex = L - h * 0.55f;
        if (apex < a.neck + 6) apex = a.neck + 6;
        float toD = 1.0f / b;
        for (int i = 0; i < EMB_COLS; i++) {
            int x = a.dx + i, dx = x - a.cx;
            float d = (dx < 0 ? -dx : dx) * toD;
            if (d > 1.25f) d = 1.25f;
            int y = (int)(apex + h * d * d * (1.6f - 0.6f * d)) + ((hg_isin(x * 7 + (t >> 2)) + 64) >> 6);
            if (y > a.bot + 1) y = a.bot + 1;
            st.topL[i] = (int16_t)y;
            if (y < lmin) lmin = y;
            if (y > lmax) lmax = y;
        }
        st.apexY = (int)apex;
    } else {
        st.apexY = a.bot;
    }
    st.topLmin = lmin; st.topLmax = lmax;
    float ampL = p * 18.0f;
    buildBed(st.dmL, ampL > 1 ? 1 : ampL);
}

static inline int surfaceAt(const State& st, const SceneArt& a, float x) {
    int i = (int)x - a.dx;
    if (i < 0) i = 0;
    if (i >= EMB_COLS) i = EMB_COLS - 1;
    return st.hasL ? st.topL[i] : a.bot;
}

static void spawnFall(State& st, const SceneArt& a) {
    for (int i = 0; i < EMB_PARTS; i++) {
        Part& q = st.p[i];
        if (q.mode) continue;
        q.x = a.cx + hg_rands() * 1.8f;
        q.y = a.neck - 3 + hg_randf() * 3;
        q.vx = hg_rands() * 0.10f;
        q.vy = 0.8f + hg_randf() * 0.9f;
        q.mode = 1;
        q.ph = (uint8_t)hg_rand();
        return;
    }
}

static void stepParticles(State& st, const SceneArt& a, int t) {
    for (int i = 0; i < EMB_PARTS; i++) {
        Part& q = st.p[i];
        if (!q.mode) continue;
        if (q.mode == 1) {
            float depth = (q.y - a.neck) * 0.022f;     // scatter grows the further it falls
            q.vy += 0.055f + (q.ph & 3) * 0.018f;
            q.vy *= 0.983f;
            q.vx += hg_rands() * (0.018f + depth * 0.05f) + hg_isin(q.ph + t * 3 + (int)q.y * 2) * (0.00012f + depth * 0.0004f);
            q.vx *= 0.985f;
            q.x += q.vx;
            q.y += q.vy;
            float dx = q.x - a.cx, r = (q.y < a.bot) ? st.rowQ[(int)q.y] / 16.0f - 1.5f : 1.0f;
            if (dx > r) { q.x = a.cx + r; q.vx = -fabsf(q.vx) * 0.4f; }
            else if (dx < -r) { q.x = a.cx - r; q.vx = fabsf(q.vx) * 0.4f; }
            int s = surfaceAt(st, a, q.x);
            if (q.y >= s || q.y >= a.bot) {
                q.y = (float)s;
                if ((q.ph & 7) < 2 && st.hasL) {     // a few skip off the surface and burn out
                    q.mode = 2;
                    q.vx = hg_rands() * 0.9f;
                    q.vy = -0.5f - hg_randf() * 1.1f;
                    q.life = q.max = (uint8_t)(14 + (hg_rand() & 15));
                } else q.mode = 0;
            }
        } else if (q.mode == 2) {
            q.vy += 0.07f;
            q.x += q.vx;
            q.y += q.vy;
            q.vx *= 0.97f;
            if (!q.life-- || q.y > surfaceAt(st, a, q.x) + 1) q.mode = 0;
        } else {                                    // rushing back up through the neck
            q.x += (a.cx - q.x) * 0.07f + q.vx;
            q.y += q.vy;
            if (q.y < a.neck - 8) q.mode = 0;
        }
    }
}

static void init(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_ember;
    st.vol.build(a);
    buildPalette(st, c.breakMix);

    for (int y = 0; y < SCR_H; y++) st.rowQ[y] = (y >= a.top && y <= a.bot) ? (int16_t)((a.gr[y] - a.gl[y]) / 2) : 0;

    // area coordinate: equal s steps cover an equal area, so the swarm has an even density
    float total = 0;
    for (int y = a.top; y <= a.neck; y++) total += st.rowQ[y];
    float sum = 0;
    int si = 0;
    for (int y = a.top; y <= a.neck && si <= 256; y++) {
        float v = st.rowQ[y];
        while (si <= 256 && sum + v >= total * si / 256.0f) {
            float f = (total * si / 256.0f - sum) / (v > 0 ? v : 1);
            st.yTab[si++] = (int16_t)((y + f) * 16);
        }
        sum += v;
    }
    while (si <= 256) st.yTab[si++] = (int16_t)(a.neck * 16);

    // jittered grid in (u, s): even but not regular
    const int nu = 30, ns = EMB_UP / nu;
    for (int j = 0, n = 0; j < ns; j++)
        for (int i = 0; i < nu && n < EMB_UP; i++, n++) {
            Ember& e = st.up[n];
            e.u = (int8_t)(-127 + (i * 2 + hg_randf() * 2) * 254 / (2 * nu));
            e.s = (uint8_t)((j + hg_randf()) * 256 / ns);
            e.ph = (uint8_t)hg_rand();
            e.k = (uint8_t)hg_rand();
        }
    // coals: jittered grid in the lower bulb, row ordered
    int n = 0;
    const int cell = 3;
    for (int y = a.neck + 2; y < a.bot - 1 && n < EMB_LOW; y += cell) {
        int r = st.rowQ[y] >> 4;
        for (int x = a.cx - r; x < a.cx + r && n < EMB_LOW; x += cell) {
            if (hg_randf() > 0.9f) continue;
            int xx = x + (hg_rand() & 3) - a.dx, yy = y + (hg_rand() & 3);
            int half = st.rowQ[yy < a.bot ? yy : a.bot] >> 4;
            if (xx + a.dx - a.cx >= half - 1 || xx + a.dx - a.cx <= -half) continue;
            Coal& e = st.low[n++];
            e.x = (uint8_t)xx;
            e.y = (uint8_t)(yy - a.neck);
            e.ph = (uint8_t)hg_rand();
            e.k = (uint8_t)hg_rand();
        }
    }
    for (; n < EMB_LOW; n++) st.low[n].y = 255;

    for (int i = 0; i < 256; i++) {
        float ph = i * 6.2831853f / 256.0f;
        float v = 0.40f + 0.26f * sinf(ph);
        float f = sinf(ph * 3 + 1.0f);
        if (f > 0) { f *= f; f *= f; v += 0.55f * f; }
        float dip = sinf(ph * 2 + 4.0f);
        if (dip > 0.7f) v -= (dip - 0.7f) * 0.9f;
        if (v < 0.05f) v = 0.05f;
        if (v > 1) v = 1;
        st.twk[i] = (uint8_t)(v * 255);
    }
    for (int i = 0; i < 64; i++) {
        float u = i / 63.0f;
        float dip = exp(-(u / 0.17f) * (u / 0.17f)) * 0.44f;        // the cleft of the heart
        float e = u > 0.5f ? (u - 0.5f) / 0.5f : 0;
        float v = dip + e * e * sqrtf(e) * 1.0f;
        st.heart[i] = (uint8_t)((v > 1 ? 1 : v) * 255);
    }
    for (int j = 0; j < EMB_GLOW_N; j++)
        for (int i = 0; i < EMB_GLOW_N; i++) {
            float dx = (i - 16) / 16.5f, dy = (j - 16) / 16.5f, d = dx * dx + dy * dy;
            float v = d >= 1 ? 0 : (1 - d) * (1 - d) * (1 - d);
            st.bloom[j * EMB_GLOW_N + i] = (uint8_t)(v * 255);
        }

    st.flowAmt = c.paused || c.progress >= 0.998f ? 0 : 1;
    st.lastP = c.progress;
    updateShapes(c);
    // start with the stream already in the air
    if (st.flowAmt > 0)
        for (int k = 0; k < 70; k++) {
            spawnFall(st, a); spawnFall(st, a); spawnFall(st, a);
            stepParticles(st, a, k);
        }
}

static void tick(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_ember;
    if (fabsf(c.breakMix - st.brk) > 0.01f || (c.breakMix != st.brk && (c.breakMix == 0 || c.breakMix == 1))) buildPalette(st, c.breakMix);
    float p = c.progress;
    updateShapes(c);

    float dp = p - st.lastP;
    st.lastP = p;
    bool rushing = dp < -0.0015f;
    st.rush += ((rushing ? 1.0f : 0.0f) - st.rush) * 0.2f;

    bool flowing = !c.paused && p < 0.998f && !rushing;
    st.flowAmt += ((flowing ? 1.0f : 0.0f) - st.flowAmt) * 0.08f;
    if (rushing) st.flow -= 500;
    else st.flow += (uint16_t)(st.flowAmt * 34);

    if (flowing) {
        int t = (int)c.tick;
        int rate = 3 + ((hg_isin(t * 2) + hg_isin(t * 7 + 40) + 254) >> 7);  // 3..6, in loose bursts
        for (int n = 0; n < rate; n++) spawnFall(st, a);
    }
    if (rushing && st.hasL) {
        for (int n = 0; n < 7; n++) {
            for (int i = 0; i < EMB_PARTS; i++) {
                Part& q = st.p[i];
                if (q.mode) continue;
                int col = hg_rand() % EMB_COLS;
                q.x = (float)(a.dx + col);
                q.y = (float)st.topL[col];
                float dx = q.x - a.cx;
                if (fabsf(dx) > art_radius(a, (int)q.y) - 2) { q.x = a.cx + hg_rands() * 10; }
                q.vx = 0;
                q.vy = -5.0f - hg_randf() * 4.0f;
                q.mode = 3;
                q.ph = (uint8_t)hg_rand();
                break;
            }
        }
    }
    stepParticles(st, a, (int)c.tick);

    // ---- a few sparks drift up off both surfaces
    for (int i = 0; i < EMB_SPARKS; i++) {
        Spark& k = st.sp[i];
        if (!k.on) {
            if ((hg_rand() & 63) != 0) continue;
            bool fromUp = st.hasU && (!st.hasL || (hg_rand() & 1));
            if (!fromUp && !st.hasL) continue;
            int col = 20 + hg_rand() % (EMB_COLS - 40);
            int y = fromUp ? st.topU[col] : st.topL[col];
            if (y >= a.bot || y <= a.top + 6) continue;
            int dx = a.dx + col - a.cx, r = st.rowQ[y] >> 4;
            if (dx > r - 5 || dx < -r + 5) continue;
            k.x = (float)(a.dx + col);
            k.y = (float)y;
            k.vy = -0.28f - hg_randf() * 0.42f;
            k.ph = (uint8_t)hg_rand();
            k.life = k.max = (uint8_t)(60 + (hg_rand() & 63));
            k.on = 1;
            continue;
        }
        k.y += k.vy;
        k.x += hg_isin(k.ph + (int)c.tick * 3) * 0.0036f;
        int y = (int)k.y;
        float r = (y >= a.top && y <= a.bot) ? st.rowQ[y] / 16.0f - 2.0f : 0;
        if (!k.life-- || r < 3 || fabsf(k.x - a.cx) > r) k.on = 0;
    }
}

// ---------------------------------------------------------------- drawing
// Branch-free saturating add. The carry bits become field masks (green tops out at 62, unseen).
// Sparkle data is random, so a saturation branch would be mispredicted constantly.
static inline uint16_t fastAdd(uint16_t a, uint16_t b) {
    uint32_t z = spread565(a) + spread565(b);
    uint32_t ov = z & 0x08010020u;
    return pack565(z | (ov - (ov >> 5)));
}

static inline void dot(const Surf& s, const uint16_t* hot, int xq, int yq, int v) {
    int x = xq >> 4, y = yq >> 4;
    if ((unsigned)(x - s.x0) >= (unsigned)(s.w - 1) || (unsigned)(y - s.y0) >= (unsigned)(s.h - 1)) return;
    int fx = xq & 15, fy = yq & 15, ix = 16 - fx, iy = 16 - fy;
    uint16_t* o = s.at(x, y);
    o[0] = fastAdd(o[0], hot[(v * ix * iy) >> 7]);
    o[1] = fastAdd(o[1], hot[(v * fx * iy) >> 7]);
    o += s.w;
    o[0] = fastAdd(o[0], hot[(v * ix * fy) >> 7]);
    o[1] = fastAdd(o[1], hot[(v * fx * fy) >> 7]);
}

// additive round glow from the prebuilt sprite. step 1: r16, 2: r8, 4: r4, 5: r3. amp 0..47
static void bloom(const Surf& s, const State& st, int cx, int cy, int step, int amp) {
    if (amp <= 0) return;
    int r = 16 / step;
    for (int j = -r; j <= r; j++) {
        int y = cy + j;
        if ((unsigned)(y - s.y0) >= (unsigned)s.h) continue;
        const uint8_t* row = st.bloom + (j * step + 16) * EMB_GLOW_N;
        for (int i = -r; i <= r; i++) {
            int x = cx + i;
            if ((unsigned)(x - s.x0) >= (unsigned)s.w) continue;
            int v = (row[i * step + 16] * amp) >> 8;
            if (v > 0) { uint16_t* o = s.at(x, y); *o = fastAdd(*o, st.glo[v]); }
        }
    }
}

static inline void bedPx(const State& st, uint16_t* o, const uint8_t* dm, int d, int wavePh, int dither) {
    int v = dm[(d > EMB_BED ? EMB_BED : d) + EMB_HALO] + dither;
    if (d >= 0) v += hg_isin(wavePh) >> 5;
    if (v > 0) *o = fastAdd(*o, st.glo[v]);
}

// the soft bed under the embers and the halo around it
static void drawBed(const Surf& s, const State& st, const SceneArt& a, int y0, int y1, const int16_t* top, int topMax, const uint8_t* dm, int inset, int t) {
    const int xMin = s.x0, xMax = s.x0 + s.w - 1;
    if (y0 < s.y0) y0 = s.y0;
    for (int y = y0; y <= y1; y++) {
        int R = (st.rowQ[y] >> 4) - inset;
        if (R < 1) R = 1;
        int xl = a.cx - R, xr = a.cx + R;
        int xa = xl - EMB_HALO, xb = xr + EMB_HALO;
        if (xa < xMin) xa = xMin;
        if (xb > xMax) xb = xMax;
        uint16_t* o = s.at(xa, y);
        const uint8_t* nz = HG_NOISE + (y & 63) * 64;
        bool chk = y <= topMax + EMB_BED;
        int wave = y * 5 - t * 2;
        const int16_t* tp = top - a.dx;
        int inL = xl + EMB_BED, inR = xr - EMB_BED;
        if (inL > inR + 1) inL = inR = a.cx;      // narrow rows: no plain middle
        const int full = dm[EMB_HALO + EMB_BED];
        const uint8_t* noise = nz;
        int x = xa;
        // left ramp
        for (; x < inL && x <= xb; x++, o++) {
            int d = x - xl;
            if (d > xr - x) d = xr - x;
            if (chk) { int k = y - tp[x]; if (k < d) d = k; }
            if (d < -EMB_HALO) continue;
            bedPx(st, o, dm, d, wave + (x & ~7) * 3, noise[x & 63] & 3);
        }
        // the long plain middle
        if (!chk) {
            for (; x <= inR && x <= xb; x++, o++) {
                int v = full + (hg_isin(wave + (x & ~7) * 3) >> 5) + (noise[x & 63] & 3);
                *o = st.solid[v < 0 ? 0 : v];
            }
        } else {
            for (; x <= inR && x <= xb; x++, o++) {
                int d = y - tp[x];
                if (d >= EMB_BED) {
                    int v = full + (hg_isin(wave + (x & ~7) * 3) >> 5) + (noise[x & 63] & 3);
                    *o = st.solid[v < 0 ? 0 : v];
                } else if (d >= -EMB_HALO) {
                    bedPx(st, o, dm, d, wave + (x & ~7) * 3, noise[x & 63] & 3);
                }
            }
        }
        // right ramp
        for (; x <= xb; x++, o++) {
            int d = xr - x;
            if (d > x - xl) d = x - xl;
            if (chk) { int k = y - tp[x]; if (k < d) d = k; }
            if (d < -EMB_HALO) continue;
            bedPx(st, o, dm, d, wave + (x & ~7) * 3, noise[x & 63] & 3);
        }
    }
}

static void draw(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_ember;
    const Surf& s = c.s;
    const int t = (int)c.tick;
    const int cxQ = a.cx << 4;

    // ---- beds
    if (st.hasU) {
        int y0 = st.topUmin - EMB_HALO;
        if (y0 < a.top) y0 = a.top;
        drawBed(s, st, a, y0, a.neck, st.topU, st.topUmax, st.dmU, 3, t);
    }
    if (st.hasL) {
        int y0 = st.topLmin - EMB_HALO;
        if (y0 < a.neck) y0 = a.neck;
        drawBed(s, st, a, y0, a.bot, st.topL, st.topLmax, st.dmL, 1, t);
    }

    // ---- light from the falling thread
    int fa = (int)(st.flowAmt * 256);
    if (fa > 8) {
        int yEnd = st.apexY;
        for (int y = a.neck; y < yEnd; y++) {
            int off = y - a.neck;
            int w = 3 + (off >> 3);
            if (w > 8) w = 8;
            int amp = 16 - (off >> 4) + (hg_isin(y * 9 - t * 15) >> 5) + (hg_isin(y * 4 - t * 9) >> 6);
            amp = (amp * fa) >> 8;
            if (amp <= 0) continue;
            int step = (amp << 8) / w;
            int xa = a.cx - w;
            uint16_t* o = s.at(xa, y);
            for (int i = -w; i <= w; i++, o++) {
                int v = amp - (((i < 0 ? -i : i) * step) >> 8);
                if (v > 0) *o = fastAdd(*o, st.glo[v]);
            }
        }
        bloom(s, st, a.cx, a.neck, 4, (fa * 34) >> 8);
        if (st.hasL) bloom(s, st, a.cx, st.apexY, 2, (fa * (20 + (hg_isin(t * 5) >> 5))) >> 8);
    }

    // ---- the top swarm
    if (st.hasU) {
        const uint16_t flow = st.flow;
        for (int i = 0; i < EMB_UP; i++) {
            const Ember& e = st.up[i];
            uint32_t S = ((uint32_t)e.s << 8) + flow;
            int si = (S >> 8) & 255, sf = S & 255;
            int yq = st.yTab[si] + (((st.yTab[si + 1] - st.yTab[si]) * sf) >> 8);
            int y = yq >> 4;
            if (y < st.topUmin) continue;
            int ph = e.ph;
            int Rq = st.rowQ[y] - 48;
            if (Rq < 10) Rq = 10;
            int xq = cxQ + ((e.u * Rq) >> 7);
            int x = xq >> 4;
            int thr = e.k & 15;
            int fd = y - st.topU[x - a.dx] - (thr >> 1);
            if (fd < 0) continue;
            int spd = ((e.k >> 4) & 3) + 2;
            int v = st.twk[(ph + ((t * spd) >> 1)) & 255];
            if (fd < 5) v = (v * (fd + 3)) >> 3;
            v = (v >> 2) + (e.k & 16 ? 8 : 0);
            if (v > 63) v = 63;
            xq += hg_isin(ph * 3 + t * 2) >> 3;     // gentle jostle, +-1 px
            yq += hg_isin(ph * 5 + t + 64) >> 3;
            dot(s, st.hot, xq, yq, v);
            if ((e.k & 0xC0) == 0xC0 && (ph & 7) == 0) bloom(s, st, xq >> 4, yq >> 4, 5, v >> 1);
        }
    }

    // ---- the mound
    if (st.hasL) {
        const int ymin = st.topLmin - a.neck;
        for (int i = 0; i < EMB_LOW; i++) {
            const Coal& e = st.low[i];
            if (e.y < ymin || e.y == 255) continue;
            int y = e.y + a.neck;
            int fd = y - st.topL[e.x] - ((e.k & 15) >> 1);
            if (fd < 0) continue;
            int spd = ((e.k >> 4) & 3) + 1;
            int v = st.twk[(e.ph + ((t * spd) >> 1)) & 255];
            if (fd < 5) v = (v * (fd + 3)) >> 3;
            else if (fd > 26) v = (v * 170) >> 8;
            v >>= 2;
            int xq = ((e.x + a.dx) << 4) + (hg_isin(e.ph * 3 + t) >> 4);
            int yq = (y << 4) + (hg_isin(e.ph * 5 + t + 64) >> 4);
            dot(s, st.hot, xq, yq, v);
            if ((e.k & 0xC0) == 0xC0 && (e.ph & 7) == 0 && fd < 24) bloom(s, st, xq >> 4, yq >> 4, 5, v >> 1);
        }
    }

    // ---- free particles: thread, skips, rush
    for (int i = 0; i < EMB_PARTS; i++) {
        const Part& q = st.p[i];
        if (!q.mode) continue;
        int v = 24 + (st.twk[(q.ph + t * 5) & 255] >> 4);
        if (q.mode == 2) v = v * q.life / (q.max ? q.max : 1);
        dot(s, st.hot, (int)(q.x * 16), (int)(q.y * 16), v);
    }

    // ---- sparks
    for (int i = 0; i < EMB_SPARKS; i++) {
        const Spark& k = st.sp[i];
        if (!k.on) continue;
        int age = k.max - k.life;
        int env = age < 12 ? age * 5 : k.life < 30 ? k.life * 2 : 60;
        int v = (env * (st.twk[(k.ph + t * 3) & 255] + 128)) >> 8;
        if (v > 63) v = 63;
        dot(s, st.hot, (int)(k.x * 16), (int)(k.y * 16), v);
        bloom(s, st, (int)k.x, (int)k.y, 4, v >> 2);
    }
}

}  // namespace ember

static const Scene SCENE_ember = {
    "Deep Focus", "A swarm of embers, falling.", &ART_ember,
    {C565(238, 244, 250), C565(140, 156, 178), C565(255, 168, 36), C565(90, 210, 230), C565(38, 48, 64), C565(14, 12, 10), C565(255, 92, 76)},
    {255, 130, 20},
    ember::init, ember::tick, ember::draw,
};
