# M5 Cardputer field firmware â€” Remote Possibility & BLE Bot

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
  "url": "https://github.com/salvador-Data/Remote-Possibility/releases/latest/download/firmware.bin",
  "description": "CTG field remote status client"
},
{
  "name": "BLE Bot",
  "version": "1.0.0",
  "url": "https://github.com/salvador-Data/BLE-Bot-Cardputer/raw/main/data/firmware/ble_bot.bin",
  "description": "Authorized BLE lab scout"
}
```

3. PlatformIO CTG client reference â†’ [Remote-Possibility](https://github.com/salvador-Data/cyberThreatGotchi/tree/main/scripts/cardputer).

## Shop

- [cardputer.html](https://salvador-Data.github.io/cyberThreatGotchi/cardputer.html) Â· `#remote-possibility` Â· `#ble-bot`

*Authorized networks only.*
