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


def test_switcher_confirm_accepts_keyboard_enter():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    confirm = text[text.index("LoadConfirmChoice promptLoadAppConfirm") : text.index(
        "}  // namespace", text.index("LoadConfirmChoice promptLoadAppConfirm")
    )]
    assert "keyboardEnterJustPressed()" in confirm
    assert "keyboardTabJustPressed()" in confirm
    assert "readButtonsExtended()" in confirm
    assert "readButtons()" not in confirm


def test_explorer_confirm_accepts_keyboard_enter():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    explorer = text[text.index("void LauncherMenu::showFileExplorer") : text.index(
        "void LauncherMenu::showThemeMenu"
    )]
    assert "promptLoadAppConfirm" in explorer
    assert "launchBinPath(fullPath, opts)" in explorer
    assert "keyboardTabJustPressed()" in text


def test_keyboard_enter_helpers():
    text = DEVICE_H.read_text(encoding="utf-8")
    kb = (ROOT / "include" / "m5os_keyboard.h").read_text(encoding="utf-8")
    assert "keyboardEnterJustPressed" in text
    assert "keyboardDrainEnter" in text
    assert "keyboardDrainBack" in text
    assert "keyboardTabJustPressed" in text
    assert "keyboardDrainTab" in text
    assert "kHidEnter" in kb
    assert "keysState().enter like WiFi password" in kb
    enter_fn = kb.split("keyboardEnterJustPressed")[1].split("keyboardEnterHeld")[0]
    assert "keyboardEnterHeld()" not in enter_fn


def test_read_buttons_enter_without_is_pressed():
    text = DEVICE_H.read_text(encoding="utf-8")
    fn = text[text.index("inline Buttons readButtons()") : text.index("}  // namespace m5os")]
    assert "keyboardEnterJustPressed()" in fn
    assert "if (!M5Cardputer.Keyboard.isChange()) return b;" in fn
    assert "if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed())" not in fn


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


def test_launch_uses_4k_io_chunks():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    assert "kIoChunkBytes = 4096" in text
    copy = text[text.index("bool copySdToOta") : text.index("LaunchResult launchFromOpenFile")]
    assert "feedWatchdog()" in copy
    assert "written % kIoChunkBytes" in copy
    sec_h = (ROOT / "include" / "m5os_security.h").read_text(encoding="utf-8")
    assert "kSha256IoChunkBytes = 4096" in sec_h
    sec = (ROOT / "src" / "m5os_security.cpp").read_text(encoding="utf-8")
    hash_fn = sec[sec.index("String computeFileSha256HexWithProgress") : sec.index("bool sha256Equal")]
    assert "buffer[kSha256IoChunkBytes]" in hash_fn
    assert "feedWatchdog()" in hash_fn
    assert "kSha256ProgressBytes" in hash_fn
    sdk = (ROOT / "sdkconfig.defaults").read_text(encoding="utf-8")
    assert "CONFIG_MBEDTLS_HARDWARE_SHA=y" in sdk
    catalog = (ROOT / "src" / "firmware_catalog.cpp").read_text(encoding="utf-8")
    assert "kSha256IoChunkBytes" in catalog


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


def test_launch_surfaces_errors_on_screen():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    helper = text[text.index("LaunchResult launchFromOpenFile") : text.index(
        "AppLauncher::AppLauncher"
    )]
    assert "surfaceLaunchFailure" in helper
    assert helper.count("surfaceLaunchFailure") >= 5
    assert "Cannot open bin" in helper


def test_launch_phases_use_load_app_label():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    helper = text[text.index("LaunchResult launchFromOpenFile") : text.index(
        "AppLauncher::AppLauncher"
    )]
    assert '"Load app"' in text[text.index("void paintLoadAppPhase") : text.index("bool canSkipFlashToCachedOta")]
    assert "Copy to run slot" in text
    assert "Rebooting" in helper
    assert "Already loaded" in helper


def test_launch_begins_session_before_copy():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    helper = text[text.index("LaunchResult launchFromOpenFile") : text.index(
        "AppLauncher::AppLauncher"
    )]
    assert "beginLaunchSession()" in helper
    assert helper.index("beginLaunchSession()") < helper.index("copySdToOta")
    assert "launchGatewaySession()" in helper
    assert "cancelLaunchSession()" in helper


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


def test_fast_load_skip_hash_option():
    launcher_h = (ROOT / "include" / "app_launcher.h").read_text(encoding="utf-8")
    assert "LaunchOptions" in launcher_h
    assert "skipHash" in launcher_h
    assert "skipHash = false" in launcher_h

    app = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = app[app.index("LaunchResult launchFromOpenFile") : app.index("AppLauncher::AppLauncher")]
    assert "userSkipHash" in fn
    assert "launch_fast_load" in fn
    assert "launch_checksum_user_skip" in fn
    assert fn.index("if (userSkipHash)") < fn.index("tryLoadCachedDigest")
    assert "if (!userSkipHash && canSkipFlashToCachedOta" in fn
    assert "if (!userSkipHash) storeLaunchCache" in fn
    assert "launch_magic_fail" in app


def test_load_confirm_enter_tab_prompt():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    assert "promptLoadAppConfirm" in text
    assert "Enter verify hash" in text
    assert "Tab fast load" in text
    assert "keyboardTabJustPressed" in text
    assert "LaunchOptions opts" in text
    assert "launchBinFile(pkg.binFile, opts)" in text
    assert "launchBinPath(fullPath, opts)" in text

    device_h = DEVICE_H.read_text(encoding="utf-8")
    assert "keyboardTabJustPressed" in device_h
    assert "keyboardDrainTab" in device_h


def test_fast_load_still_copies_with_progress():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult launchFromOpenFile") : text.index("AppLauncher::AppLauncher")]
    assert "copySdToOta" in fn
    assert fn.index("userSkipHash") < fn.index("copySdToOta")
    copy = text[text.index("bool copySdToOta") : text.index("LaunchResult launchFromOpenFile")]
    assert "0xE9" in copy
    assert "paintLaunchProgress" in copy
