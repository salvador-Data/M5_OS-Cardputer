# M5Burner and apps (Cardputer)

Quick index for Hacker Planet M5 OS. Full walkthrough: **[APP_INSTALL.md](APP_INSTALL.md)**.

## When to use M5Burner

| Task | Tool |
|------|------|
| Install or update **M5 OS base** launcher | **M5Burner** or PlatformIO USB flash |
| Install **Remote Possibility**, **BLE Bot**, other apps | **M5 OS menu** (SD + catalog) — not a full M5Burner flash |
| Return to M5 OS after running an app | Reflash M5 OS `.bin` via M5Burner or PlatformIO |

## Related docs

- **[APP_INSTALL.md](APP_INSTALL.md)** — M5Burner vs on-device catalog, SD paths, Wi-Fi download, sideload
- **[ABOUT.md](ABOUT.md)** — project scope and safety model
- **README** — build, flash, and feature overview

## PC helper

Merge a local `.bin` into the manifest:

```powershell
py -3 scripts/import_m5burner_entry.py --bin path\to\app.bin --name "My App" --merge data/manifest.example.json
```
