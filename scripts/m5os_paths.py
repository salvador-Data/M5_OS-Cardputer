#!/usr/bin/env python3
"""Host-side M5 OS VFS path helpers — mirrors firmware sanitizers and GC rules."""

from __future__ import annotations

import re
import time
from dataclasses import dataclass
from pathlib import Path

SEGMENT = re.compile(r"^[a-z0-9_-]{1,48}$")
BIN_NAME = re.compile(r"^[A-Za-z0-9._-]+\.bin$")

VFS_DIRS = (
    "/system",
    "/system/bin",
    "/apps",
    "/home",
    "/home/default",
    "/home/default/apps",
    "/home/default/cache",
    "/home/default/saves",
    "/home/default/utms",
    "/home/default/utms/quarantine",
    "/home/default/settings.json",
    "/tmp",
    "/var/log",
)

TMP_TTL_SECONDS = 24 * 3600
CACHE_TTL_SECONDS = 7 * 24 * 3600
MAX_LOG_FILES = 5
LOG_ROTATE_BYTES = 32768


class PathError(ValueError):
    pass


def slug_from_name(name: str) -> str:
    slug = name.strip().lower().replace(" ", "_").replace("-", "_")
    slug = re.sub(r"[^a-z0-9_]", "_", slug)
    slug = re.sub(r"_+", "_", slug).strip("_")
    return sanitize_path_segment(slug)


def sanitize_path_segment(raw: str) -> str:
    seg = raw.strip().lower()
    if not seg or ".." in seg or "/" in seg or "\\" in seg:
        raise PathError(f"invalid path segment: {raw!r}")
    if not SEGMENT.match(seg):
        raise PathError(f"invalid path segment chars: {raw!r}")
    return seg


def sanitize_bin_filename(raw: str) -> str:
    name = raw.strip()
    if ".." in name or "\\" in name:
        raise PathError(f"invalid bin filename: {raw!r}")
    name = name.replace("\\", "/")
    if "/" in name:
        name = name.rsplit("/", 1)[-1]
    if not BIN_NAME.match(name):
        raise PathError(f"invalid bin filename: {raw!r}")
    if len(name) > 64:
        raise PathError("bin filename too long")
    return name


def app_dir(slug: str) -> str:
    safe = sanitize_path_segment(slug)
    return f"/apps/{safe}"


def app_data_dir(slug: str) -> str:
    safe = sanitize_path_segment(slug)
    return f"/home/default/apps/{safe}"


def bin_path(slug: str, bin_file: str) -> str:
    safe_bin = sanitize_bin_filename(bin_file)
    safe_slug = sanitize_path_segment(slug)
    return f"{app_dir(safe_slug)}/{safe_bin}"


@dataclass
class GcReport:
    tmp_removed: int = 0
    cache_removed: int = 0
    logs_rotated: int = 0
    bytes_reclaimed: int = 0


def _file_age_seconds(path: Path, now: float | None = None) -> float:
    if not path.is_file():
        return 0.0
    ts = path.stat().st_mtime
    ref = now if now is not None else time.time()
    return max(0.0, ref - ts)


def sweep_tmp(root: Path, ttl_seconds: int = TMP_TTL_SECONDS, now: float | None = None) -> GcReport:
    report = GcReport()
    tmp = root / "tmp"
    if not tmp.is_dir():
        return report
    ref = now if now is not None else time.time()
    for entry in tmp.iterdir():
        if not entry.is_file():
            continue
        age = _file_age_seconds(entry, ref)
        if age >= ttl_seconds:
            size = entry.stat().st_size
            entry.unlink(missing_ok=True)
            report.tmp_removed += 1
            report.bytes_reclaimed += size
    return report


