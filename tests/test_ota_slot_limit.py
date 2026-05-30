"""OTA run-slot size limit uses staging partition (app1 on 2-slot table)."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
CONFIG_H = ROOT / "include" / "m5os_config.h"
PARTITIONS = ROOT / "partitions" / "m5os_cardputer_8MB.csv"


def test_max_ota_uses_staging_partition():
    text = FLASH_CPP.read_text(encoding="utf-8")
    assert "esp_ota_get_next_update_partition" in text
    assert "stagingOtaPartition()" in text
    assert "runSlotPartitionForLimit" not in text


def test_config_max_matches_app1_partition():
    cfg = CONFIG_H.read_text(encoding="utf-8")
    part = PARTITIONS.read_text(encoding="utf-8")
    assert "0x400000" in cfg
    assert "ota_1" in part
    assert "0x400000" in part
    assert "ota_2" not in part


def test_burner_uses_staging_not_gateway():
    text = (ROOT / "src" / "burner_install.cpp").read_text(encoding="utf-8")
    assert "stagingOtaPartition()" in text
    assert "m5os_gateway.h" not in text
    assert "gatewayOtaPartition" not in text
