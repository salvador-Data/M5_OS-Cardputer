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

**M5 OS** turns your Cardputer into a small **handheld OS shell**: list firmware on microSD, download new packages from a manifest, browse files, switch themes, and join Wi-Fi — all from a clean menu tuned for the built-in keyboard.

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
| **Launch installed apps** | Flash & run `.bin` from SD `/firmware/` (Remote Possibility, BLE Bot, …) |
| **Catalog download** | Pull entries from JSON manifest over Wi-Fi or SD `/manifest.json` |
| **Serial logging** | JSON events on USB `@ 115200` (boot, catalog, launch, burner help) |
| **File explorer** | Walk SD paths from the device |
| **Themes** | Baby Blue (default), Hacker Green, Mr. Robot Red |
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

Copy [`data/manifest.example.json`](data/manifest.example.json) to your host or to SD `/manifest.json`. Default URL is compiled from `include/m5os_config.h` (override with `-DM5OS_MANIFEST_URL=...`).

Shipped catalog entries link **external** Hacker Planet apps:

| App | Repo | SD file |
|-----|------|---------|
| Remote Possibility | [Remote-Possibility](https://github.com/salvador-Data/Remote-Possibility) | `remote_possibility.bin` |
| BLE Bot | [BLE-Bot-Cardputer](https://github.com/salvador-Data/BLE-Bot-Cardputer) | `ble_bot.bin` |

Drop offline `.bin` files on SD:

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
sha256sum ble_bot.bin   # Linux/macOS — paste digest into manifest sha256 field
```

---

## Project layout

```text
M5_OS-Cardputer/
├── src/
│   ├── main.cpp              # Boot + menu entry
│   ├── launcher_menu.cpp     # Main menu & sub-screens
│   ├── firmware_catalog.cpp  # Manifest + SD package manager
│   ├── app_launcher.cpp      # Flash .bin from SD (Update API)
│   ├── ui_display.cpp        # Keyboard + LCD UI
│   ├── burner_bridge.cpp     # M5Burner recovery workflow
│   ├── wifi_manager.cpp      # Wi-Fi scan/connect
│   └── serial_log.cpp        # USB JSON logging
├── include/                  # Headers (m5os_config, M5OSDevice, …)
├── data/manifest.example.json
├── archive/                  # Legacy monolithic sketch
├── docs/
├── platformio.ini
└── docs/CARDPUTER_PRODUCTS.md
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
