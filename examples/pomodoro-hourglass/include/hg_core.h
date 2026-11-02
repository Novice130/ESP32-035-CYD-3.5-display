// ==============================================================================
//  hg_core.h - native 320x480 portrait scene engine for the Pomodoro Hourglass.
//
//  Each scene has a pre-rendered full-screen background and a "front" layer in
//  flash (made by tools/art_<scene>.py). Every frame only the dynamic rectangle
//  around the glass is rebuilt: copy background -> scene draws -> front layer ->
//  push. Pure C++, the same code runs on the ESP32 and in tools/sim.cpp.
//
//  Scenes work in "hero space": x is the screen x, y is the screen y minus art.oy.
//  So a scene never needs to know where on the screen its hourglass stands.
// ==============================================================================
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hg_fonts.h"

#define SCR_W 320
#define SCR_H 480
#define HERO_ROWS 320   // hero space is 320 rows tall
#define TICK_HZ 30

// In portrait the panel's crack is a horizontal line near y=240. The neck of the hourglass stands
// on it, the clock sits in a card above the glass and the quote in a card below it.
#define PANEL_W 296     // widest text band

#define HERO_MAX_PIXELS 40000   // 80 KB dynamic rectangle
#define SCENE_ARENA_BYTES 24576 // per-scene RAM (only the active scene uses it)

// Implemented by src/main.cpp on the board and tools/sim.cpp on the Mac.
// stride is in pixels. px may point into flash or RAM.
void plat_push(int x, int y, int w, int h, const uint16_t* px, int stride);
void plat_sfx(uint8_t id, uint16_t param = 0);
void plat_led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);

// ---------------------------------------------------------------- colour helpers
#define C565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define MASK565 0x07E0F81Fu
static inline uint32_t spread565(uint16_t c) { return (c | ((uint32_t)c << 16)) & MASK565; }
static inline uint16_t pack565(uint32_t v) { v &= MASK565; return (uint16_t)(v | (v >> 16)); }

// t: 0 = a, 255 = b
static inline uint16_t mix565(uint16_t a, uint16_t b, int t) {
    t = (t + 4) >> 3;
    if (t <= 0) return a;
    if (t >= 32) return b;
    return pack565((spread565(a) * (32 - t) + spread565(b) * t) >> 5);
}
// t: 0 = black, 255 = c
static inline uint16_t scale565(uint16_t c, int t) {
    t = (t + 4) >> 3;
    if (t <= 0) return 0;
    if (t >= 32) return c;
    return pack565((spread565(c) * t) >> 5);
}
// saturating add, for glows
static inline uint16_t add565(uint16_t a, uint16_t b) {
    uint32_t z = spread565(a) + spread565(b);
    if (z & (1u << 5)) z |= 0x1Fu;
    if (z & (1u << 16)) z |= 0xF800u;
    if (z & (1u << 27)) z |= 0x07E00000u;
    return pack565(z);
}

// ---------------------------------------------------------------- tables
static int8_t HG_SIN[256];     // HG_SIN[phase] = sin(phase/256*2pi) * 127
static uint8_t HG_NOISE[4096]; // 64x64 tiling white noise 0..255
static uint8_t HG_GLOW[256];   // HG_GLOW[d2 * 255 / r2] = soft falloff 255..0
static uint32_t hg_rng_state = 0x9E3779B9u;

static inline uint32_t hg_rand() { // xorshift32, deterministic
    uint32_t x = hg_rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return hg_rng_state = x;
}
static inline float hg_randf() { return (hg_rand() & 0xFFFF) / 65536.0f; }      // 0..1
static inline float hg_rands() { return hg_randf() * 2.0f - 1.0f; }             // -1..1
static inline int hg_isin(int phase) { return HG_SIN[phase & 255]; }            // -127..127
static inline int hg_icos(int phase) { return HG_SIN[(phase + 64) & 255]; }
static inline uint8_t hg_noise(int x, int y) { return HG_NOISE[(y & 63) * 64 + (x & 63)]; }

static void hg_tables_init() {
    for (int i = 0; i < 256; i++) HG_SIN[i] = (int8_t)(sinf(i * 6.2831853f / 256.0f) * 127.0f);
    hg_rng_state = 0x9E3779B9u;
    for (int i = 0; i < 4096; i++) HG_NOISE[i] = (uint8_t)(hg_rand() >> 11);
    for (int i = 0; i < 256; i++) {
        float u = 1.0f - i / 255.0f;
        HG_GLOW[i] = (uint8_t)(u * u * u * 255.0f);
    }
}

