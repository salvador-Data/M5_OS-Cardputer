"""Optional session gateway firmware (not required for Load app / M5Burner)."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GATEWAY_SHARED = ROOT / "include" / "m5os_gateway_shared.h"
GATEWAY_MAIN = ROOT / "src" / "session_gateway_main.cpp"


def test_gateway_esc_ux_timings_in_optional_firmware():
    shared = GATEWAY_SHARED.read_text(encoding="utf-8")
    assert "kAutoLaunchMs = 2000" in shared
    assert "kEscHoldMs = 1000" in shared
    main = GATEWAY_MAIN.read_text(encoding="utf-8")
    assert "ESC/` = M5 OS" in main
    assert "Auto-launch" in main
