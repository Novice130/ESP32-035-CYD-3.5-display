#!/usr/bin/env python3
# Validates a clip.bin made by make_clip.py and prints a summary: header fields, index (monotonic,
# 4-byte aligned, in bounds), every frame (baseline JPEG = SOF0, size matches header, YCbCr 4:2:0,
# no EXIF, only zero padding after EOI), audio block, and total size vs the flash budget.
#
#   check_clip.py clip.bin [--budget 0x2F0000] [--render N OUT.png]
#
# --render N OUT.png: decode frame N and draw it the way the board shows it: 480x320 screen,
# black bars, frame scaled 2x (nearest) and centred (240x136 -> 480x272 at y=24).
# Exit status 1 if anything is wrong. Deps: Pillow (only for --render).
import argparse
import io
import struct
import sys

HEADER_SIZE = 64
SCREEN_W, SCREEN_H = 480, 320

# libjpeg's standard luminance table, used to estimate the encoder's quality setting
STD_LUMA_SUM = sum([16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
                    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
                    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
                    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99])
SOF_MARKERS = {0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF}


def read_header(data):
    if len(data) < HEADER_SIZE:
        raise ValueError("file shorter than the 64-byte header")
    f = struct.unpack_from("<4sHHHHIIIII", data, 0)
    keys = ("magic", "version", "width", "height", "fps_x100", "frame_count", "index_offset",
            "audio_rate", "audio_offset", "audio_bytes")
    h = dict(zip(keys, f))
    h["pad"] = data[32:64]
    return h


def parse_jpeg(b):
    """Walk the marker segments of one JPEG. Returns dict with sof, dims, sampling, markers, end."""
    if b[:2] != b"\xff\xd8":
        raise ValueError("no SOI")
    i, n = 2, len(b)
    info = {"sofs": [], "markers": [], "dqt_luma": None, "end": None}
    while i < n:
        if b[i] != 0xFF:
            raise ValueError("expected marker at byte %d" % i)
        while i < n and b[i] == 0xFF:
            i += 1
        if i >= n:
            break
        m = b[i]
        i += 1
        if m == 0xD9:  # EOI
            info["end"] = i
            break
        if m == 0x01 or 0xD0 <= m <= 0xD7:
            continue
        (length,) = struct.unpack_from(">H", b, i)
        seg = b[i + 2:i + length]
        i += length
        info["markers"].append(m)
        if m in SOF_MARKERS:
            info["sofs"].append(m)
            prec, hgt, wid, ncomp = struct.unpack_from(">BHHB", seg, 0)
            comps = [(seg[6 + 3 * k], seg[7 + 3 * k]) for k in range(ncomp)]
            info.update(width=wid, height=hgt, precision=prec, comps=comps)
        elif m == 0xDB:  # DQT: may hold several tables
            j = 0
            while j < len(seg):
                pq, tq = seg[j] >> 4, seg[j] & 15
                vals = (struct.unpack_from(">64H", seg, j + 1) if pq else seg[j + 1:j + 65])
                if tq == 0:
                    info["dqt_luma"] = list(vals)
                j += 1 + 64 * (2 if pq else 1)
        elif m == 0xDA:  # SOS: skip entropy-coded data up to the next real marker
            while i < n - 1:
                if b[i] == 0xFF and b[i + 1] != 0 and not (0xD0 <= b[i + 1] <= 0xD7):
                    break
                i += 1
    if info["end"] is None:
        raise ValueError("no EOI")
    return info


def estimate_quality(luma):
    if not luma:
        return None
    scale = sum(luma) * 100.0 / STD_LUMA_SUM
    q = (200 - scale) / 2 if scale <= 100 else 5000 / scale
    return max(1, min(100, round(q)))


