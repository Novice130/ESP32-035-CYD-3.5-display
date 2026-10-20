#!/usr/bin/env python3
"""Mirror the Mac's screen (or part of it) to the ESP32-035 CYD over Wi-Fi.

Every frame: capture -> fit into 480x320 -> draw the mouse pointer -> compare with the last
frame -> JPEG-encode only the rectangle that changed -> send. Small changes go at full 480x320;
when most of the screen moves (video, scrolling) the change goes at 240x160 shown 2x, which the
board draws twice as fast, and is re-sent sharp as soon as things calm down. Two capture threads
keep a fresh frame ready, and the board acks each frame as soon as it has it, so capturing,
sending and drawing all overlap. A still screen sends nothing but a keep-alive every 2 s.

  mirror.py                       the MacBook's built-in display
  mirror.py --display 2           another display (mirror.py --list shows them)
  mirror.py --follow              480x320 points around the mouse pointer: text stays readable
  mirror.py --follow 960          960x640 points around the pointer, shrunk to fit
  mirror.py --region X,Y,W,H      a fixed part of the screen, in points (top-left of main display = 0,0)
  mirror.py --pick                point at two corners to choose the region
"""
import argparse
import ctypes
import ctypes.util
import io
import socket
import struct
import sys
import threading
import time

try:
    import mss
    from PIL import Image, ImageChops, ImageDraw
except ImportError:
    sys.exit("Missing packages: run tools/mirror.sh (it sets up the venv) or pip install mss Pillow")

PORT = 8767
ALIGN = 16          # changed rectangles snap to JPEG block edges
PING_S = 2.0        # keep-alive while nothing changes (the board drops a silent Mac after 10 s)
SHARP_MAX = 0.35    # auto: changes up to this share of the panel go at full resolution
SETTLE_S = 0.25     # auto: blurry parts are re-sent sharp once nothing big has moved for this long


# --- mouse pointer position (CoreGraphics, global points, same space as mss) ------------------
class CGPoint(ctypes.Structure):
    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double)]


_cg = ctypes.CDLL(ctypes.util.find_library("CoreGraphics"))
_cf = ctypes.CDLL(ctypes.util.find_library("CoreFoundation"))
_cg.CGEventCreate.restype = ctypes.c_void_p
_cg.CGEventCreate.argtypes = [ctypes.c_void_p]
_cg.CGEventGetLocation.restype = CGPoint
_cg.CGEventGetLocation.argtypes = [ctypes.c_void_p]
_cf.CFRelease.argtypes = [ctypes.c_void_p]


class CGRect(ctypes.Structure):
    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double), ("w", ctypes.c_double), ("h", ctypes.c_double)]


_cg.CGGetActiveDisplayList.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32)]
_cg.CGDisplayIsBuiltin.argtypes = [ctypes.c_uint32]
_cg.CGDisplayIsBuiltin.restype = ctypes.c_bool
_cg.CGDisplayBounds.argtypes = [ctypes.c_uint32]
_cg.CGDisplayBounds.restype = CGRect


def builtin_display():
    """Bounds of the MacBook's own screen in global points, or None (lid closed, desktop Mac)."""
    ids, n = (ctypes.c_uint32 * 16)(), ctypes.c_uint32()
    _cg.CGGetActiveDisplayList(16, ids, ctypes.byref(n))
    for d in ids[:n.value]:
        if _cg.CGDisplayIsBuiltin(d):
            r = _cg.CGDisplayBounds(d)
            return {"left": int(r.x), "top": int(r.y), "width": int(r.w), "height": int(r.h)}
    return None


def mouse():
    ev = _cg.CGEventCreate(None)
    p = _cg.CGEventGetLocation(ev)
    _cf.CFRelease(ev)
    return p.x, p.y


# arrow pointer, tip at (0, 0), in screen points
ARROW = [(0, 0), (0, 17), (4, 13), (7, 20), (10, 19), (7, 12), (12, 12)]


def draw_pointer(img, x, y, k):
    """Arrow with its tip at img pixel (x, y), k img pixels per screen point."""
    pts = [(x + px * k, y + py * k) for px, py in ARROW]
    ImageDraw.Draw(img).polygon(pts, fill=(255, 255, 255), outline=(0, 0, 0))


