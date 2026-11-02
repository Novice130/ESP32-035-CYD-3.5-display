// ==============================================================================
//  game.h - the pet: moods, demo loop, tap controls, the system monitor pages, and the
//  agent's "body". No Arduino deps. tools/agent_proxy.py supplies the computer's numbers
//  and, with --brain, an LLM whose tool calls this file runs.
//
//  Reply format, one tool call per line:
//      face love | say Hello there | led pink | sound chirp | more <session>
//  "more" means the LLM asked to read the sensors and is waiting for the answer.
// ==============================================================================
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "face.h"

#define TICK_HZ 30
#define IDLE_TO_DEMO_TICKS (30 * TICK_HZ)
#define MIN_ASK_GAP_TICKS (6 * TICK_HZ)       // taps can't spend API calls faster than this
#define LIGHT_ASK_GAP_TICKS (60 * TICK_HZ)    // nor can a flickering room light
#define ASK_TIMEOUT_TICKS (30 * TICK_HZ)
#define SAY_MAX 80

enum Tap : uint8_t { TAP_NONE, TAP_SINGLE, TAP_DOUBLE };
enum Sfx : uint8_t { SFX_TAP, SFX_CHIRP, SFX_HAPPY, SFX_LOVE, SFX_SAD, SFX_SURPRISE, SFX_SLEEPY, SFX_SWITCH };
enum Mood : uint8_t { MOOD_HAPPY, MOOD_LOVE, MOOD_EXCITED, MOOD_SLEEPY, MOOD_SURPRISED, MOOD_SAD, MOOD_POUT,
                      MOOD_SHY, MOOD_DIZZY, MOOD_WINK, MOOD_THINKING, MOOD_NEUTRAL, MOOD_COUNT };

// platform hooks
void plat_sfx(uint8_t id);
void plat_led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);
// start a request on the agent thread; session is "" for a new event. false = busy or offline.
bool plat_ask(const char* kind, const char* session);
// fetch the computer's numbers on the same thread. false = busy or offline.
bool plat_stats();

static const char* const MOOD_NAMES[MOOD_COUNT] = {"happy", "love", "excited", "sleepy", "surprised", "sad", "pout",
                                                   "shy", "dizzy", "wink", "thinking", "neutral"};
struct MoodStyle { EyeStyle eye; MouthStyle mouth; float blush, bounce, squash; };
static const MoodStyle MOODS[MOOD_COUNT] = {
    {EYE_HAPPY,  MOUTH_OPEN,  0.75f, 1.0f,  0.00f},  // happy
    {EYE_HEART,  MOUTH_CAT,   1.00f, 0.6f,  0.02f},  // love
    {EYE_STAR,   MOUTH_OPEN,  0.70f, 1.8f,  0.00f},  // excited
    {EYE_SLEEP,  MOUTH_O,     0.30f, 0.2f,  0.06f},  // sleepy
    {EYE_ROUND,  MOUTH_O,     0.20f, 0.0f, -0.05f},  // surprised
    {EYE_ROUND,  MOUTH_FROWN, 0.25f, 0.1f,  0.05f},  // sad
    {EYE_ROUND,  MOUTH_POUT,  0.90f, 0.2f,  0.04f},  // pout
    {EYE_HAPPY,  MOUTH_WAVY,  1.00f, 0.3f,  0.03f},  // shy
    {EYE_SPIRAL, MOUTH_WAVY,  0.35f, 0.5f,  0.00f},  // dizzy
    {EYE_WINK,   MOUTH_CAT,   0.70f, 0.7f,  0.00f},  // wink
    {EYE_ROUND,  MOUTH_SMILE, 0.40f, 0.2f,  0.00f},  // thinking
    {EYE_ROUND,  MOUTH_SMILE, 0.50f, 0.4f,  0.00f},  // neutral
};

struct DemoBeat { Mood mood; const char* line; uint8_t r, g, b; };
static const DemoBeat DEMO[] = {
    {MOOD_NEUTRAL,   nullptr,               0, 0, 0},
    {MOOD_HAPPY,     "Hi! I live in this tiny screen.", 0, 255, 120},
    {MOOD_WINK,      nullptr,               0, 0, 0},
    {MOOD_THINKING,  "Hmm... what's that light doing?", 0, 0, 0},
    {MOOD_EXCITED,   "Tap me! I can think out loud!", 255, 220, 0},
    {MOOD_LOVE,      nullptr,               255, 60, 140},
    {MOOD_SHY,       "Oh, you were watching...", 255, 60, 140},
    {MOOD_SLEEPY,    nullptr,               0, 0, 0},
    {MOOD_SURPRISED, "! Was that a noise?", 0, 160, 255},
    {MOOD_DIZZY,     nullptr,               0, 0, 0},
    {MOOD_POUT,      "Nobody tapped me yet.", 0, 0, 0},
    {MOOD_SAD,       nullptr,               0, 0, 0},
};
#define DEMO_COUNT (int)(sizeof(DEMO) / sizeof(DEMO[0]))

struct OfflineLine { Mood mood; const char* line; };
static const OfflineLine OFFLINE_POKE[] = {{MOOD_HAPPY, "Hehe, that tickles!"}, {MOOD_LOVE, "More head pats please!"},
                                           {MOOD_SHY, "Eep! Hi there."},        {MOOD_WINK, "Boop right back."},
                                           {MOOD_EXCITED, "Again! Again!"}};
#define OFFLINE_POKE_COUNT (int)(sizeof(OFFLINE_POKE) / sizeof(OFFLINE_POKE[0]))

// ------------------------------------------------------------------ state
static uint32_t tick = 0;
static uint32_t rngState = 1;
static bool playMode = false;
static FacePose pose;
static Mood mood = MOOD_NEUTRAL;
static Mood moodNext = MOOD_NEUTRAL;
static int swapT = 0;                 // >0 = the eyes-shut moment hiding a style change
static int blinkT = 0, blinkNext = 90;
static float lookTX = 0, lookTY = 0;
static int lookNext = 60;
static float blushNow = 0.5f, squashKick = 0, bounceKick = 0;
static float speakLift = 0;

static char sayBuf[SAY_MAX + 1] = "";
static int sayShown = 0, sayHold = 0;

