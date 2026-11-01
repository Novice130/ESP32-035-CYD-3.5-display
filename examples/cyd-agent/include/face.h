// ==============================================================================
//  face.h - cute procedural faces, drawn into the 240x160 framebuffer.
//  Everything is anti-aliased floats so the face can squash and morph smoothly at
//  2x on the panel. Pure C++ (runs in the firmware and in tools/sim.cpp).
// ==============================================================================
#pragma once
#include <math.h>
#include "gfx.h"

enum Friend : uint8_t { FR_MOCHI, FR_NEKO, FR_USAGI, FR_KUMA, FR_COUNT };
enum EyeStyle : uint8_t { EYE_ROUND, EYE_HAPPY, EYE_HEART, EYE_STAR, EYE_SLEEP, EYE_SPIRAL, EYE_WINK };
enum MouthStyle : uint8_t { MOUTH_SMILE, MOUTH_CAT, MOUTH_OPEN, MOUTH_O, MOUTH_FROWN, MOUTH_WAVY, MOUTH_POUT };

struct Palette {
    uint32_t bgTop, bgBot;
    uint16_t body, bodyShade, outline, accent, blush;
};

static inline uint16_t rgb24(uint32_t c) { return RGB(c >> 16, (c >> 8) & 255, c & 255); }

static inline Palette friendPalette(Friend f) {
    switch (f) {
        case FR_NEKO:  return {0xFFE3B8, 0xFFB98A, rgb24(0xFFC77D), rgb24(0xF5A65B), rgb24(0x6B3F2A), rgb24(0xFFF1DC), rgb24(0xFF7F6E)};
        case FR_USAGI: return {0xD9F0FF, 0xB5C8FF, rgb24(0xFFFBF4), rgb24(0xE6DDEB), rgb24(0x5C4A72), rgb24(0xFFB7CE), rgb24(0xFF9DBB)};
        case FR_KUMA:  return {0xD8F5D0, 0x9FDDB6, rgb24(0xC99A6B), rgb24(0xAD7D50), rgb24(0x4F3322), rgb24(0xF2DDBD), rgb24(0xF08A7B)};
        default:       return {0xDDF6F2, 0xA7E0D8, rgb24(0xFFFCF3), rgb24(0xEFE6D2), rgb24(0x41606A), rgb24(0xFFE2B8), rgb24(0xFFA88C)};
    }
}

static inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// filled superellipse (n = 2 is an ellipse, higher = squarer, mochi-like) with 4 sub-rows of AA
static void aaBlob(float cx, float cy, float rx, float ry, float n, uint16_t top, uint16_t bot) {
    if (rx <= 0 || ry <= 0) return;
    int y0 = (int)floorf(cy - ry), y1 = (int)ceilf(cy + ry);
    for (int y = y0; y <= y1; y++) {
        if ((unsigned)y >= FB_H) continue;
        float w[4], lo = 1e9f, hi = -1;
        for (int s = 0; s < 4; s++) {
            float t = fabsf((y + (s + 0.5f) / 4 - 0.5f - cy) / ry);
            w[s] = t >= 1 ? -1 : n == 2 ? rx * sqrtf(1 - t * t) : rx * powf(1 - powf(t, n), 1 / n);
            if (w[s] < lo) lo = w[s];
            if (w[s] > hi) hi = w[s];
        }
        if (hi < 0) continue;
        uint16_t c = top == bot ? top : blend565(top, bot, (int)((y - y0) * 256 / (y1 - y0 + 1)));
        int outer = (int)ceilf(hi) + 1, inner = lo > 1 ? (int)lo - 1 : 0;
        int icx = (int)floorf(cx + 0.5f);
        float frac = cx - icx;
        for (int dx = -outer; dx <= outer; dx++) {
            int x = icx + dx;
            if ((unsigned)x >= FB_W) continue;
            float d = fabsf(dx - frac);
            if (lo > 0 && d < inner) { fb[y * FB_W + x] = c; continue; }
            float cov = 0;
            for (int s = 0; s < 4; s++) cov += clamp01(w[s] - d + 0.5f);
            int a = (int)(cov * 64);
            if (a > 0) pblend(x, y, c, a);
        }
    }
}

