// ==============================================================================
// Project: ESP32-035 / CYD 3.5" (ESP32-3248S035R)
// Role:    Video clip player - JPEG frames + 8-bit audio from a raw flash partition
// Hardware: 3.5" 480x320 TFT (ST7796), XPT2046 Touch, Speaker amp on GPIO 26 (DAC2)
//
// tools/make_clip.py turns a video into clip.bin (240x136 baseline JPEG frames +
// unsigned 8-bit mono PCM) and tools/flash_clip.sh writes it to the 'clip'
// partition at 0x110000, separately from this firmware. Each frame is read from
// flash into RAM, decoded with JPEGDEC a few 16x16 blocks at a time, scaled 2x or 3x and
// sent to the panel by DMA while the next blocks decode. Audio streams through I2S
// into the built-in DAC from a task on core 0, locked to the same playback clock.
// Controls: 1 tap = pause, 2 taps = mute, cover the light sensor = skip 4 s ahead.
// ==============================================================================

#include <Arduino.h>
#include <JPEGDEC.h>   // before TFT_eSPI.h: that sets FS_NO_GLOBALS, which hides File
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <driver/i2s.h>

// --- PIN DEFINITIONS FOR CYD 3.5" (ESP32-035) ---
#define CYD_BACKLIGHT   27   // Backlight is GPIO 27 (Active HIGH)
#define CYD_LED_RED     4    // Active LOW (0 = ON, 1 = OFF)
#define CYD_LED_GREEN   16
#define CYD_LED_BLUE    17
#define CYD_SPEAKER_PIN 26   // Audio amplifier = DAC2 = I2S "left" channel
#define CYD_LDR_PIN     34   // Light sensor (ADC1): cover it to skip ahead

// Touch Screen SPI (Shared HSPI bus: CLK=14, MISO=12, MOSI=13, CS=33)
#define XPT2046_MOSI    13
#define XPT2046_MISO    12
#define XPT2046_CLK     14
#define XPT2046_CS      33

#define TAP_DEBOUNCE_MS   45   // release must last this long to count
#define DOUBLE_TAP_MS     300  // window for the second tap

// Light-sensor skip, same detector as mario-arcade: the reading is ~0 in a lit room and
// rises in the dark. The room level is tracked slowly; rising LDR_COVER_RISE above it
// counts as covered, and each cover jumps SKIP_MS ahead.
#define LDR_DARK_IS_LOW   false // true if the ADC reading drops when the sensor is covered
#define LDR_COVER_RISE    60    // measured on this board: room 0-30, fingertip over it 100-150
#define LDR_RELEASE_MS    150   // the covered reading flickers to 0; released = low this long
#define SKIP_MS           4000

#define CLIP_SUBTYPE      0x40          // partitions.csv: clip, data, 0x40
#define MAX_FRAME_BYTES   (96 * 1024)   // biggest JPEG we are willing to buffer
#define AUDIO_DMA_BUFS    4
#define AUDIO_DMA_FRAMES  512           // I2S frames per DMA buffer (= one write)
#define AUDIO_MIN_I2S_HZ  30000         // 8 kHz clips run the DAC at 32 kHz (see startAudio)

// clip.bin header, 64 bytes, little endian (see README)
struct __attribute__((packed)) ClipHeader {
    char     magic[4];       // "CLIP"
    uint16_t version;        // 1
    uint16_t width, height;  // decoded JPEG size, shown at the biggest whole-number zoom
    uint16_t fpsX100;        // 1500 = 15 fps
    uint32_t frameCount;
    uint32_t indexOffset;    // frameCount + 1 offsets; frame i = [idx[i], idx[i+1])
    uint32_t audioRate;      // Hz, 0 = no audio
    uint32_t audioOffset;
    uint32_t audioBytes;     // unsigned 8-bit mono, 128 = silence
    uint8_t  pad[32];
};
static_assert(sizeof(ClipHeader) == 64, "clip header must be 64 bytes");

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS);
JPEGDEC jpeg;

