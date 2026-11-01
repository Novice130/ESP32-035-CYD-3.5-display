"""
system_stats.py - what the pet shows on its monitor pages: total CPU, RAM and disk, plus the
top apps. A process inside an .app counts towards that app, so all of Antigravity's helper
processes add up to one row. Sampling only happens while the board is asking.

Chip temperature: Apple Silicon has no public API, so this reads the same IOKit HID die sensors
tools like Stats use (private, no sudo). If they vanish in a macOS update the board shows no
temperature rather than a wrong one. Memory sits inside the chip package with no sensor of its own.
"""
import ctypes
import ctypes.util
import glob
import os
import re
import subprocess
import threading
import time

import psutil

ROWS = 5
FRESH_S = 0.8           # the board polls every second
BASELINE_GAP_S = 10     # older than this, CPU deltas are meaningless: measure a new one
BASELINE_S = 0.5
DISK_RESCAN_S = 3600
APP_IN_PATH = re.compile(r"/([^/]+)\.app/")
# on macOS "/" is the small read-only system volume; your files live on the data volume
DISK_PATH = "/System/Volumes/Data" if os.path.exists("/System/Volumes/Data") else os.path.abspath(os.sep)

lock = threading.Lock()
last_at = 0.0
last_text = ""
last_summary = "computer_stats=unknown"
app_sizes = []          # [(mb, name)], biggest first
disk_scan_started = False
disk_scanning = False


def clean_name(name):
    return re.sub(r"[^\x20-\x7e]", "", name or "?").strip()[:20] or "?"


def dir_mb(path):
    try:
        out = subprocess.run(["nice", "du", "-sk", path], capture_output=True, text=True, timeout=600).stdout
        return int(out.split()[0]) // 1024 if out else 0
    except (subprocess.TimeoutExpired, ValueError, OSError):
        return 0


def scan_disk_forever():
    """Size of each app plus its Application Support folder. Slow, so it has its own thread."""
    global app_sizes, disk_scanning
    home = os.path.expanduser("~")
    while True:
        disk_scanning = True
        sizes = []
        seen = set()
        for app in glob.glob("/Applications/*.app") + glob.glob(home + "/Applications/*.app"):
            real = os.path.realpath(app)   # apps moved to another drive are symlinks: du would say 0
            if real in seen:
                continue
            seen.add(real)
            name = os.path.basename(app)[:-4]
            mb = dir_mb(real)
            data = os.path.join(home, "Library/Application Support", name)
            if os.path.isdir(data):
                mb += dir_mb(data)
            sizes.append((mb, clean_name(name)))
            app_sizes = sorted(sizes, reverse=True)
        disk_scanning = False
        time.sleep(DISK_RESCAN_S)


class AppleDieSensors:
    """Hottest 'PMU tdie' sensor, in Celsius."""
    TEMPERATURE_EVENT = 15
    UTF8 = 0x08000100

    def __init__(self):
        cf = ctypes.CDLL(ctypes.util.find_library("CoreFoundation"))
        io = ctypes.CDLL(ctypes.util.find_library("IOKit"))
        V = ctypes.c_void_p
        cf.CFStringCreateWithCString.restype = V
        cf.CFStringCreateWithCString.argtypes = [V, ctypes.c_char_p, ctypes.c_uint32]
        cf.CFNumberCreate.restype = V
        cf.CFNumberCreate.argtypes = [V, ctypes.c_int, V]
        cf.CFDictionaryCreate.restype = V
        cf.CFDictionaryCreate.argtypes = [V, ctypes.POINTER(V), ctypes.POINTER(V), ctypes.c_long, V, V]
        cf.CFArrayGetCount.restype = ctypes.c_long
        cf.CFArrayGetCount.argtypes = [V]
        cf.CFArrayGetValueAtIndex.restype = V
        cf.CFArrayGetValueAtIndex.argtypes = [V, ctypes.c_long]
        cf.CFStringGetCString.restype = ctypes.c_bool
        cf.CFStringGetCString.argtypes = [V, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32]
        cf.CFRelease.argtypes = [V]
        io.IOHIDEventSystemClientCreate.restype = V
        io.IOHIDEventSystemClientCreate.argtypes = [V]
        io.IOHIDEventSystemClientSetMatching.argtypes = [V, V]
        io.IOHIDEventSystemClientCopyServices.restype = V
        io.IOHIDEventSystemClientCopyServices.argtypes = [V]
        io.IOHIDServiceClientCopyProperty.restype = V
        io.IOHIDServiceClientCopyProperty.argtypes = [V, V]
        io.IOHIDServiceClientCopyEvent.restype = V
        io.IOHIDServiceClientCopyEvent.argtypes = [V, ctypes.c_int64, ctypes.c_int32, ctypes.c_int64]
        io.IOHIDEventGetFloatValue.restype = ctypes.c_double
        io.IOHIDEventGetFloatValue.argtypes = [V, ctypes.c_int32]
        self.cf, self.io = cf, io

        def cfstr(text):
            return cf.CFStringCreateWithCString(None, text.encode(), self.UTF8)

        def cfint(value):
            box = ctypes.c_int32(value)
            return cf.CFNumberCreate(None, 3, ctypes.byref(box))

        # the sensors live under Apple's vendor page 0xff00, usage 5 = temperature
        keys = (V * 2)(cfstr("PrimaryUsagePage"), cfstr("PrimaryUsage"))
        vals = (V * 2)(cfint(0xFF00), cfint(5))
        key_cb = ctypes.addressof(V.in_dll(cf, "kCFTypeDictionaryKeyCallBacks"))
        val_cb = ctypes.addressof(V.in_dll(cf, "kCFTypeDictionaryValueCallBacks"))
        matching = cf.CFDictionaryCreate(None, keys, vals, 2, key_cb, val_cb)
        self.client = io.IOHIDEventSystemClientCreate(None)
        io.IOHIDEventSystemClientSetMatching(self.client, matching)
        self.product = cfstr("Product")
        self.services = io.IOHIDEventSystemClientCopyServices(self.client)
        self.dies = []
        name = ctypes.create_string_buffer(128)
        for i in range(cf.CFArrayGetCount(self.services) if self.services else 0):
            svc = cf.CFArrayGetValueAtIndex(self.services, i)
            prop = io.IOHIDServiceClientCopyProperty(svc, self.product)
            if prop:
                if cf.CFStringGetCString(prop, name, 128, self.UTF8) and b"tdie" in name.value:
                    self.dies.append(svc)
                cf.CFRelease(prop)

    def read(self):
        hottest = None
        for svc in self.dies:
            event = self.io.IOHIDServiceClientCopyEvent(svc, self.TEMPERATURE_EVENT, 0, 0)
            if event:
                c = self.io.IOHIDEventGetFloatValue(event, self.TEMPERATURE_EVENT << 16)
                self.cf.CFRelease(event)
                if 1 < c < 130 and (hottest is None or c > hottest):
                    hottest = c
        return hottest


