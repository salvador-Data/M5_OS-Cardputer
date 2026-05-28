<p align="center">
  <img src="docs/images/hero.png" alt="M5 OS on M5Stack Cardputer" width="720"/>
</p>

<h1 align="center">M5 OS — Cardputer Edition</h1>

<p align="center">
  <strong>A keyboard-first firmware launcher & SD package manager for the M5Stack Cardputer</strong>
</p>

<p align="center">
  <a href="https://github.com/salvador-Data/M5_OS-Cardputer/actions/workflows/build.yml">
    <img src="https://github.com/salvador-Data/M5_OS-Cardputer/actions/workflows/build.yml/badge.svg" alt="Build"/>
  </a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="MIT"/></a>
  <a href="https://docs.m5stack.com/en/core/Cardputer"><img src="https://img.shields.io/badge/hardware-M5Stack%20Cardputer-00C2FF" alt="Cardputer"/></a>
  <img src="https://img.shields.io/badge/platform-ESP32--S3-E7352C?logo=espressif&logoColor=white" alt="ESP32-S3"/>
</p>

---

## About

**M5 OS** turns your Cardputer into a small **handheld OS shell** with an SD-card “hard drive” layout: compartmentalized apps under `/apps/`, user data in `/home/default/`, temp and logs on `/tmp` and `/var/log`. List firmware, download from manifest, browse files, run storage cleanup, switch themes, and join Wi-Fi — all from a keyboard-first menu.

