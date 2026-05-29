#!/usr/bin/env python3

"""Convert a local .bin (e.g. M5Burner cache export) into an M5 OS manifest entry.



M5Burner flashes the full device image (bootloader + partition + app). M5 OS keeps

apps on SD under /apps/<slug>/ and only flashes the app slot when you launch.



Usage:

  python scripts/import_m5burner_entry.py --bin ./ble_bot.bin --name "BLE Bot" \\

      --url https://github.com/salvador-Data/BLE-Bot-Cardputer/releases/latest/download/ble_bot.bin



  python scripts/import_m5burner_entry.py --bin ./firmware.bin --name "Remote Possibility" \\

      --merge data/manifest.example.json -o data/manifest.example.json



  python scripts/import_m5burner_entry.py --bin ./bruce.bin --name "Bruce" \\

      --fid d49a9b9d8050b9bfa65246b54fc87a18 --file 61ae83f2814a8adf2442ef85a0a3d69b.bin

"""



from __future__ import annotations



import argparse

import hashlib

import json

import sys

from pathlib import Path



# Must match include/m5os_config.h kMaxAppBinBytes

MAX_APP_BIN_BYTES = 0x3C0000



ROOT = Path(__file__).resolve().parents[1]

if str(ROOT) not in sys.path:

    sys.path.insert(0, str(ROOT))



from scripts.m5burner_hookup import default_burner_cache_dir, resolve_download_url  # noqa: E402

from scripts.m5os_paths import slug_from_name  # noqa: E402

from scripts.validate_manifest import ManifestError, validate_manifest  # noqa: E402





def sha256_file(path: Path) -> str:

    digest = hashlib.sha256()

    with path.open("rb") as fh:

        for chunk in iter(lambda: fh.read(65536), b""):

            digest.update(chunk)

    return digest.hexdigest()





def find_bin_in_burner_cache(fid: str, file_name: str = "") -> Path | None:

    cache_root = Path(default_burner_cache_dir())

    if not cache_root.is_dir():

        return None

    fid_dir = cache_root / fid.lower()

    if fid_dir.is_dir() and file_name:

        candidate = fid_dir / file_name

        if candidate.is_file():

            return candidate

    matches: list[Path] = []

    for path in cache_root.rglob("*.bin"):

        if fid.lower() in {path.parent.name.lower(), path.stem.lower()}:

            matches.append(path)

        elif file_name and path.name.lower() == file_name.lower():

            matches.append(path)

    if not matches:

        return None

    return max(matches, key=lambda p: p.stat().st_mtime)





def entry_from_bin(

    bin_path: Path,

    name: str,

    url: str = "",

    version: str = "1.0.0",

    description: str = "",

    bin_name: str = "",

    fid: str = "",

    file_name: str = "",

) -> dict:

    size = bin_path.stat().st_size

    if size == 0:

        raise ValueError(f"empty bin: {bin_path}")

    if size > MAX_APP_BIN_BYTES:

        raise ValueError(

            f"{bin_path.name} is {size} bytes — exceeds M5 OS OTA limit ({MAX_APP_BIN_BYTES})"

        )

    slug = slug_from_name(name)

    if not bin_name:

        bin_name = f"{slug}.bin"

    entry: dict = {

        "name": name.strip(),

        "version": version,

        "description": description or f"Imported from {bin_path.name}",

        "bin": bin_name,

        "size": size,

        "sha256": sha256_file(bin_path),

    }

    if fid.strip() and file_name.strip():

        entry["fid"] = fid.strip().lower()

        entry["file"] = file_name.strip()

        entry["url"] = resolve_download_url(entry["fid"], entry["file"])

    elif url.strip():

        entry["url"] = url.strip()

    return entry





def merge_manifest(existing: dict, new_entry: dict) -> dict:

    firmware = list(existing.get("firmware", []))

    new_bin = new_entry["bin"]

    firmware = [e for e in firmware if str(e.get("bin", "")).strip() != new_bin]

    firmware.append(new_entry)

    return {"firmware": firmware}





def main(argv: list[str] | None = None) -> int:

    parser = argparse.ArgumentParser(

        description="Build M5 OS manifest entry from a local .bin (M5Burner cache, PIO build, etc.)"

    )

    parser.add_argument("--bin", type=Path, help="Path to local .bin file")

    parser.add_argument("--name", required=True, help="Display name for manifest entry")

    parser.add_argument("--url", default="", help="HTTPS download URL (must pass M5 OS whitelist)")

    parser.add_argument("--fid", default="", help="M5Burner / LauncherHub firmware id (32 hex)")

    parser.add_argument("--file", default="", help="M5Burner CDN filename (e.g. abc123.bin)")

    parser.add_argument(

        "--burner-cache",

        action="store_true",

        help="Locate .bin under %%LOCALAPPDATA%%\\M5Burner\\packages\\fw using --fid/--file",

    )

    parser.add_argument("--version", default="1.0.0")

    parser.add_argument("--description", default="")

    parser.add_argument("--bin-name", default="", help="SD filename (default: <slug>.bin)")

    parser.add_argument(

        "--merge",

        type=Path,

        help="Existing manifest.json to merge into (replaces same bin name)",

    )

    parser.add_argument("-o", "--output", type=Path, help="Write merged/full manifest here")

    parser.add_argument("--print-sd-path", action="store_true", help="Print recommended SD copy path")

    args = parser.parse_args(argv)



    bin_path = args.bin

    if args.burner_cache:

        if not args.fid.strip():

            print("FAIL: --burner-cache requires --fid", file=sys.stderr)

            return 1

        found = find_bin_in_burner_cache(args.fid.strip(), args.file.strip())

        if not found:

            print(f"FAIL: no .bin found in M5Burner cache for fid {args.fid}", file=sys.stderr)

            return 1

        bin_path = found

    if not bin_path or not bin_path.is_file():

        print(f"FAIL: bin not found: {bin_path}", file=sys.stderr)

        return 1



    try:

        entry = entry_from_bin(

            bin_path,

            args.name,

            url=args.url,

            version=args.version,

            description=args.description,

            bin_name=args.bin_name,

            fid=args.fid,

            file_name=args.file,

        )

        if args.merge:

            data = json.loads(args.merge.read_text(encoding="utf-8"))

            data = merge_manifest(data, entry)

        else:

            data = {"firmware": [entry]}

        validate_manifest(data)

    except (OSError, json.JSONDecodeError, ManifestError, ValueError) as exc:

        print(f"FAIL: {exc}", file=sys.stderr)

        return 1



    slug = slug_from_name(args.name)

    sd_path = f"/apps/{slug}/{entry['bin']}"

    if args.print_sd_path:

        print(f"SD path: {sd_path}")



    out_text = json.dumps(data, indent=2) + "\n"

    if args.output:

        args.output.write_text(out_text, encoding="utf-8")

        print(f"OK: wrote {args.output} ({len(data['firmware'])} entries)")

    else:

        print(out_text)



    print(f"OK: {entry['bin']} size={entry['size']} sha256={entry['sha256'][:16]}…")

    print(f"Copy bin to SD: {sd_path}")

    print("Note: sideload app bins to SD /apps/<slug>/; M5Burner full-flash is base OS only.")

    return 0





if __name__ == "__main__":

    raise SystemExit(main())

