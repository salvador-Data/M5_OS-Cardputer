# App install — M5Burner vs M5 OS vs M5 Launcher

This guide explains how to add **Remote Possibility**, **BLE Bot**, and other Cardputer apps **without reflashing M5 OS base firmware every time**.

## Three tools, three jobs

| Tool | What it flashes | When to use |
|------|-----------------|-------------|
| **M5Burner** (desktop) | Full device image: bootloader + partition table + firmware at `0x10000` | **Once** to install M5 OS base; again only for **M5 OS updates** or **returning to launcher** after running an app |
| **M5 OS** (this repo, on-device) | App `.bin` from SD → ESP32 OTA app slot (with user confirm) | Download or sideload apps to SD; launch when needed |
| **M5 Launcher** ([bmorcelli/Launcher](https://github.com/bmorcelli/Launcher)) | Custom partition table; launcher stays in a fixed partition; apps to OTA slot | Alternative community launcher with M5Burner online catalog and WebUI |

### What M5Burner can do on Cardputer

- Flash **factory / UIFlow / full firmware** images over USB
- Flash **M5 OS** `.bin` from `.pio/build/m5stack-cardputer/firmware.bin` or a GitHub release
- Download and cache `.bin` files locally (Windows: `%LOCALAPPDATA%\M5Burner\packages\fw\…`)

### What M5Burner cannot do (for our workflow)

- It does **not** keep Hacker Planet apps on SD while preserving M5 OS in flash — a full flash **replaces everything** in the app partition
- It does **not** use our `/apps/manifest.json` catalog or SHA256 whitelist
- Official M5Burner download URLs are **not** on the M5 OS HTTPS whitelist (`github.com/salvador-Data`, `raw.githubusercontent.com/salvador-Data`, `hackerplanet.dev`)

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
   - **Wi-Fi:** Menu → **Refresh manifest** → **Download from catalog**
   - **Offline:** Copy `.bin` to SD (see paths below)
   - **PC helper:** `python scripts/import_m5burner_entry.py --bin … --name … --merge data/manifest.example.json`
3. **Run app:** Menu → **Launch installed app** → confirm **Enter flash app slot**
4. **Return to M5 OS:** USB reflash M5 OS (PlatformIO or M5Burner). App `.bin` files **stay on SD**.

Launching an app writes the SD `.bin` into the ESP32 OTA slot and reboots. M5 OS is no longer in flash until you reflash it — but you do **not** re-download apps from GitHub.

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

### Option A — Wi-Fi catalog (M5Burner-like UX, apps only)

1. Menu → **WiFi setup** → connect
2. **Refresh manifest** (loads `data/manifest.example.json` from GitHub by default)
3. **Download from catalog** → pick **Remote Possibility** or **BLE Bot**
4. **Launch installed app** when ready

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

## Security (implemented)

| Control | Detail |
|---------|--------|
| HTTPS whitelist | Downloads only from `github.com/salvador-Data`, `raw.githubusercontent.com/salvador-Data`, `hackerplanet.dev` |
| SHA256 | Optional per manifest entry; verified on download and before flash |
| Size limit | App bins capped at **3 MiB** (`kMaxAppBinBytes`) — matches default 8 MB OTA slot |
| Path safety | No `..` or slashes in bin names; slugs sanitized |
| Launch confirm | User must confirm before any OTA write; failed flash leaves launcher intact |
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
- After **Launch installed app** — you ran an app and want the launcher back
- Deliberate migration to a different base firmware (UIFlow, M5 Launcher, etc.)

You do **not** re-flash base when adding, updating, or deleting app `.bin` files on SD.

## Roadmap (not yet in firmware)

| Feature | Status |
|---------|--------|
| SD manifest + Wi-Fi download + launch | **Shipped** |
| SHA256 + HTTPS whitelist + size cap | **Shipped** |
| Host importer from local/M5Burner `.bin` | **Shipped** (`scripts/import_m5burner_entry.py`) |
| On-device M5Burner online catalog | Not planned — URLs outside whitelist; use Hacker Planet manifest |
| Dual-partition launcher (return without USB, like bmorcelli Launcher) | Roadmap — requires custom `partitions.csv` |
| WebUI OTA upload from phone/PC | Roadmap |
| Auto-register SD-dropped bins in manifest | Partial — `scanInstalled()` lists them; manifest optional for SHA256 |

## Saving on SD

M5 OS persists user choices on the FAT32 microSD. If the card is missing or mount fails, menus show **Insert SD to save** instead of failing silently.

| What | Menu path | SD path |
|------|-----------|---------|
| Theme (Baby Blue, Hacker Green, etc.) | **Theme** | `/home/default/settings.json` (`theme`: 0–3) |
| Wi-Fi SSID + password (lab use) | **WiFi setup** → connect | Same file (`wifi.ssid`, `wifi.pass`) — **plaintext on SD** |
| App `.bin` from catalog | **Download from catalog** | `/apps/<slug>/<app>.bin` (confirm: **Saved to /apps/...**) |
| Manifest (offline) | Copy from PC | `/apps/manifest.json` |
| Per-app data | (apps write at runtime) | `/home/default/apps/<slug>/` |
| Log export snapshot | **Save / export to SD** → Export log | `/home/default/saves/log_export_<ms>.txt` |
| Settings backup | **Save / export to SD** → Backup settings | `/home/default/saves/settings_<ms>.json` |
| Temp / logs (automatic) | Boot + **Storage cleanup** | `/tmp`, `/var/log` |
| Browse SD | **File explorer** | Any mounted path |

Theme and Wi-Fi load automatically on boot when SD is mounted and `settings.json` exists.

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
