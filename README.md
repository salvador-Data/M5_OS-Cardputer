<p align="center">
  <img src="docs/images/hero.png" alt="M5 OS on M5Stack Cardputer" width="720"/>
</p>

<h1 align="center">M5 OS â€” Cardputer Edition</h1>

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

**M5 OS** turns your Cardputer into a small **handheld OS shell** with an SD-card â€œhard driveâ€ layout: compartmentalized apps under `/apps/`, user data in `/home/default/`, temp and logs on `/tmp` and `/var/log`. List firmware, download from manifest, browse files, run storage cleanup, switch themes, and join Wi-Fi â€” all from a keyboard-first menu.

Built by **[salvador-Data](https://github.com/salvador-Data)** / **Hacker Planet LLC** for makers, students, and authorized security researchers who live in the M5 + ESP32 ecosystem.

Part of the **Hacker Planet** toolkit â†’ [project ecosystem](https://github.com/salvador-Data/cyberThreatGotchi/blob/main/docs/ECOSYSTEM.md).

![M5 Cardputer + CTG](https://raw.githubusercontent.com/salvador-Data/cyberThreatGotchi/main/docs/images/og-m5-cardputer.png)

| Product | Description |
|---------|-------------|
| **Remote Possibility** | CTG field remote â€” poll `/api/status` over Wiâ€‘Fi |
| **BLE Bot** | Authorized BLE scout on Cardputer keyboard UI |

Product page â†’ [HackerPlanet Cardputer](https://salvador-Data.github.io/cyberThreatGotchi/cardputer.html) Â· [docs/CARDPUTER_PRODUCTS.md](docs/CARDPUTER_PRODUCTS.md)

Poll **CyberThreatGotchi** mood from the field:

| Client | Path |
|--------|------|
| MicroPython | [ctg_status.py](https://github.com/salvador-Data/cyberThreatGotchi/blob/main/scripts/cardputer/ctg_status.py) |
| PlatformIO firmware | [scripts/cardputer/platformio](https://github.com/salvador-Data/cyberThreatGotchi/tree/main/scripts/cardputer/platformio) |

```ini
# platformio.ini â€” set CTG_HOST to your BPI-R3 Mini IP
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
| **Battery / power** | Top-right status bar; auto **SAV** power savings at ≤20% battery (brightness + Wi-Fi sleep) |
| **M5Burner bridge** | On-device recovery steps + serial workflow for desktop **base OS** flash |

Longer background â†’ **[docs/ABOUT.md](docs/ABOUT.md)** Â· App install guide â†’ **[docs/APP_INSTALL.md](docs/APP_INSTALL.md)**

---

## Hardware

- [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps) (ESP32-S3, keyboard, 1.14" LCD)
- microSD card (FAT32), optional USB-C for flash/power
- Wi-Fi access point you are allowed to use

---

## Flash (quick)

**Repo layout:** `platformio.ini` and `src/` live at the **repository root**. There is **no** `platformio/` subfolder (unlike [BLE-Bot-Cardputer](https://github.com/salvador-Data/BLE-Bot-Cardputer) or [Remote-Possibility](https://github.com/salvador-Data/Remote-Possibility), which use `platformio/platformio.ini`).

Clone as a **sibling** of `cyberThreatGotchi` â€” not inside it:

```text
C:\Users\Owner\Projects\
â”œâ”€â”€ cyberThreatGotchi\          â† CTG repo (separate)
â””â”€â”€ M5_OS-Cardputer\            â† recommended clone location
    â”œâ”€â”€ platformio.ini          â† PlatformIO project root
    â”œâ”€â”€ src\                    â† firmware source
    â”œâ”€â”€ include\
    â””â”€â”€ data\
```

If you already have `cyberThreatGotchi\M5_OS-Cardputer\` on disk, that folder is a **separate nested git clone** (not a submodule). PlatformIO still works from that path â€” open the folder that contains `platformio.ini`. Run `git pull` there to stay current, or re-clone to `Projects\M5_OS-Cardputer` as shown above.

### PlatformIO CLI (PowerShell)

If you already cloned and your prompt shows `...\M5_OS-Cardputer>`, you are at the project root â€” **do not** `cd platformio` or `cd M5_OS-Cardputer` again.

Fresh clone + build:

```powershell
cd C:\Users\Owner\Projects
```

```powershell
git clone https://github.com/salvador-Data/M5_OS-Cardputer.git
```

```powershell
cd M5_OS-Cardputer
```

```powershell
.\scripts\install-deps.ps1
```

One-time on Windows: installs **intelhex** into PlatformIO's Python env (needed for `bootloader.bin`). If you see `ModuleNotFoundError: No module named 'intelhex'`, run the script above or:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pip.exe" install intelhex
```


```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer
```

Flash over USB (Cardputer connected, hold reset if needed):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -t upload
```

Serial monitor:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -b 115200
```

If `pio` is on your PATH (PlatformIO Core installed globally), you can use `pio run -e m5stack-cardputer` instead of the full path above.


**Windows build tip:** If the compile fails with `libM5GFX.a` / `M5GFX.cpp.o: No such file`, retry with a single job (avoids a rare parallel `ar.exe` race):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -j 1
```

**Corrupted `.pio` cache (Windows):** If build fails with `The system cannot find the path specified` under `.pio\build\m5stack-cardputer\libc7f\SPI` or `FileNotFoundError: sconsign311.tmp`, the build tree is stale. Clean and rebuild:

```powershell
cd C:\Users\Owner\Projects\M5_OS-Cardputer
```

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -t clean
```

If clean still errors, delete the env build folder, then rebuild single-job:

```powershell
Remove-Item -Recurse -Force .pio\build\m5stack-cardputer -ErrorAction SilentlyContinue
```

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -j 1
```

The IRremote `-Wvolatile` pragma warning is harmless and can be ignored.

### VS Code + PlatformIO extension

1. **File â†’ Open Folderâ€¦** â†’ select `C:\Users\Owner\Projects\M5_OS-Cardputer` (the folder that contains `platformio.ini`).
2. Wait for PlatformIO to finish indexing libraries.
3. Bottom toolbar: pick environment **`m5stack-cardputer`**.
4. Click **Build** (checkmark) or **Upload** (arrow). No `cd` required.

### M5Burner

1. Build with PlatformIO, then flash the generated `.bin` from `.pio/build/m5stack-cardputer/`
2. Or use [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro) with the Cardputer target once I publish a release asset

---

## Latest release (Cardputer hardware fix)

**Re-flash if you see:** screen flicker during boot/menus, SD not detected, or physical **Enter** not selecting items.

| Fix | Detail |
|-----|--------|
| SD SPI | Official Cardputer pins (SCK **40**, MOSI **14**, MISO **39**, CS **12**) on dedicated **HSPI** so display SPI3 is not reset |
| Enter key | Maps `Keyboard.keysState().enter` (HID Enter), not only `\n` in the word buffer |
| Display | Boot splash draws once per stage; menus redraw only when selection changes |

```powershell
cd C:\Users\Owner\Projects\M5_OS-Cardputer
```

```powershell
git pull
```

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -j 1 -t upload
```

Insert a **FAT32** microSD (contacts away from the screen). On success, boot shows **VFS ready** and creates `/system`, `/apps`, `/home`.

---

## Controls

| Key | Action |
|-----|--------|
| `;` / `W` | Move up |
| `.` / `S` | Move down |
| `Enter` / `Space` | Select |
| `h` / `?` | Keyboard shortcuts |
| `` ` `` | Back |

Serial monitor `@ 115200` â€” JSON boot/catalog/launch logs.

---

## Firmware catalog

Copy [`data/manifest.example.json`](data/manifest.example.json) to SD **`/apps/manifest.json`** (legacy root `/manifest.json` still accepted). Default URL is compiled from `include/m5os_config.h` (override with `-DM5OS_MANIFEST_URL=...`).

### SD â€œhard driveâ€ layout

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
/var/log/             Rotated logs (no FAT defrag â€” see SECURITY.md)
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

Use **Launch installed app** to flash from SD. To return to M5 OS, reflash this launcher via PlatformIO or M5Burner (menu â†’ **M5Burner / recovery**).

### Security

See **[SECURITY.md](SECURITY.md)** for the full threat model. Summary:

- Manifest and download URLs must be **HTTPS** from `github.com/salvador-Data`, `raw.githubusercontent.com/salvador-Data`, or `hackerplanet.dev`
- Optional **`sha256`** per entry â€” verified on download and before flash (replace placeholders in `data/manifest.example.json` before publishing)
- SD filenames sanitized â€” no path traversal under `/firmware/`
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
â”œâ”€â”€ src/
â”‚   â”œâ”€â”€ main.cpp              # Boot sequence + menu entry
â”‚   â”œâ”€â”€ m5os_vfs.cpp          # SD VFS mount + path layout
â”‚   â”œâ”€â”€ m5os_gc.cpp           # Storage cleanup (boot + menu)
â”‚   â”œâ”€â”€ launcher_menu.cpp     # Main menu & sub-screens
â”‚   â”œâ”€â”€ firmware_catalog.cpp  # Manifest + SD package manager
â”‚   â”œâ”€â”€ app_launcher.cpp      # Flash .bin from SD (Update API)
â”‚   â”œâ”€â”€ ui_display.cpp        # Keyboard + LCD UI + boot splash
â”‚   â”œâ”€â”€ burner_bridge.cpp     # M5Burner recovery workflow
â”‚   â”œâ”€â”€ wifi_manager.cpp      # Wi-Fi scan/connect
â”‚   â”œâ”€â”€ m5os_security.cpp     # URL whitelist + path sanitizers
â”‚   â””â”€â”€ serial_log.cpp        # USB JSON logging
â”œâ”€â”€ include/                  # Headers (m5os_config, m5os_vfs, â€¦)
â”œâ”€â”€ scripts/
â”‚   â”œâ”€â”€ validate_manifest.py
â”‚   â””â”€â”€ m5os_paths.py         # Host VFS/GC helpers (pytest)
â”œâ”€â”€ tests/
â”‚   â”œâ”€â”€ test_validate_manifest.py
â”‚   â””â”€â”€ test_m5os_paths.py
â”œâ”€â”€ data/manifest.example.json
```

---

## Legal

For **education and authorized testing** on devices and networks you own. You are responsible for compliance with local law and venue policies.

---

## Related projects

- [Mr.-CrackBot-AI-Nano](https://github.com/salvador-Data/Mr.-CrackBot-AI-Nano) â€” Jetson Nano lab automation
- [M5-Cardputer-Mr.-Robot-Handshake-Keeper](https://github.com/salvador-Data/M5-Cardputer-Mr.-Robot-Handshake-Keeper) â€” Cardputer security research sketch

---

<p align="center">
  <sub>â˜… If this helps your build, star the repo â€” it fuels the next Cardputer release.</sub>
</p>
