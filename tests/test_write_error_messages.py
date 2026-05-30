"""User-visible write failure messages name SD vs OTA vs gateway."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
BURNER = ROOT / "src" / "burner_install.cpp"
GATEWAY = ROOT / "src" / "m5os_gateway.cpp"
FLASH_H = ROOT / "include" / "m5os_flash.h"


def test_launcher_ota_write_message_names_run_slot():
    text = APP_LAUNCHER.read_text(encoding="utf-8")
    copy = text[text.index("bool copySdToOta") : text.index("LaunchResult launchFromOpenFile")]
    assert "OTA write failed (run slot)" in copy
    assert "Write failed — M5 OS intact" not in copy
    assert "formatEspOtaErr(writer.lastErr)" in copy


def test_burner_distinguishes_ota_and_sd_write():
    text = BURNER.read_text(encoding="utf-8")
    assert "StreamWriteFail" in text
    assert "OTA write failed (run slot)" in text
    assert "SD write failed during load" in text
    assert "SD write failed during download" in text


def test_gateway_flash_surfaces_detail():
    text = GATEWAY.read_text(encoding="utf-8")
    assert "lastGatewayFlashDetail" in text
    assert "Gateway flash write (app1)" in text
    assert "noteGatewayFlashDetail" in text
    launcher = APP_LAUNCHER.read_text(encoding="utf-8")
    assert "lastGatewayFlashDetail()" in launcher


def test_ota_writer_tracks_last_err():
    header = FLASH_H.read_text(encoding="utf-8")
    assert "lastErr" in header
    assert "formatEspOtaErr" in header
