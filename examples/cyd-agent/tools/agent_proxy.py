#!/usr/bin/env python3
"""
agent_proxy.py - the pet's link to your computer. Serves the system monitor numbers
(GET /stats), and with --brain also runs the LLM agent loop, so the API key never touches
the board: the board POSTs an event and gets back one tool call per line.

    pip install psutil                    # plus anthropic for --brain
    export CYD_AGENT_TOKEN=<AGENT_TOKEN from include/config.h>
    python3 tools/agent_proxy.py          # monitor only: no API key, no cost
    python3 tools/agent_proxy.py --brain  # Claude picks the reactions (needs ANTHROPIC_API_KEY)
    python3 tools/agent_proxy.py --mock   # scripted reactions, for testing without a key

Env: AGENT_MODEL (default claude-opus-5-5), AGENT_DAILY_LIMIT (default 200 events a day).
"""
import argparse
import hmac
import os
import random
import re
import threading
import time
import uuid
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import system_stats

MODEL = os.environ.get("AGENT_MODEL", "claude-opus-5-5")
DAILY_LIMIT = int(os.environ.get("AGENT_DAILY_LIMIT", "200"))
TOKEN = os.environ.get("CYD_AGENT_TOKEN", "")
MIN_GAP_S = 3          # the board limits itself too; this protects the bill if it misbehaves
MAX_BODY = 512
MAX_SAY = 80
MAX_STEPS = 3          # LLM turns per event
SESSION_TTL_S = 60

FACES = ["happy", "love", "excited", "sleepy", "surprised", "sad", "pout", "shy", "dizzy", "wink", "thinking", "neutral"]
COLORS = ["red", "green", "blue", "yellow", "pink", "cyan", "white", "off"]
SOUNDS = ["chirp", "happy", "love", "sad", "surprise", "sleepy"]
FRIENDS = ["Mochi the rice-cake blob", "Neko the cat", "Usagi the bunny", "Kuma the bear"]

SYSTEM = """You are the mind of a tiny, cute pet living on a 3.5 inch screen on someone's desk. \
You have a body made of tools: a face, a speech bubble, an RGB LED and a small speaker.

Something just happened to you. React in character, in one short moment:
- Always call set_face. Usually also call say. Add set_led or play_sound when they fit the mood.
- Call read_sensors first only when the answer would change your reaction, such as how bright the room is.
- Call all your other tools together in one turn.
- say: at most 12 words, plain ASCII, no emoji, playful and warm. Vary your lines; don't repeat recent ones.
- You can't do anything else in the world, so don't offer to."""

TOOLS = [
    {"name": "set_face", "description": "Change your facial expression.", "strict": True,
     "input_schema": {"type": "object", "additionalProperties": False, "required": ["face"],
                      "properties": {"face": {"type": "string", "enum": FACES}}}},
    {"name": "say", "description": "Show a short line in your speech bubble.", "strict": True,
     "input_schema": {"type": "object", "additionalProperties": False, "required": ["text"],
                      "properties": {"text": {"type": "string"}}}},
    {"name": "set_led", "description": "Flash your RGB LED for a moment.", "strict": True,
     "input_schema": {"type": "object", "additionalProperties": False, "required": ["color"],
                      "properties": {"color": {"type": "string", "enum": COLORS}}}},
    {"name": "play_sound", "description": "Play a little sound. Only audible while someone is playing with you.",
     "strict": True,
     "input_schema": {"type": "object", "additionalProperties": False, "required": ["sound"],
                      "properties": {"sound": {"type": "string", "enum": SOUNDS}}}},
    {"name": "read_sensors", "description": "Read the room brightness (0 dark to 100 bright), "
                                            "how many times you've been petted, your uptime, "
                                            "and how busy your human's computer is.", "strict": True,
     "input_schema": {"type": "object", "additionalProperties": False, "properties": {}}},
]

EVENTS = {
    "poked": "Your human just tapped you.",
    "lights_off": "The room just went dark.",
    "lights_on": "The room just got bright again.",
}

sessions = {}          # id -> {"messages": [...], "steps": n, "at": t, "tool_ids": [...]}
recent = deque(maxlen=6)
lock = threading.Lock()
last_event_at = 0.0
day, day_count = "", 0
client = None


