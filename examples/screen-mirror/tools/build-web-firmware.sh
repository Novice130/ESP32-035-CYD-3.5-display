#!/bin/sh
# Rebuild every board's firmware for the website installer (docs/firmware/<board>/).
# Run from anywhere after changing src/main.cpp, then commit docs/firmware and push:
# Cloudflare Pages publishes docs/ on every push to main.
#
#   examples/screen-mirror/tools/build-web-firmware.sh
set -e
root="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$root"
boot_app0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

# website folder              PlatformIO env               name shown in the installer
for row in \
    "screen-mirror|screen_mirror_web|CYD Screen Mirror (3.5-inch)" \
    "screen-mirror-28|screen_mirror_28_web|CYD Screen Mirror (2.8-inch, 1 USB)" \
    "screen-mirror-28-2usb|screen_mirror_28_2usb_web|CYD Screen Mirror (2.8-inch, 2 USB)" \
    "screen-mirror-24|screen_mirror_24_web|CYD Screen Mirror (2.4-inch)" \
    "screen-mirror-28-st7789|screen_mirror_28_st7789_web|CYD Screen Mirror (2.8-inch, ST7789)" \
    "screen-mirror-jc2432w328|screen_mirror_jc2432w328_web|CYD Screen Mirror (Guition JC2432W328)" \
    "screen-mirror-32|screen_mirror_32_web|CYD Screen Mirror (3.2-inch)"; do
    dir="${row%%|*}"; rest="${row#*|}"; env="${rest%%|*}"; name="${rest#*|}"
    echo "== $env -> docs/firmware/$dir"
    .venv/bin/pio run -e "$env" | tail -1
    b=".pio/build/$env"
    mkdir -p "docs/firmware/$dir"
    .venv/bin/python -m esptool --chip esp32 merge_bin -o "docs/firmware/$dir/$dir.bin" \
        --flash_mode dio --flash_freq 40m --flash_size 4MB \
        0x1000 "$b/bootloader.bin" 0x8000 "$b/partitions.bin" 0xe000 "$boot_app0" 0x10000 "$b/firmware.bin" | tail -1
    # new_install_prompt_erase: without it ESP Web Tools erases the whole chip (and the saved Wi-Fi)
    cat > "docs/firmware/$dir/manifest.json" <<EOF
{
  "name": "$name",
  "version": "1.1.0",
  "new_install_prompt_erase": true,
  "builds": [
    { "chipFamily": "ESP32", "parts": [{ "path": "$dir.bin", "offset": 0 }] }
  ]
}
EOF
done

for m in docs/firmware/*/manifest.json; do python3 -m json.tool "$m" >/dev/null || { echo "STOP: $m is not valid JSON" >&2; exit 1; }; done

# a private config.h (Wi-Fi password) must never reach the public website
cfg=examples/screen-mirror/include/config.h
if [ -f "$cfg" ]; then
    pw="$(sed -n 's/.*WIFI_PASSWORD *"\(.*\)".*/\1/p' "$cfg")"
    if [ -n "$pw" ] && grep -q -a -F "$pw" docs/firmware/*/*.bin; then
        echo "STOP: the Wi-Fi password from config.h is inside a website binary" >&2
        exit 1
    fi
fi
echo "done: docs/firmware/*"
