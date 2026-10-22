# LESSONS.md — what went wrong before, and what fixed it

Read this before changing anything. Each entry: **symptom → evidence that found the cause → fix**.
Newest first. Add to it whenever something costs more than a few minutes.

---

## Screen mirror (examples/screen-mirror, website in docs/)

### Wi-Fi picture froze after ~5–10 s, then the board "disconnected"
- **Symptom:** sharing over Wi-Fi from the website showed a few frames, froze on the last picture,
  then the page said it lost the board. Ping to the board lost 50–92 % of packets *even when idle*,
  while the router answered with 0 % loss.
- **Evidence (serial log at 2 Mbaud + a temporary `[DIAG]` line every 2 s):**
  - no crash or restart (`/stats` said `last restart: power on`), heap never below ~52 KB total,
  - but `largest block` fell to ~1.5 KB, `fillBuffer(): Not enough memory to allocate buffer`,
  - `write(): fail on fd NN, errno: 11` once per second for 10–20 s = `WiFiClient::write()` retrying
    a full socket (10 retries × 1 s select) and blocking the whole receive task,
  - frames of 5.5 KB took 1–2 s to arrive (`receive 1697 ms`), RSSI −68…−90 dBm.
- **Cause (a feedback loop):** on a weak link a frame takes > 1.5 s, so the page's stall check
  (`STALL_MS = 1500`) said hello again and re-sent a full frame **while the old POSTs were still
  running**. Every restart added connections (Chrome opens up to 6, the board keeps 5) until lwIP
  ran out of memory; then the board blocked 10 s in `write()` to a socket nobody read.
- **Fix:**
  - page: frame POSTs get an `AbortController`; `cancelAll()` on resync; 6 s timeout per POST;
    Wi-Fi stall time 4 s (USB keeps 1.5 s) — passed to the worker as `stallMs`.
  - firmware: `httpReply()` builds one buffer and sends it with `send(fd, …, MSG_DONTWAIT)`;
    if it can't all go, `c.stop()` the connection instead of waiting.
  - the weak signal is still the root of slowness: move the board near the router (−62 dBm worked).
- **Don't repeat:** never call `WiFiClient::write()` from the receive task for a client that may
  stall; never start a new frame stream without cancelling the old in-flight requests.

### "Can't reach cyd.local" although the board was on Wi-Fi
- **Evidence:** `curl -4 http://cyd.local/hello` resolved in 4 ms, plain `curl` took **5.0 s**:
  macOS asks for IPv4 *and* IPv6; the ESP32 mDNS answers only the A record, the AAAA query times out.
  The page's hello had a 4 s timeout, so it always failed. Python's resolver failed outright
  (`[Errno 8] nodename nor servname provided`).
- **Fix:** hello timeout 12 s in the page. For `tools/mirror.sh` pass the IP
  (`dns-sd -G v4 cyd.local` prints it). Typing the IP in the page always works.
- **Tried and reverted:** switching the page to the board's IP after hello. It opened a second
  connection pool (name + IP) on a board that was already short of memory.

