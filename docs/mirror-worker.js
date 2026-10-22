// Screen mirror worker: turns captured screen frames into the board's frames.
// Runs in a worker so it keeps going at full speed while the page is in the background
// (which is exactly when someone is showing another window).
//
// Every frame: fit into 480x320 -> compare with the last frame sent -> JPEG of only the
// rectangle that changed. Small changes go at full 480x320; when over 35% of the screen moves
// (video, scrolling) the change goes at 240x160 shown 2x, which the board draws twice as fast,
// and is re-sent sharp once things calm down. Same rules as tools/mirror.py.

const W = 480, H = 320;
const ALIGN = 16;          // changed rectangles snap to JPEG block edges
const SHARP_MAX = 0.35;    // auto: changes up to this share of the panel go at full resolution
const SETTLE_MS = 250;     // auto: blurry parts are re-sent sharp after this long without big motion
const PING_MS = 2000;      // keep-alive while nothing changes (the board gives up after 10 s)
const WINDOW = 2;          // frames that may be on their way before the board acks
let stallMs = 1500;        // no ack for this long: tell the page, which says hello to the board again
                           // (the page passes a longer one for Wi-Fi, where a frame can take a second)

const canvas = new OffscreenCanvas(W, H);
const ctx = canvas.getContext("2d", { willReadFrequently: true });
ctx.imageSmoothingQuality = "high";

let running = false, busy = false, latest = null;
let prev = null, blurry = null, lastBig = 0, lastSend = 0, unacked = 0;
let mode = "auto", quality = 0.75, maxBytes = 48 * 1024, maxScale = 2;
let lastAck = 0, stallReported = false;
let statFrames = 0, statBytes = 0, statSharp = 0, statT = performance.now();

onmessage = (e) => {
  const m = e.data;
  if (m.type === "start") {
    ({ mode, maxBytes, maxScale } = m);
    if (m.stallMs) stallMs = m.stallMs;
    prev = blurry = null;
    unacked = 0;
    lastBig = lastSend = 0;
    running = true;
    readFrames(m.readable);
  } else if (m.type === "ack") {
    unacked = Math.max(0, unacked - m.n);
    lastAck = performance.now();
    stallReported = false;
    pump();
  } else if (m.type === "reset") {   // the board said hello again: start over with a full frame
    prev = blurry = null;
    unacked = 0;
    stallReported = false;
    pump();
  } else if (m.type === "mode") {
    mode = m.mode;
    prev = null;   // re-send everything in the new mode
  } else if (m.type === "stop") {
    running = false;
    if (latest) { latest.close(); latest = null; }
  }
};

async function readFrames(readable) {
  const reader = readable.getReader();
  while (running) {
    const { value, done } = await reader.read();
    if (done) break;
    if (latest) latest.close();   // only the newest frame matters
    latest = value;
    pump();
  }
  reader.cancel().catch(() => {});
  postMessage({ type: "ended" });
}

setInterval(() => {   // settle timer, keep-alive and stall check, even when no new frames come
  if (running && unacked && !stallReported && performance.now() - lastAck > stallMs) {
    stallReported = true;
    postMessage({ type: "stalled" });
  }
  pump();
}, 50);

function snap(b) {
  if (!b) return null;
  return [Math.floor(b[0] / ALIGN) * ALIGN, Math.floor(b[1] / ALIGN) * ALIGN,
          Math.min(Math.ceil(b[2] / ALIGN) * ALIGN, W), Math.min(Math.ceil(b[3] / ALIGN) * ALIGN, H)];
}
const union = (a, b) => (!a || !b) ? (a || b) :
  [Math.min(a[0], b[0]), Math.min(a[1], b[1]), Math.max(a[2], b[2]), Math.max(a[3], b[3])];
const area = (b) => b ? (b[2] - b[0]) * (b[3] - b[1]) : 0;
const same = (a, b) => a && b && a.every((v, i) => v === b[i]);

