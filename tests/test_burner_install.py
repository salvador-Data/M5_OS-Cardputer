"""Tests for Boris-style M5Burner OTA install plan parsing."""

from __future__ import annotations

import json
import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.m5burner_hookup import (  # noqa: E402
    BurnerInstallPlan,
    build_install_plan,
    install_detail_url,
    is_launcherhub_download_url,
    parse_content_range_total,
    parse_install_manifest,
    parse_version_entry,
    resolve_download_url,
    version_url,
)

M5LAUNCHER_FID = "967e0377b9889c7b82f059fb8a30adda"
BRUCE_FID = "d49a9b9d8050b9bfa65246b54fc87a18"
TEST_HWID = "AA:BB:CC:DD:EE:FF"


def _fetch_json(url: str) -> dict:
    with urllib.request.urlopen(url) as response:
        return json.load(response)


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


def test_parse_version_entry_derives_no_bootloader_from_offset() -> None:
    parsed = parse_version_entry(
        {
            "version": "2.7.2",
            "file": "61ae83f2814a8adf2442ef85a0a3d69b.bin",
            "ao": 65536,
            "as": 1289312,
        }
    )
    assert parsed["no_bootloader"] is False


def test_parse_install_manifest_builds_download_url() -> None:
    fid = M5LAUNCHER_FID
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
    fid = BRUCE_FID
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
    fid = M5LAUNCHER_FID
    url = install_detail_url(fid, "1.0 beta")
    assert "version=1.0%20beta" in url


def test_build_install_plan_spiffs_app_not_rejected() -> None:
    """SPIFFS apps must build a plan; flash layer rejects extra partitions."""
    detail = {
        "fid": BRUCE_FID,
        "version": {
            "version": "1.15",
            "file": "e47b9a392e0efdceb9eca703981c15f5.bin",
            "install": {
                "app": {"source_offset": 65536, "image_size": 512000},
                "partitions": [
                    {"type": "data", "subtype": "spiffs", "copy_size": 4096},
                ],
            },
        },
    }
    plan = build_install_plan(BRUCE_FID, "1.15", detail=detail)
    assert plan is not None
    assert plan.requires_extra_partitions is True
    assert plan.app_size == 512000


def test_build_install_plan_empty_version_uses_version_list() -> None:
    version_list = _fetch_json(version_url(M5LAUNCHER_FID))
    plan = build_install_plan(M5LAUNCHER_FID, "", version_list=version_list)
    assert plan is not None
    assert plan.version == version_list["versions"][0]["version"]
    assert plan.app_size > 0


def test_launcherhub_download_requires_hwid() -> None:
    version_list = _fetch_json(version_url(M5LAUNCHER_FID))
    latest = version_list["versions"][0]
    download_url = resolve_download_url(M5LAUNCHER_FID, latest["file"])
    assert is_launcherhub_download_url(download_url)

    try:
        urllib.request.urlopen(download_url)
        raise AssertionError("expected HWID requirement to fail without header")
    except urllib.error.HTTPError as exc:
        assert exc.code == 400
        body = json.loads(exc.read().decode())
    assert "HWID" in body.get("error", "")

    req = urllib.request.Request(
        download_url,
        headers={"HWID": TEST_HWID, "Range": "bytes=0-0"},
    )
    with urllib.request.urlopen(req) as response:
        assert response.status == 206
        total = parse_content_range_total(response.headers["Content-Range"])
    assert total == latest["Fs"]


def test_live_m5launcher_install_manifest_roundtrip() -> None:
    version_list = _fetch_json(version_url(M5LAUNCHER_FID))
    version = version_list["versions"][0]["version"]
    detail = _fetch_json(install_detail_url(M5LAUNCHER_FID, version))
    plan = build_install_plan(M5LAUNCHER_FID, version, detail=detail, version_list=version_list)
    assert plan is not None
    assert plan.app_size == detail["version"]["install"]["app"]["image_size"]

    req = urllib.request.Request(
        plan.download_url,
        headers={"HWID": TEST_HWID, "Range": f"bytes={plan.app_offset}-{plan.app_offset + 1}"},
    )
    with urllib.request.urlopen(req) as response:
        assert response.status == 206
        assert len(response.read()) == 2