def rotate_logs(root: Path, max_files: int = MAX_LOG_FILES, rotate_bytes: int = LOG_ROTATE_BYTES) -> GcReport:
    report = GcReport()
    log_dir = root / "var" / "log"
    if not log_dir.is_dir():
        return report
    logs = sorted((p for p in log_dir.iterdir() if p.is_file()), key=lambda p: p.stat().st_mtime)
    for path in logs:
        size = path.stat().st_size
        if size <= rotate_bytes:
            continue
        data = path.read_bytes()
        trimmed = data[-8192:] if len(data) > 8192 else data
        path.write_bytes(trimmed)
        report.logs_rotated += 1
        report.bytes_reclaimed += size - len(trimmed)
    logs = sorted((p for p in log_dir.iterdir() if p.is_file()), key=lambda p: p.stat().st_mtime)
    while len(logs) > max_files:
        oldest = logs.pop(0)
        report.bytes_reclaimed += oldest.stat().st_size
        oldest.unlink(missing_ok=True)
        report.logs_rotated += 1
    return report


def reclaim_cache_orphans(
    root: Path,
    whitelisted_slugs: set[str],
    ttl_seconds: int = CACHE_TTL_SECONDS,
    now: float | None = None,
    user_confirmed: bool = False,
) -> GcReport:
    report = GcReport()
    if not user_confirmed:
        return report
    cache = root / "home" / "default" / "cache"
    if not cache.is_dir():
        return report
    ref = now if now is not None else time.time()
    wl = {s.lower() for s in whitelisted_slugs}
    for entry in cache.iterdir():
        if not entry.is_file():
            continue
        stem = entry.stem.lower()
        age = _file_age_seconds(entry, ref)
        if stem in wl and age < ttl_seconds:
            continue
        size = entry.stat().st_size
        entry.unlink(missing_ok=True)
        report.cache_removed += 1
        report.bytes_reclaimed += size
    return report


def quick_boot_scan(root: Path, now: float | None = None) -> GcReport:
    report = GcReport()
    tmp = sweep_tmp(root, now=now)
    logs = rotate_logs(root)
    report.tmp_removed += tmp.tmp_removed
    report.cache_removed += tmp.cache_removed
    report.logs_rotated += logs.logs_rotated
    report.bytes_reclaimed += tmp.bytes_reclaimed + logs.bytes_reclaimed
    return report


def full_cleanup(root: Path, whitelisted_slugs: set[str], user_confirmed: bool, now: float | None = None) -> GcReport:
    report = quick_boot_scan(root, now=now)
    cache = reclaim_cache_orphans(root, whitelisted_slugs, user_confirmed=user_confirmed, now=now)
    report.cache_removed += cache.cache_removed
    report.bytes_reclaimed += cache.bytes_reclaimed
    return report


PROTECTED_DELETE_EXACT = {
    "/",
    "/apps",
    "/apps/manifest.json",
    "/manifest.json",
    "/home",
    "/home/default",
    "/home/default/apps",
    "/home/default/settings.json",
    "/system",
    "/home/default/utms",
    "/tmp",
    "/var/log",
}

PROTECTED_DELETE_PREFIXES = (
    "/system/",
    "/home/default/utms/",
    "/var/log/",
)


def is_protected_delete_path(vfs_path: str) -> bool:
    path = vfs_path.rstrip("/") if vfs_path not in {"/", ""} else vfs_path
    if not path or path == "/":
        return True
    if path in PROTECTED_DELETE_EXACT:
        return True
    return any(path.startswith(prefix) for prefix in PROTECTED_DELETE_PREFIXES)


def app_delete_paths(slug: str) -> tuple[str, str, str]:
    safe = sanitize_path_segment(slug)
    return app_dir(safe), app_data_dir(safe), f"/firmware/{safe}.bin"


def remove_tree(root: Path) -> None:
    if not root.exists():
        return
    if root.is_file():
        root.unlink(missing_ok=True)
        return
    for child in sorted(root.iterdir(), reverse=True):
        remove_tree(child)
    root.rmdir()


def remove_app_on_host(sd_root: Path, slug: str) -> bool:
    app_path, data_path, legacy_bin = app_delete_paths(slug)
    removed = False
    for rel in (app_path, data_path):
        target = sd_root / rel.lstrip("/")
        if target.exists():
            remove_tree(target)
            removed = True
    legacy = sd_root / legacy_bin.lstrip("/")
    if legacy.is_file():
        legacy.unlink()
        removed = True
    return removed
