// ==============================================================================
//  Zen Ink - the one light scene: a flat, printed hourglass on alabaster paper.
//
//  Restraint is the design. The charge is two flat inks with true area-coverage
//  edges and no gradients: a slowly undulating pool, a thread that breathes, and
//  a bell-shaped mound with a calm second colour rising inside it. Once in a long
//  while a single ink drop forms under the cap, falls, and sends one slow ring
//  through the pool. Everything is filled per column from Q8 surface tables.
// ==============================================================================
#pragma once
#include "../hg_core.h"
#include "../art/zen.h"

namespace zen {

#define ZEN_COLS 150   // = dynamic rectangle width

struct Drop {
    uint8_t mode;      // 0 waiting, 1 swelling, 2 falling, 3 ring only
    float x, y, vy, r; // drop
    float ringR;       // ring radius, 0 = none
    float ringX, ringY;
    float bump;        // local dent in the surface after the impact
    int wait;
    uint32_t bumpTick;
};

struct State {
    GlassVolume vol;
    int16_t uStart[ZEN_COLS], uEnd[ZEN_COLS];  // glass rows of each column, upper bulb
    int16_t lStart[ZEN_COLS], lEnd[ZEN_COLS];  // lower bulb
    int32_t surf[ZEN_COLS + 2];                // scratch: surface y, Q8
    Drop drop;
    float headY, tailY, headV, tailV;          // the thread's two ends
    float lastP, slosh;
    bool threadOn, attached;
    uint16_t colTop, colLow, colRing, colInk;
    float brk;
};
static_assert(sizeof(State) <= SCENE_ARENA_BYTES, "");

// focus: burnt sienna over sage. break: bamboo jade over a deep pine.
static void buildPalette(State& st, float brk) {
    int t = (int)(brk * 255);
    st.colTop = mix565(C565(198, 100, 60), C565(70, 142, 106), t);
    st.colLow = mix565(C565(128, 148, 118), C565(40, 104, 84), t);
    st.colRing = mix565(st.colTop, C565(62, 28, 16), 130);
    st.colInk = C565(40, 36, 34);
    st.brk = brk;
}

// Q8 interpolated table sine: amplitude 127 << 8. ph is in 1/256 of a table step, wraps cleanly.
static inline int sinQ(uint32_t ph) {
    int i = (ph >> 8) & 255, f = ph & 255;
    return HG_SIN[i] * (256 - f) + HG_SIN[(i + 1) & 255] * f;
}

static inline void moundParams(float p, float& e, float& h, float& b) {
    if (p <= 0) { e = h = 0; b = 1; return; }
    float t = cbrtf(p > 1 ? 1 : p) * ZEN_TAB;
    int i = (int)t;
    if (i >= ZEN_TAB) i = ZEN_TAB - 1;
    float f = t - i;
    e = ZEN_MOUND_E[i] + (ZEN_MOUND_E[i + 1] - ZEN_MOUND_E[i]) * f;
    h = ZEN_MOUND_H[i] + (ZEN_MOUND_H[i + 1] - ZEN_MOUND_H[i]) * f;
    b = ZEN_MOUND_B[i] + (ZEN_MOUND_B[i + 1] - ZEN_MOUND_B[i]) * f;
}

// y of the mound's peak, for where the thread and the drop stop.
static float moundPeak(float p) {
    float e, h, b;
    moundParams(p, e, h, b);
    return ART_zen.bot + 1 - e - h;
}

static void init(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_zen;
    st.vol.build(a, ZEN_FILL);
    buildPalette(st, c.breakMix);
    for (int i = 0; i < ZEN_COLS; i++) {
        st.uStart[i] = st.lStart[i] = 32000;
        st.uEnd[i] = st.lEnd[i] = -1;
    }
    for (int y = a.top; y <= a.bot; y++) {
        int xl, xr;
        if (!art_inner(a, y, xl, xr)) continue;
        for (int x = xl - 1; x <= xr + 1; x++) {
            int i = x - a.dx;
            if (i < 0 || i >= ZEN_COLS) continue;
            if (y <= a.neck) {
                if (y < st.uStart[i]) st.uStart[i] = y;
                st.uEnd[i] = y;
            } else {
                if (y < st.lStart[i]) st.lStart[i] = y;
                st.lEnd[i] = y;
            }
        }
    }
    st.lastP = c.progress;
    bool flowing = !c.paused && c.progress < 0.998f;
    st.threadOn = flowing;
    st.attached = flowing;
    st.headY = moundPeak(c.progress);
    st.tailY = a.neck;
    st.headV = st.tailV = 0;
    st.drop.mode = 0;
    st.drop.wait = 240 + (hg_rand() & 255);
    st.drop.bump = 0;
    st.drop.ringR = 0;
    st.slosh = 0;
}

static float poolLevel(const State& st, float p) { return st.vol.upperLevel(1.0f - p); }

static void tick(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_zen;
    if (fabsf(c.breakMix - st.brk) > 0.01f || (c.breakMix != st.brk && (c.breakMix == 0 || c.breakMix == 1)))
        buildPalette(st, c.breakMix);

    float p = c.progress;
    float dp = p - st.lastP;
    st.lastP = p;
    bool rushing = dp < -0.0015f;              // restart: the charge runs back up
    if (rushing) st.slosh = 1.0f;
    st.slosh *= 0.972f;

    // ---- the thread: its leading end falls from the neck, its trailing end falls away after it
    bool flowing = !c.paused && p < 0.998f && !rushing;
    float target = moundPeak(p) + 2;
    if (flowing) {
        if (!st.threadOn || !st.attached) {
            if (!st.threadOn) { st.headY = a.neck; st.headV = 0; }
            st.threadOn = st.attached = true;
            st.tailY = a.neck;
        }
    } else if (st.threadOn && st.attached) {
        st.attached = false;
        st.tailY = a.neck;
        st.tailV = 0;
    }
    if (st.threadOn) {
        if (st.headY < target) {
            st.headV += 0.16f;
            st.headY += st.headV;
        }
        if (st.headY >= target) { st.headY = target; st.headV = 0; }
        if (!st.attached) {
            st.tailV += 0.16f;
            st.tailY += st.tailV;
            if (st.tailY >= target) st.threadOn = false;
        }
    }

    // ---- the rare ink drop
    Drop& d = st.drop;
    float level = poolLevel(st, p);
    if (d.bump > 0.01f) d.bump *= 0.982f; else d.bump = 0;
    if (d.ringR > 0) {
        d.ringR += 0.21f;                      // 6 px a second
        if (d.ringR > 62) d.ringR = 0;
    }
    if (d.mode == 0) {
        if (--d.wait <= 0) {
            float rl = art_radius(a, (int)level);
            if (c.paused || rushing || p > 0.86f || rl < 22 || d.ringR > 0) {
                d.wait = 120;
            } else {
                float span = art_radius(a, a.top + 4) * 0.62f;
                if (span > rl * 0.6f) span = rl * 0.6f;
                d.x = a.cx + hg_rands() * span;
                d.y = a.top + 0.5f;
                d.r = 0;
                d.vy = 0;
                d.mode = 1;
            }
        }
    } else if (d.mode == 1) {
        d.r += 0.03f;                          // swells under the cap for about 3 s
        d.y = a.top + 0.5f + d.r * 0.9f;
        if (d.r >= 2.7f) d.mode = 2;
    } else if (d.mode == 2) {
        d.vy += 0.045f;
        d.y += d.vy;
        int i = (int)d.x - a.dx;
        float s = (i >= 0 && i < ZEN_COLS) ? st.surf[i] / 256.0f : level;
        if (d.y >= s) {
            d.mode = 0;
            d.ringX = d.x;
            d.ringY = s;
            d.ringR = 1.0f;
            d.bump = 1.0f;
            d.bumpTick = c.tick;
            d.wait = 720 + (hg_rand() & 511);  // 24 to 41 s until the next one
        }
    }
}

// One column of flat colour with an anti-aliased top edge. surface in Q8, m = slope in 1/16 steps.
static inline void fillCol(const Surf& s, int x, int32_t sq, int m, int yMin, int yMax, uint16_t col) {
    if (yMin > yMax) return;
    if (m > 95) m = 95;
    int hyp = ZEN_HYP[m];
    int y = (sq - (int)ZEN_RUN[m] - 128) >> 8;
    if (y < yMin) y = yMin;
    if (y > yMax) return;
    uint16_t* o = s.at(x, y);
    const int w = s.w;
    for (; y <= yMax; y++, o += w) {
        int cov = (((y << 8) + 128 - sq) * hyp >> 8) + 128;
        if (cov >= 256) break;
        if (cov > 0) *o = mix565(*o, col, cov);
    }
    for (; y <= yMax; y++, o += w) *o = col;
}

static inline int slopeAt(const int32_t* t, int i) {
    int m = (t[i + 1] - t[i - 1]) >> 5;   // central difference, Q8 per 2 px -> 1/16
    return m < 0 ? -m : m;
}

// One row of thread with sub-pixel edges. cq = centre, hw = half width, Q8 screen x.
static inline void threadRow(const Surf& s, int y, int cq, int hw, uint16_t col) {
    int lq = cq - hw, rq = cq + hw;
    int xl = lq >> 8, xr = (rq - 1) >> 8;
    if (xl == xr) {
        s.mix(xl, y, col, (rq - lq) * 255 >> 8);
        return;
    }
    s.mix(xl, y, col, (256 - (lq & 255)) * 255 >> 8);
    int fr = rq - (xr << 8);
    s.mix(xr, y, col, fr >= 256 ? 255 : fr * 255 >> 8);
    for (int x = xl + 1; x < xr; x++) s.put(x, y, col);
}

// Thin ring through the pool, drawn only below the surface.
static void drawRing(SceneCtx& c, const State& st) {
    const Drop& d = st.drop;
    const Surf& s = c.s;
    const SceneArt& a = ART_zen;
    float R = d.ringR;
    float fadeIn = R < 6 ? R / 6 : 1;
    float fadeOut = 1 - R / 62;
    int alpha = (int)(170 * fadeIn * fadeOut * fadeOut);
    if (alpha < 6) return;
    float ro = R + 1.25f, ri = R - 1.25f;
    if (ri < 0) ri = 0;
    int cx = (int)d.ringX, cy = (int)d.ringY;
    int r2q = (int)(R * R * 256);
    int inv = (int)(65536.0f / (2 * R + 0.5f));
    float ro2 = ro * ro, ri2 = ri * ri;
    uint16_t col = st.colRing;
    for (int dy = 0; dy <= (int)ro; dy++) {
        int y = cy + dy;
        if (y > a.neck) break;
        float o2 = ro2 - dy * dy;
        if (o2 <= 0) break;
        float i2 = ri2 - dy * dy;
        int xo = (int)sqrtf(o2) + 1, xi = i2 > 0 ? (int)sqrtf(i2) : 0;
        for (int side = 0; side < 2; side++) {
            for (int k = xi; k <= xo; k++) {
                if (side && k == 0) continue;
                int x = side ? cx - k : cx + k;
                int i = x - a.dx;
                if (i < 0 || i >= ZEN_COLS) continue;
                if (y < st.uStart[i] || y > st.uEnd[i]) continue;
                if ((y << 8) < st.surf[i]) continue;
                int dist = ((((k * k + dy * dy) << 8) - r2q) * inv) >> 16;   // Q8 px from the ring's centre line
                if (dist < 0) dist = -dist;
                int cov = 300 - dist;                                         // 1.2 px wide with a soft shoulder
                if (cov <= 0) continue;
                if (cov > 256) cov = 256;
                uint16_t* o = s.at(x, y);
                *o = mix565(*o, col, (cov * alpha) >> 8);
            }
        }
    }
}

static void draw(SceneCtx& c) {
    State& st = SCENE_STATE(State, c);
    const SceneArt& a = ART_zen;
    const Surf& s = c.s;
    const float p = c.progress;
    const uint32_t t = c.tick;
    int32_t* surf = st.surf;

    // ---- the pool
    if (p < 0.998f) {
        float level = poolLevel(st, p);
        int rl = (int)art_radius(a, (int)level + 1);
        if (rl < 4) rl = 4;
        float depth = a.neck - level;
        // calm in a wide bulb, stiller and smaller as it narrows; a restart sloshes a little
        float size = rl / 74.0f;
        if (depth < 26) size *= depth / 26;
        size *= 1 + st.slosh * 1.6f;
        int a1 = (int)(4.6f * size * 8), a2 = (int)(2.0f * size * 8), a3 = (int)(2.6f * size * 8);
        // the surface sinks gently at the middle as the funnel takes hold
        int dimple = (int)(rl * 0.17f * (0.25f + 0.75f * p) * (depth < 14 ? depth / 14 : 1) * 256);
        int invR = (256 * 128) / rl;   // |dx| -> half-cosine phase
        int lq = (int)(level * 256);
        const Drop& d = st.drop;
        float bumpA = 0;
        int bx = 0;
        if (d.bump > 0) {
            // one slow dip and rebound where the drop landed
            int ph = (int)((c.tick - d.bumpTick) * 3);
            bumpA = d.bump * 1.7f * hg_icos(ph) / 127.0f;
            bx = (int)d.ringX;
        }
        int bq = (int)(bumpA * 256);
        for (int i = 0; i < ZEN_COLS; i++) {
            int x = a.dx + i;
            int dx = x - a.cx;
            uint32_t ph1 = (uint32_t)(x * 520) + t * 243;      // ~1.1 waves across, 9 s period
            uint32_t ph2 = (uint32_t)(x * 1290) - t * 171;     // ~2.7 waves across, 13 s, opposite way
            uint32_t ph3 = (uint32_t)(x * 190) + t * 74;       // long slow tilt, 30 s
            int w = (a1 * sinQ(ph1) >> 11) + (a2 * sinQ(ph2) >> 11) + (a3 * sinQ(ph3) >> 11);
            int adx = dx < 0 ? -dx : dx;
            int32_t v = lq + (w * 256 >> 7);
            if (adx < rl) v += (dimple >> 1) + ((dimple * hg_icos((adx * invR) >> 8)) >> 8);
            if (bq) {
                int dd = x - bx;
                if (dd < 0) dd = -dd;
                if (dd < 32) v += (bq >> 1) + ((bq * hg_icos(dd * 4)) >> 8);
            }
            surf[i] = v;
        }
        for (int i = 0; i < ZEN_COLS; i++) {
            int m = (i > 0 && i < ZEN_COLS - 1) ? slopeAt(surf, i) : 0;
            fillCol(s, a.dx + i, surf[i], m, st.uStart[i], st.uEnd[i], st.colTop);
        }
        if (st.drop.ringR > 0) drawRing(c, st);
    } else {
        for (int i = 0; i < ZEN_COLS; i++) surf[i] = (a.neck + 2) << 8;
    }

    // ---- the mound: bell of flat colour; sage rises inside it as the session passes
    float me, mh, mb;
    moundParams(p, me, mh, mb);
    if (mh > 0.2f) {
        // breathes by a fraction of a pixel, enough to feel alive at every moment
        mh += 0.35f * sinQ(t * 96) / 32512.0f;
        int floorQ = (a.bot + 1) << 8;
        int32_t baseQ = floorQ - (int)(me * 256);
        int hq = (int)(mh * 256);
        int invB = (int)(65536.0f / mb);
        int undA = (int)(0.55f * (mh / 44.0f) * 256);
        int bedW = (int)(me * 64);
        int32_t tab[ZEN_COLS + 2];
        for (int i = 0; i < ZEN_COLS; i++) {
            int x = a.dx + i, dx = x - a.cx;
            int adx = dx < 0 ? -dx : dx;
            int u = (adx * invB) >> 8;
            int pr = u >= 256 ? 0 : ZEN_PROF[u];
            int32_t v = baseQ - ((hq * pr) >> 15);
            // very long swell passing through; none beyond a young hill's foot, or it would print a sliver on the floor
            int wgt = (pr >> 7) + bedW;
            if (wgt > 256) wgt = 256;
            v += (((undA * (sinQ((uint32_t)(x * 660) + t * 140) >> 8)) >> 7) * wgt) >> 8;
            tab[i] = v;
        }
        for (int i = 0; i < ZEN_COLS; i++) {
            int m = (i > 0 && i < ZEN_COLS - 1) ? slopeAt(tab, i) : 0;
            fillCol(s, a.dx + i, tab[i], m, st.lStart[i], st.lEnd[i], st.colTop);
        }
        // second ink: a flat sage bed that rises inside the mound, with its own slow wave
        float low = (p - 0.06f) / 0.94f;
        if (low > 0) {
            int frac = (int)((0.30f + 0.32f * low) * 256 * (low < 0.12f ? low / 0.12f : 1));
            int amp = (int)(2.2f * (low < 0.2f ? low / 0.2f : 1) * 256);
            for (int i = 0; i < ZEN_COLS; i++) {
                int x = a.dx + i;
                int32_t h = floorQ - tab[i];
                if (h < 0) h = 0;
                uint32_t ph = (uint32_t)(x * 800) - t * 118;
                int32_t b = floorQ - ((h * frac) >> 8) + ((amp * (sinQ(ph) >> 8)) >> 7);
                // ease the far corners flat so it reads as a laid wash
                tab[i] = b;
            }
            for (int i = 0; i < ZEN_COLS; i++) {
                int m = (i > 0 && i < ZEN_COLS - 1) ? slopeAt(tab, i) : 0;
                fillCol(s, a.dx + i, tab[i], m, st.lStart[i], st.lEnd[i], st.colLow);
            }
        }
    }

    // ---- the thread: tapering, breathing, a slow pulse travelling down it
    if (st.threadOn) {
        int yA = st.attached ? a.neck + 1 : (int)st.tailY;
        if (yA <= a.neck) yA = a.neck + 1;
        int yB = (int)st.headY;
        if (yB > a.bot) yB = a.bot;
        int rest = 290 + (46 * (sinQ(t * 118) >> 8) >> 7);   // half width in Q8: 1.13 px, breathes +-0.18 over 9 s
        uint16_t col = st.colTop;
        for (int y = yA; y <= yB; y++) {
            int k = y - a.neck;
            int hw = rest;
            if (k < 48) hw += ZEN_TAPER[k];
            hw += (22 * (sinQ((uint32_t)(k * 2100) - t * 1040) >> 8)) >> 7;  // pulse moving down
            int sway = (k * 56 >> 8) * (sinQ((uint32_t)(k * 960) - t * 420) >> 8) >> 7;
            if (sway > 96) sway = 96;
            if (sway < -96) sway = -96;
            // soft taper at the free ends
            if (!st.attached) { int e = (y - st.tailY) * 80; if (e < hw) hw = e; }
            int eb = (st.headY - y) * 80 + 40;
            if (st.headV > 0 && eb < hw) hw = eb;
            // near the mound the thread opens slightly into the peak
            int foot = yB - y;
            if (foot < 7 && st.headV == 0) hw += (7 - foot) * (7 - foot) * 5;
            if (hw < 16) continue;
            int cq = (a.cx << 8) + sway;                    // centre of the glass in Q8 screen x
            threadRow(s, y, cq, hw, col);
        }
    }
    // ---- no thread: the charge hangs in the neck as a small round meniscus, not a flat cut
    if (p < 0.998f && !st.attached) {
        static const uint16_t MEN[5] = {1240, 1185, 1040, 800, 430};   // half width Q8
        for (int k = 0; k < 5; k++) threadRow(s, a.neck + 1 + k, a.cx << 8, MEN[k], st.colTop);
    }

    // ---- the ink drop itself
    const Drop& d = st.drop;
    if (d.mode == 1 || d.mode == 2) {
        s.disc(d.x, d.y, d.r, st.colInk);
        if (d.mode == 2) {
            float tail = d.vy * 0.9f;
            if (tail > 4) tail = 4;
            s.disc(d.x, d.y - d.r - tail * 0.35f, d.r * 0.55f, st.colInk);
            s.disc(d.x, d.y - d.r - tail * 0.8f, d.r * 0.25f, st.colInk);
        } else {
            // the neck of ink connecting it to the cap
            s.disc(d.x, a.top + 0.6f, d.r * 0.9f + 0.4f, st.colInk);
        }
    }
}

}  // namespace zen

static const Scene SCENE_zen = {
    "Zen Ink", "Still paper, one slow drop.", &ART_zen,
    {C565(36, 32, 30), C565(104, 94, 84), C565(186, 88, 50), C565(50, 116, 86), C565(216, 208, 192), C565(250, 245, 235), C565(170, 52, 42)},
    {200, 100, 60},
    zen::init, zen::tick, zen::draw,
};
