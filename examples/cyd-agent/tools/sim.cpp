// Desktop simulator: runs the real pet code and dumps 2x PPM frames, without hardware.
//
//   clang++ -std=c++17 -O2 -I../include sim.cpp -o sim
//   ./sim OUT TICKS EVERY [tick:s|d] [tick:r=<reply with ; for newlines>] [tick:l=<light>] [tick:f=<friend>] [tick:t=<stats reply>]
//   ./sim OUT 300 30 60:s "90:r=face love;say Hi there;led pink" 200:l=5
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

static int askOk = 1;

void plat_sfx(uint8_t id) { fprintf(stderr, "[%5u] sfx %d\n", tick, id); }
void plat_led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) { fprintf(stderr, "[%5u] led %d %d %d %d\n", tick, r, g, b, ms); }
bool plat_ask(const char* kind, const char* session) {
    fprintf(stderr, "[%5u] ask kind=%s session=%s -> %d\n", tick, kind, session, askOk);
    return askOk;
}

bool plat_stats() {
    fprintf(stderr, "[%5u] stats -> %d\n", tick, askOk);
    return askOk;
}

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
    if (argc < 4) { fprintf(stderr, "usage: sim OUT TICKS EVERY [events...]\n"); return 1; }
    const char* out = argv[1];
    int ticks = atoi(argv[2]), every = atoi(argv[3]);
    static uint16_t buf[FB_W * FB_H], bg[FB_W * FB_H];
    fb = buf;
    bgCache = bg;
    game_init(12345);
    int light = 80, lastMood = -1;
    for (int t = 1; t <= ticks; t++) {
        Tap tap = TAP_NONE;
        for (int a = 4; a < argc; a++) {
            int at, off = 0;
            char k;
            if (sscanf(argv[a], "%d:%c%n", &at, &k, &off) != 2 || at != t) continue;
            const char* rest = argv[a] + off;
            if (k == 's') tap = TAP_SINGLE;
            else if (k == 'd') tap = TAP_DOUBLE;
            else if (k == 'l') light = atoi(rest + 1);
            else if (k == 'f') pose.who = (Friend)atoi(rest + 1);
            else if (k == 'o') askOk = atoi(rest + 1);
            else if (k == 'x') game_reply_failed();
            else if (k == 'm') { setMood((Mood)atoi(rest + 1)); }
            else if (k == 'X') game_stats_failed();
            else if (k == 't') {
                char reply[900];
                snprintf(reply, sizeof(reply), "%s", rest + 1);
                for (char* c = reply; *c; c++) if (*c == ';') *c = '\n';
                game_apply_stats(reply);
            }
            else if (k == 'r') {
                char reply[512];
                snprintf(reply, sizeof(reply), "%s", rest + 1);
                for (char* c = reply; *c; c++) if (*c == ';') *c = '\n';
                game_apply_reply(reply);
            }
        }
        game_tick(tap, light);
        static int lastView = -1;
        if (view != lastView) { fprintf(stderr, "[%5u] view %d\n", tick, view); lastView = view; }
        if (mood != lastMood) { fprintf(stderr, "[%5u] mood %s\n", tick, MOOD_NAMES[mood]); lastMood = mood; }
        if (t % every == 0) {
            game_render();
            dump(out, t);
        }
    }
    return 0;
}
