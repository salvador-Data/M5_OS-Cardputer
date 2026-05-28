# M5 Cardputer field firmware — Remote Possibility & BLE Bot

Hacker Planet LLC ships two Philadelphia-assembled Cardputer SKUs (see [PRODUCT_PRICING.md](https://github.com/salvador-Data/cyberThreatGotchi/blob/main/docs/PRODUCT_PRICING.md)):

| Firmware | Price | Role |
|----------|-------|------|
| **Remote Possibility** | $89.99 | Poll CyberThreatGotchi `/api/status` from the field |
| **BLE Bot** | $79.99 | Authorized BLE scout / proximity lab on Cardputer keyboard UI |

## DIY flash

1. Build **M5 OS** base from this repo (`pio run -e m5stack-cardputer -t upload`).
2. Add firmware packages to your manifest (`data/manifest.example.json`):

```json
{
  "name": "Remote Possibility",
  "version": "1.0.0",
  "url": "https://github.com/salvador-Data/cyberThreatGotchi/raw/main/scripts/cardputer/platformio/.pio/build/remote_possibility/firmware.bin",
  "description": "CTG field remote status client"
},
{
  "name": "BLE Bot",
  "version": "1.0.0",
  "url": "https://github.com/salvador-Data/M5_OS-Cardputer/raw/main/data/firmware/ble_bot.bin",
  "description": "Authorized BLE lab scout"
}
```

3. PlatformIO CTG client reference → [cyberThreatGotchi/scripts/cardputer](https://github.com/salvador-Data/cyberThreatGotchi/tree/main/scripts/cardputer).

## Shop

- [cardputer.html](https://salvador-Data.github.io/cyberThreatGotchi/cardputer.html) · `#remote-possibility` · `#ble-bot`

*Authorized networks only.*
