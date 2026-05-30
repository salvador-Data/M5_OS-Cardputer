"""Session gateway on app1; Load app copies to app2 then reboots into gateway."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "partitions" / "m5os_cardputer_8MB.csv"
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
MAIN_CPP = ROOT / "src" / "main.cpp"
README = ROOT / "README.md"
PLATFORMIO = ROOT / "platformio.ini"
UPLOAD_EXTRA = ROOT / "scripts" / "upload_all_extra.py"
GATEWAY_MAIN = ROOT / "src" / "session_gateway_main.cpp"


def test_triple_slot_partition_table():
    text = PARTITIONS.read_text(encoding="utf-8")
    assert "app0" in text and "ota_0" in text
    assert "app1" in text and "ota_1" in text
    assert "app2" in text and "ota_2" in text
    assert "0x400000" in text
    assert "0x70000" in text
    assert "0x390000" in text


def test_launcher_uses_gateway_session():
    text = APP_LAUNCHER.read_text(encoding="utf-8")
    assert "launchGatewaySession()" in text
    assert "rebootIntoGatewaySession" in text
    assert "launchStagedAppSession()" not in text
    assert "m5os_gateway.h" in text
    assert "otaSlotWriterBegin" in text


def test_run_slot_resolved_in_flash_not_next_ota():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert "runSlotOtaPartition()" in flash
    assert "esp_ota_get_next_update_partition" not in flash
    assert "launchStagedAppSession() { return rebootIntoStagedApp" in flash.replace("\n", " ")
    gateway = (ROOT / "src" / "m5os_gateway.cpp").read_text(encoding="utf-8")
    assert "launchStagedAppSession()" not in gateway


def test_boot_installs_gateway_when_missing():
    main = MAIN_CPP.read_text(encoding="utf-8")
    assert "scheduleDeferredGatewayInstall()" in main
    assert "tryDeferredGatewayInstall()" in main


def test_platformio_builds_gateway_embed_and_upload_all():
    ini = PLATFORMIO.read_text(encoding="utf-8")
    assert "prebuild_gateway_embed.py" in ini
    assert "upload_all_extra.py" in ini
    assert "custom_bootloader" in ini
    assert "prebuild_bootloader.py" in ini


def test_upload_all_uses_app1_offset():
    text = UPLOAD_EXTRA.read_text(encoding="utf-8")
    assert 'APP1_OFFSET = "0x400000"' in text
    assert "0x3D0000" not in text


def test_gateway_keyboard_enter_and_esc():
    main = GATEWAY_MAIN.read_text(encoding="utf-8")
    assert "keyboardEnterJustPressed()" in main
    assert "keyboardBackEdge" in main
    assert "keyboardDrainEnter()" in main
    assert "keyboardDrainBack()" in main
    assert "m5os_keyboard.h" in main
    assert "ESC/` = M5 OS" in main


def test_keyboard_back_detects_grave_hid():
    kb = (ROOT / "include" / "m5os_keyboard.h").read_text(encoding="utf-8")
    assert "kHidGrave = 0x35" in kb
    assert "keyboardBackEdge" in kb
    held_fn = kb.split("inline bool keyboardBackHeld()")[1].split("inline bool keyboardBackEdge")[0]
    assert "kHidGrave" in held_fn
    assert "isKeyPressed('`')" in held_fn
    edge_fn = kb.split("inline bool keyboardBackEdge")[1].split("inline bool keyboardBackJustPressed")[0]
    assert "isChange()" not in edge_fn


def test_readme_documents_gateway_flow():
    readme = README.read_text(encoding="utf-8")
    assert "Load app" in readme
    assert "session gateway" in readme.lower() or "Session gateway" in readme
    assert "0x390000" in readme or "3.56" in readme or "3.5625" in readme


def test_flash_recovery_leaves_run_slot_for_load_app():
    script = (ROOT / "scripts" / "flash_recovery.ps1").read_text(encoding="utf-8")
    assert "flash_session_gateway.ps1" not in script
