"""Tests for UTMS threat pack parsing and path sanitizers."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
EXAMPLE = ROOT / "data" / "threat_pack.example.json"

import sys

sys.path.insert(0, str(SCRIPTS))

from m5os_paths import sanitize_path_segment  # noqa: E402
from utms_threat_pack import (  # noqa: E402
    ThreatPackError,
    canonical_body_for_hash,
    normalize_sha256_hex,
    pack_info,
    parse_threat_pack,
)


def test_normalize_sha256_hex_valid() -> None:
    sample = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    assert normalize_sha256_hex(sample) == sample
    assert normalize_sha256_hex(sample.upper()) == sample


def test_normalize_sha256_hex_rejects_bad() -> None:
    with pytest.raises(ThreatPackError):
        normalize_sha256_hex("not-a-hash")
    with pytest.raises(ThreatPackError):
        normalize_sha256_hex("abc")


def test_sanitize_path_segment_rejects_traversal() -> None:
    with pytest.raises(Exception):
        sanitize_path_segment("../etc")
    with pytest.raises(Exception):
        sanitize_path_segment("foo/bar")


def test_parse_threat_pack_example_file() -> None:
    doc = parse_threat_pack(EXAMPLE.read_text(encoding="utf-8"))
    info = pack_info(doc)
    assert info["version"] == "2026.05.29-stub"
    assert info["hash_count"] == 1
    assert info["string_count"] == 1


def test_parse_threat_pack_with_sha256() -> None:
    body = {
        "version": "1.0.0",
        "signatures": {"hashes": [], "strings": ["test"]},
    }
    digest = hashlib.sha256(canonical_body_for_hash(body)).hexdigest()
    body["sha256"] = digest
    doc = parse_threat_pack(body)
    assert doc["version"] == "1.0.0"


def test_parse_threat_pack_sha256_mismatch() -> None:
    body = {
        "version": "1.0.0",
        "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "signatures": {"hashes": [], "strings": []},
    }
    with pytest.raises(ThreatPackError, match="sha256 mismatch"):
        parse_threat_pack(body)


def test_parse_threat_pack_rejects_oversized_hashes() -> None:
    body = {
        "version": "1.0.0",
        "signatures": {
            "hashes": ["e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"] * 300,
            "strings": [],
        },
    }
    with pytest.raises(ThreatPackError, match="too many hashes"):
        parse_threat_pack(body)


def test_launcher_menu_has_utms_entry() -> None:
    text = (ROOT / "src" / "launcher_menu.cpp").read_text(encoding="utf-8")
    assert "UTMS / Security" in text
    assert "showUtmsMenu" in text


def test_vfs_utms_paths_in_mount_tree() -> None:
    text = (ROOT / "src" / "m5os_vfs.cpp").read_text(encoding="utf-8")
    assert "kUtmsDir" in text
    assert "kQuarantineDir" in text
