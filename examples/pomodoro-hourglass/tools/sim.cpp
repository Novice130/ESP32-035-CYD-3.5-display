// ==============================================================================
//  sim.cpp - runs the real engine on the Mac against a fake 320x480 panel.
//
//  sim <outdir> <scene-index> <commands...>     writes <outdir>/<tag>.ppm
//    p=0.35          jump the session to 35 % and run 90 ticks so particles settle
//    run=60          run 60 ticks
//    shot=name       dump the panel as name.ppm
//    seq=name,8,3    dump 8 frames, 3 ticks apart, as name_0.ppm ...
//    break           jump to the break state (next p= is break progress)
//    work            back to focus
//    pause           pause / resume
//    scene           next scene (with wipe)
//    dur             tap: next duration
//    ask             jump to the take-a-break question
//    bench           print average draw time over 120 frames
//    praise          trigger the 50 % milestone
//    layers=name     dump the layers of one frame: name_bg, name_live (scene drawing only), name_front
// ==============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include "pomodoro.h"

static uint16_t panel[SCR_W * SCR_H];
static uint32_t pushedPixels = 0;
static int dripCount = 0;

void plat_push(int x, int y, int w, int h, const uint16_t* px, int stride) {
    pushedPixels += w * h;
    for (int r = 0; r < h; r++) memcpy(panel + (y + r) * SCR_W + x, px + r * stride, w * 2);
}
void plat_sfx(uint8_t id, uint16_t) { if (id == SFX_DRIP) dripCount++; }
void plat_led(uint8_t, uint8_t, uint8_t, uint16_t) {}

static void dump(const char* dir, const char* name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        uint16_t c = panel[i];
        uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void frame(TapType t = TAP_NONE) { game_tick(t); game_render(); }

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: sim outdir scene-index cmds...\n"); return 2; }
    const char* out = argv[1];
    static uint16_t hero[HERO_MAX_PIXELS], ui[UI_BUF_PIXELS];
    static uint8_t arena[SCENE_ARENA_BYTES];
    heroBuf = hero; uiBuf = ui; sceneArena = arena;
    game_init(42);
    int want = atoi(argv[2]);
    if (want < 0 || want >= NUM_SCENES) { fprintf(stderr, "scene %d of %d\n", want, NUM_SCENES); return 2; }
    if (want) pomodoro.activateScene(want, false);
    printf("scene %s: dyn %dx%d\n", SCENES[want]->name, SCENES[want]->art->dw, SCENES[want]->art->dh);

    for (int i = 3; i < argc; i++) {
        char* c = argv[i];
        if (!strncmp(c, "p=", 2)) {
            float p = atof(c + 2);
            if (pomodoro.state == STATE_BREAK) pomodoro.breakRemainingSec = (uint32_t)(pomodoro.breakTargetSec * (1 - p));
            else pomodoro.sessionRemainingSec = (uint32_t)(pomodoro.sessionTargetSec * (1 - p));
            pomodoro.lastMilestoneTenth = (uint8_t)(p * 10);
            pomodoro.visualProgress = pomodoro.realProgress();
            for (int k = 0; k < 90; k++) frame();
        } else if (!strncmp(c, "run=", 4)) {
            int n = atoi(c + 4);
            for (int k = 0; k < n; k++) frame();
        } else if (!strncmp(c, "shot=", 5)) {
            dump(out, c + 5);
        } else if (!strncmp(c, "seq=", 4)) {
            char name[128]; int n = 0, every = 1;
            char* comma = strchr(c + 4, ',');
            memcpy(name, c + 4, comma - (c + 4)); name[comma - (c + 4)] = 0;
            sscanf(comma + 1, "%d,%d", &n, &every);
            for (int k = 0; k < n; k++) {
                for (int t = 0; t < every; t++) frame();
                char tag[160]; snprintf(tag, sizeof(tag), "%s_%d", name, k);
                dump(out, tag);
            }
        } else if (!strcmp(c, "break")) {
            pomodoro.state = STATE_BREAK;
            pomodoro.breakRemainingSec = pomodoro.breakTargetSec;
        } else if (!strcmp(c, "work")) {
            pomodoro.state = STATE_WORK;
        } else if (!strcmp(c, "ask")) {
            pomodoro.state = STATE_ASK_BREAK;
            pomodoro.askBreakTicks = 0;
        } else if (!strcmp(c, "pause")) {
            frame(TAP_LDR_STOP);
        } else if (!strcmp(c, "scene")) {
            frame(TAP_LDR_TAP);
        } else if (!strcmp(c, "dur")) {
            frame(TAP_DISPLAY);
        } else if (!strcmp(c, "praise")) {
            pomodoro.triggerMilestone(5);
        } else if (!strncmp(c, "layers=", 7)) {
            const Scene& sc = *SCENES[pomodoro.currentScene];
            const SceneArt& a = *sc.art;
            char tag[160];
            memcpy(panel, a.bg, sizeof(panel));
            snprintf(tag, sizeof(tag), "%s_bg", c + 7); dump(out, tag);
            for (int pass = 0; pass < 2; pass++) {
                memset(panel, 0, sizeof(panel));
                memset(hero, 0, a.dw * a.dh * 2);
                if (pass == 0) sc.draw(pomodoro.ctx); else art_apply_front(pomodoro.ctx.s, a);
                for (int r = 0; r < a.dh; r++) memcpy(panel + (a.dy + a.oy + r) * SCR_W + a.dx, hero + r * a.dw, a.dw * 2);
                snprintf(tag, sizeof(tag), "%s_%s", c + 7, pass ? "front" : "live"); dump(out, tag);
            }
        } else if (!strcmp(c, "bench")) {
            using clk = std::chrono::steady_clock;
            const Scene& sc = *SCENES[pomodoro.currentScene];
            pomodoro.wipeX = -1;
            long tickNs = 0, drawNs = 0, maxDraw = 0;
            dripCount = 0;
            for (int k = 0; k < 120; k++) {
                auto t0 = clk::now();
                game_tick(TAP_NONE);
                auto t1 = clk::now();
                art_copy_bg(pomodoro.ctx.s, *sc.art);
                auto t2 = clk::now();
                sc.draw(pomodoro.ctx);
                art_apply_front(pomodoro.ctx.s, *sc.art);
                auto t3 = clk::now();
                long d = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count();
                tickNs += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                drawNs += d;
                if (d > maxDraw) maxDraw = d;
            }
            printf("bench: tick %ld us, draw+front avg %ld us, max %ld us, drips in 4 s %d\n", tickNs / 120000, drawNs / 120000, maxDraw / 1000, dripCount);
        } else {
            fprintf(stderr, "unknown command %s\n", c);
            return 2;
        }
    }
    return 0;
}
