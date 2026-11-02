// ==============================================================================
//  pomodoro.h - Pomodoro state machine, text cards and frame composer.
//  Pure C++: runs identically in the macOS simulator and on the CYD.
// ==============================================================================
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hg_core.h"
#include "quotes.h"
#include "scenes.h"

enum PomodoroState : uint8_t { STATE_WORK, STATE_ASK_BREAK, STATE_BREAK };

enum TapType : uint8_t {
    TAP_NONE     = 0,
    TAP_DISPLAY  = 1,
    TAP_LDR_TAP  = 2, // brief LDR cover or long press on the display -> next scene
    TAP_LDR_HOLD = 3, // LDR held about 2 s (1.5 - 4.9 s) -> start again: continue if stopped, else restart the session
    TAP_LDR_STOP = 4  // LDR held 5 s -> stop the timer
};

enum SfxId : uint8_t { SFX_NONE = 0, SFX_DRIP = 1, SFX_TAP = 2, SFX_CHIME = 3, SFX_MILESTONE = 4, SFX_COMPLETE = 5 };

// 1m, 5m, 10m, 15m, 20m, 25m, 30m (default), 45m, 60m, 75m, 90m, 105m, 120m
static const uint16_t DURATIONS_MIN[] = {1, 5, 10, 15, 20, 25, 30, 45, 60, 75, 90, 105, 120};
static const uint8_t NUM_DURATIONS = sizeof(DURATIONS_MIN) / sizeof(DURATIONS_MIN[0]);
static const uint8_t DEFAULT_DURATION_INDEX = 6;

// Set by the platform layer before game_init().
static uint16_t* heroBuf = nullptr;  // HERO_MAX_PIXELS
static uint16_t* uiBuf = nullptr;    // UI_BUF_PIXELS
static uint8_t* sceneArena = nullptr;
#define UI_BUF_PIXELS (PANEL_W * 80)

class PomodoroEngine {
public:
    PomodoroState state;
    uint8_t durationIndex;
    uint32_t sessionTargetSec, sessionRemainingSec;
    uint32_t breakTargetSec, breakRemainingSec;
    uint32_t totalWorkSeconds;

    uint8_t currentScene;
    uint16_t sceneToastTicks;
    bool isPaused;

    uint8_t lastMilestoneTenth;
    int currentQuoteIndex;
    uint16_t quoteDisplayTicks;
    uint8_t quoteFade;     // 0..15 ticks into the fade-in
    uint16_t praiseDisplayTicks;
    char praiseText[64];

    uint8_t askBreakTimeoutSec;
    uint32_t askBreakTicks;
    uint16_t timeToastTicks;

    uint32_t tick;
    uint8_t subTick;       // 0..29, position inside the current second

    float visualProgress;  // what the scene shows; runs back up quickly at a restart
    float breakMix;

    int wipeX;             // >=0 while the new background is being wiped onto the panel
    bool panelFull;        // repaint the whole panel
    uint32_t heroMicros;   // for the serial log

    // Panel bands: only repainted when their signature changes
    enum { BAND_HEAD, BAND_TIME, BAND_BAR, BAND_TEXT, NUM_BANDS };
    uint32_t bandSig[NUM_BANDS];

    SceneCtx ctx;

    static uint32_t calcBreakSec(uint16_t workMin) {
        if (workMin <= 30) return 5 * 60;
        if (workMin <= 60) return 10 * 60;
        if (workMin <= 90) return 15 * 60;
        return 20 * 60;
    }

    void init(uint32_t seed = 42) {
        srand(seed);
        hg_tables_init();
        state = STATE_WORK;
        isPaused = false;
        durationIndex = DEFAULT_DURATION_INDEX;
        sessionTargetSec = DURATIONS_MIN[durationIndex] * 60;
        sessionRemainingSec = sessionTargetSec;
        breakTargetSec = calcBreakSec(DURATIONS_MIN[durationIndex]);
        breakRemainingSec = breakTargetSec;
        totalWorkSeconds = 0;
        lastMilestoneTenth = 0;
        currentQuoteIndex = rand() % NUM_QUOTES;
        quoteDisplayTicks = 0;
        quoteFade = 0;
        praiseDisplayTicks = 0;
        praiseText[0] = 0;
        askBreakTimeoutSec = 15;
        askBreakTicks = 0;
        timeToastTicks = 0;
        tick = 0;
        subTick = 0;
        visualProgress = 0;
        breakMix = 0;
        heroMicros = 0;
        activateScene(0, false);
    }

