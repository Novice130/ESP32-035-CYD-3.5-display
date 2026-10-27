#!/usr/bin/env python3
"""Draw the parking-sensor README pictures: docs/wiring.png (and, on request, an overview).

Drawn from code (not an image generator) so every pin name and wire is exact.
Pin layout = the common 30-pin "ESP32 DEVKIT V1" (DOIT) board, USB at the bottom.

    python3 examples/parking-sensor/tools/make_diagrams.py            # docs/wiring.png
    python3 examples/parking-sensor/tools/make_diagrams.py overview   # also docs/overview-drawn.png
"""
import sys
import math
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.join(HERE, "..", "docs")
S = 2  # supersample, scaled down at the end for smooth edges

BG = "#F7F3EA"
INK = "#1F2933"
MUTED = "#6B7280"
RED, ORANGE, GREEN, BLACK, PURPLE = "#E53935", "#F28C28", "#2E9E48", "#222222", "#B23AC9"
PCB_ESP, PCB_SONAR = "#1D3557", "#2F6DB5"
SILVER, GOLD = "#C9CED6", "#D4A72C"

FONT_DIRS = ["/System/Library/Fonts/Supplemental", "/Library/Fonts", "/usr/share/fonts/truetype/msttcorefonts"]


def font(size, bold=False, plain_bold=False):
    names = ["Arial Bold.ttf"] if plain_bold else ["Arial Rounded Bold.ttf", "Arial Bold.ttf"] if bold else ["Arial.ttf"]
    for d in FONT_DIRS:
        for n in names:
            p = os.path.join(d, n)
            if os.path.exists(p):
                return ImageFont.truetype(p, size * S)
    return ImageFont.load_default(size * S)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.im = Image.new("RGB", (w * S, h * S), BG)
        self.d = ImageDraw.Draw(self.im)

    def _p(self, pts):
        return [(x * S, y * S) for x, y in pts]

    def rect(self, x0, y0, x1, y1, fill=None, outline=None, width=2, r=0):
        self.d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), r * S, fill=fill, outline=outline,
                                 width=width * S)

    def circle(self, cx, cy, r, fill=None, outline=None, width=2):
        self.d.ellipse(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), fill=fill, outline=outline,
                       width=width * S)

    def line(self, pts, fill=INK, width=2):
        self.d.line(self._p(pts), fill=fill, width=width * S, joint="curve")
        for x, y in (pts[0], pts[-1]):  # round caps
            self.circle(x, y, width / 2, fill=fill, outline=None, width=0)

    def arc(self, cx, cy, r, a0, a1, fill=INK, width=3):
        self.d.arc(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), a0, a1, fill=fill, width=width * S)

    def text(self, xy, s, size=18, fill=INK, bold=False, anchor="la"):
        f = font(size, bold, plain_bold=bold and "Ω" in s)  # Arial Rounded has no Ω
        self.d.text((xy[0] * S, xy[1] * S), s, font=f, fill=fill, anchor=anchor)

    def pill(self, cx, cy, s, color, size=17, fg="white"):
        f = font(size, True)
        w = self.d.textlength(s, font=f) / S + 18
        h = size + 12
        self.rect(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2, fill=color, r=h / 2)
        self.text((cx, cy), s, size, fg, True, "mm")

    def paste(self, img, x, y, w, h):
        self.im.paste(img.resize((w * S, h * S), Image.NEAREST), (x * S, y * S))

    def save(self, path):
        self.im.resize((self.w, self.h), Image.LANCZOS).save(path, optimize=True)
        print(path)


