"""Tests for scripts/import_m5burner_entry.py"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "import_m5burner_entry.py"


def run_importer(*args: str):
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        capture_output=True,
        text=True,
        check=False,
    )


def test_import_creates_valid_entry(tmp_path: Path) -> None:
    bin_path = tmp_path / "demo_app.bin"
    bin_path.write_bytes(b"\x00" * 128)
    out = tmp_path / "manifest.json"
    result = run_importer(
        "--bin",
        str(bin_path),
        "--name",
        "Demo App",
        "--url",
        "https://github.com/salvador-Data/foo/releases/latest/download/demo_app.bin",
        "-o",
        str(out),
    )
    assert result.returncode == 0, result.stderr
    data = json.loads(out.read_text(encoding="utf-8"))
    assert len(data["firmware"]) == 1
    entry = data["firmware"][0]
    assert entry["bin"] == "demo_app.bin"
    assert entry["size"] == 128
    assert len(entry["sha256"]) == 64


def test_import_with_fid_file_builds_launcherhub_url(tmp_path: Path) -> None:
    bin_path = tmp_path / "demo_app.bin"
    bin_path.write_bytes(b"\x01" * 64)
    out = tmp_path / "manifest.json"
    result = run_importer(
        "--bin",
        str(bin_path),
        "--name",
        "Demo Burner App",
        "--fid",
        "967e0377b9889c7b82f059fb8a30adda",
        "--file",
        "61ae83f2814a8adf2442ef85a0a3d69b.bin",
        "-o",
        str(out),
    )
    assert result.returncode == 0, result.stderr
    data = json.loads(out.read_text(encoding="utf-8"))
    entry = data["firmware"][0]
    assert entry["fid"] == "967e0377b9889c7b82f059fb8a30adda"
    assert "api.launcherhub.net/download" in entry["url"]


def test_rejects_oversized_bin(tmp_path: Path) -> None:
    bin_path = tmp_path / "huge.bin"
    # 0x3C0000 + 1 byte exceeds kMaxAppBinBytes
    bin_path.write_bytes(b"\xff" * (0x3C0000 + 1))
    result = run_importer("--bin", str(bin_path), "--name", "Huge")
    assert result.returncode != 0
    assert "exceeds" in result.stderr.lower()
