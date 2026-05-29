"""Contract tests for Switch app SD path resolution and ESC entry."""

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


def test_keyboard_drain_back_helper():
    text = DEVICE_H.read_text(encoding="utf-8")
    assert "keyboardDrainBack" in text