    void activateScene(uint8_t i, bool wipe) {
        currentScene = i;
        const Scene& sc = *SCENES[i];
        memset(sceneArena, 0, SCENE_ARENA_BYTES);
        ctx.s.px = heroBuf;
        ctx.s.x0 = sc.art->dx; ctx.s.y0 = sc.art->dy;
        ctx.s.w = sc.art->dw;  ctx.s.h = sc.art->dh;
        ctx.mem = sceneArena;
        ctx.progress = visualProgress;
        ctx.breakMix = breakMix;
        ctx.paused = isPaused;
        ctx.tick = tick;
        ctx.remainSec = state == STATE_BREAK ? breakRemainingSec : sessionRemainingSec;
        sc.init(ctx);
        sceneToastTicks = 75;
        wipeX = wipe ? 0 : -1;
        if (!wipe) plat_push(0, 0, SCR_W, SCR_H, sc.art->bg, SCR_W);
        panelFull = true;
        memset(bandSig, 0xFF, sizeof(bandSig));
        memset(tileHash, 0xA5, sizeof(tileHash));
    }

    void nextScene() {
        activateScene((currentScene + 1) % NUM_SCENES, true);
        const Scene& sc = *SCENES[currentScene];
        plat_sfx(SFX_TAP, 2000 + currentScene * 150);
        plat_led(sc.led[0], sc.led[1], sc.led[2], 250);
    }

    void restartWorkSession() {
        state = STATE_WORK;
        isPaused = false;
        sessionTargetSec = DURATIONS_MIN[durationIndex] * 60;
        sessionRemainingSec = sessionTargetSec;
        breakTargetSec = calcBreakSec(DURATIONS_MIN[durationIndex]);
        lastMilestoneTenth = 0;
        subTick = 0;
        plat_sfx(SFX_TAP, 2400);
        plat_led(255, 200, 50, 400);
    }

    void cycleDuration() {
        durationIndex = (durationIndex + 1) % NUM_DURATIONS;
        sessionTargetSec = DURATIONS_MIN[durationIndex] * 60;
        sessionRemainingSec = sessionTargetSec;
        breakTargetSec = calcBreakSec(DURATIONS_MIN[durationIndex]);
        lastMilestoneTenth = 0;
        subTick = 0;
        timeToastTicks = 55;
        plat_sfx(SFX_TAP, 1800 + durationIndex * 80);
        plat_led(255, 195, 45, 150);
    }

    void advanceQuote() {
        currentQuoteIndex = (currentQuoteIndex + 1) % NUM_QUOTES;
        quoteDisplayTicks = 0;
        quoteFade = 0;
    }

    void triggerMilestone(uint8_t tenth) {
        lastMilestoneTenth = tenth;
        if (tenth >= 1 && tenth <= 10) {
            snprintf(praiseText, sizeof(praiseText), "%s", MILESTONE_PRAISES[tenth]);
            praiseDisplayTicks = 90;
            quoteFade = 0;
            advanceQuote();
            plat_sfx(SFX_MILESTONE, tenth);
            plat_led(255, 210, 60, 400);
        }
    }

    float realProgress() const {
        uint32_t target = state == STATE_BREAK ? breakTargetSec : sessionTargetSec;
        uint32_t rem = state == STATE_BREAK ? breakRemainingSec : sessionRemainingSec;
        if (state == STATE_ASK_BREAK) return 1.0f;
        if (!target) return 0;
        float p = (target - rem + subTick / (float)TICK_HZ) / (float)target;
        return p > 1.0f ? 1.0f : p;
    }