static int demoIdx = 0, demoNext = 0, demoFriendIn = 2;
static uint32_t lastInput = 0, lastAsk = 0, lastLightAsk = 0;
static int pets = 0;

static bool asking = false;
static uint32_t askedAt = 0;
static char moreSession[24] = "";
static bool cloudOnline = false;
static bool moodByAgent = false;      // set_face arrived, so "thinking" has been replaced

static bool isDark = false;
static int darkFor = 0, brightFor = 0;

// ------------------------------------------------------------------ system monitor
enum View : uint8_t { V_PET, V_OVERVIEW, V_TOP_CPU, V_TOP_RAM, V_TOP_DISK, V_COUNT };
#define STAT_ROWS 5
#define STAT_NAME 21
#define STATS_REFRESH_TICKS TICK_HZ
#define MONITOR_IDLE_TICKS (120 * TICK_HZ)   // numbers are for watching: stays up longer than the pet
#define HIST_LEN 64

// This panel has a vertical crack near the middle. Text, data and the pet stay out of a gutter
// around it, on every screen. Crack elsewhere? Change CRACK_X; everything below follows it.
#define CRACK_X 120
#define GUTTER 8
#define COL_L_X 6
#define COL_L_W (CRACK_X - GUTTER - COL_L_X)
#define COL_R_X (CRACK_X + GUTTER)
#define COL_R_W (FB_W - 6 - COL_R_X)
#define REACT_TICKS (4 * TICK_HZ)           // how long the pet reacts before the numbers appear
#define BRAIN_WAIT_TICKS (8 * TICK_HZ)

struct AppRow { int cpu10, mb; char name[STAT_NAME]; };
struct Stats {
    bool valid, failed, brain, scanning;
    int cpu, ramPct, ramMb, ramTotal, diskPct, diskMb, diskTotal, temp;
    int nCpu, nRam, nDisk;
    AppRow topCpu[STAT_ROWS], topRam[STAT_ROWS], topDisk[STAT_ROWS];
};
static Stats stats = {};
static View view = V_PET;
static uint32_t showMonitorAt = 0, nextStatsAt = 0;
static bool statsBusy = false, reactToStats = false;

// animation only: every target below is the latest real sample, reached in about half a second
static float dock = 0, dockV = 0;      // 0 = pet centre stage, 1 = tucked in the corner
static float panelIn = 0;              // cards sliding in around it
static float pageSlide = 0;            // 1 -> 0 while one page replaces another
static View pageOut = V_OVERVIEW, pageShown = V_OVERVIEW;
static float showCpu = 0, showRam = 0, showDisk = 0, showTemp = 0;
static uint8_t cpuHist[HIST_LEN];
static int histCount = 0;
static uint32_t lastSampleAt = 0;
static int loadBand = -1;

struct RowAnim { char name[STAT_NAME]; float y, value, appear; int target, cpu10, mb; };
static RowAnim rowAnim[STAT_ROWS];
static int rowCount = 0;

struct Bit { float x, y, vx, vy; int life; uint8_t kind; };   // floating hearts, stars, zzz
enum { BIT_HEART, BIT_STAR, BIT_Z, BIT_TEAR, BIT_SWEAT, BIT_DOT };
#define MAX_BITS 12
static Bit bits[MAX_BITS];

// background is drawn once and restored each frame: much cheaper than re-dithering it
static uint16_t* bgCache = nullptr;
static Friend bgFor = FR_COUNT;

static uint32_t rnd() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}
static int rndRange(int a, int b) { return a + (int)(rnd() % (uint32_t)(b - a + 1)); }

static void spawnBit(uint8_t kind, float x, float y, float vx, float vy, int life) {
    for (int i = 0; i < MAX_BITS; i++)
        if (bits[i].life <= 0) { bits[i] = {x, y, vx, vy, life, kind}; return; }
}

static void setMood(Mood m) {
    if (m == moodNext && m == mood) return;
    moodNext = m;
    bool styleChanges = MOODS[m].eye != MOODS[mood].eye || MOODS[m].mouth != MOODS[mood].mouth;
    if (styleChanges) swapT = 6; else mood = m;
    if (m == MOOD_SURPRISED || m == MOOD_EXCITED) bounceKick = 1;
    squashKick = 0.09f;
}

static void say(const char* text) {
    int n = 0;
    for (; text[n] && n < SAY_MAX; n++) {
        uint8_t c = (uint8_t)text[n];
        sayBuf[n] = (c >= 32 && c < 127) ? (char)c : ' ';
    }
    sayBuf[n] = 0;
    sayShown = 0;
    sayHold = 60 + n * 3;   // auto-advances: never needs a tap to read
}

static bool moodFromName(const char* name, Mood* out) {
    for (int i = 0; i < MOOD_COUNT; i++)
        if (!strcmp(name, MOOD_NAMES[i])) { *out = (Mood)i; return true; }
    return false;
}

static void ledByName(const char* c) {
    uint16_t ms = 1500;
    if (!strcmp(c, "red")) plat_led(255, 0, 0, ms);
    else if (!strcmp(c, "green")) plat_led(0, 255, 0, ms);
    else if (!strcmp(c, "blue")) plat_led(0, 0, 255, ms);
    else if (!strcmp(c, "yellow")) plat_led(255, 255, 0, ms);
    else if (!strcmp(c, "pink")) plat_led(255, 0, 255, ms);
    else if (!strcmp(c, "cyan")) plat_led(0, 255, 255, ms);
    else if (!strcmp(c, "white")) plat_led(255, 255, 255, ms);
    else if (!strcmp(c, "off")) plat_led(0, 0, 0, 1);
}

// sound belongs to play mode: a demo loop on a shelf stays silent
static void sfx(uint8_t id) { if (playMode) plat_sfx(id); }

static void soundByName(const char* s) {
    if (!strcmp(s, "chirp")) sfx(SFX_CHIRP);
    else if (!strcmp(s, "happy")) sfx(SFX_HAPPY);
    else if (!strcmp(s, "love")) sfx(SFX_LOVE);
    else if (!strcmp(s, "sad")) sfx(SFX_SAD);
    else if (!strcmp(s, "surprise")) sfx(SFX_SURPRISE);
    else if (!strcmp(s, "sleepy")) sfx(SFX_SLEEPY);
}