def wire(c, pts, color, width=7, hops=()):
    """Orthogonal wire with rounded corners; hops = (x, y) points on its horizontal runs to jump over."""
    r = 16
    out = [pts[0]]
    for i in range(1, len(pts) - 1):
        (ax, ay), (bx, by), (cx_, cy_) = pts[i - 1], pts[i], pts[i + 1]
        la = math.hypot(bx - ax, by - ay)
        lb = math.hypot(cx_ - bx, cy_ - by)
        rr = min(r, la / 2, lb / 2)
        p0 = (bx - (bx - ax) / la * rr, by - (by - ay) / la * rr)
        p1 = (bx + (cx_ - bx) / lb * rr, by + (cy_ - by) / lb * rr)
        out.append(p0)
        for t in [k / 8 for k in range(1, 8)]:  # quadratic corner
            out.append(((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * bx + t * t * p1[0],
                        (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * by + t * t * p1[1]))
        out.append(p1)
    out.append(pts[-1])
    # hops: replace a straight horizontal stretch around x=h with a half circle bump
    for hx, hy in hops:
        for i in range(len(out) - 1):
            (x0, y0), (x1, y1) = out[i], out[i + 1]
            if abs(y0 - y1) < 0.1 and abs(y0 - hy) < 0.1 and min(x0, x1) < hx - 14 and max(x0, x1) > hx + 14:
                d = 1 if x1 > x0 else -1
                bump = [(hx - d * 14 * math.cos(math.radians(a)), y0 - 14 * math.sin(math.radians(a)))
                        for a in range(0, 181, 15)]
                out[i + 1:i + 1] = bump
                break
    c.line(out, fill=color, width=width)


def resistor(c, x, y, vertical, bands, label):
    """Resistor body centred on (x, y), 60 px long, leads drawn by the wires."""
    L, T = 30, 11
    if vertical:
        c.rect(x - T, y - L, x + T, y + L, fill="#E8D3A8", outline="#8A6D3B", width=2, r=9)
        for i, col in enumerate(bands):
            yy = y - 18 + i * 11
            c.rect(x - T + 1, yy - 3, x + T - 1, yy + 3, fill=col)
        c.text((x + 20, y), label, 17, INK, True, "lm")
    else:
        c.rect(x - L, y - T, x + L, y + T, fill="#E8D3A8", outline="#8A6D3B", width=2, r=9)
        for i, col in enumerate(bands):
            xx = x - 18 + i * 11
            c.rect(xx - 3, y - T + 1, xx + 3, y + T - 1, fill=col)
        c.text((x, y - 22), label, 17, INK, True, "mb")


# ---------------------------------------------------------------------------------------------
# Parts
# ---------------------------------------------------------------------------------------------
LEFT_PINS = ["EN", "VP", "VN", "D34", "D35", "D32", "D33", "D25", "D26", "D27", "D14", "D12", "D13", "GND", "VIN"]
RIGHT_PINS = ["D23", "D22", "TX0", "RX0", "D21", "D19", "D18", "D5", "TX2", "RX2", "D4", "D2", "D15", "GND", "3V3"]


def esp32_devkit(c, x0, y0, pitch=36, used=()):
    """Vertical 30-pin ESP32 DEVKIT V1. Returns {('L'|'R', name): (x, y)} pin positions."""
    w = 260
    top = y0 + 80
    y1 = top + 14 * pitch + 90
    c.rect(x0, y0, x0 + w, y1, fill=PCB_ESP, outline="#0E1B2E", width=3, r=14)
    # WROOM module: antenna strip + metal can
    mx0, mx1 = x0 + 80, x0 + w - 80
    c.rect(mx0, y0 + 14, mx1, y0 + 64, fill="#244B7A", outline="#0E1B2E", width=2, r=4)
    zz = [(mx0 + 10 + i * 10, y0 + 28 + (i % 2) * 22) for i in range(10)]
    c.line(zz, fill=GOLD, width=3)
    c.rect(mx0, y0 + 64, mx1, y0 + 250, fill=SILVER, outline="#8B939E", width=2, r=4)
    c.text(((mx0 + mx1) / 2, y0 + 150), "ESP32", 20, "#4B5563", True, "mm")
    c.text(((mx0 + mx1) / 2, y0 + 175), "WROOM", 14, "#4B5563", False, "mm")
    # buttons + USB
    for bx, name in ((x0 + 70, "EN"), (x0 + w - 70, "BOOT")):
        c.rect(bx - 16, y1 - 58, bx + 16, y1 - 32, fill="#D9DDE3", outline="#555", width=2, r=4)
        c.circle(bx, y1 - 45, 7, fill="#333")
        c.text((bx, y1 - 26), name, 12, "#C9D3E0", True, "ma")
    c.rect(x0 + w / 2 - 34, y1 - 22, x0 + w / 2 + 34, y1 + 26, fill="#B8BEC7", outline="#6B7280", width=3, r=6)
    c.text((x0 + w / 2, y0 + 275), "ESP32 DEVKIT", 15, "#C9D3E0", True, "mm")
    pins = {}
    for side, names, px in (("L", LEFT_PINS, x0 + 22), ("R", RIGHT_PINS, x0 + w - 22)):
        for i, n in enumerate(names):
            py = top + i * pitch
            on = (side, n) in used
            c.circle(px, py, 11, fill=GOLD if on else "#B08A2E", outline="#5C4512", width=2)
            c.circle(px, py, 4, fill="#3A2C0C")
            lx = px + 18 if side == "L" else px - 18
            c.text((lx, py), n, 16 if on else 14, "white" if on else "#8FA3BF", on, "lm" if side == "L" else "rm")
            pins[(side, n)] = (px, py)
    return pins, y1


def hcsr501_back(c, x0, y0):
    """HC-SR501 PIR seen from the back (parts side, dome underneath), turned so the pins point
    right: GND, OUT, VCC from top to bottom. Returns {name: (x_pin_tip, y)}."""
    w, h = 250, 170
    c.rect(x0, y0, x0 + w, y0 + h, fill="#1E8449", outline="#145A32", width=3, r=8)
    c.rect(x0 + 80, y0 + 60, x0 + 170, y0 + 100, fill="#1B1B1B", r=3)  # BISS0001 chip
    for i in range(8):
        lx = x0 + 86 + i * 11
        c.rect(lx, y0 + 52, lx + 4, y0 + 60, fill="#BBB")
        c.rect(lx, y0 + 100, lx + 4, y0 + 108, fill="#BBB")
    c.text((x0 + 125, y0 + 80), "BISS0001", 11, "#AAA", True, "mm")
    for ky in (y0 + 70, y0 + 128):  # the two orange knobs
        c.rect(x0 + 8, ky - 18, x0 + 40, ky + 18, fill="#2B2B2B", r=3)
        c.circle(x0 + 30, ky, 13, fill="#F5A623", outline="#9C6A12", width=2)
        c.line([(x0 + 23, ky), (x0 + 37, ky)], fill="#6B4A0E", width=3)
    c.rect(x0 + 12, y0 + 8, x0 + 52, y0 + 26, fill="#F2C200", outline="#8A6D00", width=2, r=3)  # H/L jumper
    pins = {}
    for i, n in enumerate(["GND", "OUT", "VCC"]):
        py = y0 + 45 + i * 40
        c.rect(x0 + w - 14, py - 9, x0 + w + 4, py + 9, fill="#111")
        c.rect(x0 + w + 4, py - 4, x0 + w + 30, py + 4, fill=GOLD, outline="#7A5C12", width=1)
        c.text((x0 + w - 22, py), n, 14, "white", True, "rm")
        pins[n] = (x0 + w + 30, py)
    c.text((x0, y0 + h + 12), "PIR HC-SR501, back view", 15, INK, True)
    c.text((x0, y0 + h + 32), "(white dome on the other side)", 14, MUTED)
    return pins


def jumper_end(c, x, y, color):
    """Female jumper housing sitting on a pin."""
    c.rect(x - 10, y - 13, x + 10, y + 13, fill=color, outline="#111", width=2, r=3)


def hcsr04(c, x0, y0, w=360, front_labels=True):
    """HC-SR04, front view, pins along the bottom. Returns {name: (x, y_pin_tip)}."""
    h = 160
    c.rect(x0, y0, x0 + w, y0 + h, fill=PCB_SONAR, outline="#1B3F6B", width=3, r=8)
    tr = min(62, w * 0.165)
    for cx, lab in ((x0 + w * 0.25, "T"), (x0 + w * 0.75, "R")):
        c.circle(cx, y0 + 74, tr, fill=SILVER, outline="#7D8590", width=3)
        c.circle(cx, y0 + 74, tr - 14, fill="#3B4048")
        for gx in range(-36, 40, 9):  # mesh
            for gy in range(-36, 40, 9):
                if gx * gx + gy * gy < (tr - 18) ** 2:
                    c.circle(cx + gx, y0 + 74 + gy, 1.6, fill="#59606B")
        if front_labels:
            c.text((cx, y0 + 74 + 70), lab, 13, "white", True, "mm")
    c.rect(x0 + w / 2 - 22, y0 + 40, x0 + w / 2 + 22, y0 + 62, fill="#D0D5DC", outline="#777", width=2, r=8)
    c.text((x0 + w / 2, y0 + 22), "HC-SR04", 15, "white", True, "mm")
    names = ["VCC", "TRIG", "ECHO", "GND"]
    pins = {}
    for i, n in enumerate(names):
        px = x0 + w / 2 + (i - 1.5) * 44
        c.rect(px - 4, y0 + h, px + 4, y0 + h + 30, fill=GOLD, outline="#7A5C12", width=1)
        c.rect(px - 9, y0 + h - 8, px + 9, y0 + h + 4, fill="#111")
        c.text((px, y0 + h - 22), n, 13, "white", True, "mm")
        pins[n] = (px, y0 + h + 30)
    return pins


# ---------------------------------------------------------------------------------------------
# Wiring diagram
# ---------------------------------------------------------------------------------------------
def make_wiring():
    c = Canvas(1400, 1150)
    c.text((40, 24), "Wiring: ultrasonic + PIR sensors to ESP32", 34, INK, True)
    c.text((40, 72), "Unplug the USB cable while you wire. Match the pin NAMES and the wire colours.", 19, MUTED)

    used = {("L", "VIN"), ("R", "D2"), ("R", "D4"), ("R", "D15"), ("R", "GND")}
    esp, esp_bottom = esp32_devkit(c, 470, 330, used=used)
    sonar = hcsr04(c, 900, 120, front_labels=False)
    pir = hcsr501_back(c, 40, 125)
    d4 = esp[("R", "D4")]

    vin, gnd, d2, d15 = esp[("L", "VIN")], esp[("R", "GND")], esp[("R", "D2")], esp[("R", "D15")]
    sv, st, se, sg = sonar["VCC"], sonar["TRIG"], sonar["ECHO"], sonar["GND"]

    # divider: ECHO -> 1k (vertical) -> junction J1 ; J1 -> 2k (horizontal) -> GND wire at J2
    j1 = (se[0], 432)
    gnd_lane = 1240
    j2 = (gnd_lane, j1[1])

    out_lane = 745
    wire(c, [sv, (sv[0], 300), (420, 300), (420, vin[1]), vin], RED, hops=((out_lane, 300),))  # VCC -> VIN
    wire(c, [st, (st[0], d15[1]), d15], ORANGE)                                               # TRIG -> D15
    wire(c, [sg, (sg[0], 318), (gnd_lane, 318), (gnd_lane, gnd[1]), gnd], BLACK)              # GND -> GND
    wire(c, [se, j1], GREEN)                                                                   # ECHO -> 1k
    wire(c, [j1, (j1[0], d2[1]), d2], GREEN, hops=((st[0], d2[1]),))                           # 1k -> D2
    wire(c, [j1, j2], "#555555", width=6)                                                      # J1 -> 2k -> GND
    resistor(c, se[0], 380, True, ["#7B4A1E", "#111", RED, GOLD], "1 kΩ")
    resistor(c, (j1[0] + gnd_lane) / 2 + 6, j1[1], False, [RED, "#111", RED, GOLD], "2 kΩ")
    # PIR: VCC joins the VIN wire, OUT over the top to D4, GND around the sensor to the GND wire
    pv, po, pg = pir["VCC"], pir["OUT"], pir["GND"]
    j3, j4 = (420, 330), (gnd_lane, 560)
    wire(c, [pv, (j3[0], pv[1]), j3], RED)
    wire(c, [po, (out_lane, po[1]), (out_lane, d4[1]), d4], PURPLE)
    wire(c, [pg, (345, pg[1]), (345, 108), (1300, 108), (1300, j4[1]), j4], BLACK)
    for j in (j1, j2, j3, j4):
        c.circle(*j, 9, fill="#111", outline="white", width=2)
    for p, col in ((vin, RED), (d15, ORANGE), (d2, GREEN), (gnd, BLACK), (d4, PURPLE)):
        jumper_end(c, *p, col)
    c.pill(out_lane + 44, d4[1] - 24, "D4", PURPLE)

    c.pill(st[0] - 52, d15[1] - 22, "D15", ORANGE)
    c.pill(st[0] - 140, d2[1] - 22, "D2", GREEN)
    c.pill(gnd_lane - 70, gnd[1] - 22, "GND", BLACK)
    c.pill(370, vin[1], "VIN", RED)
    for i, line in enumerate(["dots = joined", "(same breadboard", "row, or twisted)"]):
        c.text(((j1[0] + gnd_lane) / 2, j1[1] + 32 + i * 19), line, 14, MUTED, False, "ma")

    # USB cable
    ux = 470 + 130
    c.line([(ux, esp_bottom + 26), (ux, esp_bottom + 70), (ux + 200, esp_bottom + 70)], fill="#555", width=10)
    c.text((ux + 214, esp_bottom + 70), "USB: computer or phone charger", 18, MUTED, False, "lm")

    # connection table
    x, y = 40, 400
    c.rect(x - 14, y - 20, x + 356, y + 462, fill="white", outline="#E2DCCD", width=2, r=14)
    c.text((x, y - 4), "Connections", 22, INK, True)
    sections = [
        ("Ultrasonic HC-SR04", 34, [
            (RED, "VCC", "VIN", "5 V power"),
            (ORANGE, "TRIG", "D15", "start a ping"),
            (GREEN, "ECHO", "D2", "through 1 kΩ"),
            (BLACK, "GND", "GND", "ground"),
        ]),
        ("PIR HC-SR501", 206, [
            (RED, "VCC", "VIN", "shared, 5 V"),
            (PURPLE, "OUT", "D4", "HIGH = motion"),
            (BLACK, "GND", "GND", "shared"),
        ]),
    ]
    for title, top, rows in sections:
        c.text((x, y + top), title, 15, MUTED, True)
        for i, (col, a, b, note) in enumerate(rows):
            yy = y + top + 38 + i * 36
            c.rect(x, yy - 6, x + 30, yy + 6, fill=col, r=6)
            c.text((x + 44, yy), a, 19, INK, True, "lm")
            c.text((x + 120, yy), "→", 19, MUTED, False, "lm")
            c.text((x + 150, yy), b, 19, INK, True, "lm")
            c.text((x + 210, yy), note, 15, MUTED, False, "lm")
    yy = y + 346
    c.text((x, yy), "Voltage divider:", 17, INK, True)
    c.text((x, yy + 26), "1 kΩ  ECHO → D2", 16, INK)
    c.text((x, yy + 50), "2 kΩ  D2 side → GND", 16, INK)
    c.text((x, yy + 78), "Bands: 1 kΩ brown-black-red,", 14, MUTED)
    c.text((x, yy + 98), "2 kΩ red-black-red.", 14, MUTED)

    c.text((40, 1110), "Other ESP32 boards put pins in different places: find D15, D2, D4, GND and VIN by the names printed on yours.",
           16, MUTED)
    c.save(os.path.join(DOCS, "wiring.png"))


# ---------------------------------------------------------------------------------------------
# Overview: car -> sensor + ESP32 -> ESP-NOW -> CYD on the computer
# ---------------------------------------------------------------------------------------------
def car(c, x, y):
    c.rect(x, y + 40, x + 170, y + 100, fill="#E4572E", outline="#8C2A12", width=3, r=22)
    c.d.polygon([( (x + 36) * S, (y + 42) * S), ((x + 62) * S, y * S), ((x + 128) * S, y * S),
                 ((x + 152) * S, (y + 42) * S)], fill="#E4572E", outline="#8C2A12", width=3 * S)
    c.d.polygon([((x + 52) * S, (y + 40) * S), ((x + 70) * S, (y + 12) * S), ((x + 92) * S, (y + 12) * S),
                 ((x + 92) * S, (y + 40) * S)], fill="#BFE3F5")
    c.d.polygon([((x + 100) * S, (y + 40) * S), ((x + 100) * S, (y + 12) * S), ((x + 122) * S, (y + 12) * S),
                 ((x + 140) * S, (y + 40) * S)], fill="#BFE3F5")
    for wx in (x + 40, x + 132):
        c.circle(wx, y + 100, 22, fill="#222")
        c.circle(wx, y + 100, 9, fill="#BBB")
    c.circle(x + 166, y + 62, 7, fill="#FFE27A")


def waves(c, cx, cy, n, r0, step, a0, a1, color, width=4):
    for i in range(n):
        c.arc(cx, cy, r0 + i * step, a0, a1, fill=color, width=width)


def badge(c, x, y, num, title, lines):
    c.circle(x + 20, y + 20, 20, fill=INK)
    c.text((x + 20, y + 21), str(num), 22, "white", True, "mm")
    c.text((x + 52, y + 20), title, 21, INK, True, "lm")
    for i, s in enumerate(lines):
        c.text((x + 52, y + 50 + i * 24), s, 16, MUTED)


def laptop(c, x, y):
    c.rect(x + 20, y, x + 240, y + 140, fill="#2D3748", outline="#111", width=3, r=10)
    c.rect(x + 32, y + 12, x + 228, y + 126, fill="#1E3A5F")
    c.text((x + 130, y + 56), "pio run", 15, "#9AE6B4", True, "mm")
    c.text((x + 130, y + 80), "-t upload", 15, "#9AE6B4", True, "mm")
    c.d.polygon([(x * S, (y + 160) * S), ((x + 260) * S, (y + 160) * S), ((x + 240) * S, (y + 140) * S),
                 ((x + 20) * S, (y + 140) * S)], fill="#A0AEC0", outline="#4A5568", width=2 * S)


def make_overview():
    c = Canvas(1400, 900)
    c.text((40, 30), "Parking sensor: how it works", 34, INK, True)
    c.text((40, 80), "Two ESP32 boards talk by radio. No Wi-Fi network or password needed.", 19, MUTED)

    # 1. car + sensor
    car(c, 40, 280)
    sonar = hcsr04(c, 280, 220, w=300, front_labels=False)
    waves(c, 280, 300, 3, 24, 22, 150, 210, "#3B82F6", 5)

    # small ESP32 lying flat under the sensor, wires down to it
    ex0, ey0, ex1, ey1 = 300, 500, 560, 590
    c.rect(ex0, ey0, ex1, ey1, fill=PCB_ESP, outline="#0E1B2E", width=3, r=10)
    c.rect(ex1 - 90, ey0 + 12, ex1 - 14, ey1 - 12, fill=SILVER, outline="#8B939E", width=2, r=4)
    c.text((ex1 - 52, ey0 + 45), "ESP32", 14, "#4B5563", True, "mm")
    for i in range(15):
        c.circle(ex0 + 20 + i * 15, ey0 + 8, 3, fill=GOLD)
        c.circle(ex0 + 20 + i * 15, ey1 - 8, 3, fill=GOLD)
    for (n, col), tx in zip((("VCC", RED), ("TRIG", ORANGE), ("ECHO", GREEN), ("GND", BLACK)), (320, 350, 380, 410)):
        px, py = sonar[n]
        wire(c, [(px, py), (px, py + 30), (tx, py + 30), (tx, ey0 + 8)], col, width=5)
    c.text(((ex0 + ex1) / 2, ey1 + 16), "4 wires: see the wiring picture", 15, MUTED, True, "ma")
    c.line([(ex0, ey0 + 45), (ex0 - 60, ey0 + 45), (ex0 - 60, ey1 + 50)], fill="#555", width=8)
    c.rect(ex0 - 90, ey1 + 50, ex0 - 30, ey1 + 104, fill="#E5E7EB", outline="#6B7280", width=2, r=8)
    c.text((ex0 - 60, ey1 + 118), "USB power (charger or computer)", 15, MUTED, True, "ma")

    # 2. ESP-NOW radio link
    ax, ay = ex1 + 20, ey0 + 20
    waves(c, ax, ay, 3, 18, 18, 300, 360, "#8B5CF6", 5)
    pts = [(ax + 70 + t * 270, ay - 40 - math.sin(t * math.pi) * 150) for t in [k / 40 for k in range(41)]]
    for i in range(0, 40, 3):  # dashed curve
        c.line(pts[i:i + 2], fill="#8B5CF6", width=5)
    tip, prev = pts[-1], pts[-3]
    ang = math.atan2(tip[1] - prev[1], tip[0] - prev[0])
    c.d.polygon([(tip[0] * S, tip[1] * S),
                 ((tip[0] - 22 * math.cos(ang - 0.45)) * S, (tip[1] - 22 * math.sin(ang - 0.45)) * S),
                 ((tip[0] - 22 * math.cos(ang + 0.45)) * S, (tip[1] - 22 * math.sin(ang + 0.45)) * S)],
                fill="#8B5CF6")
    c.pill(ax + 205, ay - 210, "ESP-NOW radio", "#8B5CF6", 19)
    c.rect(ax + 158, ay - 120, ax + 252, ay - 86, fill="white", outline="#8B5CF6", width=2, r=8)
    c.text((ax + 205, ay - 103), "34 cm", 17, "#5B21B6", True, "mm")

    # 3. CYD showing the real screenshot, plugged into a laptop
    bx, by = 900, 140
    c.rect(bx, by, bx + 440, by + 300, fill="#111", outline="#000", width=3, r=16)
    shot = Image.open(os.path.join(DOCS, "screenshot.png")).convert("RGB")
    c.paste(shot, bx + 20, by + 20, 400, 267 - 7)
    c.circle(bx + 430, by + 150, 4, fill="#F28C28")
    c.text((bx + 220, by + 318), "CYD 3.5\" screen", 17, INK, True, "ma")
    c.line([(bx + 220, by + 346), (bx + 220, by + 370), (bx + 150, by + 370), (bx + 150, by + 400)],
           fill="#555", width=8)
    laptop(c, bx + 20, by + 400)
    c.text((bx + 150, by + 572), "computer: power + upload code", 15, MUTED, True, "ma")

    c.line([(40, 750), (1360, 750)], fill="#E2DCCD", width=2)
    badge(c, 40, 778, 1, "Sensor pings", ["sends a sound you can't hear", "and times the echo"])
    badge(c, 500, 778, 2, "ESP32 sends", ["turns the time into cm", "and sends it by ESP-NOW radio"])
    badge(c, 960, 778, 3, "CYD shows", ["big number, colours, LED,", "beeps faster when close"])
    c.save(os.path.join(DOCS, "overview-drawn.png"))


if __name__ == "__main__":
    make_wiring()
    if "overview" in sys.argv[1:]:
        make_overview()