def clean_say(text):
    text = re.sub(r"[^\x20-\x7e]", " ", text)
    return re.sub(r" +", " ", text).strip()[:MAX_SAY]


def to_lines(block):
    """One tool_use block -> the line the board runs. Inputs are checked again: this goes to hardware."""
    inp = block.input if isinstance(block.input, dict) else {}
    if block.name == "set_face" and inp.get("face") in FACES:
        return "face " + inp["face"]
    if block.name == "say" and isinstance(inp.get("text"), str):
        said = clean_say(inp["text"])
        return "say " + said if said else None
    if block.name == "set_led" and inp.get("color") in COLORS:
        return "led " + inp["color"]
    if block.name == "play_sound" and inp.get("sound") in SOUNDS:
        return "sound " + inp["sound"]
    return None


def sensor_text(f):
    return (f"brightness={f.get('light', '?')} pets={f.get('pets', '?')} uptime_s={f.get('uptime', '?')} "
            + system_stats.summary())


def run_model(session_id, session):
    """One LLM turn. Returns the lines for the board; 'more' means it's waiting for sensors."""
    response = client.beta.messages.create(
        model=MODEL,
        max_tokens=2048,
        betas=["server-side-fallback-2026-07-01"],
        fallbacks="default",
        output_config={"effort": "low"},
        system=SYSTEM,
        tools=TOOLS,
        messages=session["messages"],
    )
    session["steps"] += 1
    if response.stop_reason == "refusal":
        print("[proxy] refusal", response.stop_details)
        return ["face neutral"]
    if response.stop_reason == "max_tokens":
        print("[proxy] hit max_tokens")

    # append unchanged, thinking blocks included: the API requires an append-only history
    session["messages"].append({"role": "assistant", "content": response.content})
    calls = [b for b in response.content if b.type == "tool_use"]
    lines = [ln for ln in (to_lines(b) for b in calls) if ln]
    wants_sensors = any(b.name == "read_sensors" for b in calls)

    for ln in lines:
        if ln.startswith("say "):
            recent.append(ln[4:])
    u = response.usage
    print(f"[proxy] step {session['steps']}: in={u.input_tokens} out={u.output_tokens} -> {lines}"
          f"{' + read_sensors' if wants_sensors else ''}")

    if wants_sensors and session["steps"] < MAX_STEPS:
        session["tool_ids"] = [(b.id, b.name) for b in calls]
        sessions[session_id] = session
        return lines + ["more " + session_id]
    return lines


def handle_event(fields):
    kind = fields.get("kind", "")
    sid = fields.get("session", "")
    if kind == "sensors":
        session = sessions.pop(sid, None)
        if not session:
            return 410, "gone\n"
        results = [{"type": "tool_result", "tool_use_id": tid,
                    "content": sensor_text(fields) if name == "read_sensors" else "done"}
                   for tid, name in session["tool_ids"]]
        session["messages"].append({"role": "user", "content": results})
    elif kind in EVENTS:
        who = FRIENDS[int(fields["friend"])] if fields.get("friend", "").isdigit() and int(fields["friend"]) < 4 else FRIENDS[0]
        said = "; ".join(recent) or "nothing yet"
        prompt = (f"You look like {who}. Local time {time.strftime('%A %H:%M')}. "
                  f"Things you said recently: {said}.\nEvent: {EVENTS[kind]}")
        session = {"messages": [{"role": "user", "content": prompt}], "steps": 0, "at": time.time(), "tool_ids": []}
        sid = uuid.uuid4().hex[:16]
    else:
        return 400, "unknown event\n"
    return 200, "\n".join(run_model(sid, session)) + "\n"