// runs the LLM's tool calls. Unknown lines are ignored, so a newer proxy can't crash the pet.
static void game_apply_reply(const char* reply) {
    bool more = false;
    moreSession[0] = 0;
    const char* p = reply;
    while (*p) {
        const char* end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        char ln[SAY_MAX + 16];
        if (len < sizeof(ln)) {
            memcpy(ln, p, len);
            ln[len] = 0;
            char* arg = strchr(ln, ' ');
            if (arg) *arg++ = 0;
            if (arg) {
                Mood m;
                if (!strcmp(ln, "face") && moodFromName(arg, &m)) { setMood(m); moodByAgent = true; }
                else if (!strcmp(ln, "say")) say(arg);
                else if (!strcmp(ln, "led")) ledByName(arg);
                else if (!strcmp(ln, "sound")) soundByName(arg);
                else if (!strcmp(ln, "more") && strlen(arg) < sizeof(moreSession)) {
                    strcpy(moreSession, arg);
                    more = true;
                }
            }
        }
        if (!end) break;
        p = end + 1;
    }
    cloudOnline = true;
    asking = false;
    if (demoNext < 240) demoNext = 240;   // let the demo loop wait until this reaction has played
    if (more) {
        // the LLM wants sensor readings: send them and keep thinking
        if (plat_ask("sensors", moreSession)) { asking = true; askedAt = tick; }
    } else if (!moodByAgent && moodNext == MOOD_THINKING) {
        setMood(MOOD_NEUTRAL);
    }
    if (showMonitorAt && !more) showMonitorAt = tick + REACT_TICKS;  // give its line time to be read
}

static void offlineReaction() {
    const OfflineLine& o = OFFLINE_POKE[rndRange(0, OFFLINE_POKE_COUNT - 1)];
    setMood(o.mood);
    say(o.line);
}

static void game_reply_failed() {
    cloudOnline = false;
    asking = false;
    if (mood == MOOD_THINKING || moodNext == MOOD_THINKING) offlineReaction();
}

static void askAgent(const char* kind) {
    if (asking) return;
    if (!plat_ask(kind, "")) {
        cloudOnline = false;
        return;
    }
    asking = true;
    askedAt = tick;
    lastAsk = tick;
    moodByAgent = false;
}

static void requestStats() {
    if (statsBusy || !plat_stats()) return;
    statsBusy = true;
    nextStatsAt = tick + STATS_REFRESH_TICKS;
}

// 0 calm, 1 fine, 2 working hard, 3 overloaded. RAM on a Mac normally sits high, so it only
// counts from 90%; the chip counts as hot from 85 C.
static int currentBand() {
    if (stats.cpu >= 90 || stats.ramPct >= 95 || stats.temp >= 100) return 3;
    if (stats.cpu >= 60 || stats.ramPct >= 90 || stats.temp >= 85) return 2;
    if (stats.cpu >= 20) return 1;
    return 0;
}

static void moodForBand(int band) {
    static const Mood m[4] = {MOOD_LOVE, MOOD_HAPPY, MOOD_SHY, MOOD_DIZZY};
    setMood(m[band]);
}

// the pet's own opinion of the computer: its face tells you the load before the numbers do
static void reactToLoad() {
    static const char* const lead[4] = {"So calm...", "All good!", "Phew, working hard!", "Whoa, so busy!"};
    static const uint8_t led[4][3] = {{255, 60, 140}, {0, 255, 120}, {255, 180, 0}, {255, 0, 0}};
    int band = currentBand();
    loadBand = band;
    moodForBand(band);
    plat_led(led[band][0], led[band][1], led[band][2], 500);
    char line[SAY_MAX + 1];
    if (stats.temp > 0) snprintf(line, sizeof(line), "%s CPU %d%%, RAM %d%%, chip %d C.", lead[band], stats.cpu, stats.ramPct, stats.temp);
    else snprintf(line, sizeof(line), "%s CPU %d%%, RAM %d%%.", lead[band], stats.cpu, stats.ramPct);
    say(line);
}

// keeps animated rows attached to their app, so a row glides when its rank changes
static void syncRows() {
    if (view < V_TOP_CPU || view > V_TOP_DISK) return;
    const AppRow* list = view == V_TOP_CPU ? stats.topCpu : view == V_TOP_RAM ? stats.topRam : stats.topDisk;
    int n = view == V_TOP_CPU ? stats.nCpu : view == V_TOP_RAM ? stats.nRam : stats.nDisk;
    RowAnim next[STAT_ROWS];
    for (int i = 0; i < n; i++) {
        RowAnim* found = nullptr;
        for (int k = 0; k < rowCount; k++)
            if (!strcmp(rowAnim[k].name, list[i].name)) found = &rowAnim[k];
        if (found) {
            next[i] = *found;
        } else {
            memset(&next[i], 0, sizeof(RowAnim));
            strcpy(next[i].name, list[i].name);
            next[i].y = (float)i;
            next[i].appear = -0.35f * i;   // negative = wait, so rows arrive one after another
        }
        next[i].target = view == V_TOP_CPU ? list[i].cpu10 : list[i].mb;
        next[i].cpu10 = list[i].cpu10;
        next[i].mb = list[i].mb;
    }
    memcpy(rowAnim, next, sizeof(RowAnim) * n);
    rowCount = n;
}

static void afterStatsForTap() {
    reactToStats = false;
    if (stats.failed) {
        offlineReaction();
    } else {
        reactToLoad();
        if (stats.brain && !asking && (lastAsk == 0 || tick - lastAsk >= MIN_ASK_GAP_TICKS)) {
            setMood(MOOD_THINKING);
            askAgent("poked");
        }
    }
    showMonitorAt = tick + REACT_TICKS;
}

