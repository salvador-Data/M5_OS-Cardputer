"""Tests for Boris-style M5Burner hookup helpers."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.m5burner_hookup import (  # noqa: E402
    BURNER_CATEGORY,
    catalog_url,
    resolve_download_url,
    resolve_file_url,
    version_url,
)


def test_resolve_file_url_prefixes_cdn() -> None:
    assert resolve_file_url("abc123.bin") == (
        "https://m5burner-cdn.m5stack.com/firmware/abc123.bin"
    )


def test_resolve_download_url_matches_boris_pattern() -> None:
    fid = "967e0377b9889c7b82f059fb8a30adda"
    file_name = "61ae83f2814a8adf2442ef85a0a3d69b.bin"
    url = resolve_download_url(fid, file_name)
    assert url.startswith("https://api.launcherhub.net/download?fid=")
    assert fid in url
    assert "m5burner-cdn.m5stack.com" in url
    assert file_name in url


def test_catalog_url_uses_cardputer_category() -> None:
    assert BURNER_CATEGORY == "cardputer"
    assert "category=cardputer" in catalog_url(page=1)
    assert "order_by=downloads" in catalog_url(page=2)


def test_version_url() -> None:
    fid = "d49a9b9d8050b9bfa65246b54fc87a18"
    assert version_url(fid).endswith(fid)