static const esp_partition_t* clipPart = nullptr;
static ClipHeader hdr;
static bool clipOk = false;
static bool hasAudio = false;
static char noClipReason[72];
static uint32_t clipDurMs = 0;        // one pass of the clip
static uint32_t maxFrameBytes = 0;
static uint8_t* jpgBuf = nullptr;     // current JPEG, read from flash
static uint16_t* strip[2];            // scaled pixel blocks, ping-pong for DMA
static int stripSel = 0;
static int vidScale = 2;              // whole-number zoom: 240x136 -> 2x, 160x90 -> 3x
static int vidX0, vidY0;              // top-left of the scaled picture on the panel

static volatile bool paused = false, muted = false;
static uint32_t skipShownAt = 0;      // millis() of the last skip while its label is up, 0 = none

// -----------------------------------------------------------------------------
// PLAYBACK CLOCK: milliseconds into the clip, shared by video (core 1) and audio
// (core 0). Pausing freezes it; it wraps at the end so the clip loops.
// -----------------------------------------------------------------------------
static portMUX_TYPE clockMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t clockBase = 0;   // millis() at clip position 0
static uint32_t pausedAt = 0;    // clip position while paused

static uint32_t clipPosMs() {
    portENTER_CRITICAL(&clockMux);
    uint32_t pos = pausedAt;
    if (!paused) {
        pos = millis() - clockBase;
        if (pos >= clipDurMs) {           // next loop of the clip
            clockBase += pos - pos % clipDurMs;
            pos %= clipDurMs;
        }
    }
    portEXIT_CRITICAL(&clockMux);
    return pos;
}

static void setPaused(bool p) {
    portENTER_CRITICAL(&clockMux);
    if (p && !paused) pausedAt = (millis() - clockBase) % clipDurMs;
    if (!p && paused) clockBase = millis() - pausedAt;
    paused = p;
    portEXIT_CRITICAL(&clockMux);
}

// jump ahead (wraps past the end, works while paused); the audio task re-locks by itself
static void skipAhead(uint32_t ms) {
    portENTER_CRITICAL(&clockMux);
    if (paused) pausedAt = (pausedAt + ms) % clipDurMs;
    else clockBase -= ms;
    portEXIT_CRITICAL(&clockMux);
}

// -----------------------------------------------------------------------------
// AUDIO: u8 PCM -> I2S -> built-in DAC on GPIO 26
// -----------------------------------------------------------------------------
static uint32_t audioUp = 1;               // I2S rate = audio rate * audioUp
static volatile uint32_t i2sFrames = 0;    // frames handed to I2S (rate readout)

// DAC mode takes the top byte of each 16-bit channel; both channels get the
// same sample so the left/right order quirk of DAC mode doesn't matter.
static inline uint32_t dacFrame(int v) { return ((uint32_t)v << 24) | ((uint32_t)v << 8); }

// n samples starting at pos; past the end of the audio data is silence
static void readPcm(uint32_t pos, uint32_t total, uint8_t* dst, uint32_t n) {
    while (n) {
        if (pos >= total) pos = 0;
        uint32_t run = min(n, total - pos);
        uint32_t real = pos < hdr.audioBytes ? min(run, hdr.audioBytes - pos) : 0;
        if (real) esp_partition_read(clipPart, hdr.audioOffset + pos, dst, real);
        memset(dst + real, 128, run - real);
        dst += run; pos += run; n -= run;
    }
}

