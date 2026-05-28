"""Tests for scripts/m5os_paths.py (VFS sanitizer + GC logic)."""

from __future__ import annotations

import sys
import time
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.m5os_paths import (
    PathError,
    app_data_dir,
    app_dir,
    bin_path,
    full_cleanup,
    quick_boot_scan,
    reclaim_cache_orphans,
    sanitize_bin_filename,
    sanitize_path_segment,
    slug_from_name,
    sweep_tmp,
)


def test_slug_from_name() -> None:
    assert slug_from_name("Remote Possibility") == "remote_possibility"
    assert slug_from_name("BLE-Bot") == "ble_bot"


def test_sanitize_path_segment_rejects_traversal() -> None:
    with pytest.raises(PathError):
        sanitize_path_segment("../evil")
    with pytest.raises(PathError):
        sanitize_path_segment("foo/bar")


def test_sanitize_bin_filename() -> None:
    assert sanitize_bin_filename("ble_bot.bin") == "ble_bot.bin"
    assert sanitize_bin_filename("/firmware/ble_bot.bin") == "ble_bot.bin"
    with pytest.raises(PathError):
        sanitize_bin_filename("../evil.bin")


def test_vfs_paths() -> None:
    assert app_dir("ble_bot") == "/apps/ble_bot"
    assert app_data_dir("ble_bot") == "/home/default/apps/ble_bot"
    assert bin_path("ble_bot", "ble_bot.bin") == "/apps/ble_bot/ble_bot.bin"


def test_sweep_tmp_ttl(tmp_path: Path) -> None:
    tmp_dir = tmp_path / "tmp"
    tmp_dir.mkdir(parents=True)
    old = tmp_dir / "stale.tmp"
    old.write_text("x", encoding="utf-8")
    now = time.time()
    past = now - (25 * 3600)
    import os

    os.utime(old, (past, past))
    report = sweep_tmp(tmp_path, now=now)
    assert report.tmp_removed == 1
    assert not old.exists()


def test_reclaim_cache_requires_confirm(tmp_path: Path) -> None:
    cache = tmp_path / "home" / "default" / "cache"
    cache.mkdir(parents=True)
    orphan = cache / "orphan.dat"
    orphan.write_bytes(b"1234")
    report = reclaim_cache_orphans(tmp_path, whitelisted_slugs={"ble_bot"}, user_confirmed=False)
    assert report.cache_removed == 0
    assert orphan.exists()


def test_reclaim_cache_orphans(tmp_path: Path) -> None:
    cache = tmp_path / "home" / "default" / "cache"
    cache.mkdir(parents=True)
    orphan = cache / "orphan.dat"
    orphan.write_bytes(b"1234")
    report = reclaim_cache_orphans(tmp_path, whitelisted_slugs={"ble_bot"}, user_confirmed=True)
    assert report.cache_removed == 1
    assert not orphan.exists()


def test_quick_boot_scan(tmp_path: Path) -> None:
    log_dir = tmp_path / "var" / "log"
    log_dir.mkdir(parents=True)
    big = log_dir / "boot.log"
    big.write_bytes(b"x" * 40000)
    report = quick_boot_scan(tmp_path)
    assert report.logs_rotated >= 1
    assert big.stat().st_size <= 8192


def test_full_cleanup(tmp_path: Path) -> None:
    cache = tmp_path / "home" / "default" / "cache"
    cache.mkdir(parents=True)
    (cache / "old.cache").write_bytes(b"1")
    report = full_cleanup(tmp_path, {"remote_possibility"}, user_confirmed=True)
    assert report.cache_removed == 1
