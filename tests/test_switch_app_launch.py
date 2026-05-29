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
    assert "planRequiresSdOnly" in fn
    assert "launch_spiffs_blocked" in fn