Built by **[salvador-Data](https://github.com/salvador-Data)** / **Hacker Planet LLC** for makers, students, and authorized security researchers who live in the M5 + ESP32 ecosystem.

Part of the **Hacker Planet** toolkit → [project ecosystem](https://github.com/salvador-Data/cyberThreatGotchi/blob/main/docs/ECOSYSTEM.md).

![M5 Cardputer + CTG](https://raw.githubusercontent.com/salvador-Data/cyberThreatGotchi/main/docs/images/og-m5-cardputer.png)

| Product | Description |
|---------|-------------|
| **Remote Possibility** | CTG field remote — poll `/api/status` over Wi‑Fi |
| **BLE Bot** | Authorized BLE scout on Cardputer keyboard UI |

Product page → [HackerPlanet Cardputer](https://salvador-Data.github.io/cyberThreatGotchi/cardputer.html) · [docs/CARDPUTER_PRODUCTS.md](docs/CARDPUTER_PRODUCTS.md)

Poll **CyberThreatGotchi** mood from the field:

| Client | Path |
|--------|------|
| MicroPython | [ctg_status.py](https://github.com/salvador-Data/cyberThreatGotchi/blob/main/scripts/cardputer/ctg_status.py) |
| PlatformIO firmware | [scripts/cardputer/platformio](https://github.com/salvador-Data/cyberThreatGotchi/tree/main/scripts/cardputer/platformio) |

```ini
# platformio.ini — set CTG_HOST to your BPI-R3 Mini IP
-DCTG_HOST=\"192.168.1.50\"
```

Full guide: [CARDPUTER.md](https://github.com/salvador-Data/cyberThreatGotchi/blob/main/docs/CARDPUTER.md).

<p align="center">
  <img src="docs/images/menu-ui.png" alt="M5 OS menu UI concept" width="520"/>
</p>

| Feature | Description |
|--------|-------------|
| **Launch installed apps** | Flash & run whitelisted `.bin` from `/apps/<name>/` (legacy `/firmware/` still supported) |
| **Catalog download** | Pull entries from JSON manifest over Wi-Fi or SD `/apps/manifest.json` |
| **SD VFS layout** | `/system`, `/apps`, `/home/default`, `/tmp`, `/var/log` on FAT32 microSD |
| **Storage cleanup** | Boot GC quick scan + menu **Storage cleanup** (tmp TTL, log rotation, cache reclaim) |
| **Boot splash** | Hacker Planet teal/magenta intro with progress bar + chiptune beep |
| **Serial logging** | JSON events on USB `@ 115200` (boot, catalog, launch, burner help) |
| **File explorer** | Walk SD paths from the device |
| **Themes** | Baby Blue, Hacker Green, Mr. Robot Red, **Hacker Planet** (default boot) |
| **Wi-Fi setup** | Scan and connect before downloading |
| **M5Burner bridge** | On-device recovery steps + serial workflow for desktop flash |

Longer background → **[docs/ABOUT.md](docs/ABOUT.md)**

---

## Hardware

- [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps) (ESP32-S3, keyboard, 1.14" LCD)
- microSD card (FAT32), optional USB-C for flash/power
- Wi-Fi access point you are allowed to use

---

## Flash (quick)

### PlatformIO (recommended)

```bash
git clone https://github.com/salvador-Data/M5_OS-Cardputer.git
cd M5_OS-Cardputer
pip install platformio
pio run -e m5stack-cardputer -t upload
```

### M5Burner

1. Build with PlatformIO, then flash the generated `.bin` from `.pio/build/m5stack-cardputer/`
2. Or use [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro) with the Cardputer target once I publish a release asset

---

## Controls

| Key | Action |
|-----|--------|
| `;` / `W` | Move up |
| `.` / `S` | Move down |
| `Enter` / `Space` | Select |
| `h` / `?` | Keyboard shortcuts |
| `` ` `` | Back |

Serial monitor `@ 115200` — JSON boot/catalog/launch logs.

---

## Firmware catalog

Copy [`data/manifest.example.json`](data/manifest.example.json) to SD **`/apps/manifest.json`** (legacy root `/manifest.json` still accepted). Default URL is compiled from `include/m5os_config.h` (override with `-DM5OS_MANIFEST_URL=...`).

### SD “hard drive” layout

On first boot with a FAT32 card inserted, M5 OS creates:

```text
/system/              OS marker, /system/bin/gc service note
/apps/
  manifest.json       App registry (whitelist + SHA256)
  remote_possibility/
    remote_possibility.bin
  ble_bot/
    ble_bot.bin
/home/default/
  apps/<name>/        Per-app data compartments
  cache/              Reclaimable cache (GC menu)
/tmp/                 Temp files (24h TTL sweep on boot)
/var/log/             Rotated logs (no FAT defrag — see SECURITY.md)
```

**Legacy:** flat `/firmware/*.bin` and root `/manifest.json` still work; new downloads land in `/apps/<slug>/`.

Shipped catalog entries link **external** Hacker Planet apps:

| App | Repo | SD file |
|-----|------|---------|
| Remote Possibility | [Remote-Possibility](https://github.com/salvador-Data/Remote-Possibility) | `remote_possibility.bin` |
| BLE Bot | [BLE-Bot-Cardputer](https://github.com/salvador-Data/BLE-Bot-Cardputer) | `ble_bot.bin` |

Drop offline `.bin` files on SD (preferred compartment layout):

```text
/apps/remote_possibility/remote_possibility.bin
/apps/ble_bot/ble_bot.bin
```

Or legacy flat paths:

```text
/firmware/remote_possibility.bin
/firmware/ble_bot.bin
```

Use **Launch installed app** to flash from SD. To return to M5 OS, reflash this launcher via PlatformIO or M5Burner (menu → **M5Burner / recovery**).

### Security

See **[SECURITY.md](SECURITY.md)** for the full threat model. Summary:

- Manifest and download URLs must be **HTTPS** from `github.com/salvador-Data`, `raw.githubusercontent.com/salvador-Data`, or `hackerplanet.dev`
- Optional **`sha256`** per entry — verified on download and before flash (replace placeholders in `data/manifest.example.json` before publishing)
- SD filenames sanitized — no path traversal under `/firmware/`
- Wi-Fi passwords entered on keyboard, **never logged** to USB serial

Validate a manifest on your PC:

```bash
python scripts/validate_manifest.py data/manifest.example.json
python scripts/validate_manifest.py data/manifest.example.json \
  --verify-bin ble_bot.bin=data/firmware/ble_bot.bin \
  --verify-bin remote_possibility.bin=data/firmware/remote_possibility.bin
sha256sum ble_bot.bin   # Linux/macOS - paste digest into manifest sha256 field
Get-FileHash -Algorithm SHA256 data/firmware/ble_bot.bin  # Windows PowerShell
```

---

## Project layout

```text
M5_OS-Cardputer/
├── src/
│   ├── main.cpp              # Boot sequence + menu entry
│   ├── m5os_vfs.cpp          # SD VFS mount + path layout
│   ├── m5os_gc.cpp           # Storage cleanup (boot + menu)
│   ├── launcher_menu.cpp     # Main menu & sub-screens
│   ├── firmware_catalog.cpp  # Manifest + SD package manager
│   ├── app_launcher.cpp      # Flash .bin from SD (Update API)
│   ├── ui_display.cpp        # Keyboard + LCD UI + boot splash
│   ├── burner_bridge.cpp     # M5Burner recovery workflow
│   ├── wifi_manager.cpp      # Wi-Fi scan/connect
│   ├── m5os_security.cpp     # URL whitelist + path sanitizers
│   └── serial_log.cpp        # USB JSON logging
├── include/                  # Headers (m5os_config, m5os_vfs, …)
├── scripts/
│   ├── validate_manifest.py
│   └── m5os_paths.py         # Host VFS/GC helpers (pytest)
├── tests/
│   ├── test_validate_manifest.py
│   └── test_m5os_paths.py
├── data/manifest.example.json
```

---

## Legal

For **education and authorized testing** on devices and networks you own. You are responsible for compliance with local law and venue policies.

---

## Related projects

- [Mr.-CrackBot-AI-Nano](https://github.com/salvador-Data/Mr.-CrackBot-AI-Nano) — Jetson Nano lab automation
- [M5-Cardputer-Mr.-Robot-Handshake-Keeper](https://github.com/salvador-Data/M5-Cardputer-Mr.-Robot-Handshake-Keeper) — Cardputer security research sketch

---

<p align="center">
  <sub>★ If this helps your build, star the repo — it fuels the next Cardputer release.</sub>
</p>
