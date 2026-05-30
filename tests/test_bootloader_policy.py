"""Contract tests for custom bootloader home-on-hardware-reset policy."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOOT_C = ROOT / "bootloader_components" / "main" / "bootloader_start.c"
BOOT_CMAKE = ROOT / "bootloader_components" / "main" / "CMakeLists.txt"
PLATFORMIO = ROOT / "platformio.ini"


def test_bootloader_component_layout():
    assert BOOT_C.is_file()
    assert BOOT_CMAKE.is_file()
    assert "bootloader_start.c" in BOOT_CMAKE.read_text(encoding="utf-8")


def test_bootloader_sw_reset_uses_otadata():
    text = BOOT_C.read_text(encoding="utf-8")
    assert "bootloader_utility_get_selected_boot_partition" in text
    select = text[text.index("static int select_partition_number") : text.index("struct _reent")]
    assert "m5os_should_boot_home" in select
    assert select.index("m5os_should_boot_home") < select.index(
        "bootloader_utility_get_selected_boot_partition"
    )


def test_bootloader_sw_reset_without_magic_honors_otadata():
    """Loaded apps often esp_restart() during init; must not force app0 on every SW reset."""
    text = BOOT_C.read_text(encoding="utf-8")
    assert "m5os_reset_forces_home_boot" in text
    home_fn = text[text.index("static bool m5os_should_boot_home") : text.index("void __attribute__((noreturn)) call_start_cpu0")]
    assert "m5os_reset_forces_home_boot()" in home_fn
    forces = text[text.index("m5os_reset_forces_home_boot") : text.index("static bool m5os_should_boot_home")]
    assert "RESET_REASON_CHIP_POWER_ON" in forces
    assert "RESET_REASON_CPU0_SW" in forces
    assert "RESET_REASON_CHIP_BROWN_OUT" not in forces


def test_bootloader_recovery_gpio():
    text = BOOT_C.read_text(encoding="utf-8")
    assert "bootloader_common_check_long_hold_gpio_level" in text
    assert "M5OS_RECOVERY_GPIO 0" in text


def test_platformio_uses_stock_bootloader_for_load_app():
    text = PLATFORMIO.read_text(encoding="utf-8")
    assert "custom_bootloader" not in text
    assert "prebuild_bootloader.py" not in text
