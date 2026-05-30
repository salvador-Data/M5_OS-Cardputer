"""Static contract tests for OTA recovery and flash progress UI."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BURNER_CPP = ROOT / "src" / "burner_install.cpp"
MAIN_CPP = ROOT / "src" / "main.cpp"
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
FLASH_H = ROOT / "include" / "m5os_flash.h"
UI_CPP = ROOT / "src" / "ui_display.cpp"
UI_H = ROOT / "include" / "ui_display.h"
LAUNCHER_CPP = ROOT / "src" / "launcher_menu.cpp"


def test_catalog_flash_does_not_auto_reboot():
    text = BURNER_CPP.read_text(encoding="utf-8")
    fn_start = text.index("BurnerFlashResult flashAppToOta")
    fn_end = text.index("BurnerDownloadResult downloadPlanToSd", fn_start)
    body = text[fn_start:fn_end]
    assert "esp_restart()" not in body
    assert "restoreBootToHome()" in text


def test_spiffs_apps_use_sd_only_path():
    text = BURNER_CPP.read_text(encoding="utf-8")
    assert "planRequiresSdOnly" in text
    assert "requiresFlashAssets" in text
    assert "downloadPlanToSd(plan, sdPath)" in text
    assert "Needs SPIFFS" in text


def test_single_reboot_launch_sets_boot_partition():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert "partitionIsLaunchable" in flash
    assert "esp_ota_get_state_partition" in flash
    fn = flash[flash.index("bool rebootIntoStagedApp") : flash.index("}  // namespace", flash.index("bool rebootIntoStagedApp"))]
    assert "restoreBootToHome()" not in fn
    assert "setBootPartitionForLaunch(target)" in fn
    assert "m5os_launch_reboot" in fn
    launcher = (ROOT / "src" / "app_launcher.cpp").read_text(encoding="utf-8")
    assert "launchStagedAppSession()" in launcher
    assert "launchGatewaySession()" not in launcher


def test_launch_fail_detail_wired_in_main():
    main = MAIN_CPP.read_text(encoding="utf-8")
    assert "consumeLaunchFailDetail()" in main
    assert "no_target" in main


def test_recovery_boot_wired_in_main():
    text = MAIN_CPP.read_text(encoding="utf-8")
    assert "tryEarlyRecoveryBoot()" in text
    assert "logBootPartitionContext()" in text
    assert "applyColdBootHomeRestore()" in text
    assert "applyCrashResetHomeRestore()" in text
    assert "tryHandleLaunchSnapBack()" in text
    assert "tryLaunchPendingHandoff()" not in text
    assert "saveHomeAppPartition()" in text
    assert "gatewayPartitionReady()" not in text
    assert "beginWatchdog()" in text
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert "shouldHardwareResetRestoreHome" in flash
    policy = (ROOT / "include" / "m5os_boot_policy.h").read_text(encoding="utf-8")
    assert "ESP_RST_POWERON" in policy
    assert "ESP_RST_EXT" in policy


def test_recovery_helpers_declared():
    header = FLASH_H.read_text(encoding="utf-8")
    source = FLASH_CPP.read_text(encoding="utf-8")
    for sym in (
        "saveHomeAppPartition",
        "restoreBootToHome",
        "ensureOtadataBootsHome",
        "recoveryBootRequested",
        "tryEarlyRecoveryBoot",
        "applyColdBootHomeRestore",
        "applyCrashResetHomeRestore",
        "setLaunchPending",
        "isLaunchPending",
        "clearLaunchPending",
        "launchStagedAppSession",
        "markPartitionOtaState",
        "tryLaunchPendingHandoff",
        "tryHandleLaunchSnapBack",
        "logBootPartitionContext",
        "beginLaunchSession",
        "resolveLaunchBootPartition",
        "stagingOtaPartition",
        "consumeLaunchFailDetail",
        "isAppSessionActive",
        "isSessionExitPending",
        "isRunningHomePartition",
    ):
        assert sym in header
        assert sym in source


def test_flash_progress_ui():
    assert "showFlashProgress" in UI_H.read_text(encoding="utf-8")
    ui = UI_CPP.read_text(encoding="utf-8")
    assert "drawFlashProgressFrame" in ui
    assert "gFlashProgressSprite" in ui
    assert "pushSprite" in ui[ui.index("void showFlashProgress") : ui.index("Theme& theme()")]


def test_burner_streams_report_progress():
    text = BURNER_CPP.read_text(encoding="utf-8")
    assert "showFlashProgress" in text
    assert "reportStreamProgress" in text
    assert "streamIdleTimeoutMs" in text
    chunk = text[text.index("RangeStreamResult streamRangeChunk") : text.index(
        "RangeStreamResult streamRangeToFile"
    )]
    assert "m5os::update()" in chunk
    assert "feedWatchdog()" in chunk


def test_launcher_menu_no_auto_boot_hint():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    assert "No auto-boot" in text or "save to SD" in text.lower() or "insert sd to save" in text.lower()


def test_app_switcher_on_main_menu_esc():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    assert "showAppSwitcher" in text
    assert "Load app (ESC/`)" in text
