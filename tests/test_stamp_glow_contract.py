"""Contract tests for stamp glow LED and theme presets."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAMP_CPP = ROOT / "src" / "stamp_glow.cpp"
STAMP_H = ROOT / "include" / "stamp_glow.h"
CONFIG_H = ROOT / "include" / "m5os_config.h"
UI_CPP = ROOT / "src" / "ui_display.cpp"
SETTINGS_CPP = ROOT / "src" / "m5os_settings.cpp"
MAIN_CPP = ROOT / "src" / "main.cpp"
DEVICE_H = ROOT / "include" / "M5OSDevice.h"


def test_stamp_led_gpio_cardputer():
    text = CONFIG_H.read_text(encoding="utf-8")
    assert "kStampLedGpio" in text
    assert "= 21" in text.split("kStampLedGpio")[1].split("\n")[0]


def test_stamp_glow_uses_m5_led_and_breathe():
    text = STAMP_CPP.read_text(encoding="utf-8")
    assert "M5.Led" in text
    assert "breatheLevel" in text or "sinf" in text
    assert "ui::theme()" in text
    assert "power::isSaving()" in text


def test_stamp_wired_in_main_and_update():
    assert "stamp::begin()" in MAIN_CPP.read_text(encoding="utf-8")
    assert "stamp::tick()" in DEVICE_H.read_text(encoding="utf-8")


def test_default_theme_hacker_green():
    config = CONFIG_H.read_text(encoding="utf-8")
    assert "kDefaultThemePreset = 1" in config
    settings = SETTINGS_CPP.read_text(encoding="utf-8")
    assert "kDefaultThemePreset" in settings
    ui = UI_CPP.read_text(encoding="utf-8")
    assert "kDefaultThemePreset" in ui
    menu = (ROOT / "src" / "launcher_menu.cpp").read_text(encoding="utf-8")
    assert "Matrix Neon" in menu
    assert "Amber Terminal" in menu


def test_theme_preset_count_six():
    assert "kThemePresetCount = 6" in CONFIG_H.read_text(encoding="utf-8")
    fn = UI_CPP.read_text(encoding="utf-8")
    block = fn[fn.index("void setThemePreset") : fn.index("void drawHeader")]
    assert "case 4:" in block
    assert "case 5:" in block