static inline void aaDisc(float cx, float cy, float r, uint16_t c) { aaBlob(cx, cy, r, r, 2, c, c); }
static inline void aaOval(float cx, float cy, float rx, float ry, uint16_t c) { aaBlob(cx, cy, rx, ry, 2, c, c); }

// round-capped line, by distance to the segment
static void aaStroke(float x0, float y0, float x1, float y1, float r, uint16_t c) {
    int bx0 = (int)floorf((x0 < x1 ? x0 : x1) - r - 1), bx1 = (int)ceilf((x0 > x1 ? x0 : x1) + r + 1);
    int by0 = (int)floorf((y0 < y1 ? y0 : y1) - r - 1), by1 = (int)ceilf((y0 > y1 ? y0 : y1) + r + 1);
    float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    for (int y = by0; y <= by1; y++) {
        if ((unsigned)y >= FB_H) continue;
        for (int x = bx0; x <= bx1; x++) {
            if ((unsigned)x >= FB_W) continue;
            float t = len2 > 0 ? clamp01(((x - x0) * dx + (y - y0) * dy) / len2) : 0;
            float ex = x - x0 - t * dx, ey = y - y0 - t * dy;
            int a = (int)(clamp01(r + 0.5f - sqrtf(ex * ex + ey * ey)) * 256);
            if (a > 0) pblend(x, y, c, a);
        }
    }
}

// quadratic curve as short strokes
static void aaCurve(float x0, float y0, float qx, float qy, float x1, float y1, float r, uint16_t c) {
    const int N = 8;
    float px = x0, py = y0;
    for (int i = 1; i <= N; i++) {
        float t = (float)i / N, u = 1 - t;
        float x = u * u * x0 + 2 * u * t * qx + t * t * x1, y = u * u * y0 + 2 * u * t * qy + t * t * y1;
        aaStroke(px, py, x, y, r, c);
        px = x; py = y;
    }
}

// filled triangle with 1 px soft edges, by edge distance
static void aaTri(float ax, float ay, float bx, float by, float cx, float cy, uint16_t c) {
    float minx = fminf(ax, fminf(bx, cx)), maxx = fmaxf(ax, fmaxf(bx, cx));
    float miny = fminf(ay, fminf(by, cy)), maxy = fmaxf(ay, fmaxf(by, cy));
    float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (area == 0) return;
    float sign = area > 0 ? 1.f : -1.f;
    float l0 = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    float l1 = sqrtf((cx - bx) * (cx - bx) + (cy - by) * (cy - by));
    float l2 = sqrtf((ax - cx) * (ax - cx) + (ay - cy) * (ay - cy));
    for (int y = (int)miny - 1; y <= (int)maxy + 1; y++) {
        if ((unsigned)y >= FB_H) continue;
        for (int x = (int)minx - 1; x <= (int)maxx + 1; x++) {
            if ((unsigned)x >= FB_W) continue;
            float d0 = sign * ((bx - ax) * (y - ay) - (by - ay) * (x - ax)) / l0;
            float d1 = sign * ((cx - bx) * (y - by) - (cy - by) * (x - bx)) / l1;
            float d2 = sign * ((ax - cx) * (y - cy) - (ay - cy) * (x - cx)) / l2;
            int a = (int)(clamp01(fminf(d0, fminf(d1, d2)) + 0.5f) * 256);
            if (a > 0) pblend(x, y, c, a);
        }
    }
}

static void drawHeart(float x, float y, float s, uint16_t c) {
    aaDisc(x - s * 0.5f, y - s * 0.25f, s * 0.58f, c);
    aaDisc(x + s * 0.5f, y - s * 0.25f, s * 0.58f, c);
    aaTri(x - s * 1.03f, y, x + s * 1.03f, y, x, y + s * 1.15f, c);
}