    // ---------------------------------------------------------------- 30 Hz
    void update(TapType tap) {
        tick++;
        // Input first, then everything else keeps animating
        if (tap == TAP_LDR_STOP) {
            if (!isPaused) { isPaused = true; plat_sfx(SFX_COMPLETE); plat_led(255, 0, 0, 800); }
        } else if (tap == TAP_LDR_TAP) {
            nextScene();
        } else if (tap == TAP_LDR_HOLD) {
            if (isPaused) { isPaused = false; plat_sfx(SFX_TAP, 2400); plat_led(0, 255, 100, 400); }
            else restartWorkSession();
        } else if (isPaused && tap == TAP_DISPLAY) {
            isPaused = false;
            plat_sfx(SFX_TAP, 2200);
            plat_led(0, 255, 100, 300);
        } else if (tap == TAP_DISPLAY) {
            if (state == STATE_WORK) {
                cycleDuration();
            } else if (state == STATE_ASK_BREAK) {
                state = STATE_BREAK;
                breakRemainingSec = breakTargetSec;
                subTick = 0;
                plat_sfx(SFX_TAP, 1500);
                plat_led(0, 220, 110, 400);
            } else {
                restartWorkSession();
            }
        }

        if (!isPaused) stepClock();

        if (timeToastTicks) timeToastTicks--;
        if (sceneToastTicks) sceneToastTicks--;
        if (praiseDisplayTicks) praiseDisplayTicks--;
        if (quoteFade < 15) quoteFade++;

        // Sand follows the clock; when the clock jumps back it races home instead of teleporting
        float real = realProgress();
        float gap = real - visualProgress;
        if (gap < -0.004f) visualProgress += gap * 0.09f - 0.002f;
        else if (gap > 0.02f) visualProgress += gap * 0.09f;
        else visualProgress = real;

        float wantBreak = state == STATE_BREAK ? 1.0f : 0.0f;
        breakMix += (wantBreak - breakMix) * 0.06f;
        if (fabsf(wantBreak - breakMix) < 0.004f) breakMix = wantBreak;

        ctx.progress = visualProgress;
        ctx.breakMix = breakMix;
        ctx.paused = isPaused;
        ctx.tick = tick;
        ctx.remainSec = state == STATE_BREAK ? breakRemainingSec : sessionRemainingSec;
        SCENES[currentScene]->tick(ctx);
    }

    void stepClock() {
        subTick++;
        bool second = subTick >= TICK_HZ;
        if (second) subTick = 0;

        if (state == STATE_WORK) {
            if (second) {
                totalWorkSeconds++;
                if (sessionRemainingSec > 0) {
                    sessionRemainingSec--;
                    uint32_t elapsed = sessionTargetSec - sessionRemainingSec;
                    uint8_t tenth = (uint8_t)((elapsed * 10) / sessionTargetSec);
                    if (tenth > lastMilestoneTenth && tenth <= 10) triggerMilestone(tenth);
                } else {
                    state = STATE_ASK_BREAK;
                    askBreakTicks = 0;
                    plat_sfx(SFX_COMPLETE);
                    plat_led(255, 160, 20, 800);
                    return;
                }
            }
            if (++quoteDisplayTicks >= TICK_HZ * 30) advanceQuote();
        } else if (state == STATE_ASK_BREAK) {
            askBreakTicks++;
            if (askBreakTicks >= (uint32_t)askBreakTimeoutSec * TICK_HZ) restartWorkSession();
        } else {
            if (second) {
                if (breakRemainingSec > 0) breakRemainingSec--;
                else { restartWorkSession(); plat_sfx(SFX_CHIME); return; }
            }
            if (++quoteDisplayTicks >= TICK_HZ * 25) advanceQuote();
        }
    }

    // ---------------------------------------------------------------- text cards
    // Top card: big clock on the left, status column on the right, progress bar under both.
    // Bottom card: quote, praise or the break question. Both stay clear of the crack at y=240.
    struct Band { int16_t x, y, w, h; };
    static const Band& bandRect(int band) {
        static const Band R[NUM_BANDS] = {
            {212, 7, 96, 58},    // BAND_HEAD
            {12, 5, 198, 60},    // BAND_TIME
            {14, 66, 292, 8},    // BAND_BAR
            {12, 404, PANEL_W, 72},  // BAND_TEXT
        };
        return R[band];
    }

    uint32_t percentDone() const {
        uint32_t rem = state == STATE_BREAK ? breakRemainingSec : sessionRemainingSec;
        uint32_t target = state == STATE_BREAK ? breakTargetSec : sessionTargetSec;
        return target ? (target - rem) * 100 / target : 0;
    }