function changedBox(a, b) {
  if (!a) return [0, 0, W, H];
  let x0 = W, y0 = H, x1 = -1, y1 = -1;
  for (let y = 0; y < H; y++) {
    const row = y * W;
    let first = -1, last = -1;
    for (let x = 0; x < W; x++) if (a[row + x] !== b[row + x]) { first = x; break; }
    if (first < 0) continue;
    for (let x = W - 1; x >= first; x--) if (a[row + x] !== b[row + x]) { last = x; break; }
    if (y < y0) y0 = y;
    y1 = y;
    if (first < x0) x0 = first;
    if (last > x1) x1 = last;
  }
  return y1 < 0 ? null : snap([x0, y0, x1 + 1, y1 + 1]);
}

// fit the frame into the canvas, keeping its shape (black bars fill the rest)
function draw(frame) {
  const fw = frame.displayWidth, fh = frame.displayHeight;
  const k = Math.min(W / fw, H / fh);
  const dw = Math.max(1, Math.round(fw * k)), dh = Math.max(1, Math.round(fh * k));
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, W, H);
  ctx.drawImage(frame, (W - dw) >> 1, (H - dh) >> 1, dw, dh);
}

async function encode(box, scale) {
  const [x0, y0, x1, y1] = box;
  const w = (x1 - x0) / scale, h = (y1 - y0) / scale;
  const c = new OffscreenCanvas(w, h);
  const cx = c.getContext("2d");
  cx.imageSmoothingQuality = "high";
  cx.drawImage(canvas, x0, y0, x1 - x0, y1 - y0, 0, 0, w, h);
  for (let q = quality; ; q -= 0.15) {   // lower the quality until it fits the board's buffer
    const blob = await c.convertToBlob({ type: "image/jpeg", quality: q });
    if (blob.size <= maxBytes || q <= 0.2) return new Uint8Array(await blob.arrayBuffer());
  }
}

function send(tag, scale, x, y, jpg) {
  const msg = new Uint8Array(12 + jpg.length);
  const v = new DataView(msg.buffer);
  v.setUint8(0, tag.charCodeAt(0));
  v.setUint8(1, scale);
  v.setUint16(2, x, true);
  v.setUint16(4, y, true);
  v.setUint32(8, jpg.length, true);
  msg.set(jpg, 12);
  if (!unacked) lastAck = performance.now();   // the ack clock starts with the first frame in flight
  unacked++;
  lastSend = performance.now();
  postMessage({ type: "send", data: msg }, [msg.buffer]);
}

async function pump() {
  if (!running || busy || unacked >= WINDOW) return;
  busy = true;
  try {
    let cur = null, box = null;
    if (latest) {
      draw(latest);
      latest.close();
      latest = null;
      cur = new Uint32Array(ctx.getImageData(0, 0, W, H).data.buffer);
      box = changedBox(prev, cur);
    }
    const now = performance.now();
    if (!box && !blurry) {
      if (now - lastSend > PING_MS) send("J", 1, 0, 0, new Uint8Array(0));
      return;
    }
    const settled = now - lastBig >= SETTLE_MS;
    const sharp = union(box, blurry);
    let scale = 1, out;
    if (mode === "fast" && maxScale >= 2) { scale = 2; out = box; }
    else if (mode === "sharp" || maxScale < 2) out = box;
    else if (area(box) > SHARP_MAX * W * H) { scale = 2; out = box; lastBig = now; }   // lots moving: half res
    else if (settled) out = sharp;   // calm again: full res, including what was sent blurry
    else out = box;                  // small change in a busy moment: sharp, the blurry part waits
    if (!out) return;

    const jpg = await encode(out, scale);
    send("J", scale, out[0], out[1], jpg);
    if (scale === 2) blurry = mode === "auto" ? union(blurry, out) : null;
    else if (same(out, sharp)) blurry = null;
    if (cur) prev = cur;

    statFrames++;
    statBytes += jpg.length;
    if (scale === 1) statSharp++;
    if (now - statT >= 1000) {
      postMessage({ type: "stats", fps: statFrames * 1000 / (now - statT),
                    kb: statBytes / 1024 / statFrames, sharp: statSharp / statFrames });
      statFrames = statBytes = statSharp = 0;
      statT = now;
    }
  } finally {
    busy = false;
  }
  pump();
}