static void audioTask(void*) {
    static uint8_t pcm[AUDIO_DMA_FRAMES];
    static uint32_t out[AUDIO_DMA_FRAMES];
    const uint32_t rate = hdr.audioRate, up = audioUp, n = AUDIO_DMA_FRAMES / up;
    const uint32_t total = (uint64_t)clipDurMs * rate / 1000;       // samples per loop
    const uint32_t lead = AUDIO_DMA_BUFS * AUDIO_DMA_FRAMES / up;   // queued ahead in DMA
    const int32_t nudge = rate / 200, jump = rate / 10;             // 5 ms / 100 ms
    uint32_t pos = 0;
    bool synced = false;
    int prev = 128, gain = 0;
    size_t done;

    // ramp the DAC from 0 V up to mid-rail (silence) so the speaker doesn't pop
    for (int i = 0; i < AUDIO_DMA_FRAMES; i++) out[i] = dacFrame(i * 128 / AUDIO_DMA_FRAMES);
    i2s_write(I2S_NUM_0, out, sizeof(out), &done, portMAX_DELAY);

    for (;;) {
        const bool hold = paused;
        uint32_t step = n;
        if (hold) {
            synced = false;               // re-lock on resume
        } else {
            // where the clock says we are, plus what's already queued in DMA
            uint32_t want = ((uint64_t)clipPosMs() * rate / 1000 + lead) % total;
            int32_t diff = (int32_t)((pos + total - want) % total);
            if (diff > (int32_t)(total / 2)) diff -= total;
            if (!synced || diff > jump || diff < -jump) { pos = want; synced = true; }
            else if (diff > nudge) step--;    // ahead of the picture: repeat a sample
            else if (diff < -nudge) step++;   // behind: drop a sample
        }
        readPcm(pos, total, pcm, n);

        // linear upsampling + an 8 ms fade for pause / mute (no clicks)
        const bool quiet = hold || muted;
        uint32_t* o = out;
        for (uint32_t i = 0; i < n; i++) {
            int s = pcm[i];
            for (uint32_t j = 0; j < up; j++) {
                if (quiet) { if (gain > 0) gain--; }
                else if (gain < 256) gain++;
                int v = prev + (s - prev) * (int)j / (int)up;
                *o++ = dacFrame(128 + (v - 128) * gain / 256);
            }
            prev = s;
        }
        i2s_write(I2S_NUM_0, out, sizeof(out), &done, portMAX_DELAY);
        i2sFrames += done / 4;
        if (!hold) pos = (pos + step) % total;
    }
}

static bool startAudio() {
    // Below ~20 kHz the DAC-mode clock divider (8 bits) overflows: asking for 8 kHz
    // really runs at ~44 kHz. So run I2S at a power of two times the clip rate and
    // interpolate (which also smooths the 8-bit steps).
    audioUp = 1;
    while (hdr.audioRate * audioUp < AUDIO_MIN_I2S_HZ) audioUp <<= 1;

    i2s_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = hdr.audioRate * audioUp;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.dma_buf_count = AUDIO_DMA_BUFS;
    cfg.dma_buf_len = AUDIO_DMA_FRAMES;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = false;   // an underrun must not drop the DAC to 0 V (pop)
    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);   // DAC2 / GPIO 26 only
    xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 0);
    return true;
}

// -----------------------------------------------------------------------------
// CLIP: find the partition and check the header + frame index
// -----------------------------------------------------------------------------
static bool fail(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(noClipReason, sizeof(noClipReason), fmt, ap);
    va_end(ap);
    return false;
}