# --- what to capture ---------------------------------------------------------------------------
def monitor_at(monitors, x, y):
    for m in monitors[1:]:
        if m["left"] <= x < m["left"] + m["width"] and m["top"] <= y < m["top"] + m["height"]:
            return m
    return monitors[1]


def follow_region(monitors, w, h):
    """w x h points centred on the pointer, kept inside the display the pointer is on."""
    x, y = mouse()
    m = monitor_at(monitors, x, y)
    w, h = min(w, m["width"]), min(h, m["height"])
    left = min(max(int(x - w / 2), m["left"]), m["left"] + m["width"] - w)
    top = min(max(int(y - h / 2), m["top"]), m["top"] + m["height"] - h)
    return {"left": left, "top": top, "width": int(w), "height": int(h)}


def pick_region():
    input("Put the mouse pointer on the TOP-LEFT corner of the area to mirror, then press Enter here... ")
    x0, y0 = mouse()
    input("Now on the BOTTOM-RIGHT corner, then press Enter... ")
    x1, y1 = mouse()
    x0, x1 = sorted((int(x0), int(x1)))
    y0, y1 = sorted((int(y0), int(y1)))
    if x1 - x0 < 16 or y1 - y0 < 16:
        sys.exit("That area is too small, try again")
    r = {"left": x0, "top": y0, "width": x1 - x0, "height": y1 - y0}
    print(f"Region: --region {x0},{y0},{x1 - x0},{y1 - y0}  (reuse this next time)")
    return r


# --- talking to the board --------------------------------------------------------------------
def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("the board closed the connection")
        buf += chunk
    return buf


def connect(host, port):
    print(f"Connecting to {host}:{port} ...")
    sock = socket.create_connection((host, port), timeout=5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    magic, w, h, max_bytes, max_scale = struct.unpack("<4sHHIB", recv_exact(sock, 13))
    if magic != b"CYM1":
        raise ConnectionError(f"{host}:{port} isn't the screen-mirror firmware")
    print(f"Connected: panel {w}x{h}, frames up to {max_bytes // 1024} KB")
    return sock, w, h, max_bytes, max_scale


class Link:
    """Sends frames with up to WINDOW of them not yet acked, so the next frame is prepared and on
    its way while the board is still taking the previous one."""
    WINDOW = 2

    def __init__(self, sock):
        self.sock, self.unacked = sock, 0

    def send(self, scale, x, y, jpg):
        while self.unacked >= self.WINDOW:
            if recv_exact(self.sock, 1) != b"K":
                raise ConnectionError("bad ack")
            self.unacked -= 1
        self.sock.sendall(struct.pack("<BBHHHI", ord("J"), scale, x, y, 0, len(jpg)) + jpg)
        self.unacked += 1


def encode(img, quality, max_bytes):
    while True:
        out = io.BytesIO()
        img.save(out, "JPEG", quality=quality, subsampling=2 if quality < 90 else 0)
        if out.tell() <= max_bytes or quality <= 20:
            return out.getvalue()
        quality -= 15


def changed_box(prev, cur):
    """Bounding box of what changed, snapped to ALIGN, or None if nothing did."""
    if prev is None:
        return (0, 0) + cur.size
    return snap(ImageChops.difference(prev, cur).getbbox(), cur.size)


def snap(box, size):
    if not box:
        return None
    x0, y0, x1, y1 = box
    a = ALIGN
    return (x0 // a * a, y0 // a * a, min(-(-x1 // a) * a, size[0]), min(-(-y1 // a) * a, size[1]))


def union(a, b):
    if not a or not b:
        return a or b
    return (min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3]))


def area(box):
    return (box[2] - box[0]) * (box[3] - box[1]) if box else 0


