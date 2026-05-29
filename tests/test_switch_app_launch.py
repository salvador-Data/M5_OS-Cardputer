"""Contract tests for Load app SD path resolution and keyboard entry."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAUNCHER_CPP = ROOT / "src" / "launcher_menu.cpp"
APP_LAUNCHER_CPP = ROOT / "src" / "app_launcher.cpp"
DEVICE_H = ROOT / "include" / "M5OSDevice.h"


def test_switch_launch_resolves_package_slug_path():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::launchBinFile") : text.index(
        "LaunchResult AppLauncher::flashBurnerPackage"
    )]
    assert "findByBinFile(safeBin)" in fn
    assert "binPathForPackage(*meta)" in fn
    assert fn.index("binPathForPackage") < fn.index("binPathFor(safeBin)")


def test_hash_skip_uses_staging_partition_only():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    assert "canSkipFlashToCachedOta" in text
    assert "stagingOtaPartition()" in text
    assert "tryBootCachedOta" not in text
    skip = text[text.index("bool canSkipFlashToCachedOta") : text.index("void storeLaunchCache")]
    assert "esp_ota_set_boot_partition" not in skip


def test_switcher_drains_esc_before_loop():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    switcher = text[text.index("void LauncherMenu::showAppSwitcher") : text.index(
        "void LauncherMenu::showInstalledApps"
    )]
    assert "keyboardDrainBack()" in switcher
    assert switcher.index("keyboardDrainBack()") < switcher.index("while (true)")


def test_switcher_confirm_uses_keysstate_enter():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    switcher = text[text.index("void LauncherMenu::showAppSwitcher") : text.index(
        "void LauncherMenu::showInstalledApps"
    )]
    confirm = switcher[
        switcher.index("keyboardDrainEnter()") : switcher.index(
            "redrawSwitcher();", switcher.index("keyboardDrainEnter()")
        )
    ]
    assert "keyboardEnterJustPressed()" in confirm
    assert "readButtons()" not in confirm


def test_keyboard_enter_helpers():
    text = DEVICE_H.read_text(encoding="utf-8")
    assert "keyboardEnterJustPressed" in text
    assert "keyboardDrainEnter" in text
    assert "keyboardDrainBack" in text


def test_launch_blocks_spiffs_apps():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::launchBinFile") : text.index(
        "LaunchResult AppLauncher::flashBurnerPackage"
    )]
    assert "needsFlashSpiffs" in fn
    assert "launch_spiffs_blocked" in fn


def test_launch_shows_progress_before_hash():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::launchBinFile") : text.index(
        "LaunchResult AppLauncher::flashBurnerPackage"
    )]
    assert fn.index("showFlashProgress") < fn.index("computeFileSha256HexWithProgress")
    assert "Hashing" in fn
    assert "copySdToOta" in fn


def test_switcher_shows_progress_before_launch():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    switcher = text[text.index("void LauncherMenu::showAppSwitcher") : text.index(
        "void LauncherMenu::showInstalledApps"
    )]
    assert "Starting..." in switcher
    assert "launchBinFile" in switcher
    launch_idx = switcher.index("launchBinFile")
    confirm_block = switcher[launch_idx - 200 : launch_idx + 80]
    assert "showFlashProgress" in confirm_block
    assert confirm_block.index("showFlashProgress") < confirm_block.index("launchBinFile")


def test_launch_reopens_sd_file_after_hash():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::launchBinFile") : text.index(
        "LaunchResult AppLauncher::flashBurnerPackage"
    )]
    assert "Cannot reopen bin for copy" in fn
    assert "launch_reopen_fail" in fn
    reopen = fn[fn.index("firmware.close();") : fn.index("copySdToOta")]
    assert reopen.count("SD.open(path") >= 1
    assert "launch_magic_fail" in text


def test_show_message_feeds_watchdog():
    text = (ROOT / "src" / "ui_display.cpp").read_text(encoding="utf-8")
    fn = text[text.index("void showMessage(") : text.index("void bootIntroBegin")]
    assert "m5os::update()" in fn
    assert "delay(holdMs)" not in fn