static bool loadClip() {
    clipPart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)CLIP_SUBTYPE, "clip");
    if (!clipPart) return fail("No 'clip' partition: flash with pio env video_clip");
    const uint32_t size = clipPart->size;

    if (esp_partition_read(clipPart, 0, &hdr, sizeof(hdr)) != ESP_OK) return fail("Flash read error");
    if (memcmp(hdr.magic, "CLIP", 4) != 0) return fail("Clip area is empty (no CLIP header)");
    if (hdr.version != 1) return fail("Unsupported clip version %u", hdr.version);
    if (!hdr.width || !hdr.height || hdr.width > 240 || hdr.height > 160)
        return fail("Bad frame size %ux%u (max 240x160)", hdr.width, hdr.height);
    if (hdr.fpsX100 < 100 || hdr.fpsX100 > 6000) return fail("Bad frame rate (%u / 100 fps)", hdr.fpsX100);
    uint64_t idxEnd = (uint64_t)hdr.indexOffset + ((uint64_t)hdr.frameCount + 1) * 4;
    if (!hdr.frameCount || hdr.indexOffset < sizeof(hdr) || idxEnd > size) return fail("Bad frame index");

    // offsets must rise, stay inside the partition and each frame must fit in RAM
    uint32_t chunk[64], prev = 0;
    maxFrameBytes = 0;
    for (uint32_t i = 0; i <= hdr.frameCount; i += 64) {
        uint32_t n = min<uint32_t>(64, hdr.frameCount + 1 - i);
        if (esp_partition_read(clipPart, hdr.indexOffset + i * 4, chunk, n * 4) != ESP_OK) return fail("Flash read error");
        for (uint32_t j = 0; j < n; j++) {
            uint32_t off = chunk[j];
            if (i + j == 0) {
                if (off < sizeof(hdr)) return fail("Bad frame index (entry 0)");
            } else {
                if (off <= prev) return fail("Bad frame index (entry %u)", (unsigned)(i + j));
                maxFrameBytes = max(maxFrameBytes, off - prev);
            }
            prev = off;
        }
    }
    if (prev > size) return fail("Frame data runs past the clip partition");
    if (maxFrameBytes > MAX_FRAME_BYTES) return fail("Frame too big (%u bytes)", (unsigned)maxFrameBytes);
    jpgBuf = (uint8_t*)malloc((maxFrameBytes + 3) & ~3u);
    if (!jpgBuf) return fail("No RAM for a %u byte frame", (unsigned)maxFrameBytes);

    clipDurMs = (uint64_t)hdr.frameCount * 100000 / hdr.fpsX100;
    vidScale = min(480 / hdr.width, 320 / hdr.height);   // biggest whole-number zoom that fits
    vidX0 = (480 - hdr.width * vidScale) / 2;
    vidY0 = (320 - hdr.height * vidScale) / 2;

    hasAudio = false;
    if (hdr.audioRate) {
        if (hdr.audioRate < 4000 || hdr.audioRate > 48000 || hdr.audioOffset < sizeof(hdr) ||
            (uint64_t)hdr.audioOffset + hdr.audioBytes > size)
            Serial.println("[Clip] audio fields invalid - playing without sound");
        else
            hasAudio = hdr.audioBytes > 0;
    }
    return true;
}

