#!/usr/bin/env python3
"""Boris Morcelli LauncherHub + M5Burner CDN hookup helpers (host-side mirror)."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any
from urllib.parse import quote

LAUNCHER_HUB_CATALOG_BASE = "https://api.launcherhub.net/firmwares"
LAUNCHER_HUB_DOWNLOAD_BASE = "https://api.launcherhub.net/download"
M5_BURNER_CDN_BASE = "https://m5burner-cdn.m5stack.com/firmware/"
BURNER_CATEGORY = "cardputer"
# Run slot app2 in partitions/m5os_cardputer_8MB.csv.
MAX_APP_BIN_BYTES = 0x3C0000
MAX_OTA_APP1_BYTES = 0x3C0000

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
class BurnerDataSlice:
    subtype: str
    label: str = ""
    source_offset: int = 0
    copy_size: int = 0


@dataclass
class BurnerInstallPlan:
    fid: str
    version: str
    file: str
    download_url: str
    app_offset: int = 0
    app_size: int = 0
    no_bootloader: bool = False
    from_manifest: bool = False
    requires_flash_assets: bool = False
    data_slices: list[BurnerDataSlice] = field(default_factory=list)


def parse_version_entry(entry: dict[str, Any]) -> dict[str, Any]:
    """Mirror Boris loopVersions ao/as/nb fields."""
    app_offset = int(entry.get("ao") or 0)
    app_size = int(entry.get("as") or 0)
    if "nb" in entry:
        no_bootloader = bool(entry.get("nb"))
    else:
        no_bootloader = app_offset == 0
    return {
        "version": str(entry.get("version", "")).strip(),
        "file": sanitize_burner_file(str(entry.get("file", ""))),
        "app_offset": app_offset,
        "app_size": app_size,
        "no_bootloader": no_bootloader,
    }


def is_launcherhub_download_url(url: str) -> bool:
    return url.startswith(LAUNCHER_HUB_DOWNLOAD_BASE)


def parse_content_range_total(content_range: str) -> int:
    """Mirror burner_install.cpp Content-Range total size parsing."""
    slash = content_range.rfind("/")
    if slash < 0 or slash + 1 >= len(content_range):
        return 0
    try:
        total = int(content_range[slash + 1 :])
    except ValueError:
        return 0
    return total if total > 0 else 0


def finalize_plan_size(plan: BurnerInstallPlan, ota_limit: int = MAX_OTA_APP1_BYTES) -> bool:
    """Mirror finalizePlanSize — app slice must fit inactive OTA slot."""
    if plan.app_size == 0:
        if not plan.no_bootloader and plan.app_offset == 0:
            return False
        plan.no_bootloader = True
    if plan.app_size > ota_limit:
        return False
    return bool(plan.download_url)


def build_install_plan(
    fid: str,
    version: str = "",
    *,
    detail: dict[str, Any] | None = None,
    version_list: dict[str, Any] | None = None,
) -> BurnerInstallPlan | None:
    """Mirror buildInstallPlan (manifest first, version list fallback)."""
    safe_fid = normalize_fid(fid)
    parsed: BurnerInstallPlan | None = None

    if detail is not None:
        try:
            parsed = parse_install_manifest(detail, version)
        except ValueError:
            parsed = None

    if parsed is None and version_list is not None:
        versions = version_list.get("versions") or []
        if not isinstance(versions, list) or not versions:
            return None
        pick: dict[str, Any] | None = None
        for entry in versions:
            if not isinstance(entry, dict):
                continue
            entry_version = str(entry.get("version", "")).strip()
            if version and entry_version != version:
                continue
            pick = entry
            break
        if pick is None:
            pick = versions[0]
        meta = parse_version_entry(pick)
        parsed = BurnerInstallPlan(
            fid=safe_fid,
            version=meta["version"],
            file=meta["file"],
            download_url=resolve_download_url(safe_fid, meta["file"]),
            app_offset=meta["app_offset"],
            app_size=meta["app_size"],
            no_bootloader=meta["no_bootloader"],
        )

    if parsed is None:
        return None

    if parsed.app_size == 0:
        return None
    if not finalize_plan_size(parsed):
        return None
    return parsed


def parse_install_manifest(detail: dict[str, Any], version: str = "") -> BurnerInstallPlan:
    """Mirror installFirmwareFromManifest install JSON (app slice only)."""
    version_obj = detail.get("version") or {}
    if not isinstance(version_obj, dict):
        raise ValueError("missing version object")
    file_name = sanitize_burner_file(str(version_obj.get("file", "")))
    resolved_version = version or str(version_obj.get("version", "")).strip()
    fid_raw = str(detail.get("fid") or "")
    fid = normalize_fid(fid_raw) if fid_raw else ""

    app_offset = 0
    app_size = 0
    no_bootloader = True
    requires_flash_assets = False
    data_slices: list[BurnerDataSlice] = []

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
                required = bool(part.get("required"))
                if ptype == "app" and subtype == "ota":
                    app_offset = int(part.get("source_offset") or app_offset)
                    app_size = int(part.get("copy_size") or app_size)
                    no_bootloader = app_offset == 0
                elif ptype == "data" and subtype in {"spiffs", "fat"}:
                    copy_size = int(part.get("copy_size") or 0)
                    if copy_size > 0:
                        data_slices.append(
                            BurnerDataSlice(
                                subtype=subtype,
                                label=str(part.get("label") or ""),
                                source_offset=int(part.get("source_offset") or 0),
                                copy_size=copy_size,
                            )
                        )
                        requires_flash_assets = True
                    elif required:
                        requires_flash_assets = True

    if app_size > MAX_OTA_APP1_BYTES:
        raise ValueError("app slice exceeds M5 OS OTA limit")

    return BurnerInstallPlan(
        fid=fid,
        version=resolved_version,
        file=file_name,
        download_url=resolve_download_url(fid, file_name) if fid else resolve_file_url(file_name),
        app_offset=app_offset,
        app_size=app_size,
        no_bootloader=no_bootloader,
        from_manifest=True,
        requires_flash_assets=requires_flash_assets,
        data_slices=data_slices,
    )
