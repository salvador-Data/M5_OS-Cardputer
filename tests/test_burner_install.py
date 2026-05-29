"""Tests for Boris-style M5Burner OTA install plan parsing."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.m5burner_hookup import (  # noqa: E402
    BurnerInstallPlan,
    install_detail_url,
    parse_install_manifest,
    parse_version_entry,
    resolve_download_url,
)


def test_parse_version_entry_matches_boris_fields() -> None:
    parsed = parse_version_entry(
        {
            "version": "1.2.3",
            "file": "abc123.bin",
            "ao": 65536,
            "as": 1048576,
            "nb": False,
        }
    )
    assert parsed["version"] == "1.2.3"
    assert parsed["app_offset"] == 65536
    assert parsed["app_size"] == 1048576
    assert parsed["no_bootloader"] is False


def test_parse_install_manifest_builds_download_url() -> None:
    fid = "967e0377b9889c7b82f059fb8a30adda"
    detail = {
        "fid": fid,
        "name": "Demo",
        "version": {
            "version": "2.0.0",
            "file": "61ae83f2814a8adf2442ef85a0a3d69b.bin",
            "install": {
                "app": {"source_offset": 65536, "image_size": 512000},
                "partitions": [],
            },
        },
    }
    plan = parse_install_manifest(detail, "2.0.0")
    assert isinstance(plan, BurnerInstallPlan)
    assert plan.fid == fid
    assert plan.app_offset == 65536
    assert plan.app_size == 512000
    assert plan.no_bootloader is False
    assert plan.download_url == resolve_download_url(fid, plan.file)


def test_parse_install_manifest_flags_spiffs() -> None:
    fid = "d49a9b9d8050b9bfa65246b54fc87a18"
    detail = {
        "fid": fid,
        "version": {
            "version": "1.0.0",
            "file": "demo.bin",
            "install": {
                "app": {"source_offset": 0, "image_size": 1000},
                "partitions": [
                    {"type": "data", "subtype": "spiffs", "copy_size": 4096},
                ],
            },
        },
    }
    plan = parse_install_manifest(detail)
    assert plan.requires_extra_partitions is True
    assert plan.no_bootloader is True


def test_install_detail_url_encodes_version() -> None:
    fid = "967e0377b9889c7b82f059fb8a30adda"
    url = install_detail_url(fid, "1.0 beta")
    assert "version=1.0%20beta" in url
