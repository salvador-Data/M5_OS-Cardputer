"""Static contract tests for WiFi password UI (Cardputer keyboard behavior)."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI_CPP = ROOT / "src" / "ui_display.cpp"
WIFI_CPP = ROOT / "src" / "wifi_manager.cpp"
UI_H = ROOT / "include" / "ui_display.h"


def test_password_prompt_result_enum():
    text = UI_H.read_text(encoding="utf-8")
    assert "enum class PasswordPromptResult" in text
    assert "ChangeNetwork" in text
    assert "Confirmed" in text
    assert "Cancelled" in text


def test_password_backspace_and_tab_in_ui():
    text = UI_CPP.read_text(encoding="utf-8")
    assert "keyboardWantsErase" in text
    assert "erasePasswordChar" in text
    assert "status.tab" in text
    assert "PasswordPromptResult::ChangeNetwork" in text
    assert "Del erase  Tab AP" in text


def test_password_does_not_use_readbuttons_ok():
    """Space-as-OK in readButtons() must not submit password."""
    text = UI_CPP.read_text(encoding="utf-8")
    start = text.index("PasswordPromptResult promptPassword")
    body = text[start : start + 3500]
    assert "readButtons" not in body
    assert "status.enter" in body


def test_wifi_change_network_flow():
    text = WIFI_CPP.read_text(encoding="utf-8")
    assert "Change network" in text
    assert "PasswordPromptResult::ChangeNetwork" in text
    assert "wifiPickNetwork" in text
    assert "wifiAttemptConnect" in text


def test_wifi_saves_before_clearing_password_buffer():
    text = WIFI_CPP.read_text(encoding="utf-8")
    assert "settings::saveWifi" in text
    idx = text.index("wifiSaveCredentials")
    chunk = text[idx : idx + 400]
    assert "passOut[0] = '\\0'" in chunk
    assert chunk.index("saveWifi") < chunk.index("passOut[0]")
