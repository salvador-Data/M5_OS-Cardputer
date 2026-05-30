<p align="center">
  <img src="docs/images/m5-cardputer.jpg" alt="M5Stack Cardputer (SKU K132)" width="640"/>
</p>

<h1 align="center">M5 OS - Cardputer Edition</h1>

<p align="center">
  <strong>A keyboard-first firmware launcher and SD package manager for the M5Stack Cardputer</strong>
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

**M5 OS** turns your [M5Stack Cardputer](https://docs.m5stack.com/en/core/Cardputer) into a pocket operating shell. Apps live on a microSD "hard drive" under `/apps/`, user settings in `/home/default/`, and temp or log data on `/tmp` and `/var/log`. From the keyboard-first menu you can load installed apps, pull packages from a JSON catalog, browse the SD card, run storage cleanup, switch themes, and join Wi-Fi.

Built by **[salvador-Data](https://github.com/salvador-Data)** / **[Hacker Planet LLC](https://salvador-Data.github.io/cyberThreatGotchi/)** for makers, students, and authorized security researchers in the M5 + ESP32 ecosystem.

Part of the Hacker Planet toolkit - see the [project ecosystem](https://github.com/salvador-Data/cyberThreatGotchi/blob/main/docs/ECOSYSTEM.md).

| Product | Description |
|---------|-------------|
| **Remote Possibility** | CTG field remote - poll `/api/status` over Wi-Fi |
| **BLE Bot** | Authorized BLE scout on Cardputer keyboard UI |

Product page: [Hacker Planet Cardputer](https://salvador-Data.github.io/cyberThreatGotchi/cardputer.html) | [docs/CARDPUTER_PRODUCTS.md](docs/CARDPUTER_PRODUCTS.md)

---

## Features

| Feature | Menu item | What it does |
|---------|-----------|--------------|
| **Load app** | Load app (ESC/`) | Pick a whitelisted `.bin` from `/apps/<name>/` and flash into **app1** run slot (**4 MB** max). **Enter** confirm → one reboot into the app. **Tab** = fast load (skip hash). Side reset / cold power returns M5 OS |
| **Load from catalog** | Load from catalog | Download manifest entries over Wi-Fi or from SD `/apps/manifest.json` |
| **Load from M5Burner** | Load from M5Burner catalog | Browse LauncherHub Cardputer apps, stream firmware OTA, save a copy to SD |
| **SD hard drive** | (automatic on boot) | FAT32 layout: `/system`, `/apps`, `/home/default`, `/tmp`, `/var/log` |
| **Wi-Fi** | WiFi setup | Scan, connect, and persist credentials in `/home/default/settings.json` |
| **ESC recovery** | Hold ESC/` at power-on | Restores M5 OS boot partition after running a third-party app |
| **Themes** | Theme | Baby Blue, Hacker Green, Mr. Robot Red, Hacker Planet, Matrix Neon, Amber Terminal |
| **Storage cleanup** | Storage cleanup | Boot GC quick scan plus menu reclaim of tmp, cache, and rotated logs |
| **File explorer** | File explorer | Walk SD paths from the device |
| **Save on SD** | Save / export to SD | Theme, Wi-Fi, log snapshots, and settings backups on the card |
| **M5Burner bridge** | M5Burner / recovery | On-device recovery steps plus serial workflow for desktop base OS flash |
| **UTMS / Security** | UTMS / Security | Micro-AV scan, threat-pack OTA, IDS status, quarantine, firewall stub, UTMS logs |
| **Serial logging** | (automatic) | JSON events on USB at 115200 baud |
| **Battery / power** | (status bar) | Auto SAV power savings at 20% battery or below |

Longer background: [docs/ABOUT.md](docs/ABOUT.md) | App install guide: [docs/APP_INSTALL.md](docs/APP_INSTALL.md)

### Fast load (skip hash)

When you confirm loading an app from **Load app** or the file explorer:

- **Enter** — default. SHA256 is computed from the SD file (and checked against manifest `sha256` when present) before copying to the run slot.
- **Tab** — fast load. Skips hashing to save time on large `.bin` files. The copy still runs with a progress bar, and the launcher still verifies the ESP image magic byte (`0xE9`) in the run slot after copy.

Use fast load only for apps you trust (your own builds or verified downloads). Serial log event: `launch_fast_load`.

**Hash speed:** On ESP32-S3, mbedtls uses the on-chip SHA accelerator (`CONFIG_MBEDTLS_HARDWARE_SHA`). File reads hash in **4 KiB** chunks (not 64 bytes), and unchanged files skip re-hash when size + mtime match the last successful load cache.

---

## Hardware

- [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps) (ESP32-S3, 56-key keyboard, 1.14 inch LCD)
- microSD card (FAT32) - contacts away from the screen
- USB-C for flash and power
- Wi-Fi access point you are allowed to use

---

## Flash (PlatformIO on COM13)

**Repo layout:** `platformio.ini` and `src/` live at the **repository root**. There is no `platformio/` subfolder.

Clone as a sibling of `cyberThreatGotchi` - not inside it:

```text
C:\Users\Owner\Projects\
|-- cyberThreatGotchi\          <- CTG repo (separate)
`-- M5_OS-Cardputer\            <- recommended clone location
    |-- platformio.ini          <- PlatformIO project root
    |-- src\                    <- firmware source
    |-- include\
    `-- data\
```

### Fresh clone and build (PowerShell)

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

One-time on Windows: installs **intelhex** into PlatformIO's Python env (needed for `bootloader.bin`).

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer
```

Flash over USB when the Cardputer is connected on **COM13** (or your assigned port). Hold **G0** at power-on if the port does not appear:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -t upload --upload-port COM13
```

Serial monitor:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -b 115200 --port COM13
```

**Windows build tip:** If compile fails with `libM5GFX.a` / `M5GFX.cpp.o: No such file`, retry single-job:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -j 1
```

**Corrupted `.pio` cache:** Clean and rebuild:

```powershell
cd C:\Users\Owner\Projects\M5_OS-Cardputer
```

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -t clean
```

```powershell
Remove-Item -Recurse -Force .pio\build\m5stack-cardputer -ErrorAction SilentlyContinue
```

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -j 1 -t upload --upload-port COM13
```

### VS Code + PlatformIO extension

1. **File -> Open Folder** -> select the folder that contains `platformio.ini`.
2. Wait for PlatformIO to finish indexing libraries.
3. Bottom toolbar: pick environment **`m5stack-cardputer`**.
4. Click **Build** or **Upload**. Set upload port to **COM13** in `platformio.ini` or the PlatformIO toolbar if needed.

### M5Burner (base OS flash)

1. Build with PlatformIO, then flash `.pio/build/m5stack-cardputer/firmware.bin` via M5Burner desktop.
2. Or use [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro) with the Cardputer target and a release asset from this repo.

Insert a **FAT32** microSD before boot. On success, boot shows **VFS ready** and creates `/system`, `/apps`, `/home`.

---

## Controls

| Key | Action |
|-----|--------|
| `;` / `W` | Move up |
| `.` / `S` | Move down |
| `Enter` / `Space` | Select |
| `h` / `?` | Keyboard shortcuts |
| `` ` `` / `ESC` | Back (main menu); at power-on, ESC recovery into M5 OS |

Serial monitor at 115200 baud - JSON boot, catalog, and launch logs.

---

## Load app, catalog, and M5Burner

### Load app (ESC/`)

1. Insert SD with apps under `/apps/<slug>/<slug>.bin`.
2. Main menu -> **Load app (ESC/`)** -> pick app -> **Enter** to flash OTA and reboot.
3. To return to M5 OS: press the **side reset button**, **power-cycle** the device, or hold **BtnA / ESC/` while powering on**. USB reflash M5 OS to **COM13** if needed.

Recent fix (28528a7): Load app uses confirm-on-Enter, phased progress bar, and on-screen errors instead of freezing the menu.

### Load from catalog

1. Connect Wi-Fi (**WiFi setup**) or copy manifest to SD.
2. Main menu -> **Load from catalog** -> pick entry -> download to `/apps/<slug>/`.
3. Use **Load app** to run without Wi-Fi.

Default manifest URL is compiled from `include/m5os_config.h` (override with `-DM5OS_MANIFEST_URL=...`).

### Load from M5Burner catalog

1. Connect Wi-Fi first.
2. Main menu -> **Load from M5Burner catalog** -> pick LauncherHub Cardputer firmware.
3. Firmware streams OTA and saves a copy to SD. SPIFFS/composite apps may need a M5Burner USB full flash to run - M5 OS stays active until you **Load app**.

See [docs/APP_INSTALL.md](docs/APP_INSTALL.md) for the full M5Burner vs M5 OS workflow.

---

## SD hard drive layout

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
  settings.json       Theme + Wi-Fi (saved from menu)
  saves/              Log/settings export snapshots
  apps/<name>/        Per-app data compartments
  cache/              Reclaimable cache (GC menu)
  utms/
    threat_pack.json  OTA threat signatures (hashes + strings)
    quarantine/       Isolated suspicious files
    utms.log          UTMS event log
    firewall_rules.json  Lab soft-AP allow/deny (stub)
/tmp/                 Temp files (24h TTL sweep on boot)
/var/log/             Rotated logs (no FAT defrag - see SECURITY.md)
```

**Legacy:** flat `/firmware/*.bin` and root `/manifest.json` still work; new downloads land in `/apps/<slug>/`.

---

## UTMS (Unified Threat Management)

Cardputer-feasible defensive features under **Main menu -> UTMS / Security**:

| Capability | Status | Notes |
|------------|--------|-------|
| **Micro-AV** | Initial | SD `/apps/*.bin` hash scan against threat pack; quarantine dir on SD |
| **Launch gate IPS** | Existing | SHA256 verify before Load app (Enter confirm); blocks bad bins |
| **Threat intel OTA** | Initial | **Update signatures** fetches signed JSON pack over HTTPS |
| **IDS status** | Stub | Last check time, pack version, alert count placeholder |
| **Quarantine** | Initial | List files in `/home/default/utms/quarantine/` |
| **Firewall rules** | Stub | View/edit JSON allow/deny list on SD (lab soft-AP) |
| **Logging & export** | Initial | Append-only `utms.log`; export via Save / export |
| **Session integrity** | Existing | `m5os_session` save prompt on app exit |
| **Certificate pinning** | Partial | HTTPS URL whitelist (github/salvador-Data, hackerplanet.dev, LauncherHub, M5Burner CDN) |
| **Wi-Fi IDS** | Future | Deauth / rogue SSID on lab AP |
| **DNS/URL blocklist** | Future | Small curated lists for lab HTTP client |
| **Honeypot tripwire** | Future | Decoy files / beacon on SD |
| **CyberThreatGotchi Pro feed** | Future | Pro API key + signed pack channel |

**Not feasible on ESP32-S3 Cardputer:** full gateway DPI, ClamAV-scale signature DB, enterprise UTM appliance throughput, or always-on inline network bridge for arbitrary LAN traffic.

### Threat pack OTA

1. Connect Wi-Fi, insert SD.
2. **UTMS / Security -> Update signatures** (or enable auto-check in **UTMS settings**).
3. Pack downloads from compile-time URL (`M5OS_UTMS_PACK_URL`) or `settings.json` override `utms.pack_url`.
4. Optional `sha256` in JSON is verified against canonical body (without the sha256 key).
5. Atomic write to `/home/default/utms/threat_pack.json`; NVS stores `last_ver` and `last_chk`.

Pack format (`data/threat_pack.example.json`):

```json
{
  "version": "2026.05.29-stub",
  "sha256": "<optional hex of body without sha256 key>",
  "signatures": {
    "hashes": ["<sha256 hex>", "..."],
    "strings": ["EICAR-STANDARD-ANTIVIRUS-TEST-FILE"]
  }
}
```

Validate on PC:

```powershell
python scripts/utms_threat_pack.py
```

Or in pytest: `pytest tests/test_utms_threat_pack.py -v`

Override pack URL at build time:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -DM5OS_UTMS_PACK_URL=\"https://hackerplanet.dev/api/utms/pack.json\"
```

Shipped catalog entries link Hacker Planet apps:

| App | Repo | SD file |
|-----|------|---------|
| Remote Possibility | [Remote-Possibility](https://github.com/salvador-Data/Remote-Possibility) | `remote_possibility.bin` |
| BLE Bot | [BLE-Bot-Cardputer](https://github.com/salvador-Data/BLE-Bot-Cardputer) | `ble_bot.bin` |

---

## Wi-Fi and themes

- **Wi-Fi:** Menu -> **WiFi setup** -> scan -> pick SSID -> enter password on keyboard. Credentials save to `/home/default/settings.json`. Passwords are never logged to USB serial.
- **Themes:** Menu -> **Theme** -> pick preset. Six palettes: Baby Blue, Hacker Green (default), Mr. Robot Red, Hacker Planet, Matrix Neon, Amber Terminal. Saved to SD when the card is mounted.

---

## Load app, session gateway, exit paths, and freeze handling

Partition layout (`partitions/m5os_cardputer_8MB.csv`) on 8 MB flash:

| Slot | Role | Offset | Size |
|------|------|--------|------|
| **app0** | M5 OS home | 0x10000 | ~3.94 MiB (0x3F0000) |
| **app1** | Session gateway (permanent) | 0x400000 | 448 KiB (0x70000) |
| **app2** | Foreign app run slot | 0x470000 | ~3.56 MiB (0x390000) |

**Load app** and **Load from M5Burner catalog** copy an **app-only ESP32-S3** `.bin` to **app2** via `esp_ota_begin` / `esp_ota_write` / `esp_ota_end`, mark the slot **VALID**, ensure the session gateway is on **app1** (embedded auto-install on boot if missing), then reboot into the **session gateway**. On the gateway screen:

- **ESC** or **`** (or hold 1 s) → M5 OS home + **Save files before exit?** when returning from a loaded app session
- **Enter** or **6 s auto-launch** → boot **app2** (foreign firmware)

**M5Burner catalog:** streams the app over Wi-Fi, saves a copy under `/apps/<slug>/` on SD, then chains **Load app** with Tab fast load when the bin is on SD.

**Troubleshooting — “App too large for OTA slot”:** Apps must fit **app2** (**0x390000** / ~3.56 MiB). Load app shows **App X.XX MB / slot Y.YY MB — too large** before copying.

**Troubleshooting — snapback to M5 OS after gateway Enter:** Load app copies via OTA API, sets **otadata** with `esp_ota_set_boot_partition`, marks **VALID**, then gateway launches app2. If you land back in M5 OS with an SW reset after Enter, the bootloader rejected **app2** — common causes:

| Symptom / log | Likely cause | Fix |
|---------------|--------------|-----|
| `launch_composite_reject` / “Merged flash bin” | SD file is a **full flash image** (bootloader + partition table + app at 0x10000), not an app-only `.bin` | Use M5Burner **catalog** (slices app offset) or an **app-only** build artifact |
| `launch_chip_fail` / “Wrong chip 0x…” | Bin built for **ESP32** or **C3**, not **ESP32-S3** (Cardputer) | Rebuild with `board = m5stack-cardputer` / ESP32-S3 target |
| `m5os_ota_end_fail` / “Image verify failed” | Corrupt download or truncated file | Re-copy; use Enter (hash verify) not Tab fast load |
| `launch_gateway_fail` | Gateway missing on app1 | `scripts/flash_all.ps1` or `pio run -t upload-all` (app0 + gateway) |
| App needs **SPIFFS** on flash | Composite Bruce/Nemo-style package | **M5Burner USB full flash** — Load app cannot create SPIFFS on this partition table |

On-screen **Load app** debug shows `run:` / `boot:` / `slot:` partition labels, flash offsets, and sizes before copy and reboot. USB serial: `m5os_boot_part`, `gw_launch_reboot`, `m5os_launch_snapback`, `m5os_launch_snapback_dbg`.

**Honest limit:** M5 OS uses a **triple-OTA** table with **no SPIFFS partition**. Apps that require on-flash SPIFFS/LittleFS still need a **M5Burner USB composite flash** (Boris Launcher “Part Change” schemes). SD-stored copies are fine; running them is not.

| Trigger | Result |
|---------|--------|
| **Load app → Enter confirm** | Progress bar → copy app2 → **session gateway** → Enter or wait → app2 |
| **ESC/` on session gateway** | M5 OS → **Save files before exit?** when session was active |
| **ESC/` while M5 OS menu is running** | Recovery splash → home |
| **ESC inside a loaded third-party app** | **Not available** — that firmware owns the keyboard |
| **Side reset while app loaded** | M5 OS → **Save files before exit?** when session was active |
| **Cold power switch off/on** | M5 OS home (no save prompt) |
| **Hold BtnA or ESC/` at power-on** | Recovery splash → M5 OS |
| **Menu watchdog (30 s)** | TWDT restores home otadata and reboots |

USB dev flash: **`scripts/flash_all.ps1`** or **`pio run -t upload-all`** writes M5 OS to **app0** and session gateway to **app1** @ **0x400000**. Plain **`pio run -t upload`** writes M5 OS only (gateway auto-installs from embed on next boot if missing). Menu → **M5Burner / recovery** shows USB and on-device recovery steps.

---

## Security

See **[SECURITY.md](SECURITY.md)** for the full threat model. Summary:

- Manifest and download URLs must be **HTTPS** from `github.com/salvador-Data`, `raw.githubusercontent.com/salvador-Data`, `hackerplanet.dev`, LauncherHub, or M5Burner CDN
- Optional **`sha256`** per entry - verified on download and before flash (unless you choose Tab fast load on confirm)
- SD filenames sanitized - no path traversal under `/apps/` or `/firmware/`
- Wi-Fi passwords entered on keyboard, never logged to USB serial

Validate a manifest on your PC:

```powershell
python scripts/validate_manifest.py data/manifest.example.json
```

---

## Project layout

```text
M5_OS-Cardputer/
|-- src/
|   |-- main.cpp              # Boot sequence + menu entry
|   |-- m5os_vfs.cpp          # SD VFS mount + path layout
|   |-- m5os_gc.cpp           # Storage cleanup (boot + menu)
|   |-- launcher_menu.cpp     # Main menu and sub-screens
|   |-- firmware_catalog.cpp  # Manifest + SD package manager
|   |-- app_launcher.cpp      # Flash .bin from SD (esp_ota API → app1)
|   |-- burner_install.cpp    # M5Burner / LauncherHub OTA load
|   |-- ui_display.cpp        # Keyboard + LCD UI + boot splash
|   |-- burner_bridge.cpp     # M5Burner recovery workflow
|   |-- wifi_manager.cpp      # Wi-Fi scan/connect
|   |-- m5os_security.cpp     # URL whitelist + path sanitizers
|   |-- utms_menu.cpp         # UTMS submenu screens
|   |-- utms_threat_pack.cpp  # Threat pack OTA + NVS
|   `-- serial_log.cpp        # USB JSON logging
|-- include/
|-- scripts/
|-- tests/
`-- data/manifest.example.json
```

---

## Legal

For **education and authorized testing** on devices and networks you own. You are responsible for compliance with local law and venue policies.

---

## Related projects

- [CyberThreatGotchi](https://github.com/salvador-Data/cyberThreatGotchi) - defensive security lab platform
- [Mr.-CrackBot-AI-Nano](https://github.com/salvador-Data/Mr.-CrackBot-AI-Nano) - Jetson Nano lab automation
- [M5-Cardputer-Mr.-Robot-Handshake-Keeper](https://github.com/salvador-Data/M5-Cardputer-Mr.-Robot-Handshake-Keeper) - Cardputer security research sketch

---

<p align="center">
  <sub>Product photo courtesy of <a href="https://docs.m5stack.com/en/core/Cardputer">M5Stack</a> (SKU K132).</sub><br/>
  <sub>If this helps your build, star the repo - it fuels the next Cardputer release.</sub>
</p>