// ---------------------------------------------------------------- surface
// Screen coordinates, clipped to the rectangle this surface covers.
struct Surf {
    uint16_t* px;
    int x0, y0, w, h;

    inline bool in(int x, int y) const { return (unsigned)(x - x0) < (unsigned)w && (unsigned)(y - y0) < (unsigned)h; }
    inline uint16_t* at(int x, int y) const { return px + (y - y0) * w + (x - x0); }
    inline void put(int x, int y, uint16_t c) const { if (in(x, y)) *at(x, y) = c; }
    inline uint16_t get(int x, int y) const { return in(x, y) ? *at(x, y) : 0; }
    inline void mix(int x, int y, uint16_t c, int a) const { if (in(x, y)) { uint16_t* p = at(x, y); *p = mix565(*p, c, a); } }
    inline void add(int x, int y, uint16_t c) const { if (in(x, y)) { uint16_t* p = at(x, y); *p = add565(*p, c); } }

    void hspan(int x0_, int x1_, int y, uint16_t c) const {  // x1 inclusive
        if ((unsigned)(y - y0) >= (unsigned)h) return;
        if (x0_ < x0) x0_ = x0;
        if (x1_ >= x0 + w) x1_ = x0 + w - 1;
        if (x1_ < x0_) return;
        uint16_t* p = at(x0_, y);
        for (int n = x1_ - x0_ + 1; n > 0; n--) *p++ = c;
    }
    void hspanMix(int x0_, int x1_, int y, uint16_t c, int a) const {
        if ((unsigned)(y - y0) >= (unsigned)h) return;
        if (x0_ < x0) x0_ = x0;
        if (x1_ >= x0 + w) x1_ = x0 + w - 1;
        if (x1_ < x0_) return;
        uint16_t* p = at(x0_, y);
        for (int n = x1_ - x0_ + 1; n > 0; n--, p++) *p = mix565(*p, c, a);
    }
    // Soft additive glow. c is the colour at full strength, amp 0..255.
    void glow(int cx, int cy, int r, uint16_t c, int amp) const {
        if (r <= 0) return;
        int r2 = r * r;
        for (int dy = -r; dy <= r; dy++) {
            int y = cy + dy;
            if ((unsigned)(y - y0) >= (unsigned)h) continue;
            for (int dx = -r; dx <= r; dx++) {
                int d2 = dx * dx + dy * dy;
                if (d2 >= r2) continue;
                int x = cx + dx;
                if ((unsigned)(x - x0) >= (unsigned)w) continue;
                uint16_t* p = at(x, y);
                *p = add565(*p, scale565(c, (HG_GLOW[d2 * 255 / r2] * amp) >> 8));
            }
        }
    }
    // Filled circle with soft (anti-aliased) edge. Radius in pixels, centre may be fractional.
    void disc(float cx, float cy, float r, uint16_t c, int alpha = 255) const {
        int ix0 = (int)floorf(cx - r - 1), ix1 = (int)ceilf(cx + r + 1);
        int iy0 = (int)floorf(cy - r - 1), iy1 = (int)ceilf(cy + r + 1);
        float ro = r + 0.5f, ri = r - 0.5f;
        if (ri < 0) ri = 0;
        float ro2 = ro * ro, ri2 = ri * ri;
        for (int y = iy0; y <= iy1; y++) {
            if ((unsigned)(y - y0) >= (unsigned)h) continue;
            float dy = y - cy;
            for (int x = ix0; x <= ix1; x++) {
                if ((unsigned)(x - x0) >= (unsigned)w) continue;
                float dx = x - cx, d2 = dx * dx + dy * dy;
                if (d2 >= ro2) continue;
                int a = alpha;
                if (d2 > ri2) a = (int)(alpha * (ro - sqrtf(d2)));
                uint16_t* p = at(x, y);
                *p = mix565(*p, c, a);
            }
        }
    }
};

// ---------------------------------------------------------------- text
static inline const FontGlyph* fontFind(const Font& f, char ch) {
    for (int i = 0; i < f.count; i++) if (f.glyphs[i].ch == (uint8_t)ch) return &f.glyphs[i];
    return nullptr;
}

static int textWidth(const Font& f, const char* s) {
    int w = 0;
    for (; *s; s++) { const FontGlyph* g = fontFind(f, *s); if (g) w += g->adv; }
    return w;
}