static void drawStar(float x, float y, float s, uint16_t c) {
    // four-point sparkle with a round heart: reads better than five points when small
    aaTri(x - s, y, x, y - s * 0.22f, x, y + s * 0.22f, c);
    aaTri(x + s, y, x, y - s * 0.22f, x, y + s * 0.22f, c);
    aaTri(x, y - s, x - s * 0.22f, y, x + s * 0.22f, y, c);
    aaTri(x, y + s, x - s * 0.22f, y, x + s * 0.22f, y, c);
    aaDisc(x, y, s * 0.3f, c);
}

// ---------------------------------------------------------------------- the pose
struct FacePose {
    Friend who = FR_MOCHI;
    EyeStyle eye = EYE_ROUND;
    MouthStyle mouth = MOUTH_SMILE;
    float x = 120, y = 86;     // body centre
    float scale = 1;
    float squash = 0;          // +wide and short, -tall and thin
    float open = 1;            // eye openness, 0 = shut (blink)
    float lookX = 0, lookY = 0;// -1..1
    float blush = 0.5f;
    float mouthSize = 1;
    float earWiggle = 0;       // radians-ish, ears only
    int tick = 0;
};

static void drawEars(const FacePose& p, const Palette& pal, float rx, float ry, bool outlinePass) {
    float s = p.scale, grow = outlinePass ? 2.0f : 0;
    uint16_t c = outlinePass ? pal.outline : pal.body;
    float w = p.earWiggle * 10 * s;
    switch (p.who) {
        case FR_NEKO:
            for (int side = -1; side <= 1; side += 2) {
                float bx = p.x + side * rx * 0.62f, by = p.y - ry * 0.74f;
                float tx = p.x + side * (rx * 0.82f + w), ty = p.y - ry * 1.32f;
                float ox = side * grow, ix = bx - side * 21 * s, ex = bx + side * 15 * s;
                aaTri(ix - ox, by + grow, ex + ox, by + 8 * s + grow, tx + ox, ty - grow * 1.6f, c);
                if (!outlinePass)
                    aaTri(bx - side * 11 * s, by - 1 * s, bx + side * 8 * s, by + 3 * s,
                          p.x + side * (rx * 0.80f + w), ty + 11 * s, pal.blush);
            }
            break;
        case FR_USAGI:
            for (int side = -1; side <= 1; side += 2) {
                float bx = p.x + side * rx * 0.40f, by = p.y - ry * 0.72f;
                float lean = side * 7 * s + w;
                float hx = bx + lean, hy = by - 13 * s;
                aaOval(hx, hy, 11.5f * s + grow, 24 * s + grow, c);
                if (!outlinePass) aaOval(hx, hy + 1 * s, 5.5f * s, 16 * s, pal.accent);
            }
            break;
        case FR_KUMA:
            for (int side = -1; side <= 1; side += 2) {
                float ex = p.x + side * (rx * 0.74f + w * 0.4f), ey = p.y - ry * 0.78f;
                aaDisc(ex, ey, 17 * s + grow, c);
                if (!outlinePass) aaDisc(ex, ey + 1 * s, 9.5f * s, pal.accent);
            }
            break;
        default: {  // mochi: little sprout on top, so it still has a silhouette
            float tx = p.x + w * 0.6f, ty = p.y - ry;
            aaCurve(tx, ty + 2 * s, tx - 1, ty - 8 * s, tx + 5 * s, ty - 14 * s, 1.6f * s + grow * 0.7f,
                    outlinePass ? pal.outline : rgb24(0x7CC68A));
            if (!outlinePass) aaOval(tx + 8 * s, ty - 15 * s, 6 * s, 3.5f * s, rgb24(0x8FD99C));
            break;
        }
    }
}

