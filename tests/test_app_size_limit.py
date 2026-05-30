"""Load app size pre-check helpers and bundled firmware limits."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
FLASH_H = ROOT / "include" / "m5os_flash.h"
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
CONFIG_H = ROOT / "include" / "m5os_config.h"
PARTITIONS = ROOT / "partitions" / "m5os_cardputer_8MB.csv"
RUN_SLOT_BYTES = 0x400000


def test_size_helpers_declared():
    header = FLASH_H.read_text(encoding="utf-8")
    cpp = FLASH_CPP.read_text(encoding="utf-8")
    for sym in ("formatFlashSizeMb", "formatAppTooLargeMessage"):
        assert sym in header
        assert sym in cpp


def test_launcher_precheck_uses_size_helper():
    text = APP_LAUNCHER.read_text(encoding="utf-8")
    launch = text[text.index("LaunchResult launchFromOpenFile") : text.index("AppLauncher::AppLauncher")]
    assert "formatAppTooLargeMessage" in launch
    assert "App too large for OTA slot" not in launch
    assert "maxOtaAppBytes()" in launch


def test_copy_path_rejects_oversize_before_write():
    text = APP_LAUNCHER.read_text(encoding="utf-8")
    copy_fn = text[text.index("bool copySdToOta") : text.index("LaunchResult launchFromOpenFile")]
    assert "formatAppTooLargeMessage" in copy_fn
    assert copy_fn.index("formatAppTooLargeMessage") < copy_fn.index("esp_partition_erase_range")


def test_max_loadable_matches_partition():
    cfg = CONFIG_H.read_text(encoding="utf-8")
    part = PARTITIONS.read_text(encoding="utf-8")
    assert "0x400000" in cfg
    assert "0x400000" in part
    assert RUN_SLOT_BYTES == 4194304


def test_bundled_firmware_bins_fit_run_slot():
    fw_dir = ROOT / "data" / "firmware"
    if not fw_dir.is_dir():
        return
    for path in fw_dir.glob("*.bin"):
        size = path.stat().st_size
        assert size <= RUN_SLOT_BYTES, f"{path.name} is {size} bytes > run slot {RUN_SLOT_BYTES}"


def test_burner_plan_uses_size_helper_message():
    text = (ROOT / "src" / "burner_install.cpp").read_text(encoding="utf-8")
    assert "planTooLargeMessage" in text
    assert "formatAppTooLargeMessage" in text
    assert '"App too large for OTA slot"' not in text
