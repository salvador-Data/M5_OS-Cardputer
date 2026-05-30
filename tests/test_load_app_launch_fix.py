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
    assert fn.index("markPartitionOtaState(target") < fn.index("setBootPartitionForLaunch(target)")
    assert "ensureOtadataBootsHome()" in fn
    assert "verifyPartitionAppImage(target)" in fn
    assert 'noteLaunchFail("otadata")' in fn
    assert "setLaunchPending(true)" in fn
    assert "clearLaunchPending()" not in fn
    assert "feedWatchdog()" in fn


def test_ota_abort_restores_home_otadata():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("void otaSlotWriterAbort") : flash.index("String formatOtaSlotDebug")]
    assert "ensureOtadataBootsHome()" in fn
    assert "isRunningHomePartition()" in fn


def test_copy_uses_esp_ota_api():
    launcher = APP_LAUNCHER.read_text(encoding="utf-8")
    copy = launcher[launcher.index("bool copySdToOta") : launcher.index("LaunchResult launchFromOpenFile")]
    assert "otaSlotWriterBegin" in copy
    assert "esp_ota_write" not in copy  # via otaSlotWriterAppend in flash.cpp
    assert "esp_partition_write" not in copy
    assert "detectMergedFlashBin" in launcher
    assert "validateAppImageChipTarget" in copy
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert "esp_ota_begin" in flash
    assert "esp_ota_end" in flash


def test_snapback_logs_slot_debug():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool tryHandleLaunchSnapBack") : flash.index("void logBootPartitionContext")]
    assert "formatOtaSlotDebug()" in fn


def test_verify_partition_app_image_exported():
    header = FLASH_H.read_text(encoding="utf-8")
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert "verifyPartitionAppImage" in header
    assert "esp_image_verify" in flash


def test_launcher_surfaces_specific_reboot_failures():
    launcher = APP_LAUNCHER.read_text(encoding="utf-8")
    assert "surfaceLaunchRebootFailure" in launcher
    assert "formatLaunchFailMessage" in launcher
    assert "validateAppImageChipTarget" in launcher
    assert "launch_chip_fail" in launcher
    assert "Copy OK, reboot failed" not in launcher


def test_main_handoff_failure_tags_include_otadata_and_image():
    main = MAIN_CPP.read_text(encoding="utf-8")
    assert '"otadata"' in main
    assert '"image_verify"' in main
    assert '"snapback"' in main


def test_launch_snapback_handler():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool tryHandleLaunchSnapBack") : flash.index("void logBootPartitionContext")]
    assert "m5os_launch_snapback" in fn
    assert 'noteLaunchFail("snapback")' in fn
    assert "cancelLaunchSession()" in fn