static void showNoClip() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("No video clip in flash", 240, 50, 4);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString(noClipReason, 240, 95, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("On your computer, in examples/video-clip/:", 240, 145, 2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("tools/make_clip.py VIDEO -o clip.bin", 240, 175, 2);
    tft.drawString("tools/flash_clip.sh clip.bin", 240, 200, 2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("The clip goes to its own flash area; this firmware stays.", 240, 250, 2);
}

// -----------------------------------------------------------------------------
// VIDEO: decode one frame and push it scaled up with DMA
// -----------------------------------------------------------------------------
// JPEGDEC hands over up to MAX_BUFFERED_PIXELS at a time (eight 16x16 blocks for
// 4:2:0). Each group is doubled into one strip buffer and queued for DMA;
// pushImageDMA waits for the previous transfer first, so the other buffer is
// free to fill while this one is on the bus.
static int drawBlocks(JPEGDRAW* d) {
    int w = d->iWidthUsed, h = d->iHeight;
    if (d->x + w > hdr.width) w = hdr.width - d->x;    // never draw outside the picture
    if (d->y + h > hdr.height) h = hdr.height - d->y;
    if (w <= 0 || h <= 0 || w * h > MAX_BUFFERED_PIXELS) return 1;

    const int S = vidScale, ow = w * S;
    uint16_t* out = strip[stripSel];
    stripSel ^= 1;
    for (int y = 0; y < h; y++) {
        const uint16_t* src = d->pPixels + y * d->iWidth;
        uint16_t* row = out + y * S * ow;                // S output rows per source row
        for (int x = 0; x < w; x++)
            for (int k = 0; k < S; k++) row[x * S + k] = src[x];   // each pixel S times
        for (int k = 1; k < S; k++) memcpy(row + k * ow, row, ow * 2);
    }
    tft.pushImageDMA(vidX0 + d->x * S, vidY0 + d->y * S, ow, h * S, out);
    return 1;
}

static bool showFrame(uint32_t i) {
    uint32_t off[2];
    if (esp_partition_read(clipPart, hdr.indexOffset + i * 4, off, sizeof(off)) != ESP_OK) return false;
    uint32_t len = off[1] - off[0];   // checked against maxFrameBytes at boot
    if (esp_partition_read(clipPart, off[0], jpgBuf, len) != ESP_OK) return false;
    if (!jpeg.openRAM(jpgBuf, len, drawBlocks)) return false;
    jpeg.setPixelType(RGB565_BIG_ENDIAN);   // panel byte order, so DMA sends it as-is
    tft.startWrite();
    int ok = jpeg.decode(0, 0, 0);
    tft.dmaWait();
    tft.endWrite();                         // frees the shared bus for the touch chip
    jpeg.close();
    return ok;
}

// PAUSED / skip / MUTED labels in the bottom black bar (if there is one)
static void drawStatus() {
    const int barY = vidY0 + hdr.height * vidScale;
    if (320 - barY < 18) return;
    tft.fillRect(0, barY, 480, 320 - barY, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    const int y = barY + (320 - barY) / 2;
    if (paused) { tft.setTextDatum(ML_DATUM); tft.drawString("PAUSED", 8, y, 2); }
    if (skipShownAt) {
        char label[16];
        snprintf(label, sizeof(label), ">> +%u s", SKIP_MS / 1000);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(label, 240, y, 2);
    }
    if (muted)  { tft.setTextDatum(MR_DATUM); tft.drawString("MUTED", 472, y, 2); }
}

// -----------------------------------------------------------------------------
// TOUCH: single vs double tap detector
// -----------------------------------------------------------------------------
enum Tap { TAP_NONE, TAP_SINGLE, TAP_DOUBLE };
enum TouchState { TS_IDLE, TS_DOWN, TS_WAIT_SECOND, TS_DOWN_SECOND };
TouchState touchState = TS_IDLE;
unsigned long lastSeenDown = 0, releasedAt = 0;
Tap tapQueue = TAP_NONE;

bool touchDown() {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    return p.z >= 500 && p.z < 3900;  // reject phantom/noise readings
}

void pollTouch() {
    unsigned long now = millis();
    bool down = touchDown();
    if (down) lastSeenDown = now;
    bool released = !down && now - lastSeenDown > TAP_DEBOUNCE_MS;

    switch (touchState) {
        case TS_IDLE:
            if (down) touchState = TS_DOWN;
            break;
        case TS_DOWN:
            if (released) { touchState = TS_WAIT_SECOND; releasedAt = now; }
            break;
        case TS_WAIT_SECOND:
            if (down) {
                touchState = TS_DOWN_SECOND;
                tapQueue = TAP_DOUBLE;
            } else if (now - releasedAt > DOUBLE_TAP_MS) {
                touchState = TS_IDLE;
                if (tapQueue == TAP_NONE) tapQueue = TAP_SINGLE;
            }
            break;
        case TS_DOWN_SECOND:
            if (released) touchState = TS_IDLE;
            break;
    }
}

Tap takeTap() {
    Tap t = tapQueue;
    tapQueue = TAP_NONE;
    return t;
}

// -----------------------------------------------------------------------------
// LIGHT SENSOR: a fingertip over the LDR = skip ahead
// -----------------------------------------------------------------------------
int ldrBase = -1;           // room darkness level (bigger = darker)
int ldrLast = 0;            // latest reading, for the serial stats
bool ldrCovered = false;
unsigned long ldrCoveredAt = 0, ldrBaseAt = 0, ldrLastDark = 0;

// true once per cover
bool pollLdr() {
    unsigned long now = millis();
    int v = analogRead(CYD_LDR_PIN);
    if (LDR_DARK_IS_LOW) v = 4095 - v;
    ldrLast = v;
    if (ldrBase < 0) ldrBase = v;
    if (!ldrCovered) {
        if (v - ldrBase >= LDR_COVER_RISE) {
            ldrCovered = true;
            ldrCoveredAt = ldrLastDark = now;
            Serial.printf("[LDR] covered %d (room %d)\n", v, ldrBase);
            return true;
        }
        if (now - ldrBaseAt >= 100) {  // follow the room light slowly (~1.5 s)
            ldrBaseAt = now;
            ldrBase += (v - ldrBase) / 16;
        }
    } else if (v - ldrBase >= LDR_COVER_RISE / 2) {
        ldrLastDark = now;
        if (now - ldrCoveredAt > 10000) {  // dark for 10 s: the room got darker, not a finger
            ldrCovered = false;
            ldrBase = v;
        }
    } else if (now - ldrLastDark >= LDR_RELEASE_MS) {
        ldrCovered = false;
    }
    return false;
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n[ESP32-035] Starting video clip player (scaled JPEG frames + DAC audio)...");

    // 1. Backlight on GPIO 27 (Active HIGH)
    pinMode(CYD_BACKLIGHT, OUTPUT);
    digitalWrite(CYD_BACKLIGHT, HIGH);

    // 2. RGB LED off (Active LOW)
    pinMode(CYD_LED_RED,   OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE,  OUTPUT);
    digitalWrite(CYD_LED_RED,   HIGH);
    digitalWrite(CYD_LED_GREEN, HIGH);
    digitalWrite(CYD_LED_BLUE,  HIGH);

    // 3. Touch CS + light sensor
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);
    pinMode(CYD_LDR_PIN, INPUT);

    // 4. TFT Display Initialisation (Standard Working Landscape Rotation 1)
    tft.init();
    tft.setRotation(1);
    tft.setSwapBytes(false);  // JPEGDEC outputs big-endian RGB565 already
    tft.fillScreen(TFT_BLACK);

    // 5. Touch Controller Initialisation, then DMA (touchSPI.begin resets the bus)
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);
    tft.initDMA();

    // 6. Clip, then the scaled strip buffers (2 x S*S*4 KB, DMA capable: 16 KB at 2x, 36 KB at 3x)
    clipOk = loadClip();
    for (int k = 0; clipOk && k < 2; k++) {
        strip[k] = (uint16_t*)heap_caps_malloc(MAX_BUFFERED_PIXELS * vidScale * vidScale * 2, MALLOC_CAP_DMA);
        if (!strip[k]) clipOk = fail("No RAM for %dx strip buffers", vidScale);
    }
    if (!clipOk) {
        Serial.printf("[ESP32-035] No clip: %s\n", noClipReason);
        Serial.println("[ESP32-035] Run examples/video-clip/tools/flash_clip.sh CLIP.bin");
        showNoClip();
    } else {
        Serial.printf("[Clip] %ux%u x%d @ %u.%02u fps, %u frames (%.1f s), biggest frame %u bytes\n",
                      hdr.width, hdr.height, vidScale, hdr.fpsX100 / 100, hdr.fpsX100 % 100, (unsigned)hdr.frameCount,
                      clipDurMs / 1000.0f, (unsigned)maxFrameBytes);
        clockBase = millis();   // playback starts now (audio task not running yet)
        if (hasAudio && !startAudio()) { hasAudio = false; Serial.println("[Clip] I2S DAC init failed"); }
        if (hasAudio) Serial.printf("[Clip] audio %u Hz u8 (%u bytes), DAC at %u Hz\n", (unsigned)hdr.audioRate,
                                    (unsigned)hdr.audioBytes, (unsigned)(hdr.audioRate * audioUp));
        else Serial.println("[Clip] no audio");
    }
    if (!hasAudio) {
        pinMode(CYD_SPEAKER_PIN, OUTPUT);
        digitalWrite(CYD_SPEAKER_PIN, LOW);
    }
    Serial.printf("[ESP32-035] Heap free: %u bytes (largest block %u)\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

// -----------------------------------------------------------------------------
// MAIN LOOP: show whichever frame the clock points at, skip frames when behind
// -----------------------------------------------------------------------------
void loop() {
    static unsigned long fpsT = millis();
    static uint32_t frames = 0, skipped = 0, errors = 0, i2sT = 0;
    static uint64_t busyUs = 0;
    static uint32_t lastFrame = UINT32_MAX;
    static bool skipped4s = false;   // the next frame jump is a skip, not a slow frame

    if (!clipOk) {
        delay(50);
        if (millis() - fpsT >= 5000) {
            Serial.printf("[ESP32-035] No clip: %s - run tools/flash_clip.sh CLIP.bin\n", noClipReason);
            fpsT = millis();
        }
        return;
    }

    pollTouch();
    Tap tap = takeTap();
    if (tap == TAP_SINGLE) {
        setPaused(!paused);
        Serial.println(paused ? "[Touch] single tap: paused" : "[Touch] single tap: playing");
        drawStatus();
    } else if (tap == TAP_DOUBLE) {
        muted = !muted;
        Serial.println(muted ? "[Touch] double tap: muted" : "[Touch] double tap: sound on");
        drawStatus();
    }
    if (pollLdr()) {
        skipAhead(SKIP_MS);
        skipped4s = true;
        skipShownAt = millis() | 1;
        Serial.printf("[LDR] skip +%u s\n", SKIP_MS / 1000);
        drawStatus();
    } else if (skipShownAt && millis() - skipShownAt > 1000) {
        skipShownAt = 0;
        drawStatus();
    }

    // while paused the clock stands still, so this only changes after a skip
    uint32_t frame = min<uint32_t>((uint64_t)clipPosMs() * hdr.fpsX100 / 100000, hdr.frameCount - 1);
    if (frame != lastFrame) {
        if (lastFrame != UINT32_MAX && !skipped4s) skipped += (frame + hdr.frameCount - lastFrame - 1) % hdr.frameCount;
        skipped4s = false;
        uint32_t t0 = micros();
        if (!showFrame(frame) && errors++ < 5)
            Serial.printf("[ESP32-035] frame %u failed (JPEG error %d)\n", (unsigned)frame, jpeg.getLastError());
        busyUs += micros() - t0;
        lastFrame = frame;
        frames++;
    } else {
        delay(2);
    }

    unsigned long now = millis();
    if (now - fpsT >= 5000) {
        uint32_t i2sNow = i2sFrames;
        char audio[32] = "";
        if (hasAudio) snprintf(audio, sizeof(audio), ", audio %.0f Hz",
                               (i2sNow - i2sT) * 1000.0f / (now - fpsT) / audioUp);
        Serial.printf("[ESP32-035] %.1f fps (%u skipped, %.1f ms/frame)%s, ldr %d (room %d)%s%s\n",
                      frames * 1000.0f / (now - fpsT), (unsigned)skipped, frames ? busyUs / 1000.0f / frames : 0.0f,
                      audio, ldrLast, ldrBase, paused ? ", paused" : "", muted ? ", muted" : "");
        frames = skipped = 0;
        busyUs = 0;
        i2sT = i2sNow;
        fpsT = now;
    }
}
