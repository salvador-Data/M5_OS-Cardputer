# M5 Cardputer field firmware — Remote Possibility & BLE Bot

Hacker Planet LLC ships two Philadelphia-assembled Cardputer SKUs (see [PRODUCT_PRICING.md](https://github.com/salvador-Data/cyberThreatGotchi/blob/main/docs/PRODUCT_PRICING.md)):

| Firmware | Price | Role |
|----------|-------|------|
| **Remote Possibility** | $99.99 | Universal IR remote — learn, SD library, scan TV/fan/AC |
| **BLE Bot** | $79.99 | Authorized BLE scout / proximity lab on Cardputer keyboard UI |

**M5 OS** (this repo) is the primary Cardputer launcher — install it first, then sideload app `.bin` files from the manifest or SD.

## External app repos (not bundled)

| App | Repository | SD filename |
|-----|------------|-------------|
| Remote Possibility | [Remote-Possibility](https://github.com/salvador-Data/Remote-Possibility) | `remote_possibility.bin` |
| BLE Bot | [BLE-Bot-Cardputer](https://github.com/salvador-Data/BLE-Bot-Cardputer) | `ble_bot.bin` |

Build each app in its own repo, copy the `.bin` to SD `/firmware/`, or download via M5 OS **Download from catalog** when Wi-Fi is connected.

Host-side manifest check: `python scripts/validate_manifest.py data/manifest.example.json` — see [SECURITY.md](../SECURITY.md).

## SD layout

```text
/
├── manifest.json              ← optional offline catalog (copy from data/manifest.example.json)
├── M5OS_CARDPUTER.txt         ← auto-created launcher marker
└── firmware/
    ├── remote_possibility.bin
    └── ble_bot.bin
```

## Manifest entry template

Host [`data/manifest.example.json`](data/manifest.example.json) or copy to SD `/manifest.json`:

```json
{
  "name": "Remote Possibility",
  "version": "1.0.0",
  "url": "https://github.com/salvador-Data/Remote-Possibility/releases/latest/download/firmware.bin",
  "bin": "remote_possibility.bin",
  "sha256": "<64-char lowercase hex — sha256sum firmware.bin>",
  "description": "Universal IR remote — learn, library, scan (Hacker Planet LLC)"
},
{
  "name": "BLE Bot",
  "version": "1.0.0",
  "url": "https://github.com/salvador-Data/BLE-Bot-Cardputer/releases/latest/download/ble_bot.bin",
  "bin": "ble_bot.bin",
  "sha256": "<64-char lowercase hex — sha256sum ble_bot.bin>",
  "description": "Authorized BLE lab scout"
}
```

Override the default manifest URL at build time:

```ini
build_flags =
    -DM5OS_MANIFEST_URL=\"https://example.com/my-manifest.json\"
```

## Launch workflow

1. Flash **M5 OS** (`pio run -e m5stack-cardputer -t upload`) or via [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro) (Cardputer target).
2. Insert SD, **Refresh manifest** (Wi-Fi or `/manifest.json`).
3. **Download from catalog** or copy `.bin` files to `/firmware/`.
4. **Launch installed app** — M5 OS flashes the selected `.bin` and reboots into that app.

**Return to M5 OS:** reflash M5 OS via M5Burner or PlatformIO (menu → **M5Burner / recovery** on device shows steps). Field apps may also offer their own return menu when co-installed.

## Shop

- [cardputer.html](https://hackerplanet.dev/cardputer.html) · `#remote-possibility` · `#ble-bot`

*Authorized networks only.*