# --- capture threads: the newest frame is always ready, so capturing overlaps sending -----------
class Capturer:
    THREADS = 2   # one grab takes ~25 ms; two threads grab ~2x as often

    def __init__(self, args, region_fn, cw, ch):
        self.args, self.region_fn, self.cw, self.ch = args, region_fn, cw, ch
        self.cond = threading.Condition()
        self.frame, self.frame_t, self.seq, self.error = None, 0.0, 0, None
        for i in range(self.THREADS):
            threading.Thread(target=self.worker, args=(i,), daemon=True).start()

    def worker(self, i):
        period = self.THREADS / self.args.fps   # each thread's share of the frame rate
        time.sleep(i * period / self.THREADS)    # stagger them
        try:
            with mss.MSS() as grabber:   # mss objects belong to the thread that made them
                while True:
                    t0 = time.monotonic()
                    canvas = self.capture(grabber)
                    with self.cond:
                        if t0 > self.frame_t:   # a slower grab started earlier must not replace a newer one
                            self.frame, self.frame_t, self.seq = canvas, t0, self.seq + 1
                            self.cond.notify()
                    time.sleep(max(0.0, period - (time.monotonic() - t0)))
        except Exception as e:   # e.g. a display unplugged: the sender reports it
            with self.cond:
                self.error = e
                self.cond.notify()

    def capture(self, grabber):
        cw, ch = self.cw, self.ch
        reg = self.region_fn(grabber.monitors)
        shot = grabber.grab(reg)
        src = Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX")
        # fit the capture into the canvas, keeping its shape (black bars fill the rest)
        k = min(cw / src.width, ch / src.height)
        fw, fh = max(1, round(src.width * k)), max(1, round(src.height * k))
        ox, oy = (cw - fw) // 2, (ch - fh) // 2
        canvas = Image.new("RGB", (cw, ch))
        canvas.paste(src.resize((fw, fh), Image.BILINEAR, reducing_gap=2.0), (ox, oy))
        if not self.args.no_pointer:
            mx, my = mouse()
            px = ox + (mx - reg["left"]) * fw / reg["width"]
            py = oy + (my - reg["top"]) * fh / reg["height"]
            if ox <= px < ox + fw and oy <= py < oy + fh:
                draw_pointer(canvas, px, py, max(fw / reg["width"], 0.6))   # >= 12 px on the panel
        return canvas

    def next(self, after, timeout):
        """The newest frame newer than seq `after`, or (None, after) on timeout."""
        with self.cond:
            self.cond.wait_for(lambda: self.seq > after or self.error, timeout)
            if self.error:
                raise self.error
            return (self.frame, self.seq) if self.seq > after else (None, after)


