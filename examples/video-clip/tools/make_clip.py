#!/usr/bin/env python3
# Converts a video into clip.bin for the video-clip player: baseline JPEG frames (240x136, YCbCr 4:2:0,
# shown 2x on the 480x320 screen) + unsigned 8-bit mono PCM, packed to fit the 0x2F0000-byte "clip"
# flash partition. Flash it with tools/flash_clip.sh; check it with tools/check_clip.py.
#
#   make_clip.py VIDEO [--audio AUDIO_FILE] [--start SEC] [--dur SEC] [--fps 15]
#                [--width 240 --height 136] [--rate 8000] [--no-audio]
#                [--budget 0x2F0000] [--quality auto|1-95] [--preview PNG] -o clip.bin
#   make_clip.py --test-pattern [--dur 10] -o test.bin     # synthetic clip, no input video
#
# --audio         separate audio file (e.g. YouTube's video-only .mp4 + audio-only .m4a); default is
#                 VIDEO's own audio track.
# --quality auto  binary-searches one JPEG quality for every frame so the whole file fits --budget.
# --preview       contact sheet of 12 frames decoded back from the JPEGs actually written, at 2x.
# Picture: scaled to cover width x height and centre-cropped (16:9 -> 240x136 loses ~2 columns);
# sources more than 10% off that aspect ratio are letterboxed instead. Frames are resampled to --fps
# (nearest source frame). Audio: mixed to mono, resampled to --rate, trimmed/padded to exactly the
# video length, 10 ms fades at both ends (the player loops), peak-normalised (max 4x gain).
# Test pattern: scrolling colour bars, grey ramp with a sweeping bar, R/G/B swatches, big frame counter
# and seconds; a beep at every whole second (higher pitch at 0 s) and a green dot lit while it plays.
#
# clip.bin (little endian): 64-byte header "CLIP", u16 version=1, u16 width, u16 height,
# u16 fps_x100, u32 frame_count N, u32 index_offset, u32 audio_rate (0 = none), u32 audio_offset,
# u32 audio_bytes, 32 zero bytes. Then N+1 u32 offsets (frame i = [idx[i], idx[i+1]), each 4-byte
# aligned, zero padded), the JPEGs, then the audio. All offsets are from the start of the file.
#
# Deps: pip install av numpy pillow
import argparse
import io
import math
import os
import struct
import sys

import av
import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_clip  # noqa: E402

HEADER_SIZE = 64
MIN_AUTO_Q, MAX_Q = 20, 95


def align4(n):
    return (n + 3) & ~3


def font(size):
    try:
        return ImageFont.load_default(size=size)
    except (TypeError, OSError):  # Pillow without FreeType: small bitmap font
        return ImageFont.load_default()


# ---------------------------------------------------------------- video