static void game_apply_stats(const char* reply) {
    statsBusy = false;
    Stats st = {};
    st.valid = true;
    st.temp = -1;
    const char* p = reply;
    while (*p) {
        int a = 0, b = 0, c = 0, off = 0;
        const char* end = strchr(p, '\n');
        if (sscanf(p, "cpu %d", &a) == 1) st.cpu = a;
        else if (sscanf(p, "ram %d %d %d", &a, &b, &c) == 3) { st.ramPct = a; st.ramMb = b; st.ramTotal = c; }
        else if (sscanf(p, "disk %d %d %d", &a, &b, &c) == 3) { st.diskPct = a; st.diskMb = b; st.diskTotal = c; }
        else if (sscanf(p, "brain %d", &a) == 1) st.brain = a != 0;
        else if (sscanf(p, "temp %d", &a) == 1) st.temp = a;
        else if (sscanf(p, "scan %d", &a) == 1) st.scanning = a != 0;
        else if ((p[0] == 'c' || p[0] == 'm') && sscanf(p + 1, " %d %d %n", &a, &b, &off) == 2 && off > 0) {
            AppRow* rows = p[0] == 'c' ? st.topCpu : st.topRam;
            int& n = p[0] == 'c' ? st.nCpu : st.nRam;
            if (n < STAT_ROWS) {
                AppRow& r = rows[n++];
                r.cpu10 = a; r.mb = b;
                sscanf(p + 1 + off, "%20[^\n]", r.name);
            }
        } else if (p[0] == 'd' && sscanf(p + 1, " %d %n", &a, &off) == 1 && off > 0 && st.nDisk < STAT_ROWS) {
            AppRow& r = st.topDisk[st.nDisk++];
            r.mb = a;
            sscanf(p + 1 + off, "%20[^\n]", r.name);
        }
        if (!end) break;
        p = end + 1;
    }
    stats = st;
    cloudOnline = true;
    lastSampleAt = tick;
    if (histCount < HIST_LEN) cpuHist[histCount++] = (uint8_t)(st.cpu > 100 ? 100 : st.cpu);
    else {
        memmove(cpuHist, cpuHist + 1, HIST_LEN - 1);
        cpuHist[HIST_LEN - 1] = (uint8_t)(st.cpu > 100 ? 100 : st.cpu);
    }
    syncRows();
    if (view != V_PET) {   // the pet in the corner keeps reacting to the live numbers
        int band = currentBand();
        if (band != loadBand) { loadBand = band; moodForBand(band); }
    }
    if (reactToStats) afterStatsForTap();
}

static void game_stats_failed() {
    statsBusy = false;
    stats.failed = true;
    cloudOnline = false;
    if (reactToStats) afterStatsForTap();
}

static void poke() {
    pets++;
    bounceKick = 1;
    squashKick = 0.14f;
    for (int i = 0; i < 3; i++)
        spawnBit(BIT_HEART, pose.x + rndRange(-34, 34), pose.y - 40, rndRange(-6, 6) * 0.05f, -0.9f - rndRange(0, 5) * 0.1f, 42);
    sfx(SFX_CHIRP);
    plat_led(255, 60, 140, 250);
    setMood(MOOD_THINKING);
    reactToStats = true;
    showMonitorAt = 0;
    requestStats();
    if (!statsBusy) game_stats_failed();   // no Wi-Fi yet: react anyway, the page explains
}

static void enterMonitor() {
    showMonitorAt = 0;
    view = V_OVERVIEW;
    pageShown = V_OVERVIEW;
    pageSlide = 0;
    panelIn = 0;
    showCpu = showRam = showDisk = 0;   // gauges sweep up to the real value as they arrive
    showTemp = stats.temp > 0 ? stats.temp - 12.f : 0;
    sayBuf[0] = 0;
    sayHold = 0;
    bounceKick = 0.7f;
    requestStats();
}

static void tapInMonitor() {
    sfx(SFX_TAP);
    View next = (View)(view + 1);
    if (next >= V_COUNT) {   // past the last page: the pet walks back to centre stage
        view = V_PET;
        return;
    }
    pageOut = view;
    view = next;
    pageShown = next;
    pageSlide = 1;
    rowCount = 0;
    syncRows();
}

static void nextFriend() {
    pose.who = (Friend)((pose.who + 1) % FR_COUNT);
    squashKick = 0.2f;
    bounceKick = 1;
}

static void game_init(uint32_t seed) {
    rngState = seed ? seed : 1;
    tick = 0;
    pose = FacePose();
}

static void updateDemo() {
    if (--demoNext > 0) return;
    const DemoBeat& b = DEMO[demoIdx];
    setMood(b.mood);
    if (b.line) say(b.line);
    if (b.r || b.g || b.b) plat_led(b.r, b.g, b.b, 600);
    demoNext = b.line ? 150 : 90;
    if (++demoIdx >= DEMO_COUNT) {
        demoIdx = 0;
        if (--demoFriendIn <= 0) { demoFriendIn = 2; nextFriend(); }
    }
}

// light: 0 = pitch dark .. 100 = bright. Debounced so a passing hand isn't "night".
static void updateLight(int light) {
    if (light < 0) return;
    if (light < 25) { darkFor++; brightFor = 0; }
    else if (light > 45) { brightFor++; darkFor = 0; }
    bool change = false;
    if (!isDark && darkFor > 2 * TICK_HZ) { isDark = true; change = true; }
    if (isDark && brightFor > 2 * TICK_HZ) { isDark = false; change = true; }
    if (!change) return;
    setMood(isDark ? MOOD_SLEEPY : MOOD_SURPRISED);
    if (lastLightAsk == 0 || tick - lastLightAsk >= LIGHT_ASK_GAP_TICKS) {
        lastLightAsk = tick;
        askAgent(isDark ? "lights_off" : "lights_on");
    }
}

static void updateBits() {
    for (int i = 0; i < MAX_BITS; i++) {
        Bit& b = bits[i];
        if (b.life <= 0) continue;
        b.life--;
        b.x += b.vx;
        b.y += b.vy;
    }
    float s = pose.scale;
    if (mood == MOOD_LOVE && tick % 22 == 0)
        spawnBit(BIT_HEART, pose.x + rndRange(-60, 60) * s, pose.y - 30 * s, 0, -0.7f * s, 48);
    if (mood == MOOD_EXCITED && tick % 12 == 0)
        spawnBit(BIT_STAR, pose.x + rndRange(-78, 78) * s, pose.y + rndRange(-48, 22) * s, 0, -0.25f, 30);
    if (mood == MOOD_SLEEPY && tick % 42 == 0)
        spawnBit(BIT_Z, pose.x + 42 * s, pose.y - 42 * s, 0.35f * s, -0.45f * s, 70);
    if (mood == MOOD_SAD && tick % 36 == 0)
        spawnBit(BIT_TEAR, pose.x + (rnd() & 1 ? -30 : 30) * s, pose.y + 14 * s, 0, 0.9f * s, 28);
    if (mood == MOOD_SHY && tick % 60 == 0)
        spawnBit(BIT_SWEAT, pose.x + 52 * s, pose.y - 34 * s, 0.1f * s, 0.45f * s, 36);
    if (mood == MOOD_THINKING && tick % 18 == 0 && asking)
        spawnBit(BIT_DOT, pose.x + (50 + (tick / 18 % 3) * 12) * s, pose.y - 58 * s, 0, -0.12f, 30);
}

