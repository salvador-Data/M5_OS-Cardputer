"""Contract tests for session gateway launch path."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "partitions" / "m5os_cardputer_8MB.csv"
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
GATEWAY_CPP = ROOT / "src" / "m5os_gateway.cpp"
GATEWAY_MAIN = ROOT / "src" / "session_gateway_main.cpp"
GATEWAY_H = ROOT / "include" / "m5os_gateway.h"
README = ROOT / "README.md"
PLATFORMIO = ROOT / "platformio.ini"


def test_three_slot_partition_table():
    text = PARTITIONS.read_text(encoding="utf-8")
    assert "app1" in text and "ota_1" in text
    assert "app2" in text and "ota_2" in text
    assert "0x10000" in text
    assert "0x3C0000" in text
    assert "0x70000" in text


def test_gateway_nvs_and_paths():
    shared = (ROOT / "include" / "m5os_gateway_shared.h").read_text(encoding="utf-8")
    assert 'kGatewayActiveKey[] = "gw_active"' in shared
    assert ".staging/run_app.bin" in shared


def test_launcher_uses_gateway_session():
    text = APP_LAUNCHER.read_text(encoding="utf-8")
    assert "launchGatewaySession()" in text
    assert "runSlotOtaPartition()" in text
    assert "flashEmbeddedGatewayIfNeeded()" in text
    assert "esp_partition_write(runSlot" in text
    assert "launchStagedAppSession()" not in text


def test_gateway_firmware_ui():
    main = GATEWAY_MAIN.read_text(encoding="utf-8")
    assert "ESC = M5 OS" in main
    assert "Enter = launch app" in main
    assert "kMinGatewayUiMs" in main
    assert "sess_exit" in main or '"sess_exit"' in main


def test_gateway_helpers_in_tree():
    for sym in (
        "gatewayOtaPartition",
        "runSlotOtaPartition",
        "launchGatewaySession",
        "flashEmbeddedGatewayIfNeeded",
    ):
        assert sym in GATEWAY_H.read_text(encoding="utf-8")
        assert sym in GATEWAY_CPP.read_text(encoding="utf-8")


def test_staged_launch_delegates_to_gateway():
    gateway = GATEWAY_CPP.read_text(encoding="utf-8")
    assert "launchStagedAppSession() { return launchGatewaySession();" in gateway.replace("\n", " ")
    assert "setStagedBootHandoff()" in gateway


def test_gateway_rtc_handoff_before_run_slot():
    main = GATEWAY_MAIN.read_text(encoding="utf-8")
    launch = main[main.index("void launchRunSlot") : main.index("}  // namespace")]
    assert "setStagedBootHandoff()" in launch
    exit_fn = main[main.index("void exitToHome") : main.index("void launchRunSlot")]
    assert "setStagedBootHandoff()" not in exit_fn


def test_platformio_gateway_env():
    ini = PLATFORMIO.read_text(encoding="utf-8")
    assert "m5os-session-gateway" in ini
    assert "session_gateway_main.cpp" in ini


def test_readme_documents_esc_scope():
    readme = README.read_text(encoding="utf-8")
    assert "session gateway" in readme.lower() or "Session gateway" in readme
    assert "ESC" in readme