def check(path, budget=0x2F0000, quiet=False):
    """Returns (ok, header, index, data). Prints problems and a summary unless quiet."""
    data = open(path, "rb").read()
    errs = []
    out = (lambda *a: None) if quiet else print

    try:
        h = read_header(data)
    except ValueError as e:
        print("FAIL:", e)
        return False, None, None, data
    if h["magic"] != b"CLIP":
        errs.append("bad magic %r" % h["magic"])
    if h["version"] != 1:
        errs.append("version %d != 1" % h["version"])
    if any(h["pad"]):
        errs.append("header padding (bytes 32..63) not zero")
    W, H, N = h["width"], h["height"], h["frame_count"]
    if not (0 < W <= SCREEN_W // 2 and 0 < H <= SCREEN_H // 2):
        errs.append("size %dx%d does not fit 2x on %dx%d" % (W, H, SCREEN_W, SCREEN_H))
    if h["fps_x100"] == 0 or N == 0:
        errs.append("fps_x100=%d frame_count=%d" % (h["fps_x100"], N))
    if len(data) > budget:
        errs.append("file is %d bytes > budget %d" % (len(data), budget))

    io_ = h["index_offset"]
    idx = []
    n_errs = len(errs)
    if io_ % 4 or io_ < HEADER_SIZE or io_ + 4 * (N + 1) > len(data):
        errs.append("index_offset %d bad (aligned? past header? in file?)" % io_)
    else:
        idx = list(struct.unpack_from("<%dI" % (N + 1), data, io_))
        if idx[0] < io_ + 4 * (N + 1):
            errs.append("first frame overlaps the index")
        if idx[-1] > len(data):
            errs.append("index end %d past file end %d" % (idx[-1], len(data)))
        bad_align = [i for i, o in enumerate(idx) if o % 4]
        if bad_align:
            errs.append("%d index entries not 4-byte aligned (first: #%d)" % (len(bad_align), bad_align[0]))
        bad_order = [i for i in range(N) if idx[i + 1] <= idx[i]]
        if bad_order:
            errs.append("index not strictly increasing at #%d" % bad_order[0])

    sizes, quals, sof_bad, frame_errs = [], set(), 0, []
    idx_ok = idx and len(errs) == n_errs  # don't walk frames through a broken index
    if idx_ok:
        for i in range(N):
            b = data[idx[i]:idx[i + 1]]
            try:
                j = parse_jpeg(b)
            except (ValueError, struct.error) as e:
                frame_errs.append("frame %d: %s" % (i, e))
                continue
            sizes.append(j["end"])
            if j["sofs"] != [0xC0]:
                sof_bad += 1
                frame_errs.append("frame %d: SOF markers %s (need only SOF0 = baseline)"
                                  % (i, ["%02X" % s for s in j["sofs"]]))
            if (j.get("width"), j.get("height")) != (W, H):
                frame_errs.append("frame %d: JPEG is %sx%s, header says %dx%d"
                                  % (i, j.get("width"), j.get("height"), W, H))
            comps = j.get("comps", [])
            if len(comps) != 3 or comps[0][1] != 0x22 or comps[1][1] != 0x11 or comps[2][1] != 0x11:
                frame_errs.append("frame %d: not YCbCr 4:2:0 (components %s)" % (i, comps))
            if 0xE1 in j["markers"]:
                frame_errs.append("frame %d: has APP1 (EXIF)" % i)
            tail = b[j["end"]:]
            if len(tail) > 3 or any(tail):
                frame_errs.append("frame %d: %d bytes after EOI are not 0-3 zero pad bytes" % (i, len(tail)))
            quals.add(estimate_quality(j["dqt_luma"]))
        errs += frame_errs[:10]
        if len(frame_errs) > 10:
            errs.append("... %d more frame errors" % (len(frame_errs) - 10))

    fps = h["fps_x100"] / 100.0 if h["fps_x100"] else 0
    vdur = N / fps if fps else 0
    rate, ao, ab = h["audio_rate"], h["audio_offset"], h["audio_bytes"]
    if rate:
        if ao + ab > len(data):
            errs.append("audio block [%d, %d) past file end %d" % (ao, ao + ab, len(data)))
        if idx and ao < idx[-1]:
            errs.append("audio_offset %d overlaps frames (end %d)" % (ao, idx[-1]))
        want = round(N * rate / fps) if fps else 0
        if abs(ab - want) > 1:
            errs.append("audio is %d samples, video length needs %d" % (ab, want))
    elif ab:
        errs.append("audio_rate is 0 but audio_bytes = %d" % ab)

    out("%s: %d bytes" % (path, len(data)))
    out("  header : CLIP v%d  %dx%d  %.2f fps  %d frames = %.2f s" % (h["version"], W, H, fps, N, vdur))
    if sizes:
        q = sorted(x for x in quals if x is not None)
        out("  frames : JPEG %s, 4:2:0, avg %.2f KB (min %.2f, max %.2f), quality ~%s, %d/%d SOF0"
            % ("baseline" if not sof_bad else "NOT baseline", sum(sizes) / len(sizes) / 1024,
               min(sizes) / 1024, max(sizes) / 1024,
               q[0] if len(q) == 1 else "%s..%s" % (q[0], q[-1]) if q else "?", N - sof_bad, N))
        out("  layout : index @%d (%d entries), frames %d..%d (all 4-byte aligned)"
            % (io_, N + 1, idx[0], idx[-1]))
    if rate:
        a = data[ao:ao + ab]
        out("  audio  : %d Hz u8 mono, %d bytes = %.3f s @%d, range %d..%d"
            % (rate, ab, ab / rate, ao, min(a) if a else 0, max(a) if a else 0))
    else:
        out("  audio  : none")
    out("  budget : %d / %d bytes (%.1f%%), %d free" % (len(data), budget, 100.0 * len(data) / budget,
                                                       budget - len(data)))
    for e in errs:
        print("  ERROR:", e)
    out("  result : %s" % ("OK" if not errs else "FAILED (%d problems)" % len(errs)))
    return not errs, h, idx, data


def render(h, idx, data, n, out_png):
    from PIL import Image
    if not 0 <= n < h["frame_count"]:
        raise SystemExit("frame %d out of range 0..%d" % (n, h["frame_count"] - 1))
    img = Image.open(io.BytesIO(data[idx[n]:idx[n + 1]])).convert("RGB")
    big = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
    screen = Image.new("RGB", (SCREEN_W, SCREEN_H), (0, 0, 0))
    screen.paste(big, ((SCREEN_W - big.width) // 2, (SCREEN_H - big.height) // 2))
    screen.save(out_png)
    print("wrote", out_png, "(frame %d as shown on the %dx%d screen)" % (n, SCREEN_W, SCREEN_H))


def main():
    ap = argparse.ArgumentParser(description="Validate a CYD video-clip clip.bin")
    ap.add_argument("clip")
    ap.add_argument("--budget", type=lambda s: int(s, 0), default=0x2F0000)
    ap.add_argument("--render", nargs=2, metavar=("N", "OUT_PNG"))
    a = ap.parse_args()
    ok, h, idx, data = check(a.clip, a.budget)
    if ok and a.render:
        render(h, idx, data, int(a.render[0]), a.render[1])
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
