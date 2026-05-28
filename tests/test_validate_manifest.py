"""Tests for scripts/validate_manifest.py"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "validate_manifest.py"
MANIFEST = ROOT / "data" / "manifest.example.json"


def run_validator(*args: str):
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        capture_output=True,
        text=True,
        check=False,
    )


def test_example_manifest_validates() -> None:
    result = run_validator(str(MANIFEST))
    assert result.returncode == 0, result.stderr
    assert "OK:" in result.stdout


def test_rejects_http_url(tmp_path: Path) -> None:
    bad = {
        "firmware": [
            {
                "name": "Bad",
                "url": "http://github.com/salvador-Data/foo/releases/x.bin",
                "bin": "bad.bin",
            }
        ]
    }
    path = tmp_path / "bad.json"
    path.write_text(json.dumps(bad), encoding="utf-8")
    result = run_validator(str(path))
    assert result.returncode != 0
    assert "HTTPS" in result.stderr


def test_rejects_path_traversal_bin(tmp_path: Path) -> None:
    bad = {
        "firmware": [
            {
                "name": "Evil",
                "url": "https://github.com/salvador-Data/foo/releases/x.bin",
                "bin": "../evil.bin",
            }
        ]
    }
    path = tmp_path / "bad.json"
    path.write_text(json.dumps(bad), encoding="utf-8")
    result = run_validator(str(path))
    assert result.returncode != 0
    assert "path traversal" in result.stderr.lower()


def test_rejects_unknown_host(tmp_path: Path) -> None:
    bad = {
        "firmware": [
            {
                "name": "Evil",
                "url": "https://evil.example/firmware.bin",
                "bin": "evil.bin",
            }
        ]
    }
    path = tmp_path / "bad.json"
    path.write_text(json.dumps(bad), encoding="utf-8")
    result = run_validator(str(path))
    assert result.returncode != 0
    assert "host not allowed" in result.stderr


def test_sha256_format(tmp_path: Path) -> None:
    bad = {
        "firmware": [
            {
                "name": "Bad hash",
                "url": "https://github.com/salvador-Data/foo/releases/x.bin",
                "bin": "bad.bin",
                "sha256": "not-a-hash",
            }
        ]
    }
    path = tmp_path / "bad.json"
    path.write_text(json.dumps(bad), encoding="utf-8")
    result = run_validator(str(path))
    assert result.returncode != 0
    assert "sha256" in result.stderr.lower()
