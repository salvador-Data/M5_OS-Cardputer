# About M5 OS (Cardputer)

**M5 OS** is a pocket operating shell for the [M5Stack Cardputer](https://docs.m5stack.com/en/core/Cardputer) — a keyboard-first ESP32-S3 handheld with a 1.14" display, microSD slot, Wi-Fi, and IR.

It is built for **owners and builders** who want a Flipper-style *launcher* experience on Cardputer hardware: browse SD-stored firmware payloads, pull packages from a JSON manifest over Wi-Fi, explore files, and theme the UI — without reflashing the whole device for every experiment.

## Who made it

- **Author:** [salvador-Data](https://github.com/salvador-Data)
- **Org:** Hacker Planet LLC — defensive security tooling, portable labs, and education-first hardware projects.

## Design goals

1. **Keyboard-native UI** — navigation matches Cardputer keys (`;` / `.` / Enter / `` ` ``).
2. **SD-first** — install `.bin` artifacts under `/firmware/` on the microSD card.
3. **Manifest-driven downloads** — host a simple JSON catalog (see `data/manifest.example.json`).
4. **Readable codebase** — PlatformIO + Arduino, CI build on every push.

## What it is not

- Not a cloud-locked app store.
- Not a tool for attacking networks you do not own.
- Not a replacement for M5Burner official factory firmware — it complements your dev workflow.

## Legacy code

The original monolithic sketch (M5Stack-era APIs) lives in [`archive/M5_OS_Carputer_legacy.ino`](../archive/M5_OS_Carputer_legacy.ino) for reference. The current `src/` tree is the maintained Cardputer port.

## License

MIT — see [LICENSE](../LICENSE).