static void drawEye(const FacePose& p, const Palette& pal, float ex, float ey, EyeStyle st, bool left) {
    float s = p.scale;
    uint16_t ink = RGB(44, 30, 52);
    float r = 2.2f * s;
    switch (st) {
        case EYE_HAPPY:
            aaCurve(ex - 11 * s, ey + 4 * s, ex, ey - 11 * s, ex + 11 * s, ey + 4 * s, r, ink);
            break;
        case EYE_SLEEP:
            aaCurve(ex - 11 * s, ey - 1 * s, ex, ey + 9 * s, ex + 11 * s, ey - 1 * s, r, ink);
            break;
        case EYE_HEART: {
            float beat = 1 + 0.08f * sinf(p.tick * 0.35f);
            drawHeart(ex, ey - 2 * s, 11.5f * s * beat, RGB(240, 70, 112));
            aaDisc(ex - 5 * s, ey - 6 * s, 2.5f * s, 0xFFFF);
            break;
        }
        case EYE_STAR: {
            float tw = 1 + 0.1f * sinf(p.tick * 0.4f);
            aaOval(ex, ey, 13 * s, 15 * s, ink);
            drawStar(ex, ey - 1 * s, 12 * s * tw, RGB(255, 222, 90));
            break;
        }
        case EYE_SPIRAL: {
            float a = p.tick * 0.28f * (left ? 1 : -1), px = ex, py = ey;
            aaOval(ex, ey, 13 * s, 13 * s, 0xFFFF);
            for (int i = 1; i <= 22; i++) {
                float t = i / 22.f, rad = t * 11 * s, ang = a + t * 11;
                float x = ex + cosf(ang) * rad, y = ey + sinf(ang) * rad;
                aaStroke(px, py, x, y, 0.9f * s, ink);
                px = x; py = y;
            }
            break;
        }
        case EYE_WINK:
            if (!left) {
                aaCurve(ex - 11 * s, ey + 4 * s, ex, ey - 11 * s, ex + 11 * s, ey + 4 * s, r, ink);
                break;
            }
            // fall through: the other eye stays round
        default: {
            float ry = 15.5f * s * p.open, rx = 13 * s;
            if (p.open < 0.16f) {  // a blink is a soft little line, not a vanished eye
                aaCurve(ex - 11 * s, ey, ex, ey + 4 * s, ex + 11 * s, ey, r, ink);
                break;
            }
            float lx = p.lookX * 3.5f * s, ly = p.lookY * 3.5f * s;
            aaOval(ex + lx, ey + ly, rx, ry, ink);
            // warm iris glow at the bottom, the anime-eye trick
            aaOval(ex + lx, ey + ly + ry * 0.48f, rx * 0.68f, ry * 0.40f, blend565(ink, pal.blush, 150));
            if (p.open > 0.45f) {
                float bob = 0.6f * sinf(p.tick * 0.09f);
                aaOval(ex + lx - 4.5f * s, ey + ly - 6 * s * p.open + bob, 4.8f * s, 5.8f * s * p.open, 0xFFFF);
                aaDisc(ex + lx + 5 * s, ey + ly + 5 * s * p.open - bob, 2.2f * s, 0xFFFF);
            }
            break;
        }
    }
}

static void drawMouth(const FacePose& p, const Palette& pal, float mx, float my) {
    float s = p.scale * p.mouthSize, r = 1.7f * p.scale;
    uint16_t ink = RGB(66, 34, 52);
    switch (p.mouth) {
        case MOUTH_CAT:
            aaCurve(mx - 9 * s, my, mx - 4.5f * s, my + 7 * s, mx, my + 1 * s, r, ink);
            aaCurve(mx, my + 1 * s, mx + 4.5f * s, my + 7 * s, mx + 9 * s, my, r, ink);
            break;
        case MOUTH_OPEN:
            // big D-shaped laugh: flat on top, round below
            for (int i = 0; i <= 10; i++) {
                float t = i / 10.f, w = 10.5f * s * sqrtf(1 - t * t);
                aaStroke(mx - w, my - 1 * s + t * 12 * s, mx + w, my - 1 * s + t * 12 * s, 0.9f * s, RGB(130, 38, 68));
            }
            aaOval(mx, my + 7.5f * s, 5.5f * s, 3.4f * s, RGB(255, 132, 150));
            break;
        case MOUTH_O:
            aaOval(mx, my + 2 * s, 4.6f * s, 5.4f * s, RGB(120, 36, 64));
            break;
        case MOUTH_FROWN:
            aaCurve(mx - 7 * s, my + 5 * s, mx, my - 3 * s, mx + 7 * s, my + 5 * s, r, ink);
            break;
        case MOUTH_WAVY: {
            float px = mx - 10 * s, py = my + 1;
            for (int i = 1; i <= 10; i++) {
                float x = mx - 10 * s + i * 2 * s, y = my + 1 + sinf(i * 1.3f + p.tick * 0.25f) * 2.2f * s;
                aaStroke(px, py, x, y, r * 0.8f, ink);
                px = x; py = y;
            }
            break;
        }
        case MOUTH_POUT:
            aaCurve(mx - 5 * s, my + 3 * s, mx, my - 2 * s, mx + 5 * s, my + 3 * s, r, ink);
            aaCurve(mx - 3 * s, my + 5 * s, mx, my + 3 * s, mx + 3 * s, my + 5 * s, r * 0.7f, ink);
            break;
        default:
            aaCurve(mx - 8 * s, my, mx, my + 9 * s, mx + 8 * s, my, r, ink);
            break;
    }
}