### Saved Wi-Fi vanished after every "Install"
- **Evidence:** boot log `Preferences.cpp: begin(): nvs_open failed: NOT_FOUND` = NVS empty.
- **Cause:** ESP Web Tools erases the whole flash on a new install unless the manifest has
  `"new_install_prompt_erase": true` (source: `install-dialog.js`, "Default is to erase a device
  that does not support Improv Serial").
- **Fix:** manifests set `new_install_prompt_erase: true`; the erase checkbox defaults to unticked.
- **Also:** the page used to show empty Wi-Fi fields on every cable connect (it can't read the
  password back), which looked like the settings were gone. Now it hides them behind "Change Wi-Fi"
  when the board reports Wi-Fi saved. Firmware status state 3 = saved but paused for the cable.

### Connecting with the cable turns the board's Wi-Fi off
- By design: the UART loses bytes at 2 Mbaud while the radio is on, so the firmware pauses Wi-Fi
  on the `'U'` message and restarts it 15 s (`WIFI_RESUME_MS`) after USB goes quiet.
  Anything that talks to the board over USB (the page, a test script) kills Wi-Fi for that time —
  don't debug Wi-Fi while something holds the serial port.

### Opening `http://<board>:8767` in a browser kicks the Mac sender off
- Port 8767 is the raw binary protocol for `tools/mirror.py`, not HTTP (browser shows
  `ERR_INVALID_HTTP_RESPONSE`). The board takes one sender; the newest wins, so a browser hitting
  that port disconnects `mirror.py` (`Broken pipe`). Port 80 only answers `/hello`, `/frame`, `/stats`.

### Serial port problems on the Mac
- `termios.error: (22, 'Invalid argument')` when opening the CH340 port: the driver got stuck.
  Unplug/replug the cable (a port on the Mac, not the hub) — nothing else fixes it.
- `[Errno 16] Resource busy` on upload: a browser tab (Web Serial) still holds the port even after
  "Disconnect". `lsof /dev/cu.usbserial*` shows who; close that tab.
- Open the port at 2000000 baud, `dtr=False`, pulse `rts` to reset. 115200 shows garbage.

### Website hosting
- GitHub Pages was never switched on, so the README link 404'd. The site now lives on
  **Cloudflare Pages**, project `learnnovice-robo`, connected to this GitHub repo (branch `main`,
  output directory `docs`, no build command) → every push to `main` redeploys
  **https://robo.learnnovice.com/** (CNAME `robo` → `learnnovice-robo.pages.dev`, proxied).
- A Direct Upload Pages project can't be switched to Git later; a new Git project was created via
  the API (`source.type = github` works when the Cloudflare GitHub app is already authorised) and
  the custom domain moved over.
- **Checking a deploy:** Pages answers **200 with index.html for missing files**, so
  `curl -w %{http_code}` on a new file proves nothing. Check content instead
  (`curl -s .../manifest.json | python3 -m json.tool`, or grep the page for new text) and the
  deployment list (`GET /accounts/<id>/pages/projects/learnnovice-robo/deployments` → commit hash).
- Pushes did **not** trigger builds at first, even though the project showed `source.type github`:
  the Cloudflare GitHub app hadn't been given access to this repo. Fixed by adding the repo under
  GitHub → Settings → Applications → Cloudflare Workers and Pages → Repository access.
  `POST .../deployments` (API) builds the latest `main` by hand if it ever stops again.
- Manifest names: a `"` inside (like `2.8"`) breaks the JSON and the installer silently fails.
  `build-web-firmware.sh` now names boards `2.8-inch` and validates every manifest.
- `docs/` is public: never put secrets there. The web firmware is built with `MIRROR_NO_CONFIG`
  so `include/config.h` (Wi-Fi password) can't get into `docs/firmware/*.bin` — check with
  `grep -c -a "<password>" docs/firmware/*/*.bin` (must print 0) before every push.
- After changing firmware, rebuild **every** web binary (see "Rebuilding the website firmware"
  in `examples/screen-mirror/README.md`), or the installer ships old code.

### One CYD model number, several different screens
- The user's board ("ESP32-035") turned out to be the 3.5" ESP32-3248S035R, not the common 2.8"
  CYD: backlight 27 (not 21) and touch on the display bus (not its own pins). The 2.8" itself
  exists with ILI9341, ILI9341 + inverted colours, and ST7789 panels.
- So the website offers one build per known board (driver, size, backlight pin) and a
  **Fix colours** button for the rest (`'I'` message: invert / swap red-blue, saved in NVS).
  Black screen after install = wrong backlight pin or driver → try the next choice.
  See BOARDS.md.

### Tab icon didn't change after a new logo
- Browsers cache favicons by URL, for a long time. Give icon files a **new name** whenever the
  picture changes (now `tree-32.png`, `tree-192.png`, `tree-touch-180.png`, plus `favicon.ico`
  because some browsers only ask for that). Also check the tab isn't an old `localhost` copy.

### Board pictures
- The picker shows drawings from `examples/screen-mirror/tools/make_board_pictures.py`, not
  seller photos (those are copyrighted). They show what a child can check: screen size (same
  scale for all), USB sockets, "7789" sticker, "GUITION" print. Online sources didn't confirm
  PCB colours for every model (the Guition one is drawn dark). Real photos taken by us can
  replace `docs/boards/<id>.png` with the same names.

### Speed expectations (so nobody promises 30 fps over Wi-Fi again)
- USB at 2 Mbaud ≈ 200 KB/s: ~29 fps only while frames stay under ~7 KB (Smooth mode,
  240×160 shown 2×); full-screen sharp frames are 15–25 KB → 8–12 fps.
- Wi-Fi depends on signal: −62 dBm is fine, −75 dBm is slow, −90 dBm freezes.
- Brave can work but isn't supported; tell students Chrome or Edge.

---

## Debugging recipe that worked
1. `curl http://<ip>/stats` — uptime, last restart reason, heap, largest block, RSSI.
2. Serial log (board on USB, nothing else holding the port): 2 Mbaud, timestamps per line.
3. If the log doesn't say enough, add a temporary `[DIAG]` line (heap / largest block / RSSI /
   open connections) every 2 s, reproduce, then **remove it** before committing.
4. Compare against something that works (router ping, `curl -4`, the Mac script vs the page).