// y = top of the line box. alpha 0..255 scales the whole string (for fades).
static int drawText(const Surf& s, int x, int y, const Font& f, const char* str, uint16_t ink, int alpha = 255) {
    for (; *str; str++) {
        const FontGlyph* g = fontFind(f, *str);
        if (!g) continue;
        const uint8_t* d = f.data + g->off;
        int gy = y + g->yo, gx = x + g->xo;
        for (int r = 0; r < g->h; r++, d += g->w) {
            int yy = gy + r;
            if ((unsigned)(yy - s.y0) >= (unsigned)s.h) continue;
            for (int c = 0; c < g->w; c++) {
                int a = d[c];
                if (!a) continue;
                int xx = gx + c;
                if ((unsigned)(xx - s.x0) >= (unsigned)s.w) continue;
                uint16_t* p = s.at(xx, yy);
                *p = mix565(*p, ink, alpha == 255 ? a : (a * alpha) >> 8);
            }
        }
        x += g->adv;
    }
    return x;
}

// Word wrap. Returns the y below the last line.
static int drawWrapped(const Surf& s, int x, int y, int maxW, int lineH, const Font& f, const char* str, uint16_t ink,
                       int maxLines, int alpha = 255) {
    char line[96];
    int lines = 0;
    const char* p = str;
    while (*p && lines < maxLines) {
        while (*p == ' ') p++;
        int len = 0, lastBreak = 0, w = 0;
        const char* q = p;
        while (*q && len < (int)sizeof(line) - 1) {
            const FontGlyph* g = fontFind(f, *q);
            int adv = g ? g->adv : 0;
            if (w + adv > maxW) break;
            if (*q == ' ') lastBreak = len;
            w += adv;
            line[len++] = *q++;
        }
        if (*q && lastBreak > 0) { len = lastBreak; q = p + len; }
        line[len] = 0;
        drawText(s, x, y, f, line, ink, alpha);
        y += lineH;
        lines++;
        p = q;
    }
    return y;
}

// Soft rounded pill
static void drawPill(const Surf& s, int x, int y, int w, int h, uint16_t c, int alpha = 255) {
    float r = h / 2.0f;
    for (int yy = 0; yy < h; yy++) {
        float dy = yy + 0.5f - r;
        float half = sqrtf(r * r - dy * dy);
        float left = r - half, right = w - r + half;
        for (int xx = (int)left; xx <= (int)right && xx < w; xx++) {
            float cov = 1.0f;
            if (xx < left + 1) cov = xx + 1 - left;
            else if (xx + 1 > right) cov = right - xx;
            if (cov > 0) s.mix(x + xx, y + yy, c, (int)(alpha * (cov > 1 ? 1 : cov)));
        }
    }
}

// ---------------------------------------------------------------- generated art
struct SceneArt {
    const uint16_t* bg;        // SCR_W * SCR_H RGB565, full screen, in flash (screen rows, not hero rows)
    const uint8_t* front;      // run-coded front layer (see art_apply_front)
    const uint32_t* frontIdx;  // offset of each row of the dynamic rectangle in front[]
    const int16_t* gl;         // gl[y], gr[y]: inner glass edges x*16 for every screen row; gl>=gr outside the glass
    const int16_t* gr;
    int16_t dx, dy, dw, dh;    // dynamic rectangle
    int16_t cx;                // glass centre x
    int16_t top, neck, bot;    // glass inner top row, narrowest row, bottom row
    int16_t oy;                // screen y of hero row 0
};

// Background pixels of a hero-space row.
static inline const uint16_t* art_bg_row(const SceneArt& a, int y) { return a.bg + (y + a.oy) * SCR_W; }

// Inner glass span on row y in pixels. false when the row has no glass.
static inline bool art_inner(const SceneArt& a, int y, int& xl, int& xr) {
    if (y < a.top || y > a.bot) return false;
    xl = (a.gl[y] + 15) >> 4;
    xr = a.gr[y] >> 4;
    return xr >= xl;
}
static inline float art_radius(const SceneArt& a, int y) {
    if (y < a.top || y > a.bot) return 0;
    return (a.gr[y] - a.gl[y]) / 32.0f;
}

