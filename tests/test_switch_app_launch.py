"""Contract tests for Load app SD path resolution and keyboard entry."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAUNCHER_CPP = ROOT / "src" / "launcher_menu.cpp"
APP_LAUNCHER_CPP = ROOT / "src" / "app_launcher.cpp"
DEVICE_H = ROOT / "include" / "M5OSDevice.h"


def test_switch_launch_resolves_package_slug_path():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::launchBinFile") : text.index(
        "LaunchResult AppLauncher::launchBinPath"
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
    helper = text[text.index("LaunchResult launchFromOpenFile") : text.index(
        "AppLauncher::AppLauncher"
    )]
    assert "needsFlashSpiffs" in helper
    assert "launch_spiffs_blocked" in helper


def test_launch_shows_starting_before_hash():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::launchBinFile") : text.index(
        "LaunchResult AppLauncher::launchBinPath"
    )]
    helper = text[text.index("LaunchResult launchFromOpenFile") : text.index(
        "AppLauncher::AppLauncher"
    )]
    assert "Starting..." in helper
    assert helper.index("Starting...") < helper.index("computeFileSha256HexWithProgress")
    assert "tryLoadCachedDigest" in helper
    assert "launch_hash_skip" in helper


def test_launch_uses_64_byte_io_chunks():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    assert "kIoChunkBytes = 64" in text
    copy = text[text.index("bool copySdToOta") : text.index("LaunchResult launchFromOpenFile")]
    assert "feedWatchdog()" in copy
    assert "written % kIoChunkBytes" in copy
    sec = (ROOT / "src" / "m5os_security.cpp").read_text(encoding="utf-8")
    hash_fn = sec[sec.index("String computeFileSha256HexWithProgress") : sec.index("bool sha256Equal")]
    assert "buffer[64]" in hash_fn
    assert "feedWatchdog()" in hash_fn
    assert "hashed % 64" in hash_fn


def test_launch_bin_path_for_explorer():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    assert "LaunchResult AppLauncher::launchBinPath" in text
    menu = LAUNCHER_CPP.read_text(encoding="utf-8")
    explorer = menu[menu.index("void LauncherMenu::showFileExplorer") : menu.index(
        "void LauncherMenu::showThemeMenu"
    )]
    assert "launchBinPath" in explorer
    assert ".bin" in explorer


def test_launch_shows_progress_before_hash():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    helper = text[text.index("LaunchResult launchFromOpenFile") : text.index(
        "AppLauncher::AppLauncher"
    )]
    assert helper.index("showFlashProgress") < helper.index("computeFileSha256HexWithProgress")
    assert "Hashing" in helper
    assert "copySdToOta" in text


def test_launch_reopens_sd_file_after_hash():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    helper = text[text.index("LaunchResult launchFromOpenFile") : text.index(
        "AppLauncher::AppLauncher"
    )]
    assert "Cannot reopen bin for copy" in helper
    assert "launch_reopen_fail" in helper
    reopen = helper[helper.index("firmware.close();") : helper.index("copySdToOta")]
    assert reopen.count("SD.open(path") >= 1
    assert "launch_magic_fail" in text


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


def test_show_message_feeds_watchdog():
    text = (ROOT / "src" / "ui_display.cpp").read_text(encoding="utf-8")
    fn = text[text.index("void showMessage(") : text.index("void bootIntroBegin")]
    assert "m5os::update()" in fn
    assert "delay(holdMs)" not in fn
