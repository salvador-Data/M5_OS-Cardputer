#!/usr/bin/env python3
"""Validate M5 OS Cardputer firmware manifest JSON (host-side)."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

ALLOWED_HOSTS = {
    "github.com": "/salvador-Data/",
    "raw.githubusercontent.com": "/salvador-Data/",
    "hackerplanet.dev": None,
    "www.hackerplanet.dev": None,
}

BIN_NAME = re.compile(r"^[A-Za-z0-9._-]+\.bin$")
SHA256_HEX = re.compile(r"^[0-9a-fA-F]{64}$")


class ManifestError(ValueError):
    pass


def _check_url(url: str, field: str) -> None:
    if not url.startswith("https://"):
        raise ManifestError(f"{field}: URL must use HTTPS")
    rest = url[8:]
    slash = rest.find("/")
    host = rest if slash < 0 else rest[:slash]
    path = "/" if slash < 0 else rest[slash:]
    prefix = ALLOWED_HOSTS.get(host)
    if prefix is None and host not in ALLOWED_HOSTS:
        raise ManifestError(f"{field}: host not allowed: {host}")
    if prefix is not None and not path.startswith(prefix):
        raise ManifestError(f"{field}: path must start with {prefix!r}")


def _check_bin(name: str, field: str) -> None:
    if ".." in name or "/" in name or "\\" in name:
        raise ManifestError(f"{field}: path traversal rejected")
    if not BIN_NAME.match(name):
        raise ManifestError(f"{field}: invalid bin filename {name!r}")
    if len(name) > 64:
        raise ManifestError(f"{field}: bin filename too long")


def _check_sha256(value: str, field: str) -> str:
    if not SHA256_HEX.match(value):
        raise ManifestError(f"{field}: sha256 must be 64 hex chars")
    return value.lower()


def validate_manifest(data: dict[str, Any]) -> list[dict[str, Any]]:
    firmware = data.get("firmware")
    if not isinstance(firmware, list) or not firmware:
        raise ManifestError("firmware must be a non-empty array")

    seen_bins: set[str] = set()
    validated: list[dict[str, Any]] = []
    for i, item in enumerate(firmware):
        if not isinstance(item, dict):
            raise ManifestError(f"firmware[{i}] must be an object")
        name = str(item.get("name", "")).strip()
        if not name:
            raise ManifestError(f"firmware[{i}].name is required")
        url = str(item.get("url", "")).strip()
        if url:
            _check_url(url, f"firmware[{i}].url")
        bin_name = str(item.get("bin", "")).strip()
        if not bin_name:
            slug = name.lower().replace(" ", "_") + ".bin"
            bin_name = slug
        _check_bin(bin_name, f"firmware[{i}].bin")
        if bin_name in seen_bins:
            raise ManifestError(f"duplicate bin filename: {bin_name}")
        seen_bins.add(bin_name)
        sha256 = item.get("sha256")
        if sha256 is not None and str(sha256).strip():
            _check_sha256(str(sha256).strip(), f"firmware[{i}].sha256")
        validated.append(item)
    return validated


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_url(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "M5OS-manifest-validator/1.0"})
    digest = hashlib.sha256()
    with urllib.request.urlopen(req, timeout=30) as resp:
        while True:
            chunk = resp.read(65536)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate M5 OS firmware manifest JSON")
    parser.add_argument("manifest", type=Path, help="Path to manifest.json")
    parser.add_argument(
        "--verify-bin",
        action="append",
        metavar="BIN=PATH",
        help="Verify local .bin SHA256 against manifest entry (e.g. remote_possibility.bin=./f.bin)",
    )
    parser.add_argument(
        "--verify-url",
        action="store_true",
        help="Download each package URL and verify sha256 when present",
    )
    args = parser.parse_args(argv)

    try:
        data = json.loads(args.manifest.read_text(encoding="utf-8"))
        entries = validate_manifest(data)
    except (OSError, json.JSONDecodeError, ManifestError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1

    print(f"OK: {len(entries)} firmware entries validated")

    if args.verify_bin:
        by_bin = {str(e.get("bin", "")).strip(): e for e in entries}
        for spec in args.verify_bin:
            if "=" not in spec:
                print(f"FAIL: invalid --verify-bin {spec!r}", file=sys.stderr)
                return 1
            bin_name, path_str = spec.split("=", 1)
            entry = by_bin.get(bin_name.strip())
            if not entry:
                print(f"FAIL: no manifest entry for bin {bin_name!r}", file=sys.stderr)
                return 1
            expected = str(entry.get("sha256", "")).strip().lower()
            if not expected:
                print(f"SKIP: {bin_name} has no sha256 in manifest")
                continue
            actual = sha256_file(Path(path_str))
            if actual != expected:
                print(f"FAIL: {bin_name} SHA256 mismatch", file=sys.stderr)
                return 1
            print(f"OK: {bin_name} SHA256 verified")

    if args.verify_url:
        for entry in entries:
            url = str(entry.get("url", "")).strip()
            expected = str(entry.get("sha256", "")).strip().lower()
            bin_name = str(entry.get("bin", "")).strip()
            if not url or not expected:
                print(f"SKIP: {bin_name} (no url or sha256)")
                continue
            try:
                actual = sha256_url(url)
            except (urllib.error.URLError, TimeoutError) as exc:
                print(f"FAIL: download {url}: {exc}", file=sys.stderr)
                return 1
            if actual != expected:
                print(f"FAIL: {bin_name} remote SHA256 mismatch", file=sys.stderr)
                return 1
            print(f"OK: {bin_name} remote SHA256 verified")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