static void art_copy_bg(Surf& s, const SceneArt& a) {
    const uint16_t* src = art_bg_row(a, a.dy) + a.dx;
    uint16_t* dst = s.px;
    for (int y = 0; y < a.dh; y++, src += SCR_W, dst += a.dw) memcpy(dst, src, a.dw * 2);
}

// row -> u8 nruns, then nruns of { u8 x (from dx), u8 len, len * {u8 lo, u8 hi, u8 alpha} }
static void art_apply_front(Surf& s, const SceneArt& a) {
    if (!a.front) return;
    for (int y = 0; y < a.dh; y++) {
        const uint8_t* p = a.front + a.frontIdx[y];
        int runs = *p++;
        uint16_t* row = s.px + y * a.dw;
        while (runs-- > 0) {
            int x = *p++, len = *p++;
            uint16_t* o = row + x;
            while (len-- > 0) {
                uint16_t c = (uint16_t)(p[0] | (p[1] << 8));
                int al = p[2];
                p += 3;
                *o = al == 255 ? c : mix565(*o, c, al);
                o++;
            }
        }
    }
}

// ---------------------------------------------------------------- sand volume maths
// Levels are computed from the real shape of the glass, so a bulb drains the way the eye expects:
// the level barely moves in the wide part and races through the narrow part.
struct GlassVolume {
    float upperTotal, lowerTotal, v0;
    const SceneArt* art;

    void build(const SceneArt& a, float fillRatio = 0.72f) {
        art = &a;
        upperTotal = lowerTotal = 0;
        for (int y = a.top; y <= a.neck; y++) { float r = art_radius(a, y); upperTotal += r * r; }
        for (int y = a.neck; y <= a.bot; y++) { float r = art_radius(a, y); lowerTotal += r * r; }
        v0 = (upperTotal < lowerTotal ? upperTotal : lowerTotal) * fillRatio;
    }
    // Flat level in the upper bulb when `remain` (0..1) of the charge is still up there.
    float upperLevel(float remain) const {
        const SceneArt& a = *art;
        float target = remain * v0, sum = 0;
        for (int y = a.neck; y >= a.top; y--) {
            float r = art_radius(a, y), v = r * r;
            if (sum + v >= target) return y + 1 - (target - sum) / (v > 0.01f ? v : 0.01f);
            sum += v;
        }
        return a.top;
    }
    // Flat level in the lower bulb when `fallen` (0..1) of the charge has landed.
    float lowerLevel(float fallen) const {
        const SceneArt& a = *art;
        float target = fallen * v0, sum = 0;
        for (int y = a.bot; y >= a.neck; y--) {
            float r = art_radius(a, y), v = r * r;
            if (sum + v >= target) return y + 1 - (target - sum) / (v > 0.01f ? v : 0.01f);
            sum += v;
        }
        return a.neck;
    }
};

// ---------------------------------------------------------------- scene interface
struct UiTheme {
    uint16_t ink;       // main text and digits
    uint16_t dim;       // secondary text
    uint16_t accent;    // focus colour: chip, progress bar
    uint16_t accentBrk; // break colour
    uint16_t track;     // progress bar track
    uint16_t chipInk;   // text on the accent chip
    uint16_t alert;     // paused
};

struct SceneCtx {
    Surf s;             // dynamic rectangle, already holds the background
    float progress;     // 0 = all charge on top, 1 = all at the bottom (smoothed, runs back at a restart)
    float breakMix;     // 0 = focus palette, 1 = break palette (fades over ~1 s)
    bool paused;        // flow must stop, what is in the air may finish
    uint32_t tick;      // counts up at 30 Hz, also while paused
    uint32_t remainSec; // seconds left on the clock, for in-scene readouts
    void* mem;          // SCENE_ARENA_BYTES of scratch RAM, zeroed before init
};

struct Scene {
    const char* name;        // shown on the screen when the scene starts, short
    const char* tagline;     // one short line under the name
    const SceneArt* art;
    UiTheme ui;
    uint8_t led[3];          // LED flash colour when the scene starts
    void (*init)(SceneCtx&); // called once whenever the scene becomes active
    void (*tick)(SceneCtx&); // 30 Hz simulation, no drawing
    void (*draw)(SceneCtx&); // draw into ctx.s; the front layer goes on top afterwards
};

// Scenes ask for a soft drip sound with this. SFX id matches pomodoro.h.
static inline void hg_drip(uint16_t hz) { plat_sfx(1, hz); }

#define SCENE_STATE(T, ctx) (*static_cast<T*>((ctx).mem))
