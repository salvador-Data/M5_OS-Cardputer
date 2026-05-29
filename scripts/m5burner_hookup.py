#!/usr/bin/env python3
"""Boris Morcelli LauncherHub + M5Burner CDN hookup helpers (host-side mirror)."""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Any
from urllib.parse import quote

LAUNCHER_HUB_CATALOG_BASE = "https://api.launcherhub.net/firmwares"
LAUNCHER_HUB_DOWNLOAD_BASE = "https://api.launcherhub.net/download"
M5_BURNER_CDN_BASE = "https://m5burner-cdn.m5stack.com/firmware/"
BURNER_CATEGORY = "cardputer"
MAX_APP_BIN_BYTES = 3145728

FID_HEX = re.compile(r"^[0-9a-fA-F]{32}$")
BURNER_FILE = re.compile(r"^[A-Za-z0-9._-]+\.bin$")


def normalize_fid(raw: str) -> str:
    fid = raw.strip().lower()
    if not FID_HEX.match(fid):
        raise ValueError(f"invalid M5Burner fid: {raw!r}")
    return fid


def sanitize_burner_file(raw: str) -> str:
    file = raw.strip()
    if not file:
        raise ValueError("empty burner file")
    if file.startswith("https://"):
        return file
    if ".." in file or "/" in file or "\\" in file:
        raise ValueError(f"invalid burner file path: {raw!r}")
    if not BURNER_FILE.match(file):
        raise ValueError(f"invalid burner file name: {raw!r}")
    if len(file) > 96:
        raise ValueError("burner file name too long")
    return file


def resolve_file_url(file_name: str) -> str:
    safe = sanitize_burner_file(file_name)
    if safe.startswith("https://"):
        return safe
    return f"{M5_BURNER_CDN_BASE}{safe}"


def resolve_download_url(fid: str, file_name: str) -> str:
    safe_fid = normalize_fid(fid)
    file_url = resolve_file_url(file_name)
    return f"{LAUNCHER_HUB_DOWNLOAD_BASE}?fid={safe_fid}&file={quote(file_url, safe='')}"


def catalog_url(page: int = 1, order_by: str = "downloads") -> str:
    return f"{LAUNCHER_HUB_CATALOG_BASE}?category={BURNER_CATEGORY}&order_by={order_by}&page={page}"


def version_url(fid: str) -> str:
    return f"{LAUNCHER_HUB_CATALOG_BASE}?fid={normalize_fid(fid)}"


def install_detail_url(fid: str, version: str) -> str:
    return f"{LAUNCHER_HUB_CATALOG_BASE}?fid={normalize_fid(fid)}&version={quote(version, safe='')}"


def default_burner_cache_dir() -> str:
    import os

    local = os.environ.get("LOCALAPPDATA", "")
    if not local:
        return ""
    return os.path.join(local, "M5Burner", "packages", "fw")


@dataclass
class BurnerInstallPlan:
    fid: str
    version: str
    file: str
    download_url: str
    app_offset: int = 0
    app_size: int = 0
    no_bootloader: bool = False
    requires_extra_partitions: bool = False


def parse_version_entry(entry: dict[str, Any]) -> dict[str, Any]:
    """Mirror Boris loopVersions ao/as/nb fields."""
    return {
        "version": str(entry.get("version", "")).strip(),
        "file": sanitize_burner_file(str(entry.get("file", ""))),
        "app_offset": int(entry.get("ao") or 0),
        "app_size": int(entry.get("as") or 0),
        "no_bootloader": bool(entry.get("nb")),
    }


def parse_install_manifest(detail: dict[str, Any], version: str = "") -> BurnerInstallPlan:
    """Mirror installFirmwareFromManifest install JSON (app slice only)."""
    version_obj = detail.get("version") or {}
    if not isinstance(version_obj, dict):
        raise ValueError("missing version object")
    file_name = sanitize_burner_file(str(version_obj.get("file", "")))
    resolved_version = version or str(version_obj.get("version", "")).strip()
    fid = normalize_fid(str(detail.get("fid") or ""))

    app_offset = 0
    app_size = 0
    no_bootloader = True
    requires_extra = False

    install = version_obj.get("install") or {}
    if isinstance(install, dict):
        app = install.get("app") or {}
        if isinstance(app, dict):
            app_offset = int(app.get("source_offset") or 0)
            app_size = int(app.get("image_size") or 0)
            no_bootloader = app_offset == 0
        partitions = install.get("partitions") or []
        if isinstance(partitions, list):
            for part in partitions:
                if not isinstance(part, dict):
                    continue
                ptype = str(part.get("type", ""))
                subtype = str(part.get("subtype", ""))
                if ptype == "app" and subtype == "ota":
                    app_offset = int(part.get("source_offset") or app_offset)
                    app_size = int(part.get("copy_size") or app_size)
                    no_bootloader = app_offset == 0
                elif ptype == "data" and subtype in {"spiffs", "fat"}:
                    if int(part.get("copy_size") or 0) > 0:
                        requires_extra = True

    if app_size > MAX_APP_BIN_BYTES:
        raise ValueError("app slice exceeds M5 OS OTA limit")

    return BurnerInstallPlan(
        fid=fid,
        version=resolved_version,
        file=file_name,
        download_url=resolve_download_url(fid, file_name),
        app_offset=app_offset,
        app_size=app_size,
        no_bootloader=no_bootloader,
        requires_extra_partitions=requires_extra,
    )