static void game_tick(Tap tap, int light) {
    tick++;

    if (tap != TAP_NONE) {
        lastInput = tick;
        if (!playMode) {
            playMode = true;   // first tap wakes it up; it still counts as a pet
            plat_sfx(SFX_SWITCH);
        }
        if (view != V_PET) {
            if (tap == TAP_DOUBLE) view = V_PET;   // straight back to the pet
            else tapInMonitor();
        } else if (tap == TAP_DOUBLE) {
            showMonitorAt = 0;
            reactToStats = false;
            nextFriend();
            setMood(MOOD_EXCITED);
            sfx(SFX_SWITCH);
            plat_led(0, 255, 255, 300);
        } else if (!reactToStats && !showMonitorAt) {
            poke();
        }
    }
    if (playMode && tick - lastInput > (view != V_PET ? MONITOR_IDLE_TICKS : IDLE_TO_DEMO_TICKS)) {
        playMode = false;
        view = V_PET;
        showMonitorAt = 0;
        demoNext = 60;
    }

    // numbers appear once it has reacted, even if the LLM is slow or silent
    if (view == V_PET && showMonitorAt && tick >= showMonitorAt &&
        (!asking || tick - askedAt > BRAIN_WAIT_TICKS)) enterMonitor();
    if (view != V_PET && tick >= nextStatsAt) requestStats();

    updateLight(light);
    if (asking && tick - askedAt > ASK_TIMEOUT_TICKS) game_reply_failed();
    if (!playMode && !asking && !isDark) updateDemo();
    if (!playMode && !asking && isDark && !sayBuf[0]) setMood(MOOD_SLEEPY);

    // the blink that hides a change of eyes or mouth
    if (swapT > 0 && --swapT == 3) mood = moodNext;
    float blinkOpen = 1;
    if (swapT > 0) blinkOpen = abs(swapT - 3) / 3.f;

    // ordinary blinks
    if (blinkT > 0) { blinkT--; blinkOpen = fminf(blinkOpen, abs(blinkT - 3) / 3.f); }
    else if (--blinkNext <= 0) { blinkT = 6; blinkNext = rndRange(70, 170); }

    // look around; while thinking, look up and to the side
    if (--lookNext <= 0) {
        lookTX = rndRange(-10, 10) * 0.1f;
        lookTY = rndRange(-5, 5) * 0.1f;
        lookNext = rndRange(45, 130);
    }
    float wantX = lookTX, wantY = lookTY;
    if (mood == MOOD_THINKING) { wantX = 0.9f * sinf(tick * 0.05f); wantY = -0.7f; }
    if (mood == MOOD_SAD) wantY = 0.7f;
    pose.lookX += (wantX - pose.lookX) * 0.14f;
    pose.lookY += (wantY - pose.lookY) * 0.14f;

    const MoodStyle& st = MOODS[mood];
    pose.eye = st.eye;
    pose.mouth = st.mouth;
    blushNow += (st.blush - blushNow) * 0.1f;
    pose.blush = blushNow;

    float wantOpen = mood == MOOD_SURPRISED ? 1.12f : mood == MOOD_SAD ? 0.78f : 1;
    pose.open = wantOpen * blinkOpen;

    // springy idle: breathing, a mood bounce, and a squash that decays after every poke
    squashKick *= 0.86f;
    bounceKick *= 0.88f;
    float breathe = sinf(tick * 0.075f);
    float hop = fabsf(sinf(tick * 0.17f)) * st.bounce * 5;
    float kick = fabsf(sinf(tick * 0.5f)) * bounceKick * 14;
    pose.squash = st.squash + breathe * 0.012f + squashKick * cosf(tick * 0.9f);
    pose.earWiggle = sinf(tick * 0.11f) * 0.25f + (mood == MOOD_EXCITED ? sinf(tick * 0.6f) * 0.6f : 0) +
                     (mood == MOOD_SAD ? -0.3f : 0);
    pose.mouthSize = mood == MOOD_HAPPY || mood == MOOD_EXCITED ? 1 + 0.12f * sinf(tick * 0.25f) : 1;

    // make room for the bubble while it is talking
    if (sayHold > 0) sayHold--;
    if (sayHold == 0) sayBuf[0] = 0;
    if (sayBuf[0] && sayShown < (int)strlen(sayBuf) && tick % 2 == 0) {
        sayShown++;
        if (sayBuf[sayShown - 1] != ' ' && sayShown % 3 == 0) sfx(SFX_TAP);
    }
    float wantLift = sayBuf[0] ? 1.f : 0.f;
    speakLift += (wantLift - speakLift) * 0.16f;
    // springy trip to the corner: a little overshoot makes it feel like it jumped there
    dockV += ((view != V_PET ? 1.f : 0.f) - dock) * 0.11f;
    dockV *= 0.74f;
    dock += dockV;
    float d = clamp01(dock), travel = fabsf(sinf(d * 3.14159f)) * 16;   // arcs up on the way
    pose.scale = 0.8f * (1 - d) + 0.34f * d;
    pose.x = 56 + (31 - 56) * dock;     // centre stage is the left column: the crack runs beside it
    pose.y = (84 - hop - kick + breathe * 0.7f) * (1 - d) + (26 - (hop + kick) * 0.2f + breathe * 0.4f) * d - travel;

    // dashboard animation: the numbers chase the latest real sample
    bool out = view != V_PET;
    panelIn += ((out && dock > 0.25f ? 1.f : 0.f) - panelIn) * (out ? 0.09f : 0.2f);
    if (pageSlide > 0) { pageSlide *= 0.78f; if (pageSlide < 0.004f) pageSlide = 0; }
    if (stats.valid && out) {
        const float k = 0.2f;
        showCpu += (stats.cpu - showCpu) * k;
        showRam += (stats.ramPct - showRam) * k;
        showDisk += (stats.diskPct - showDisk) * k;
        if (stats.temp > 0) showTemp += (stats.temp - showTemp) * k;
    }
    for (int i = 0; i < rowCount; i++) {
        RowAnim& r = rowAnim[i];
        r.y += (i - r.y) * 0.18f;
        r.value += (r.target - r.value) * 0.2f;
        r.appear += 0.075f;
        if (r.appear > 1) r.appear = 1;
    }
    pose.tick = (int)tick;

    updateBits();
}

