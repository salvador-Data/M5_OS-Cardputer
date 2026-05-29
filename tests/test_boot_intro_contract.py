"""Static contract tests for Hacker Planet boot intro animation."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI_CPP = ROOT / "src" / "ui_display.cpp"
BOOT_JOKES_CPP = ROOT / "src" / "boot_jokes.cpp"
BOOT_JOKES_H = ROOT / "include" / "boot_jokes.h"


def test_boot_intro_plays_hacker_planet_animation():
    text = UI_CPP.read_text(encoding="utf-8")
    assert "playHackerPlanetIntro" in text
    assert "drawGuyFawkesMask" in text
    assert "Hacker Planet" in text
    assert "by salvadorData" in text


def test_boot_intro_uses_sprite_double_buffer():
    text = UI_CPP.read_text(encoding="utf-8")
    assert "LGFX_Sprite" in text
    assert "pushSprite" in text


def test_boot_intro_optional_sd_wav_paths():
    text = UI_CPP.read_text(encoding="utf-8")
    assert "/home/default/boot/mr_roboto.wav" in text
    assert "/system/boot/mr_roboto.wav" in text
    assert "/boot/mr_roboto.wav" in text
    assert "resolveBootWavPath" in text
    assert "playRaw" in text


def test_boot_intro_synth_fallback():
    text = UI_CPP.read_text(encoding="utf-8")
    assert "playRobotoBootThemeFrame" in text
    assert "kRobotoSynthTheme" in text
    assert "allowBootChime()" in text


def test_boot_intro_called_from_begin():
    text = UI_CPP.read_text(encoding="utf-8")
    assert "void bootIntroBegin()" in text
    idx = text.index("void bootIntroBegin()")
    body = text[idx : idx + 400]
    assert "playHackerPlanetIntro();" in body
    assert "playBootChime();" not in body


def test_boot_jokes_module_exists():
    assert BOOT_JOKES_H.is_file()
    assert BOOT_JOKES_CPP.is_file()
    header = BOOT_JOKES_H.read_text(encoding="utf-8")
    source = BOOT_JOKES_CPP.read_text(encoding="utf-8")
    assert "randomJokeForBoot" in header
    assert "bootJokeCount" in header
    assert "wrapBootJoke" in header
    assert "randomJokeForBoot" in source
    assert "esp_random" in source


def test_boot_jokes_count_at_least_fifty():
    text = BOOT_JOKES_CPP.read_text(encoding="utf-8")
    jokes = re.findall(r'^\s+"[^"]+",\s*$', text, flags=re.MULTILINE)
    assert len(jokes) >= 50, f"expected >= 50 jokes, found {len(jokes)}"


def test_boot_intro_displays_random_joke():
    text = UI_CPP.read_text(encoding="utf-8")
    assert '#include "boot_jokes.h"' in text
    assert "prepareIntroJoke()" in text
    assert "gIntroJoke.line1" in text
    assert "gIntroJoke.line2" in text
