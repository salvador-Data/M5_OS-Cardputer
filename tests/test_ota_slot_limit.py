"""OTA run-slot size limit uses app2 on triple-OTA partition table."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
CONFIG_H = ROOT / "include" / "m5os_config.h"
PARTITIONS = ROOT / "partitions" / "m5os_cardputer_8MB.csv"


def test_max_ota_uses_run_slot_partition():
    text = FLASH_CPP.read_text(encoding="utf-8")
    assert "runSlotOtaPartition()" in text
    assert "stagingOtaPartition()" in text
    assert "esp_ota_get_next_update_partition" not in text


def test_config_max_matches_app2_partition():
    cfg = CONFIG_H.read_text(encoding="utf-8")
    part = PARTITIONS.read_text(encoding="utf-8")
    assert "0x390000" in cfg
    assert "kMinRunSlotPartitionBytes" in cfg
    assert "0x200000" in cfg
    assert "ota_2" in part
    assert "0x390000" in part
    assert "ota_1" in part
    assert "0x70000" in part


def test_session_gateway_uses_min_run_slot_constant():
    gw = (ROOT / "src" / "session_gateway_main.cpp").read_text(encoding="utf-8")
    assert "kMinRunSlotPartitionBytes" in gw


def test_burner_uses_run_slot_not_gateway():
    text = (ROOT / "src" / "burner_install.cpp").read_text(encoding="utf-8")
    assert "stagingOtaPartition()" in text
    assert "m5os_gateway.h" not in text
    assert "gatewayOtaPartition" not in text
