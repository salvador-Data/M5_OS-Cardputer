# Security — M5 OS Cardputer

**Authorized lab use only.** M5 OS downloads and flashes firmware on hardware you own. You are responsible for compliance with local law and venue policies.

## Threat model

| Asset | Risk | Mitigation |
|-------|------|------------|
| Firmware images on SD / OTA | Tampered `.bin` flash | Optional `sha256` in manifest; verified on download and before `Update.write` |
| Manifest JSON | Malicious URLs or paths | HTTPS-only URL whitelist; `bin` basename sanitization (no `..` or `/`) |
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

Firmware is stored only under `/firmware/` using sanitized basenames (`[A-Za-z0-9._-]+.bin`, max 64 chars). Entries like `../evil.bin` or `/etc/passwd.bin` are rejected.

## Flash failure and rollback

- If `Update.write` or `Update.end` fails, the **M5 OS launcher partition is not replaced** — the device stays on the launcher.
- After a successful flash, the device reboots into the selected app. **Return to M5 OS:** reflash the M5 OS `.bin` via PlatformIO or M5Burner (menu → **M5Burner / recovery**).
- Keep a copy of the M5 OS launcher `.bin` on your PC and on SD `/firmware/` is not used for the launcher itself — use USB recovery.

## Reporting

Security issues for Hacker Planet LLC projects: GitHub Security Advisories on [M5_OS-Cardputer](https://github.com/salvador-Data/M5_OS-Cardputer) or email via [hackerplanet.dev/contact](https://hackerplanet.dev/contact.html).
