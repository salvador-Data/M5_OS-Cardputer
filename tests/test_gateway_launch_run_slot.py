"""Contract tests for session gateway run-slot launch handoff."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GATEWAY_MAIN = ROOT / "src" / "session_gateway_main.cpp"
GATEWAY_SHARED = ROOT / "include" / "m5os_gateway_shared.h"
OTADATA_CPP = ROOT / "src" / "m5os_otadata.cpp"
PLATFORMIO = ROOT / "platformio.ini"


def test_gateway_launch_marks_valid_before_set_boot():
    text = GATEWAY_MAIN.read_text(encoding="utf-8")
    fn = text[text.index("void launchRunSlot") : text.index("void setup()")]
    assert "markPartitionOtaState(run, ESP_OTA_IMG_VALID)" in fn
    assert fn.index("markPartitionOtaState") < fn.index("esp_ota_set_boot_partition(run)")
    assert "setStagedBootHandoff()" in fn
    assert "kLaunchPendingKey" in fn


def test_gateway_build_links_shared_otadata():
    ini = PLATFORMIO.read_text(encoding="utf-8")
    assert "m5os_otadata.cpp" in ini
    assert OTADATA_CPP.is_file()


def test_gateway_nvs_keys_shared_with_m5_os():
    shared = GATEWAY_SHARED.read_text(encoding="utf-8")
    flash = (ROOT / "src" / "m5os_flash.cpp").read_text(encoding="utf-8")
    assert 'kLaunchPendingKey[] = "launch_pend"' in shared
    assert 'kLaunchPendingKey[] = "launch_pend"' in flash