// ------------------------------------------------------------------ drawing
static void buildBackground() {
    Palette pal = friendPalette(pose.who);
    gradientV(0, FB_H, pal.bgTop, pal.bgBot);
    // quiet confetti: fixed positions per friend, so the frame diff stays small
    uint32_t r = 2463534242u + pose.who * 977u;
    for (int i = 0; i < 26; i++) {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        int x = r % FB_W, y = (r >> 10) % FB_H, sz = 1 + ((r >> 22) & 1);
        blendEllipse(x, y, sz, sz, 0xFFFF, 110);
    }
    if (bgCache) memcpy(bgCache, fb, FB_W * FB_H * 2);
    bgFor = pose.who;
}

static void drawBits() {
    for (int i = 0; i < MAX_BITS; i++) {
        const Bit& b = bits[i];
        if (b.life <= 0) continue;
        float grow = (b.life < 8 ? b.life / 8.f : 1) * (0.55f + 0.45f * pose.scale);
        switch (b.kind) {
            case BIT_HEART: drawHeart(b.x, b.y, 5.5f * grow, RGB(244, 84, 128)); break;
            case BIT_STAR:  drawStar(b.x, b.y, 7 * grow, RGB(255, 208, 60)); break;
            case BIT_TEAR:
                aaTri(b.x - 3, b.y, b.x + 3, b.y, b.x, b.y - 6, RGB(120, 190, 255));
                aaDisc(b.x, b.y, 3 * grow, RGB(120, 190, 255));
                break;
            case BIT_SWEAT:
                aaTri(b.x - 3, b.y, b.x + 3, b.y, b.x, b.y - 7, RGB(150, 215, 255));
                aaDisc(b.x, b.y, 3.2f, RGB(150, 215, 255));
                break;
            case BIT_DOT: aaDisc(b.x, b.y, 2.6f * grow, RGB(110, 80, 130)); break;
            case BIT_Z: {
                uint16_t c = RGB(110, 100, 170);
                text((int)b.x, (int)b.y, "z", c, RGB(255, 255, 255));
                break;
            }
        }
    }
}

// greedy word wrap into at most 2 lines of the bubble; returns the start of line 2 or -1
static int wrapPoint(const char* s, int maxW) {
    char tmp[SAY_MAX + 1];
    int lastSpace = -1, n = (int)strlen(s);
    for (int i = 0; i < n; i++) {
        memcpy(tmp, s, i + 1);
        tmp[i + 1] = 0;
        if (s[i] == ' ') lastSpace = i;
        if (textWidth(tmp) > maxW) return lastSpace > 0 ? lastSpace + 1 : i;
    }
    return -1;
}

// sits in the right column beside the pet, so the words never cross the crack
static void drawBubble() {
    if (!sayBuf[0] || speakLift < 0.1f) return;
    Palette pal = friendPalette(pose.who);
    const int maxW = COL_R_W - 14;
    int starts[8], lines = 0, pos = 0;
    while (lines < 8) {
        starts[lines++] = pos;
        int next = wrapPoint(sayBuf + pos, maxW);
        if (next < 0) break;
        pos += next;
    }
    int h = lines * 12 + 12, y = (FB_H - h) / 2 - 2;
    int x = COL_R_X + 2 + (int)((1 - speakLift) * 110);
    panel(x, y, COL_R_W - 2, h, pal.outline, RGB(255, 255, 255), RGB(255, 252, 248));
    aaTri(x + 2, y + h / 2 - 5, x + 2, y + h / 2 + 5, x - 3, y + h / 2, RGB(255, 252, 248));
    uint16_t ink = RGB(58, 56, 74), shade = RGB(222, 222, 226);
    for (int i = 0; i < lines; i++) {
        int len = (i + 1 < lines ? starts[i + 1] : (int)strlen(sayBuf)) - starts[i];
        char ln[SAY_MAX + 1];
        memcpy(ln, sayBuf + starts[i], len);
        ln[len] = 0;
        int shown = sayShown - starts[i];
        if (shown > 0) text(x + 7, y + 7 + i * 12, ln, ink, shade, shown);
    }
}

// ------------------------------------------------------------------ the dashboard
static inline uint16_t loadColor(int pct) {
    return pct >= 90 ? RGB(236, 74, 92) : pct >= 70 ? RGB(246, 158, 56) : RGB(68, 196, 128);
}
static inline uint16_t tempColor(int c) {
    return c >= 95 ? RGB(236, 74, 92) : c >= 80 ? RGB(246, 158, 56) : RGB(78, 170, 226);
}

static void fmtSize(char* out, size_t n, int mb) {
    if (mb >= 1024) snprintf(out, n, "%d.%d GB", mb / 1024, (mb % 1024) * 10 / 1024);
    else snprintf(out, n, "%d MB", mb);
}

// font at 2x, for the numbers you read from across the desk
static int bigWidth(const char* s) { return textWidth(s) * 2; }
static void bigText(int x, int y, const char* s, uint16_t c) {
    for (; *s; s++) {
        uint8_t ch = (uint8_t)*s;
        if (ch >= 128 || !FONT[ch].w) continue;
        const Glyph& g = FONT[ch];
        for (int r = 0; r < 8; r++)
            for (int i = 0; i < g.w; i++)
                if (g.rows[r] >> i & 1) fillRect(x + i * 2, y + r * 2, 2, 2, c);
        x += (g.w + 1) * 2;
    }
}