# --- main loop: send whatever changed in the newest frame, one frame in flight -----------------
def run(capturer, sock, max_scale, max_bytes, args):
    full = capturer.cw * capturer.ch
    link = Link(sock)
    prev, seq = None, 0
    blurry = None   # part of the panel last drawn at half resolution, to re-send sharp
    last_big = 0.0  # when a big change last went out at half resolution
    last_send = time.monotonic()
    stat_t, frames, halves, sent, covered = time.monotonic(), 0, 0, 0, 0

    def send(canvas, box, scale):
        img = canvas.crop(box)
        if scale > 1:
            img = img.resize((img.width // scale, img.height // scale), Image.BILINEAR)
        jpg = encode(img, args.quality, max_bytes)
        link.send(scale, box[0], box[1], jpg)
        return len(jpg)

    while True:
        canvas, seq = capturer.next(seq, PING_S)
        box = changed_box(prev, canvas) if canvas else None
        if not box and not blurry:
            if time.monotonic() - last_send > PING_S:
                link.send(1, 0, 0, b"")
                last_send = time.monotonic()
            continue
        if canvas is None:
            canvas = prev   # timed out with a blurry patch left: sharpen it from the last frame

        now = time.monotonic()
        settled = now - last_big >= SETTLE_S
        sharp = union(box, blurry)
        if args.mode == "fast" and max_scale >= 2:
            scale, out = 2, box
        elif args.mode == "sharp" or max_scale < 2:
            scale, out = 1, box
        elif area(box) > SHARP_MAX * full:
            scale, out = 2, box     # most of the screen moves: half resolution, twice the speed
            last_big = now
        elif settled:
            scale, out = 1, sharp   # calm again: full resolution, including what was sent blurry
        else:
            scale, out = 1, box     # a small change in a busy moment: sharp, the blurry part waits
        if not out:
            continue
        sent += send(canvas, out, scale)
        if scale == 2:
            blurry = union(blurry, out) if args.mode == "auto" else None
            halves += 1
        elif out == sharp:
            blurry = None
        prev = canvas
        last_send = time.monotonic()
        frames += 1
        covered += area(out)

        now = time.monotonic()
        if now - stat_t >= 5:
            print(f"{frames / (now - stat_t):5.1f} fps  {sent / 1024 / frames:5.1f} KB/frame  "
                  f"{covered * 100 // frames // full:3d}% of screen per frame  "
                  f"{(frames - halves) * 100 // frames:3d}% sharp")
            stat_t, frames, halves, sent, covered = now, 0, 0, 0, 0


def parse_region(s):
    try:
        x, y, w, h = (int(v) for v in s.split(","))
    except ValueError:
        raise argparse.ArgumentTypeError("use X,Y,W,H in points, e.g. 0,0,1200,800")
    return {"left": x, "top": y, "width": w, "height": h}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("host", nargs="?", default="cyd.local", help="the board (default cyd.local, or its IP)")
    ap.add_argument("--port", type=int, default=PORT)
    what = ap.add_mutually_exclusive_group()
    what.add_argument("--display", type=int, help="which display to mirror (see --list); default the built-in one")
    what.add_argument("--region", type=parse_region, help="X,Y,W,H in points")
    what.add_argument("--pick", action="store_true", help="choose the region by pointing at two corners")
    what.add_argument("--follow", type=int, nargs="?", const=480, metavar="WIDTH",
                      help="follow the pointer: a WIDTH x WIDTH*2/3 points window around it (default 480)")
    ap.add_argument("--list", action="store_true", help="list the displays and exit")
    ap.add_argument("--fps", type=float, default=60, help="capture at most this often (default 60)")
    ap.add_argument("--quality", type=int, default=75, help="JPEG quality 20-95 (default 75)")
    res = ap.add_mutually_exclusive_group()
    res.add_argument("--fast", dest="mode", action="store_const", const="fast",
                     help="always send 240x160 shown 2x: blurry but fastest")
    res.add_argument("--sharp", dest="mode", action="store_const", const="sharp",
                     help="always full 480x320: sharpest, slower when a lot moves")
    ap.set_defaults(mode="auto")
    ap.add_argument("--no-pointer", action="store_true", help="don't draw the mouse pointer")
    args = ap.parse_args()

    with mss.MSS() as g:
        monitors = g.monitors
    if args.list:
        b = builtin_display()
        for i, m in enumerate(monitors[1:], 1):
            print(f"  --display {i}: {m['width']}x{m['height']} points at ({m['left']},{m['top']})"
                  + ("  <- built-in (default)" if b and all(m[k] == b[k] for k in b) else ""))
        return
    if args.region:
        region_fn = lambda mons: args.region
    elif args.pick:
        r = pick_region()
        region_fn = lambda mons: r
    elif args.follow:
        region_fn = lambda mons: follow_region(mons, args.follow, args.follow * 2 // 3)
    elif args.display:
        if not 1 <= args.display < len(monitors):
            sys.exit(f"No display {args.display}; --list shows them")
        region_fn = lambda mons: mons[args.display]
    else:
        b = builtin_display()
        if not b:
            sys.exit("No built-in display (lid closed?); pick one with --display N (--list shows them)")
        print(f"Mirroring the built-in display ({b['width']}x{b['height']} points)")
        region_fn = lambda mons: b

    capturer, sock = None, None
    while True:
        try:
            sock = None
            sock, w, h, max_bytes, max_scale = connect(args.host, args.port)
            sock.settimeout(5)
            if not capturer:
                capturer = Capturer(args, region_fn, w, h)
            run(capturer, sock, max_scale, max_bytes, args)
        except KeyboardInterrupt:
            print()
            try:   # 'Q': the board goes straight back to its waiting screen
                sock and sock.sendall(struct.pack("<BBHHHI", ord("Q"), 1, 0, 0, 0, 0))
            except OSError:
                pass
            return
        except (OSError, ConnectionError) as e:
            print(f"Lost the board ({e}); retrying in 2 s. Is it on and showing 'Screen mirror'?")
            time.sleep(2)


if __name__ == "__main__":
    main()
