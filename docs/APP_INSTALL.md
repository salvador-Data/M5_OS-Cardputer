# App install — M5Burner vs M5 OS vs M5 Launcher

This guide explains how to add **Remote Possibility**, **BLE Bot**, and other Cardputer apps **without reflashing M5 OS base firmware every time**.

## Three tools, three jobs

| Tool | What it flashes | When to use |
|------|-----------------|-------------|
| **M5Burner** (desktop) | Full device image: bootloader + partition table + firmware at `0x10000` | **Once** to install M5 OS base; again only for **M5 OS updates** or **returning to launcher** after running an app |
| **M5 OS** (this repo, on-device) | App `.bin` from SD → ESP32 OTA app slot (with user confirm) | Load or sideload apps to SD; switch/launch when needed |
| **M5 Launcher** ([bmorcelli/Launcher](https://github.com/bmorcelli/Launcher)) | Custom partition table; launcher stays in a fixed partition; apps to OTA slot | Alternative community launcher with M5Burner online catalog and WebUI |

### What M5Burner can do on Cardputer

- Flash **factory / UIFlow / full firmware** images over USB
- Flash **M5 OS** `.bin` from `.pio/build/m5stack-cardputer/firmware.bin` or a GitHub release
- Download and cache `.bin` files locally (Windows: `%LOCALAPPDATA%\M5Burner\packages\fw\…`)

### What M5Burner cannot do (for our workflow)

- It does **not** keep Hacker Planet apps on SD while preserving M5 OS in flash — a full flash **replaces everything** in the app partition
- It does **not** use our `/apps/manifest.json` catalog or SHA256 whitelist
- Official third-party M5Burner CDN URLs were previously blocked — M5 OS now allows **LauncherHub** and **M5Burner CDN** for Boris-style discovery/download and **on-device OTA load** (apps also saved to SD)

**Rule of thumb:** M5Burner = **base OS once** (+ recovery). M5 OS menu = **apps on SD**.

## M5 OS app model (already implemented)

```text
Flash partition (USB/M5Burner, rare)     SD card (persistent)
─────────────────────────────────      ─────────────────────
M5 OS launcher  ←── reflash to return   /apps/manifest.json
                                         /apps/remote_possibility/remote_possibility.bin
                                         /apps/ble_bot/ble_bot.bin
                                         /home/default/apps/<slug>/   (per-app data)
```

1. **Install base once:** PlatformIO upload or M5Burner → Cardputer target → flash M5 OS `.bin`
2. **Add apps (no base reflash):**
   - **Wi-Fi:** Menu → **Refresh manifest** → **Load from catalog**
   - **Offline:** Copy `.bin` to SD (see paths below)
   - **PC helper:** `python scripts/import_m5burner_entry.py --bin … --name … --merge data/manifest.example.json`
3. **Load app:** Menu → **Load app (ESC/`)** → pick app → **Enter run slot**
4. **Return to M5 OS:** Press the **side reset button**, **power-cycle**, or hold **BtnA / ESC/` while powering on**. Side reset shows **Save files before exit?** when a session was active. App `.bin` files **stay on SD**.

Launching an app writes the SD `.bin` into the ESP32 OTA slot and reboots. M5 OS is no longer in flash until you reflash or use recovery boot — but you do **not** re-load apps from GitHub.

### App switcher (ESC)

| Key | In main menu | In app switcher |
|-----|--------------|-----------------|
| **ESC/`** | Opens **Load app** picker | Returns to main menu |
| **Tab** | — | Cycles to next installed app |
| **;/. w/s** | Navigate menu | Navigate list |
| **Enter** | Select menu item | Load highlighted app |

While a third-party app is running, M5 OS is not active — **hold ESC/` at power-on** to restore M5 OS boot (recovery), or USB reflash M5 OS to **COM13** (or your Cardputer port).

## User steps: Remote Possibility + BLE Bot

### One-time base flash

```powershell
cd C:\Users\Owner\Projects\M5_OS-Cardputer
```

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -j 1 -t upload
```

Or M5Burner: Cardputer target → select M5 OS release `.bin`.

Insert **FAT32** microSD (contacts away from screen). Boot should show **VFS ready**.

### Option A — Wi-Fi catalog (M5Burner-like UX)

1. Menu → **WiFi setup** → connect
2. **Refresh manifest** (loads Hacker Planet manifest + merges LauncherHub `category=cardputer`)
3. Either:
   - **Load from M5Burner catalog** → pick firmware → saves to SD (SPIFFS/composite apps **do not auto-reboot** — M5 OS stays active)
   - **Load from catalog** → save `.bin` to SD only
4. **Load app (ESC/`)** to run from SD without Wi-Fi

### Option B — Manual SD sideload (no Wi-Fi)

Build each app in its repo, then copy:

```text
/apps/remote_possibility/remote_possibility.bin
/apps/ble_bot/ble_bot.bin
```

Legacy flat path also works: `/firmware/remote_possibility.bin`

Optional: copy [`data/manifest.example.json`](../data/manifest.example.json) to `/apps/manifest.json` for descriptions and SHA256 checks.

### Option C — Import from M5Burner cache

If M5Burner downloaded a `.bin` to your PC:

```powershell
cd C:\Users\Owner\Projects\M5_OS-Cardputer
```

```powershell
python scripts/import_m5burner_entry.py --bin "C:\path\to\ble_bot.bin" --name "BLE Bot" --url "https://github.com/salvador-Data/BLE-Bot-Cardputer/releases/latest/download/ble_bot.bin" --merge data/manifest.example.json -o data/manifest.example.json --print-sd-path
```

Copy the `.bin` to the printed SD path. Publish the updated manifest to GitHub or copy to `/apps/manifest.json` on SD.

## Freeze and crash recovery (M5 OS menu)

| Event | Behavior |
|-------|----------|
| **Menu freeze** (no `m5os::update()` for **30 s**) | ESP task watchdog panic → shutdown hook restores home boot partition → auto-reboot into M5 OS main menu |
| **M5 OS panic / watchdog reset** | `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT` — auto-reboot; on boot `applyCrashResetHomeRestore()` points otadata at saved M5 OS home |
| **Cold power-on / side reset** | Custom bootloader always boots M5 OS (app0); `applyColdBootHomeRestore()` fixes otadata. Side reset may show save prompt |
| **ESC/` or BtnA at boot** | Bootloader or `tryEarlyRecoveryBoot()` forces M5 OS home |
| **Load app reboot (SW reset)** | otadata stays on staged app1 (VALID — no auto-revert) |

Third-party apps that crash-loop without hardware reset may still require **side reset**, **power-cycle**, or **BtnA at boot** — M5 OS cannot run code inside a foreign app binary.

## Security (implemented)

| Control | Detail |
|---------|--------|
| HTTPS whitelist | Downloads from `github.com/salvador-Data`, `raw.githubusercontent.com/salvador-Data`, `hackerplanet.dev`, `api.launcherhub.net`, `m5burner-cdn.m5stack.com` |
| SHA256 | Optional per manifest entry; verified on download and before load to run slot |
| Size limit | App bins capped at **~3.94 MiB** (`kMaxAppBinBytes` = 0x3F0000) — matches `partitions/m5os_cardputer_8MB.csv` OTA slot |
| Path safety | No `..` or slashes in bin names; slugs sanitized |
| Load app confirm | User must confirm before any OTA write; failed load leaves launcher intact |
| No partition-0 flash | M5 OS never writes bootloader/partition table from the menu — USB/M5Burner only |

Validate before publishing:

```powershell
python scripts/validate_manifest.py data/manifest.example.json
```

```powershell
pytest tests/ -v
```

## Re-flash M5 OS base only when

- First install on a new Cardputer
- M5 OS release notes (SD pins, Enter key, boot splash fixes, etc.)
- After **Load app** — you ran an app and want the launcher back
- Deliberate migration to a different base firmware (UIFlow, M5 Launcher, etc.)

You do **not** re-flash base when adding, updating, or deleting app `.bin` files on SD.

## Roadmap (not yet in firmware)

| Feature | Status |
|---------|--------|
| SD manifest + Wi-Fi download + launch | **Shipped** |
| SHA256 + HTTPS whitelist + size cap | **Shipped** |
| On-device M5Burner online catalog (LauncherHub hookup) | **Shipped** — `Refresh manifest` merges Hacker Planet manifest + Boris LauncherHub `category=cardputer` list |
| On-device M5Burner OTA load (Boris `installFirmwareFromManifest` app slice) | **Shipped** — **Load from M5Burner catalog**; HTTP Range via `api.launcherhub.net/download?fid=…&file=…`; skips bootloader offset (`ao`/`source_offset`); saves app copy to SD |
| Host M5Burner cache import | **Shipped** (`scripts/import_m5burner_entry.py` — `--fid`, `--file`, `--burner-cache`, Windows `%LOCALAPPDATA%\M5Burner\packages\fw\…`) |
| Dual-partition launcher (return without USB, like bmorcelli Launcher) | Roadmap — requires custom `partitions.csv` |
| WebUI OTA upload from phone/PC | Roadmap |
| Auto-register SD-dropped bins in manifest | Partial — `scanInstalled()` lists them; manifest optional for SHA256 |

## Saving on SD

M5 OS persists user choices on the FAT32 microSD. If the card is missing or mount fails, menus show **Insert SD to save** instead of failing silently.

| What | Menu path | SD path |
|------|-----------|---------|
| Theme (Baby Blue, Hacker Green, etc.) | **Theme** | `/home/default/settings.json` (`theme`: 0–3) |
| Wi-Fi SSID + password (lab use) | **WiFi setup** → connect | Same file (`wifi.ssid`, `wifi.pass`) — **plaintext on SD** |
| App `.bin` from catalog | **Download from catalog** | `/apps/<slug>/<app>.bin` |
| App load from M5Burner/LauncherHub | **Load from M5Burner catalog** | OTA slot + copy to `/apps/<slug>/` |
| Manifest (offline) | Copy from PC | `/apps/manifest.json` |
| Per-app data | (apps write at runtime) | `/home/default/apps/<slug>/` |
| Log export snapshot | **Save / export to SD** → Export log | `/home/default/saves/log_export_<ms>.txt` |
| Settings backup | **Save / export to SD** → Backup settings | `/home/default/saves/settings_<ms>.json` |
| Temp / logs (automatic) | Boot + **Storage cleanup** | `/tmp`, `/var/log` |
| Browse SD | **File explorer** | Any mounted path |

Theme and Wi-Fi load automatically on boot when SD is mounted and `settings.json` exists.

### Optional boot intro WAV (user-provided)

M5 OS does **not** ship copyrighted music in firmware. If you own a licensed copy of a boot clip (for example *Mr. Roboto* by Styx), you may add it to microSD as **`mr_roboto.wav`** (PCM WAV, 8- or 16-bit mono/stereo). On boot, M5 OS searches (first match wins):

```text
/home/default/boot/mr_roboto.wav
/system/boot/mr_roboto.wav
/boot/mr_roboto.wav
```

The file plays during the Hacker Planet Guy Fawkes intro animation. If the card is missing, the file is absent, or power saving (**SAV**) is active, firmware plays an **original** short robotic synth theme instead. Post-intro boot stages stay quiet.

Create the folder on SD if needed (example):

```text
/home/default/boot/
```

### Re-flash steps (when you need the launcher back)

1. **Apps stay on SD** — no need to re-download `.bin` files after USB reflash of M5 OS base.
2. **Settings stay on SD** — reflash M5 OS base firmware only (PlatformIO upload or M5Burner); insert the same card before boot.
3. **After running an app** (OTA slot): USB reflash M5 OS `.bin` from `.pio/build/m5stack-cardputer/firmware.bin` or a GitHub release.

```powershell
cd C:\Users\Owner\Projects\M5_OS-Cardputer
```

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer -j 1 -t upload
```

## Related

- [README.md](../README.md) — build and flash
- [SECURITY.md](../SECURITY.md) — threat model
- [CARDPUTER_PRODUCTS.md](CARDPUTER_PRODUCTS.md) — Remote Possibility & BLE Bot SKUs