def handle_mock(fields):
    """No API key needed: same protocol and same two-step read_sensors flow as the real loop."""
    kind = fields.get("kind", "")
    if kind == "poked" and random.random() < 0.5:
        sid = uuid.uuid4().hex[:16]
        sessions[sid] = {"at": time.time()}
        return 200, f"face thinking\nmore {sid}\n"
    if kind == "sensors":
        if not sessions.pop(fields.get("session", ""), None):
            return 410, "gone\n"
        return 200, f"face love\nsay Mock brain: light is {fields.get('light', '?')}, pets {fields.get('pets', '?')}\nled pink\nsound love\n"
    if kind == "lights_off":
        return 200, "face sleepy\nsay Mock brain: goodnight!\nsound sleepy\n"
    if kind == "lights_on":
        return 200, "face surprised\nsay Mock brain: too bright!\nled yellow\nsound surprise\n"
    if kind == "poked":
        face = random.choice(["happy", "excited", "wink", "shy"])
        return 200, f"face {face}\nsay Mock brain says hello!\nled cyan\nsound chirp\n"
    return 400, "unknown event\n"


def over_limits(kind):
    """Returns an error message, or None. Only new events count: a sensors step belongs to one."""
    global last_event_at, day, day_count
    if kind == "sensors":
        return None
    now = time.time()
    today = time.strftime("%Y-%m-%d")
    if today != day:
        day, day_count = today, 0
    if now - last_event_at < MIN_GAP_S:
        return "too fast"
    if day_count >= DAILY_LIMIT:
        return "daily limit reached"
    last_event_at = now
    day_count += 1
    return None


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    mock = False
    brain = False

    def reply(self, code, text):
        data = text.encode("ascii", "replace")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(data)
        self.close_connection = True

    def log_message(self, fmt, *args):
        print(f"[proxy] {self.address_string()} {fmt % args}")

    def authorised(self):
        if hmac.compare_digest(self.headers.get("X-Agent-Token", ""), TOKEN):
            return True
        self.reply(401, "bad token\n")
        return False

    def do_GET(self):
        if self.path != "/stats":
            return self.reply(404, "not found\n")
        if self.authorised():
            self.reply(200, system_stats.stats_text(self.brain))

    def do_POST(self):
        if self.path != "/event":
            return self.reply(404, "not found\n")
        if not self.authorised():
            return
        if not self.brain:
            return self.reply(200, "\n")   # monitor-only: no calls to run
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            return self.reply(400, "bad length\n")
        if length <= 0 or length > MAX_BODY:
            return self.reply(413, "too big\n")
        body = self.rfile.read(length).decode("ascii", "replace")
        fields = dict(ln.split("=", 1) for ln in body.splitlines() if "=" in ln)
        print(f"[proxy] event {fields}")

        with lock:
            for sid in [s for s, v in sessions.items() if time.time() - v["at"] > SESSION_TTL_S]:
                del sessions[sid]
            limit = over_limits(fields.get("kind", ""))
            if limit:
                print(f"[proxy] refused: {limit}")
                return self.reply(429, limit + "\n")
            try:
                code, text = handle_mock(fields) if self.mock else handle_event(fields)
            except Exception as e:  # the board falls back to its offline lines on any non-200
                import anthropic
                if isinstance(e, anthropic.RateLimitError):
                    print("[proxy] rate limited")
                elif isinstance(e, anthropic.APIStatusError):
                    print(f"[proxy] API error {e.status_code}: {e.message}")
                elif isinstance(e, anthropic.APIConnectionError):
                    print(f"[proxy] can't reach the API: {e}")
                else:
                    raise
                code, text = 503, "brain offline\n"
        print(f"[proxy] -> {code} {text.strip()!r}")
        self.reply(code, text)


def main():
    global client
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8787)
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--brain", action="store_true", help="Claude picks the reactions (needs an API key, costs money)")
    ap.add_argument("--mock", action="store_true", help="scripted reactions, no API key or cost")
    args = ap.parse_args()
    if not TOKEN:
        raise SystemExit("Set CYD_AGENT_TOKEN to the AGENT_TOKEN in include/config.h")
    Handler.mock = args.mock
    Handler.brain = args.brain or args.mock
    if args.brain and not args.mock:
        import anthropic
        client = anthropic.Anthropic(timeout=20.0, max_retries=1)
    mode = "mock brain" if args.mock else f"{MODEL}, {DAILY_LIMIT} events a day" if args.brain else "monitor only, no LLM"
    print(f"[proxy] {mode}, on {args.host}:{args.port}")
    ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