static void meter(float x, int y, int w, int h, float frac, uint16_t c) {
    float r = h / 2.f;
    aaStroke(x + r, y + r, x + w - r - 1, y + r, r - 0.5f, RGB(236, 229, 238));
    if (frac <= 0) return;
    if (frac > 1) frac = 1;
    aaStroke(x + r, y + r, x + r + (w - h - 1) * frac, y + r, r - 0.5f, c);
}

static void ring(float cx, float cy, float r, float pct, uint16_t c) {
    const float DEG = 0.017453f;
    float px = cx, py = cy - r;
    for (int a = 15; a <= 360; a += 15) {
        float x = cx + sinf(a * DEG) * r, y = cy - cosf(a * DEG) * r;
        aaStroke(px, py, x, y, 2.6f, RGB(236, 229, 238));
        px = x; py = y;
    }
    if (pct <= 0.2f) return;
    float end = (pct > 100 ? 100 : pct) * 3.6f;
    px = cx; py = cy - r;
    for (float a = 12; ; a += 12) {
        if (a > end) a = end;
        float x = cx + sinf(a * DEG) * r, y = cy - cosf(a * DEG) * r;
        aaStroke(px, py, x, y, 2.6f, c);
        px = x; py = y;
        if (a >= end) break;
    }
}

// a card's slide-in: each one leaves a beat after the last
static float cardOffset(int order) {
    float t = clamp01(panelIn * 1.5f - order * 0.2f);
    float e = 1 - (1 - t) * (1 - t) * (1 - t);
    return (1 - e) * 250;
}

static void card(float x, int y, int w, int h) {
    Palette pal = friendPalette(pose.who);
    panel((int)x, y, w, h, blend565(pal.outline, 0xFFFF, 110), RGB(255, 255, 255), RGB(255, 252, 248));
}

// filled area chart of real CPU samples, gliding left between them
static void sparkline(int x, int y, int w, int h, float ox, uint16_t c) {
    if (histCount < 1) return;
    float pxPerSample = (float)w / (HIST_LEN - 2);
    float phase = (tick - lastSampleAt) / (float)STATS_REFRESH_TICKS;
    if (phase > 1) phase = 1;
    uint16_t fill = blend565(0xFFFF, c, 70);
    int bottom = y + h - 1;
    float prevX = 0, prevY = 0;
    for (int i = 0; i < histCount; i++) {
        float sx = x + ox + w - (histCount - 1 - i + phase) * pxPerSample;
        float sy = bottom - cpuHist[i] * (h - 2) / 100.f;
        if (i > 0 && sx > x + ox) {
            int cx0 = (int)(prevX < x + ox ? x + ox : prevX), cx1 = (int)sx;
            for (int cx = cx0; cx <= cx1; cx++) {
                float t = sx > prevX ? (cx - prevX) / (sx - prevX) : 1;
                int top = (int)(prevY + (sy - prevY) * t);
                for (int py = top; py <= bottom; py++) pset(cx, py, fill);
            }
            if (prevX >= x + ox) aaStroke(prevX, prevY, sx, sy, 0.7f, c);
        }
        prevX = sx; prevY = sy;
    }
    // the beat on the newest sample shows the numbers are live
    if (histCount > 1 && cloudOnline) aaDisc(prevX, prevY, 1.6f + 1.2f * (1 - phase), c);
}

static void drawOverview(float ox) {
    uint16_t ink = RGB(58, 56, 74), soft = RGB(112, 112, 130), shade = RGB(255, 255, 255), wash = RGB(222, 222, 226);
    char num[12], a[16], line[32];

    if (!stats.valid) {
        float x = ox + cardOffset(0);
        card(x + COL_R_X, 40, COL_R_W, 64);
        const char* msg[4] = {"Can't reach", "your computer.", "Is the proxy", "running?"};
        const char* wait[4] = {"Asking your", "computer...", "", ""};
        for (int i = 0; i < 4; i++) text((int)x + COL_R_X + 8, 48 + i * 12, stats.failed ? msg[i] : wait[i], ink, wash);
        return;
    }

    // left of the crack: the pet and the CPU number
    float x = ox + cardOffset(0);
    card(x + 58, 4, COL_L_X + COL_L_W - 58, 48);
    text((int)x + 65, 9, "CPU", ink, shade);
    snprintf(num, sizeof(num), "%d%%", (int)(showCpu + 0.5f));
    bigText((int)x + 65, 24, num, loadColor((int)showCpu));

    // right of it: temperature and the CPU history
    card(x + COL_R_X, 4, COL_R_W, 48);
    int gx = (int)x + COL_R_X + 7;
    if (stats.temp > 0) {
        int t = (int)(showTemp + 0.5f);
        uint16_t tc = tempColor(t);
        aaDisc(gx + 3, 14, 2.6f, tc);                // thermometer whose level is the temperature
        aaStroke(gx + 3, 7, gx + 3, 13, 1.5f, RGB(226, 226, 232));
        aaStroke(gx + 3, 13, gx + 3, 13 - clamp01((t - 20) / 80.f) * 6, 1.0f, tc);
        snprintf(num, sizeof(num), "chip %dC", t);
        text(gx + 9, 9, num, tc, shade);
    }
    sparkline(gx, 20, COL_R_W - 14, 28, 0, loadColor((int)showCpu));

    // RAM ring
    x = ox + cardOffset(1);
    card(x + COL_L_X, 56, COL_L_W, 92);
    float cx = x + COL_L_X + COL_L_W / 2.f;
    ring(cx, 88, 24, showRam, loadColor((int)showRam));
    snprintf(num, sizeof(num), "%d%%", (int)(showRam + 0.5f));
    text((int)cx - 8, 76, "RAM", soft, shade);
    bigText((int)cx - bigWidth(num) / 2, 85, num, ink);
    fmtSize(a, sizeof(a), stats.ramMb);
    snprintf(line, sizeof(line), "%s / %d GB", a, (stats.ramTotal + 512) / 1024);
    text((int)cx - textWidth(line) / 2, 116, line, soft, shade);
    if (stats.temp > 0) {
        // unified memory has no sensor of its own: this is honestly the chip it sits in
        snprintf(line, sizeof(line), "chip %dC", (int)(showTemp + 0.5f));
        text((int)cx - textWidth(line) / 2, 130, line, tempColor((int)showTemp), shade);
    }

    // disk ring
    x = ox + cardOffset(2);
    card(x + COL_R_X, 56, COL_R_W, 92);
    cx = x + COL_R_X + COL_R_W / 2.f;
    ring(cx, 88, 24, showDisk, loadColor((int)showDisk));
    snprintf(num, sizeof(num), "%d%%", (int)(showDisk + 0.5f));
    text((int)cx - 10, 76, "DISK", soft, shade);
    bigText((int)cx - bigWidth(num) / 2, 85, num, ink);
    snprintf(line, sizeof(line), "%d / %d GB", stats.diskMb / 1024, stats.diskTotal / 1024);
    text((int)cx - textWidth(line) / 2, 116, line, soft, shade);
    fmtSize(a, sizeof(a), stats.diskTotal - stats.diskMb);
    snprintf(line, sizeof(line), "%s free", a);
    text((int)cx - textWidth(line) / 2, 130, line, soft, shade);
}

