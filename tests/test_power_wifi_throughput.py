"""Contract tests for WiFi throughput guard during bulk downloads."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POWER_CPP = ROOT / "src" / "power_manager.cpp"
POWER_H = ROOT / "include" / "power_manager.h"


def test_wifi_throughput_guard_restores_ps_mode():
    cpp = POWER_CPP.read_text(encoding="utf-8")
    hdr = POWER_H.read_text(encoding="utf-8")
    assert "class WifiThroughputGuard" in hdr
    assert "esp_wifi_get_ps" in cpp
    assert "WIFI_PS_NONE" in cpp
    assert "esp_wifi_set_ps" in cpp
