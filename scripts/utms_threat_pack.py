#!/usr/bin/env python3
"""Host-side UTMS threat pack validation — mirrors firmware parse rules."""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

SHA256_HEX = re.compile(r"^[a-f0-9]{64}$")
VERSION = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$")
STRING_MAX = 128
HASH_MAX = 256
STRING_MAX_COUNT = 64


class ThreatPackError(ValueError):
    pass


def normalize_sha256_hex(raw: str) -> str:
    hex_val = raw.strip().lower()
    if not hex_val:
        return ""
    if not SHA256_HEX.match(hex_val):
        raise ThreatPackError(f"invalid sha256 hex: {raw!r}")
    return hex_val


def sanitize_path_segment(raw: str) -> str:
    seg = raw.strip().lower()
    if not seg or ".." in seg or "/" in seg or "\\" in seg:
        raise ThreatPackError(f"invalid path segment: {raw!r}")
    if not re.match(r"^[a-z0-9_-]{1,48}$", seg):
        raise ThreatPackError(f"invalid path segment chars: {raw!r}")
    return seg


def canonical_body_for_hash(doc: dict[str, Any]) -> bytes:
    """SHA256 target: JSON object without the sha256 key (compact, sorted keys)."""
    body = {k: v for k, v in doc.items() if k != "sha256"}
    return json.dumps(body, sort_keys=True, separators=(",", ":")).encode("utf-8")


def verify_pack_sha256(doc: dict[str, Any]) -> None:
    expected_raw = doc.get("sha256", "")
    if expected_raw is None or str(expected_raw).strip() == "":
        return
    expected = normalize_sha256_hex(str(expected_raw))
    actual = hashlib.sha256(canonical_body_for_hash(doc)).hexdigest()
    if actual != expected:
        raise ThreatPackError("sha256 mismatch for threat pack body")


def parse_threat_pack(raw: str | bytes | dict[str, Any]) -> dict[str, Any]:
    if isinstance(raw, dict):
        doc = raw
    else:
        if isinstance(raw, bytes):
            raw = raw.decode("utf-8")
        doc = json.loads(raw)
    if not isinstance(doc, dict):
        raise ThreatPackError("threat pack root must be object")

    version = str(doc.get("version", "")).strip()
    if not version or not VERSION.match(version):
        raise ThreatPackError(f"invalid version: {version!r}")

    sigs = doc.get("signatures")
    if not isinstance(sigs, dict):
        raise ThreatPackError("signatures must be object")

    hashes = sigs.get("hashes", [])
    if not isinstance(hashes, list):
        raise ThreatPackError("signatures.hashes must be array")
    if len(hashes) > HASH_MAX:
        raise ThreatPackError(f"too many hashes (max {HASH_MAX})")
    for item in hashes:
        normalize_sha256_hex(str(item))

    allow_hashes = sigs.get("allow_hashes", [])
    if not isinstance(allow_hashes, list):
        raise ThreatPackError("signatures.allow_hashes must be array")
    if len(allow_hashes) > HASH_MAX:
        raise ThreatPackError(f"too many allow_hashes (max {HASH_MAX})")
    for raw in allow_hashes:
        normalize_sha256_hex(str(raw))

    strings = sigs.get("strings", [])
    if not isinstance(strings, list):
        raise ThreatPackError("signatures.strings must be array")
    if len(strings) > STRING_MAX_COUNT:
        raise ThreatPackError(f"too many strings (max {STRING_MAX_COUNT})")
    for item in strings:
        s = str(item)
        if not s or len(s) > STRING_MAX:
            raise ThreatPackError(f"invalid signature string: {item!r}")

    verify_pack_sha256(doc)
    return doc


def pack_info(doc: dict[str, Any]) -> dict[str, int | str]:
    sigs = doc["signatures"]
    return {
        "version": doc["version"],
        "hash_count": len(sigs.get("hashes", [])),
        "string_count": len(sigs.get("strings", [])),
    }


def load_threat_pack_file(path: Path) -> dict[str, Any]:
    return parse_threat_pack(path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    import sys

    target = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "data" / "threat_pack.example.json"
    doc = load_threat_pack_file(target)
    info = pack_info(doc)
    print(json.dumps(info, indent=2))