def make_fitter(sw, sh, w, h):
    """Returns f(PIL image) -> w x h image: cover + centre crop, or letterbox if the crop is > 10%."""
    cover = max(w / sw, h / sh)
    cw, ch = w / cover, h / cover  # source region that maps onto the output
    if cw * ch >= 0.9 * sw * sh:
        box = ((sw - cw) / 2, (sh - ch) / 2, (sw + cw) / 2, (sh + ch) / 2)
        return (lambda im: im.resize((w, h), Image.LANCZOS, box=box)), "crop to %.0fx%.0f" % (cw, ch)
    fit = min(w / sw, h / sh)
    nw, nh = max(1, round(sw * fit)), max(1, round(sh * fit))

    def letterbox(im):
        out = Image.new("RGB", (w, h))
        out.paste(im.resize((nw, nh), Image.LANCZOS), ((w - nw) // 2, (h - nh) // 2))
        return out
    return letterbox, "letterbox to %dx%d" % (nw, nh)


def load_frames(path, start, dur, fps, w, h):
    """Decode VIDEO from start, pick the nearest source frame for each output tick k/fps."""
    c = av.open(path)
    if not c.streams.video:
        sys.exit("%s has no video stream" % path)
    vs = c.streams.video[0]
    vs.thread_type = "AUTO"
    sw, sh = vs.codec_context.width, vs.codec_context.height
    src_fps = float(vs.average_rate or 30)
    total = c.duration / 1e6 if c.duration else None
    if total is not None and start >= total:
        sys.exit("--start %.2f is past the end of the video (%.2f s)" % (start, total))
    want = round(dur * fps) if dur else None
    fitter, how = make_fitter(sw, sh, w, h)
    print("video : %s %dx%d %.2f fps%s, %s -> %dx%d @ %g fps"
          % (os.path.basename(path), sw, sh, src_fps, " %.1f s" % total if total else "", how, w, h, fps))

    if start > 0:
        c.seek(int(start / vs.time_base), stream=vs, backward=True)
    frames, k = [], 0
    prev = prev_t = prev_img = None

    def emit(until):
        nonlocal k, prev_img
        while (want is None or k < want) and start + k / fps < until:
            if prev_img is None:
                prev_img = np.asarray(fitter(prev.to_image()))
            frames.append(prev_img)
            k += 1

    for f in c.decode(vs):
        if f.time is None:
            continue
        if prev is not None:
            emit((prev_t + f.time) / 2)
            if want is not None and k >= want:
                break
        prev, prev_t, prev_img = f, f.time, None
    if prev is not None and (want is None or k < want):
        emit(prev_t + 1 / src_fps)
    c.close()
    if not frames:
        sys.exit("no frames decoded")
    if want is not None and len(frames) < want:
        print("note  : video ends early, clip is %.2f s instead of %.2f s" % (len(frames) / fps, dur))
    return np.stack(frames)


# ---------------------------------------------------------------- audio

def to_float_mono(f):
    a = f.to_ndarray()
    ch = len(f.layout.channels)
    if not f.format.is_planar:
        a = a.reshape(-1, ch).T
    if a.dtype == np.int16:
        a = a / 32768.0
    elif a.dtype == np.int32:
        a = a / 2147483648.0
    elif a.dtype == np.uint8:
        a = (a.astype(np.float32) - 128) / 128.0
    return a.astype(np.float32).mean(axis=0)


def load_audio(path, start, n_out, rate):
    """Audio from start, mono, resampled to rate, exactly n_out samples, as u8 (128 = silence)."""
    c = av.open(path)
    if not c.streams.audio:
        sys.exit("%s has no audio stream (use --audio FILE or --no-audio)" % path)
    s = c.streams.audio[0]
    sr = s.codec_context.sample_rate
    need = start + n_out / rate + 0.25  # a little extra for the resampler's filter tail
    if start > 0:
        c.seek(int(max(0.0, start - 1.0) / s.time_base), stream=s, backward=True)
    chunks, first_t, got = [], None, 0
    for f in c.decode(s):
        if f.time is None:
            continue
        if first_t is None:
            first_t = f.time
        chunks.append(to_float_mono(f))
        got += f.samples
        if first_t + got / sr >= need:
            break
    c.close()
    if first_t is None:
        sys.exit("no audio decoded from %s" % path)
    mono = np.concatenate(chunks)
    skip = round((start - first_t) * sr)
    mono = mono[skip:] if skip >= 0 else np.concatenate([np.zeros(-skip, np.float32), mono])
    mono = mono[:round(n_out * sr / rate) + sr // 4]

    rs = av.AudioResampler(format="flt", layout="mono", rate=rate)
    outs = []
    for i in range(0, len(mono), 8192):
        fr = av.AudioFrame.from_ndarray(np.ascontiguousarray(mono[None, i:i + 8192]), format="flt",
                                        layout="mono")
        fr.sample_rate = sr
        for o in rs.resample(fr):
            outs.append(o.to_ndarray().reshape(-1))
    for o in rs.resample(None):
        outs.append(o.to_ndarray().reshape(-1))
    pcm = np.concatenate(outs) if outs else np.zeros(0, np.float32)
    pcm = np.concatenate([pcm, np.zeros(max(0, n_out - len(pcm)), np.float32)])[:n_out]
    fade = min(len(pcm) // 2, rate // 100)  # 10 ms fade in/out: no click when the clip loops
    if fade:
        ramp = np.linspace(0.0, 1.0, fade, dtype=np.float32)
        pcm[:fade] *= ramp
        pcm[-fade:] *= ramp[::-1]
    peak = float(np.abs(pcm).max()) if len(pcm) else 0.0
    gain = min(4.0, 0.98 / peak) if peak > 1e-4 else 1.0
    print("audio : %s %d Hz -> %d Hz u8 mono, %d samples, peak %.2f, gain %.2fx"
          % (os.path.basename(path), sr, rate, n_out, peak, gain))
    return np.clip(np.round(pcm * gain * 127) + 128, 0, 255).astype(np.uint8)


# ---------------------------------------------------------------- test pattern

BARS = [(235, 235, 235), (235, 235, 16), (16, 235, 235), (16, 235, 16),
        (235, 16, 235), (235, 16, 16), (16, 16, 235)]


def test_frames(n, fps, w, h):
    big, small = font(max(12, h * 30 // 100)), font(max(9, h * 11 // 100))
    bar_h, ramp_y = h * 45 // 100, h * 45 // 100
    ramp_h = h * 12 // 100
    info_y = ramp_y + ramp_h
    bw = max(1, w // len(BARS))
    frames = []
    for k in range(n):
        t = k / fps
        im = Image.new("RGB", (w, h), (0, 0, 0))
        d = ImageDraw.Draw(im)
        shift = int(t * 30)  # colour bars scroll left 30 px/s
        for x in range(w):
            d.line((x, 0, x, bar_h - 1), fill=BARS[((x + shift) // bw) % len(BARS)])
        for x in range(w):  # grey ramp, black -> white, 16 steps
            v = min(255, (x * 16 // w) * 17)
            d.line((x, ramp_y, x, ramp_y + ramp_h - 1), fill=(v, v, v))
        sx = int((0.5 - 0.5 * math.cos(math.pi * t)) * (w - 4))  # bar sweeps across every second
        d.rectangle((sx, ramp_y, sx + 3, ramp_y + ramp_h - 1), fill=(255, 40, 40))
        for i, (lab, col) in enumerate((("R", (255, 0, 0)), ("G", (0, 255, 0)), ("B", (0, 0, 255)))):
            y0 = info_y + 3 + i * ((h - info_y - 6) // 3)
            sz = (h - info_y - 6) // 3 - 2
            d.rectangle((4, y0, 4 + sz, y0 + sz), fill=col)
            d.text((8 + sz, y0 + sz // 2), lab, fill=(255, 255, 255), font=small, anchor="lm")
        d.text((w // 2, bar_h // 2), "%04d" % k, fill=(255, 255, 255), font=big, anchor="mm",
               stroke_width=3, stroke_fill=(0, 0, 0))
        d.text((w // 2 + 8, (info_y + h) // 2), "%6.2f s" % t, fill=(255, 220, 0), font=small,
               anchor="mm")
        d.text((w - 4, h - 3), "%dx%d %gfps" % (w, h, fps), fill=(150, 150, 150),
               font=font(max(8, h * 7 // 100)), anchor="rd")
        lit = (t % 1.0) < 0.12  # same instant as the beep
        r = max(4, h // 14)
        d.ellipse((w - 6 - 2 * r, info_y + 4, w - 6, info_y + 4 + 2 * r),
                  fill=(0, 255, 60) if lit else (0, 60, 20), outline=(255, 255, 255))
        d.rectangle((0, 0, w - 1, h - 1), outline=(255, 255, 255))  # 1 px border: shows any crop
        frames.append(np.asarray(im))
    return np.stack(frames)


def test_audio(n, rate):
    t = np.arange(n) / rate
    sec = np.floor(t)
    ph = t - sec
    freq = np.where(sec % 10 == 0, 1760.0, 880.0)
    on = ph < 0.12
    env = np.clip(np.minimum(ph, 0.12 - ph) / 0.005, 0, 1) * on  # 5 ms fades, no clicks
    return np.clip(np.round(128 + 100 * env * np.sin(2 * np.pi * freq * ph)), 0, 255).astype(np.uint8)


# ---------------------------------------------------------------- encode / pack

def encode(frames, q):
    out = []
    for fr in frames:
        buf = io.BytesIO()
        Image.fromarray(fr).save(buf, "JPEG", quality=q, subsampling=2, progressive=False,
                                 optimize=True)
        b = buf.getvalue()
        if b"\xff\xc0" not in b[:1024]:
            sys.exit("Pillow did not write a baseline (SOF0) JPEG")
        out.append(b)
    return out


def total_size(jpegs, audio_bytes):
    return HEADER_SIZE + 4 * (len(jpegs) + 1) + sum(align4(len(j)) for j in jpegs) + audio_bytes


def pick_quality(frames, audio_bytes, budget, fps):
    cache = {}

    def size(q):
        if q not in cache:
            cache[q] = encode(frames, q)
        return total_size(cache[q], audio_bytes)

    if size(MIN_AUTO_Q) > budget:
        per_s = size(MIN_AUTO_Q) / (len(frames) / fps)  # bytes per second of clip
        sys.exit("does not fit even at quality %d: %d bytes > budget %d.\n"
                 "Use a shorter --dur (about %.0f s fits at this fps) or a lower --fps."
                 % (MIN_AUTO_Q, size(MIN_AUTO_Q), budget, budget / per_s))
    lo, hi = MIN_AUTO_Q, MAX_Q  # invariant: lo fits
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if size(mid) <= budget:
            lo = mid
        else:
            hi = mid - 1
    return lo, cache[lo]


def write_clip(path, jpegs, w, h, fps, rate, audio):
    n = len(jpegs)
    index_off = HEADER_SIZE
    off = index_off + 4 * (n + 1)
    idx, body = [], bytearray()
    for j in jpegs:
        idx.append(off + len(body))
        body += j + b"\0" * (align4(len(j)) - len(j))
    idx.append(off + len(body))
    audio_off = idx[-1]
    ab = len(audio) if rate else 0
    hdr = struct.pack("<4sHHHHIIIII", b"CLIP", 1, w, h, round(fps * 100), n, index_off,
                      rate if ab else 0, audio_off, ab)
    hdr += b"\0" * (HEADER_SIZE - len(hdr))
    with open(path, "wb") as f:
        f.write(hdr + struct.pack("<%dI" % (n + 1), *idx) + bytes(body) + (audio.tobytes() if ab else b""))


def write_preview(path, jpegs, fps, q, title):
    picks = sorted(set(round(i * (len(jpegs) - 1) / 11) for i in range(12)))
    tiles = []
    for i in picks:
        im = Image.open(io.BytesIO(jpegs[i])).convert("RGB")
        tiles.append((i, im.resize((im.width * 2, im.height * 2), Image.NEAREST)))
    tw, th = tiles[0][1].size
    cols, lab, gap = 3, 18, 8
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * tw + (cols + 1) * gap, 26 + rows * (th + lab + gap)), (32, 32, 32))
    d = ImageDraw.Draw(sheet)
    avg = sum(len(j) for j in jpegs) / len(jpegs) / 1024
    d.text((gap, 6), "%s  |  %d frames @ %g fps, JPEG q%d, avg %.2f KB/frame  (2x, as on the board)"
           % (title, len(jpegs), fps, q, avg), fill=(255, 255, 255), font=font(14))
    for n, (i, im) in enumerate(tiles):
        x = gap + (n % cols) * (tw + gap)
        y = 26 + (n // cols) * (th + lab + gap)
        d.text((x, y + 1), "#%d  t=%.2fs  %.1f KB" % (i, i / fps, len(jpegs[i]) / 1024),
               fill=(255, 220, 0), font=font(13))
        sheet.paste(im, (x, y + lab))
    sheet.save(path)
    print("wrote preview", path)


def main():
    ap = argparse.ArgumentParser(description="Make clip.bin for the CYD video-clip player")
    ap.add_argument("video", nargs="?")
    ap.add_argument("--test-pattern", action="store_true", help="synthetic clip, no input video")
    ap.add_argument("--audio", help="separate audio file (default: VIDEO's audio track)")
    ap.add_argument("--start", type=float, default=0.0)
    ap.add_argument("--dur", type=float, help="seconds (default: to the end; test pattern 10)")
    ap.add_argument("--fps", type=float, default=15.0)
    ap.add_argument("--width", type=int, default=240)
    ap.add_argument("--height", type=int, default=136)
    ap.add_argument("--rate", type=int, default=8000)
    ap.add_argument("--no-audio", action="store_true")
    ap.add_argument("--budget", type=lambda s: int(s, 0), default=0x2F0000)
    ap.add_argument("--quality", default="auto", help="auto or 1-95")
    ap.add_argument("--preview", metavar="PNG")
    ap.add_argument("-o", "--out", required=True)
    a = ap.parse_args()

    if a.test_pattern == bool(a.video):
        ap.error("give either VIDEO or --test-pattern")
    if a.quality != "auto" and not (a.quality.isdigit() and 1 <= int(a.quality) <= MAX_Q):
        ap.error("--quality must be auto or 1-%d" % MAX_Q)
    if not (0 < a.width <= 240 and 0 < a.height <= 160):
        ap.error("--width/--height must fit 2x on the 480x320 screen (max 240x160)")

    if a.test_pattern:
        dur = a.dur or 10.0
        n = round(dur * a.fps)
        print("test pattern: %d frames, %.2f s @ %g fps, %dx%d" % (n, n / a.fps, a.fps, a.width, a.height))
        frames = test_frames(n, a.fps, a.width, a.height)
    else:
        frames = load_frames(a.video, a.start, a.dur, a.fps, a.width, a.height)
    n = len(frames)

    rate = 0 if a.no_audio else a.rate
    audio = np.zeros(0, np.uint8)
    if rate:
        n_audio = round(n * rate / a.fps)
        audio = test_audio(n_audio, rate) if a.test_pattern else \
            load_audio(a.audio or a.video, a.start, n_audio, rate)

    if a.quality == "auto":
        q, jpegs = pick_quality(frames, len(audio), a.budget, a.fps)
    else:
        q, jpegs = int(a.quality), encode(frames, int(a.quality))
        if total_size(jpegs, len(audio)) > a.budget:
            sys.exit("%d bytes > budget %d at quality %d: lower --quality (or use auto), --dur or --fps"
                     % (total_size(jpegs, len(audio)), a.budget, q))
    size = total_size(jpegs, len(audio))
    print("quality %d%s: avg %.2f KB/frame, total %d / %d bytes (%.1f%%)"
          % (q, " (auto)" if a.quality == "auto" else "", sum(len(j) for j in jpegs) / n / 1024,
             size, a.budget, 100.0 * size / a.budget))

    write_clip(a.out, jpegs, a.width, a.height, a.fps, rate, audio)
    print("wrote", a.out)
    if a.preview:
        write_preview(a.preview, jpegs, a.fps, q,
                      "test pattern" if a.test_pattern else os.path.basename(a.video))
    ok = check_clip.check(a.out, a.budget)[0]
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
