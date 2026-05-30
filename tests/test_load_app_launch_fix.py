"""Contract tests for Load app otadata boot + launch failure messaging."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
FLASH_H = ROOT / "include" / "m5os_flash.h"
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
MAIN_CPP = ROOT / "src" / "main.cpp"


def test_mark_partition_writes_new_otadata_when_missing():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool markPartitionOtaState") : flash.index("bool saveHomeAppPartition")]
    assert "bootloader_common_get_active_otadata" in fn
    assert "writeOtadataSelectEntry" in fn
    assert "m5os_stage_ota_boot" in fn


def test_reboot_sets_rtc_handoff_and_skip_validate_fallback():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool rebootIntoStagedApp") : flash.index("bool nvsSetFlag")]
    assert "setRtcBootStagedHandoff()" in fn
    assert "setBootPartitionForLaunch(target)" in fn
    assert "verifyPartitionAppImage(target)" in fn
    assert 'noteLaunchFail("otadata")' in fn
    assert "verifyPartitionAppImage(target)" in fn
    assert 'noteLaunchFail("otadata")' in fn


def test_verify_partition_app_image_exported():
    header = FLASH_H.read_text(encoding="utf-8")
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert "verifyPartitionAppImage" in header
    assert "esp_image_verify" in flash


def test_launcher_surfaces_specific_reboot_failures():
    launcher = APP_LAUNCHER.read_text(encoding="utf-8")
    assert "surfaceLaunchRebootFailure" in launcher
    assert "formatLaunchFailMessage" in launcher
    assert "verifyPartitionAppImage(staged)" in launcher
    assert "launch_otadata_fail" in launcher
    assert "Copy OK, reboot failed" not in launcher


def test_main_handoff_failure_tags_include_otadata_and_image():
    main = MAIN_CPP.read_text(encoding="utf-8")
    assert '"otadata"' in main
    assert '"image_verify"' in main
