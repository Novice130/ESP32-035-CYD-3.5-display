// Desktop simulator: runs the exact game code and dumps frames as PPM (2x scaled,
// like the real panel) so the graphics and the demo player can be checked without
// hardware.
//
//   clang++ -std=c++17 -O2 -I../include sim.cpp -o sim
//   ./sim OUT_DIR TICKS EVERY [input ...]     e.g. ./sim out 900 15 100:j3 160:j12 200:r 260:f
//     tick:jN  press JUMP and hold it N ticks     tick:f  FIRE     tick:r  RUN toggle
//     @N       only dump frames from tick N on (the demo still plays from tick 1)
//     #N       random seed (default 12345, same as --stats)
//     inf      endless lives (to reach a late level the way --stats does)
//   ./sim --stats TICKS [seed]                    headless demo run with endless lives,
//                                                 prints clears/deaths for every level
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

void plat_sfx(uint8_t, uint16_t) {}
void plat_led(uint8_t, uint8_t, uint8_t, uint16_t) {}

static void dump(const char* dir, int n) {
    char path[512];
    snprintf(path, sizeof(path), "%s/f%05d.ppm", dir, n);
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", FB_W * 2, FB_H * 2);
    for (int y = 0; y < FB_H * 2; y++)
        for (int x = 0; x < FB_W * 2; x++) {
            uint16_t c = fb[(y / 2) * FB_W + x / 2];
            uint8_t rgb[3] = {R8(c), G8(c), B8(c)};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

// log the interesting state changes
static void watch(bool verbose) {
    static int ls = -1, lst = -1, lli = -1, llives = -1, lform = -1;
    if (scene != ls || m.state != lst || li != lli || lives != llives || m.form != lform) {
        if (verbose || m.state == M_DYING || scene != ls)
            printf("[%6u] scene=%d level=%s state=%d form=%d lives=%d x=%d score=%u\n", tick, scene,
                   LV->name, m.state, m.form, lives, bx(m.b) / 16, score);
        ls = scene; lst = m.state; lli = li; llives = lives; lform = m.form;
    }
}

int main(int argc, char** argv) {
    static uint16_t buf[FB_W * FB_H];
    fb = buf;

    if (argc >= 3 && !strcmp(argv[1], "--stats")) {
        int ticks = atoi(argv[2]);
        game_init(argc > 3 ? atoi(argv[3]) : 12345);
        int deaths[LEVEL_COUNT] = {0}, clears[LEVEL_COUNT] = {0}, stuck = 0;
        int lastState = -1, lastLi = li, lastX = 0, sameX = 0;
        for (int t = 0; t < ticks; t++) {
            if (lives < 3) lives = 3;  // never game over, so every level gets played
            game_tick(Input{});
            if (m.state == M_DYING && lastState != M_DYING) {
                deaths[li]++;
                printf("[%6u] death on %s at col %d (form was small, timeLeft=%d)\n", tick, LV->name, bx(m.b) / 16, timeLeft);
            }
            if (li != lastLi && lastState == M_INCASTLE) { clears[lastLi]++; printf("[%6u] cleared %s score=%u\n", tick, LEVELS[lastLi].name, score); }
            if (scene == SC_PLAY && m.state == M_PLAY) {
                if (bx(m.b) == lastX) { if (++sameX == 60) { stuck++; printf("[%6u] stuck on %s at col %d\n", tick, LV->name, bx(m.b) / 16); } }
                else sameX = 0;
                lastX = bx(m.b);
            }
            lastState = m.state;
            lastLi = li;
        }
        for (int i = 0; i < LEVEL_COUNT; i++) printf("%s: clears %d deaths %d\n", LEVELS[i].name, clears[i], deaths[i]);
        printf("stuck events: %d\n", stuck);
        return 0;
    }

    if (argc < 4) { fprintf(stderr, "usage: sim OUT TICKS EVERY [tick:jN|f|r ...] | sim --stats TICKS [seed]\n"); return 1; }
    const char* out = argv[1];
    int ticks = atoi(argv[2]), every = atoi(argv[3]);
    int holdLeft = 0, from = 0, seed = 12345;
    bool endless = false;
    for (int a = 4; a < argc; a++) {
        if (argv[a][0] == '@') from = atoi(argv[a] + 1);
        if (argv[a][0] == '#') seed = atoi(argv[a] + 1);
        if (!strcmp(argv[a], "inf")) endless = true;
    }
    game_init(seed);
    for (int t = 1; t <= ticks; t++) {
        Input in = {};
        if (endless && lives < 3) lives = 3;
        if (holdLeft > 0) { in.jumpHeld = true; holdLeft--; }
        for (int a = 4; a < argc; a++) {
            int at, n = 1;
            char k;
            if (sscanf(argv[a], "%d:%c%d", &at, &k, &n) >= 2 && at == t) {
                if (k == 'j') { in.jumpPressed = in.jumpHeld = true; holdLeft = n - 1; }
                if (k == 'f') in.firePressed = true;
                if (k == 'r') in.runPressed = true;
            }
        }
        game_tick(in);
        watch(true);
        if (t >= from && t % every == 0) {
            game_render();
            dump(out, t);
        }
    }
    return 0;
}