    uint32_t bandSignature(int band) const {
        uint32_t rem = state == STATE_BREAK ? breakRemainingSec : sessionRemainingSec;
        bool blink = isPaused && ((tick / 15) & 1);
        switch (band) {
            case BAND_HEAD: return state | (isPaused << 2) | (durationIndex << 3) | ((timeToastTicks > 0) << 8)
                                   | (percentDone() << 9) | ((totalWorkSeconds / 60) << 16);
            case BAND_TIME: return rem | (blink << 20);
            case BAND_BAR: {
                uint32_t target = state == STATE_BREAK ? breakTargetSec : sessionTargetSec;
                return target ? (target - rem) * PANEL_W / target : 0;
            }
            default: {
                if (state == STATE_ASK_BREAK) return 0x40000000u | (askBreakTimeoutSec - askBreakTicks / TICK_HZ);
                if (sceneToastTicks) return 0x20000000u | currentScene;
                if (praiseDisplayTicks) return 0x10000000u | quoteFade | (lastMilestoneTenth << 8);
                return currentQuoteIndex | (quoteFade << 16);
            }
        }
    }

    void paintPanel(const Surf& s, int band) {
        const Scene& sc = *SCENES[currentScene];
        const UiTheme& th = sc.ui;
        bool brk = state == STATE_BREAK;
        uint16_t accent = brk ? th.accentBrk : th.accent;
        const Band& R = bandRect(band);

        if (band == BAND_HEAD) {
            const char* label = isPaused ? "PAUSED" : state == STATE_ASK_BREAK ? "DONE" : brk ? "BREAK" : "FOCUS";
            uint16_t chip = isPaused ? th.alert : accent;
            drawPill(s, R.x, R.y, textWidth(FONT_SMALL, label) + 20, 22, chip);
            drawText(s, R.x + 10, R.y + 2, FONT_SMALL, label, th.chipInk);
            char buf[24];
            snprintf(buf, sizeof(buf), "%u min", DURATIONS_MIN[durationIndex]);
            drawText(s, R.x, R.y + 22, FONT_TXT, buf, timeToastTicks ? accent : th.dim);
            snprintf(buf, sizeof(buf), "%u%%", (unsigned)percentDone());
            drawText(s, R.x + R.w - textWidth(FONT_SMALL, buf), R.y + 26, FONT_SMALL, buf, th.dim);
            uint32_t totalMin = totalWorkSeconds / 60;
            if (totalMin >= 60) snprintf(buf, sizeof(buf), "Work %uh %02um", (unsigned)(totalMin / 60), (unsigned)(totalMin % 60));
            else snprintf(buf, sizeof(buf), "Worked %um", (unsigned)totalMin);
            drawText(s, R.x, R.y + 42, FONT_SMALL, buf, th.dim);
        } else if (band == BAND_TIME) {
            uint32_t rem = brk ? breakRemainingSec : sessionRemainingSec;
            char buf[12];
            snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(rem / 60), (unsigned)(rem % 60));
            bool wide = rem / 60 > 99;   // 120:00 only fits in the smaller digits
            if (wide) snprintf(buf, sizeof(buf), "%u:%02u", (unsigned)(rem / 60), (unsigned)(rem % 60));
            const Font& big = wide ? FONT_BIG3 : FONT_BIG;
            bool hide = isPaused && ((tick / 15) & 1);
            drawText(s, R.x + 2, wide ? R.y - 7 : R.y - 18, big, buf, hide ? th.dim : th.ink);
        } else if (band == BAND_BAR) {
            uint32_t rem = brk ? breakRemainingSec : sessionRemainingSec;
            uint32_t target = brk ? breakTargetSec : sessionTargetSec;
            int fill = target ? (int)((target - rem) * (uint32_t)R.w / target) : 0;
            drawPill(s, R.x, R.y + 1, R.w, 6, th.track);
            if (fill >= 6) drawPill(s, R.x, R.y + 1, fill, 6, accent);
        } else {
            const int X = R.x + 2, RW = R.w - 4;
            int y = R.y;
            if (state == STATE_ASK_BREAK) {
                drawText(s, X, y - 2, FONT_MED, "Take a break?", accent);
                char buf[48];
                snprintf(buf, sizeof(buf), "%u minutes. Tap the screen to rest.", (unsigned)(breakTargetSec / 60));
                drawText(s, X, y + 28, FONT_TXT, buf, th.ink);
                snprintf(buf, sizeof(buf), "Next session starts in %us", (unsigned)(askBreakTimeoutSec - askBreakTicks / TICK_HZ));
                drawText(s, X, y + 52, FONT_SMALL, buf, th.dim);
            } else if (sceneToastTicks) {
                drawText(s, X, y + 2, FONT_MED, sc.name, accent);
                drawWrapped(s, X, y + 32, RW, 20, FONT_TXT, sc.tagline, th.ink, 2);
            } else if (praiseDisplayTicks) {
                drawText(s, X, y + 2, FONT_SMALL, "MILESTONE", accent);
                drawWrapped(s, X, y + 22, RW, 21, FONT_TXT, praiseText, th.ink, 2, quoteFade * 17);
            } else {
                const Quote& q = INSPIRATIONAL_QUOTES[currentQuoteIndex];
                int a = quoteFade * 17;
                int bottom = drawWrapped(s, X, y - 2, RW, 19, FONT_TXT, q.text, th.ink, 3, a);
                char buf[64];
                snprintf(buf, sizeof(buf), "- %s", q.author);
                drawText(s, X, bottom + 2, FONT_SMALL, buf, th.dim, a);
            }
        }
    }

    void paintBand(int band) {
        const Band& R = bandRect(band);
        const SceneArt& a = *SCENES[currentScene]->art;
        const int maxRows = UI_BUF_PIXELS / R.w;
        for (int y = R.y; y < R.y + R.h; y += maxRows) {
            int h = R.y + R.h - y;
            if (h > maxRows) h = maxRows;
            Surf s = {uiBuf, R.x, y, R.w, h};
            const uint16_t* src = a.bg + y * SCR_W + R.x;
            for (int r = 0; r < h; r++) memcpy(uiBuf + r * R.w, src + r * SCR_W, R.w * 2);
            paintPanel(s, band);
            plat_push(R.x, y, R.w, h, uiBuf, R.w);
        }
    }

    // ---------------------------------------------------------------- dirty hero push
    // The rectangle is cut into tiles per row and each tile hashed. Rows whose changed range
    // matches the previous row are grouped, so a still bulb costs almost no SPI time.
    enum { HERO_TILES = 6 };
    uint16_t tileHash[HERO_ROWS][HERO_TILES];

    void pushHeroDirty(const SceneArt& a) {
        const int tw = (a.dw + HERO_TILES - 1) / HERO_TILES;
        int runY = -1, runA = 0, runB = 0;
        for (int y = 0; y <= a.dh; y++) {
            int first = HERO_TILES, last = -1;
            if (y < a.dh) {
                const uint16_t* row = heroBuf + y * a.dw;
                for (int t = 0; t < HERO_TILES; t++) {
                    int x0 = t * tw, x1 = x0 + tw;
                    if (x1 > a.dw) x1 = a.dw;
                    uint32_t h = 0;
                    for (int x = x0; x < x1; x++) h = h * 31 + row[x];
                    uint16_t hh = (uint16_t)(h ^ (h >> 16));
                    if (hh != tileHash[y][t]) {
                        tileHash[y][t] = hh;
                        if (t < first) first = t;
                        last = t;
                    }
                }
            }
            bool same = runY >= 0 && first == runA && last == runB;
            if (runY >= 0 && !same) {
                int x0 = runA * tw, x1 = (runB + 1) * tw;
                if (x1 > a.dw) x1 = a.dw;
                plat_push(a.dx + x0, a.dy + a.oy + runY, x1 - x0, y - runY, heroBuf + runY * a.dw + x0, a.dw);
                runY = -1;
            }
            if (runY < 0 && last >= 0) { runY = y; runA = first; runB = last; }
        }
    }

    // ---------------------------------------------------------------- one frame
    void render() {
        const Scene& sc = *SCENES[currentScene];
        const SceneArt& a = *sc.art;
        if (wipeX >= 0) {   // new background slides in, column by column
            int w = 32;
            if (wipeX + w > SCR_W) w = SCR_W - wipeX;
            plat_push(wipeX, 0, w, SCR_H, a.bg + wipeX, SCR_W);
            wipeX += w;
            if (wipeX >= SCR_W) { wipeX = -1; panelFull = true; memset(bandSig, 0xFF, sizeof(bandSig)); memset(tileHash, 0xA5, sizeof(tileHash)); }
            return;
        }
        art_copy_bg(ctx.s, a);
        sc.draw(ctx);
        art_apply_front(ctx.s, a);
        pushHeroDirty(a);

        for (int b = 0; b < NUM_BANDS; b++) {
            uint32_t sig = bandSignature(b);
            if (sig != bandSig[b]) { bandSig[b] = sig; paintBand(b); }
        }
    }
};

static PomodoroEngine pomodoro;

static inline void game_init(uint32_t seed) { pomodoro.init(seed); }
static inline void game_tick(TapType tap) { pomodoro.update(tap); }
static inline void game_render() { pomodoro.render(); }
