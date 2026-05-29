"""Contract tests for TWDT freeze recovery and crash reboot."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN_CPP = ROOT / "src" / "main.cpp"
DEVICE_H = ROOT / "include" / "M5OSDevice.h"
WATCHDOG_CPP = ROOT / "src" / "m5os_watchdog.cpp"
WATCHDOG_H = ROOT / "include" / "m5os_watchdog.h"
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
FLASH_H = ROOT / "include" / "m5os_flash.h"
CONFIG_H = ROOT / "include" / "m5os_config.h"
PLATFORMIO = ROOT / "platformio.ini"
SDKCONFIG = ROOT / "sdkconfig.defaults"


def test_watchdog_timeout_constant():
    text = CONFIG_H.read_text(encoding="utf-8")
    assert "kWatchdogTimeoutSec" in text
    assert "30" in text.split("kWatchdogTimeoutSec")[1].split("\n")[0]


def test_watchdog_wired_in_main_and_update():
    main = MAIN_CPP.read_text(encoding="utf-8")
    assert "beginWatchdog()" in main
    assert "applyCrashResetHomeRestore()" in main
    device = DEVICE_H.read_text(encoding="utf-8")
    assert "feedWatchdog()" in device


def test_watchdog_uses_twdt_and_shutdown_restore():
    text = WATCHDOG_CPP.read_text(encoding="utf-8")
    assert "esp_task_wdt" in text
    assert "esp_register_shutdown_handler" in text
    assert "restoreBootToHome()" in text
    assert "isLaunchPending()" in text
    assert "trigger_panic" in text or "esp_task_wdt_init(kWatchdogTimeoutSec" in text


def test_launch_pending_skips_shutdown_restore_on_load_app():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    launch = flash[flash.index("bool launchStagedAppSession") : flash.index("void tryEarlyRecoveryBoot")]
    assert "setLaunchPending(true)" in launch
    assert "esp_restart()" in launch


def test_crash_reset_restores_home():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("void applyCrashResetHomeRestore") : flash.index("void setLaunchPending")]
    for reason in ("ESP_RST_PANIC", "ESP_RST_TASK_WDT", "ESP_RST_INT_WDT", "ESP_RST_WDT"):
        assert reason in fn
    assert "restoreBootToHome()" in fn


def test_panic_reboot_enabled_in_sdkconfig():
    assert "CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT" in SDKCONFIG.read_text(encoding="utf-8")
    assert "sdkconfig.defaults" in PLATFORMIO.read_text(encoding="utf-8")


def test_watchdog_symbols_declared():
    header = WATCHDOG_H.read_text(encoding="utf-8")
    assert "beginWatchdog" in header
    assert "feedWatchdog" in header
    for sym in ("applyCrashResetHomeRestore", "setLaunchPending", "isLaunchPending", "clearLaunchPending"):
        assert sym in FLASH_H.read_text(encoding="utf-8")