static void drawFriend(const FacePose& p) {
    Palette pal = friendPalette(p.who);
    float s = p.scale;
    float rx = 62 * s * (1 + p.squash), ry = 56 * s * (1 - p.squash);
    float n = p.who == FR_MOCHI ? 2.5f : 2.25f;

    // soft floor shadow
    blendEllipse((int)p.x, (int)(p.y + ry + 2), (int)(rx * 0.72f), (int)(6 * s), RGB(90, 60, 100), 60);

    drawEars(p, pal, rx, ry, true);
    drawEars(p, pal, rx, ry, false);
    aaBlob(p.x, p.y, rx + 2.2f, ry + 2.2f, n, pal.outline, pal.outline);
    aaBlob(p.x, p.y, rx, ry, n, pal.body, pal.bodyShade);

    // glossy highlight
    blendEllipse((int)(p.x - rx * 0.46f), (int)(p.y - ry * 0.62f), (int)(15 * s), (int)(8 * s), 0xFFFF, 150);

    float ey = p.y + ry * 0.02f + p.lookY * 1.5f * s;
    float ex = rx * 0.47f;
    float mx = p.x + p.lookX * 2 * s, my = p.y + ry * 0.32f;

    if (p.who == FR_KUMA) aaOval(mx, my + 3 * s, 19 * s, 14 * s, pal.accent);
    if (p.who == FR_NEKO) aaOval(mx, my + 4 * s, 22 * s, 15 * s, pal.accent);

    // blush: three soft rings make a smooth cheek without per-pixel gradients
    if (p.blush > 0.02f) {
        int a = (int)(p.blush * 90);
        for (int side = -1; side <= 1; side += 2) {
            int bx = (int)(p.x + side * rx * 0.70f), by = (int)(p.y + ry * 0.31f);
            blendEllipse(bx, by, (int)(13 * s), (int)(8 * s), pal.blush, a);
            blendEllipse(bx, by, (int)(9 * s), (int)(5 * s), pal.blush, a);
        }
    }

    if (p.who == FR_NEKO) {
        uint16_t w = pal.outline;
        for (int side = -1; side <= 1; side += 2) {
            float wx = p.x + side * rx * 0.86f, wy = p.y + ry * 0.24f;
            aaStroke(wx, wy - 3 * s, wx + side * 17 * s, wy - 7 * s, 0.7f, w);
            aaStroke(wx, wy + 3 * s, wx + side * 17 * s, wy + 5 * s, 0.7f, w);
        }
    }

    drawEye(p, pal, p.x - ex, ey, p.eye, true);
    drawEye(p, pal, p.x + ex, ey, p.eye, false);

    if (p.who == FR_KUMA) aaOval(mx, my - 1 * s, 3.8f * s, 2.8f * s, pal.outline);
    drawMouth(p, pal, mx, p.who == FR_KUMA ? my + 3 * s : my);
}
