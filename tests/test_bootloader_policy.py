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


def test_bootloader_recovery_gpio():
    text = BOOT_C.read_text(encoding="utf-8")
    assert "bootloader_common_check_long_hold_gpio_level" in text
    assert "M5OS_RECOVERY_GPIO 0" in text


def test_platformio_bootloader_build_wired():
    text = PLATFORMIO.read_text(encoding="utf-8")
    boot_ini = (ROOT / "platformio_bootloader.ini").read_text(encoding="utf-8")
    assert "m5stack-cardputer-bootloader" in boot_ini
    assert "prebuild_bootloader.py" in text
    assert "board_build.arduino.custom_bootloader" in text
