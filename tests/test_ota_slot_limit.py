"""OTA run-slot size limit must use app2, not gateway-sized app1."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
CONFIG_H = ROOT / "include" / "m5os_config.h"
PARTITIONS = ROOT / "partitions" / "m5os_cardputer_8MB.csv"


def test_max_ota_uses_run_slot_not_next_update_partition():
    text = FLASH_CPP.read_text(encoding="utf-8")
    assert "esp_ota_get_next_update_partition" not in text
    assert "runSlotPartitionForLimit" in text
    assert "ESP_PARTITION_SUBTYPE_APP_OTA_2" in text


def test_config_max_matches_app2_partition():
    cfg = CONFIG_H.read_text(encoding="utf-8")
    part = PARTITIONS.read_text(encoding="utf-8")
    assert "0x3C0000" in cfg
    assert "ota_2" in part
    assert "0x70000" in part  # gateway app1 — must not be the load limit
    assert "kMinRunSlotPartitionBytes" in cfg


def test_run_slot_never_falls_back_to_gateway_app1():
    text = FLASH_CPP.read_text(encoding="utf-8")
    fn = text[text.index("runSlotPartitionForLimit") : text.index("size_t maxOtaAppBytes")]
    assert "kMinRunSlotPartitionBytes" in fn
    gateway = (ROOT / "src" / "m5os_gateway.cpp").read_text(encoding="utf-8")
    run_fn = gateway[gateway.index("runSlotOtaPartition") : gateway.index("const esp_partition_t* stagingOtaPartition")]
    assert "kMinRunSlotPartitionBytes" in run_fn
