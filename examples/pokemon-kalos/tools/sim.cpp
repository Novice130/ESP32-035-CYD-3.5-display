// Desktop simulator: runs the exact game code and dumps frames as PPM (2x scaled,
// like the real panel) so the graphics can be reviewed without hardware.
//
//   clang++ -std=c++17 -O2 -I../include sim.cpp -o sim
//   ./sim OUT_DIR TICKS EVERY [tick:s|d ...]      e.g. ./sim out 900 15 100:s 400:d
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

void plat_sfx(uint8_t id, uint16_t param) { fprintf(stderr, "[%5u] sfx %d %d\n", tick, id, param); }
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

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: sim OUT TICKS EVERY [tick:s|d ...]\n"); return 1; }
    const char* out = argv[1];
    int ticks = atoi(argv[2]), every = atoi(argv[3]);
    static uint16_t buf[FB_W * FB_H];
    fb = buf;
    game_init(12345);
    for (int t = 1; t <= ticks; t++) {
        Tap tap = TAP_NONE;
        for (int a = 4; a < argc; a++) {
            int at; char k;
            if (sscanf(argv[a], "%d:%c", &at, &k) == 2 && at == t) tap = k == 'd' ? TAP_DOUBLE : TAP_SINGLE;
        }
        game_tick(tap);
        static int ls = -1, lb = -1;
        if (scene != ls || (scene == SC_BATTLE && bs != lb)) {
            fprintf(stderr, "[%5u] scene %d bs %d sp %s\n", tick, scene, scene == SC_BATTLE ? bs : -1, SPECIES[enemy.sp].name);
            ls = scene; lb = bs;
        }
        if (t % every == 0) {
            game_render();
            dump(out, t);
        }
    }
    return 0;
}