// names on the left of the crack, values and bars on the right
static void drawTopPage(View v, float ox, bool animated) {
    uint16_t ink = RGB(58, 56, 74), soft = RGB(112, 112, 130), shade = RGB(255, 255, 255), wash = RGB(222, 222, 226);
    const AppRow* list = v == V_TOP_CPU ? stats.topCpu : v == V_TOP_RAM ? stats.topRam : stats.topDisk;
    int n = v == V_TOP_CPU ? stats.nCpu : v == V_TOP_RAM ? stats.nRam : stats.nDisk;
    uint16_t accent = v == V_TOP_CPU ? RGB(240, 138, 60) : v == V_TOP_RAM ? RGB(112, 142, 240) : RGB(52, 182, 156);
    const char* title = v == V_TOP_CPU ? "CPU" : v == V_TOP_RAM ? "RAM" : "DISK";
    float x = ox + cardOffset(0);
    int rightEnd = (int)x + COL_R_X + COL_R_W - 8;

    card(x + 58, 4, COL_L_X + COL_L_W - 58, 38);
    bigText((int)x + 64, 15, title, accent);
    card(x + COL_R_X, 4, COL_R_W, 38);
    char head[24];
    if (v == V_TOP_CPU) snprintf(head, sizeof(head), "%d%%", (int)(showCpu + 0.5f));
    else if (v == V_TOP_RAM) snprintf(head, sizeof(head), "%d%%", (int)(showRam + 0.5f));
    else snprintf(head, sizeof(head), "biggest apps");
    if (v == V_TOP_DISK) text(rightEnd - textWidth(head), 19, head, soft, shade);
    else bigText(rightEnd - bigWidth(head), 15, head, ink);

    card(x + COL_L_X, 46, COL_L_W, 103);
    card(x + COL_R_X, 46, COL_R_W, 103);
    if (n == 0) {
        const char* msg = stats.failed ? "No connection" : v == V_TOP_DISK && stats.scanning ? "Measuring..." : "Nothing yet";
        text((int)x + COL_L_X + 8, 92, msg, ink, wash);
        return;
    }
    int disk0 = list[0].mb > 0 ? list[0].mb : 1;
    for (int i = 0; i < n; i++) {
        const char* name = list[i].name;
        float yy = (float)i, value = v == V_TOP_CPU ? list[i].cpu10 : list[i].mb, appear = 1;
        int cpu10 = list[i].cpu10, mb = list[i].mb;
        if (animated && i < rowCount) {
            const RowAnim& r = rowAnim[i];
            name = r.name;
            yy = r.y;
            value = r.value;
            appear = clamp01(r.appear);
            cpu10 = (int)(r.value + 0.5f);
            mb = (int)(r.value + 0.5f);
        }
        int y = (int)(53 + yy * 19.5f + 0.5f);
        char val[16], shortName[STAT_NAME];
        if (v == V_TOP_CPU) snprintf(val, sizeof(val), "%d.%d%%", cpu10 / 10, cpu10 % 10);
        else fmtSize(val, sizeof(val), mb);
        // long app names stop short of the crack
        snprintf(shortName, sizeof(shortName), "%s", name);
        while (textWidth(shortName) > COL_L_W - 12) shortName[strlen(shortName) - 1] = 0;
        text((int)(x + COL_L_X + 6 + (1 - appear) * (1 - appear) * -40), y + 4, shortName, ink, shade);
        text(rightEnd - textWidth(val), y, val, ink, shade);
        // bars are true shares: of the whole machine, of all RAM, or against the biggest app
        float frac = v == V_TOP_CPU ? value / 1000.f : v == V_TOP_RAM ? value / (stats.ramTotal > 0 ? stats.ramTotal : 1) : value / disk0;
        meter(x + COL_R_X + 7, y + 10, COL_R_W - 15, 5, frac * appear, accent);
    }
}

static void drawPage(View v, float ox, bool animated) {
    if (v == V_OVERVIEW) drawOverview(ox);
    else drawTopPage(v, ox, animated);
}

static void game_render() {
    if (bgFor != pose.who || !bgCache) buildBackground();
    else memcpy(fb, bgCache, FB_W * FB_H * 2);

    if (panelIn > 0.01f) {
        if (pageSlide > 0) {
            float e = pageSlide * pageSlide * (3 - 2 * pageSlide);
            drawPage(pageOut, -(1 - e) * 240, false);
            drawPage(pageShown, e * 240, true);
        } else {
            drawPage(pageShown, 0, true);
        }
        if (view != V_PET) {
            Palette pal = friendPalette(pose.who);
            for (int i = 0; i < 4; i++) {
                bool on = view == V_OVERVIEW + i;
                aaDisc(COL_R_X + COL_R_W / 2 + (i - 1.5f) * 10, FB_H - 5, on ? 2.6f : 1.7f, on ? pal.outline : blend565(pal.outline, 0xFFFF, 150));
            }
        }
    }
    drawFriend(pose);
    drawBits();
    drawBubble();

    // tiny status pip: green when the cloud brain answered last, grey when using offline lines
    if (panelIn < 0.01f) {
        aaDisc(231, 9, 3.4f, RGB(255, 255, 255));
        aaDisc(231, 9, 2.4f, cloudOnline ? RGB(70, 210, 120) : RGB(178, 170, 186));
    }
}
