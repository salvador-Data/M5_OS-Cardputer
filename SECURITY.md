# Security — M5 OS Cardputer

**Authorized lab use only.** M5 OS downloads and flashes firmware on hardware you own. You are responsible for compliance with local law and venue policies.

## Threat model

| Asset | Risk | Mitigation |
|-------|------|------------|
| Firmware images on SD / OTA | Tampered `.bin` flash | Optional `sha256` in manifest; verified on download and before `Update.write` |
| Manifest JSON | Malicious URLs or paths | HTTPS-only URL whitelist; `bin` basename sanitization (no `..` or `/`); VFS slug segments whitelisted |
| SD compartments | Cross-app tampering | Apps isolated under `/apps/<slug>/`; user data under `/home/default/apps/<slug>/` |
| Storage cleanup | Deletes app or user data | Boot GC: `/tmp` TTL + log rotation only; cache reclaim requires menu confirm; never auto-deletes `/apps/<slug>/` |
| Wi-Fi credentials | Leak via serial log | Passwords entered on-device only; cleared from RAM after connect; never logged |
| Serial JSON log | Sensitive data exposure | Logs events and IPs only — no passwords, tokens, or full HTTP bodies |

## URL whitelist

Manifest and download URLs must use **HTTPS** and one of:

- `https://github.com/salvador-Data/...`
- `https://raw.githubusercontent.com/salvador-Data/...`
- `https://hackerplanet.dev/...`

Override `M5OS_MANIFEST_URL` at build time is still validated at runtime.

## Firmware integrity

Each manifest entry may include a lowercase **SHA-256** hex digest:

```json
{
  "name": "BLE Bot",
  "url": "https://github.com/salvador-Data/BLE-Bot-Cardputer/releases/latest/download/ble_bot.bin",
  "bin": "ble_bot.bin",
  "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

Generate on your build host:

```bash
# Linux / macOS / Git Bash
sha256sum ble_bot.bin

# Windows PowerShell
Get-FileHash ble_bot.bin -Algorithm SHA256
```

Host-side validation before publishing:

```bash
python scripts/validate_manifest.py data/manifest.example.json
python scripts/validate_manifest.py data/manifest.example.json --verify-bin ble_bot.bin=./ble_bot.bin
```

On device:

1. **Download** — hash computed while streaming to SD; file deleted if digest mismatch when `sha256` is set.
2. **Launch** — file re-hashed from SD before flash; flash aborted on mismatch.

If `sha256` is omitted, download and launch proceed with a serial `*_checksum_skip` event (development only — ship production manifests with digests).

## SD path safety

Firmware is stored under **`/apps/<slug>/<name>.bin`** (preferred) or legacy **`/firmware/`** using sanitized basenames (`[A-Za-z0-9._-]+.bin`, max 64 chars). Directory slugs are sanitized (`[a-z0-9_-]+`, max 48 chars). Entries like `../evil.bin`, `foo/bar.bin`, or `/etc/passwd.bin` are rejected.

Manifest path: **`/apps/manifest.json`** (legacy `/manifest.json` accepted). Only entries in the manifest whitelist are offered for download; SHA256 verification on download and launch is unchanged.

Host-side path/GC helpers:

```bash
python scripts/m5os_paths.py  # importable module
pytest tests/test_m5os_paths.py -v
```

## Garbage collection

| Scope | When | Actions |
|-------|------|---------|
| Boot quick scan | Every boot | Delete `/tmp` files older than 24h; rotate `/var/log` files >32KB; trim to 5 log files |
| Menu **Storage cleanup** | User confirm | Above + reclaim orphaned files in `/home/default/cache` not tied to whitelisted apps |

**FAT32 limitation:** ESP32 SD does not perform filesystem defragmentation/compaction. M5 OS rotates and trims logs instead of claiming contiguous free space. Document lab cards with occasional reformat if heavily fragmented.

Whitelisted app directories under `/apps/` are **never** auto-deleted.

## Flash failure and rollback

- If `Update.write` or `Update.end` fails, the **M5 OS launcher partition is not replaced** — the device stays on the launcher.
- After a successful flash, the device reboots into the selected app. **Return to M5 OS:** reflash the M5 OS `.bin` via PlatformIO or M5Burner (menu → **M5Burner / recovery**).
- App `.bin` files on SD under `/apps/` are **not** deleted when you launch or reflash base OS.
- **Size limit:** app bins must be ≤ **~3.75 MiB** (`kMaxAppBinBytes` = 0x3C0000), matching the **app2 run slot** in `partitions/m5os_cardputer_8MB.csv`. The session gateway on **app1** (~448 KiB) is not used for app storage.
- M5Burner full-flash is for **base OS only** — never use it to install sideload apps; copy app bins to SD instead. See [docs/APP_INSTALL.md](docs/APP_INSTALL.md).

## Reporting

Security issues for Hacker Planet LLC projects: GitHub Security Advisories on [M5_OS-Cardputer](https://github.com/salvador-Data/M5_OS-Cardputer) or email via [hackerplanet.dev/contact](https://hackerplanet.dev/contact.html).