def make_temp_reader():
    if os.uname().sysname == "Darwin" and os.uname().machine == "arm64":
        try:
            sensors = AppleDieSensors()
            if sensors.dies:
                return sensors.read
        except (OSError, ValueError, AttributeError) as e:
            print(f"[stats] no chip temperature: {e}")
        return lambda: None
    if hasattr(psutil, "sensors_temperatures"):  # Linux and friends
        def read():
            temps = [t.current for group in psutil.sensors_temperatures().values() for t in group]
            return max(temps) if temps else None
        return read
    return lambda: None


read_temp = make_temp_reader()


def ram_used_mb(mem):
    """Same sum as Activity Monitor's Memory Used. psutil's figure counts compressed memory oddly."""
    try:
        out = subprocess.run(["vm_stat"], capture_output=True, text=True, timeout=2).stdout
        page = int(re.search(r"page size of (\d+)", out).group(1))
        pages = {k.strip(): int(v.strip(" .")) for k, v in (ln.split(":") for ln in out.splitlines()[1:] if ":" in ln)}
        used = (pages["Anonymous pages"] - pages["Pages purgeable"] + pages["Pages wired down"]
                + pages["Pages occupied by compressor"])
        return used * page // 1048576
    except (OSError, subprocess.TimeoutExpired, AttributeError, KeyError, ValueError):  # not a Mac
        return (mem.total - mem.available) // 1048576


def group_processes():
    """{app: [cpu %, rss MB]}. cpu_percent() measures since this same call last ran."""
    apps = {}
    for p in psutil.process_iter(["exe", "name", "cpu_percent", "memory_info"], ad_value=None):
        info = p.info
        mem = info["memory_info"]
        if mem is None:   # another user's process (root): macOS doesn't let us read it
            continue
        m = APP_IN_PATH.search(info["exe"] or "")
        name = clean_name(m.group(1) if m else info["name"])
        row = apps.setdefault(name, [0.0, 0.0])
        row[0] += info["cpu_percent"] or 0.0
        row[1] += mem.rss / 1048576
    return apps


def sample(brain):
    global last_at, last_summary
    now = time.time()
    if now - last_at > BASELINE_GAP_S:
        psutil.cpu_percent(None)
        group_processes()
        time.sleep(BASELINE_S)
    cpu = psutil.cpu_percent(None)
    apps = group_processes()
    last_at = time.time()

    mem = psutil.virtual_memory()
    disk = psutil.disk_usage(DISK_PATH)
    cores = psutil.cpu_count() or 1
    temp = read_temp()
    ram_mb = ram_used_mb(mem)
    total_mb = mem.total // 1048576
    ram_pct = ram_mb * 100 / total_mb
    last_summary = f"computer_cpu={cpu:.0f}% computer_ram={ram_pct:.0f}%" + (f" chip_temp={temp:.0f}C" if temp else "")
    lines = [
        f"cpu {cpu:.0f}",
        f"ram {ram_pct:.0f} {ram_mb} {total_mb}",
        f"disk {disk.percent:.0f} {disk.used // 1048576} {disk.total // 1048576}",
        f"brain {1 if brain else 0}",
        f"temp {temp:.0f}" if temp else "temp -1",
        f"scan {1 if disk_scanning or not disk_scan_started else 0}",
    ]
    # tenths of a percent of the whole machine, so app rows add up towards the total above
    for name, (c, m) in sorted(apps.items(), key=lambda kv: -kv[1][0])[:ROWS]:
        lines.append(f"c {c * 10 / cores:.0f} {m:.0f} {name}")
    for name, (c, m) in sorted(apps.items(), key=lambda kv: -kv[1][1])[:ROWS]:
        lines.append(f"m {c * 10 / cores:.0f} {m:.0f} {name}")
    for mb, name in app_sizes[:ROWS]:
        lines.append(f"d {mb} {name}")
    return "\n".join(lines) + "\n"


def stats_text(brain):
    global last_text, disk_scan_started
    with lock:
        if not disk_scan_started:
            disk_scan_started = True
            threading.Thread(target=scan_disk_forever, daemon=True).start()
        if time.time() - last_at >= FRESH_S:
            last_text = sample(brain)
        return last_text


def summary():
    """One line for the LLM's read_sensors tool."""
    return last_summary
