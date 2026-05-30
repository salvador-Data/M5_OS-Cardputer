"""Load app uses direct OTA reboot (c27be49); gateway code optional, not on load path."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "partitions" / "m5os_cardputer_8MB.csv"
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
MAIN_CPP = ROOT / "src" / "main.cpp"
README = ROOT / "README.md"
PLATFORMIO = ROOT / "platformio.ini"


def test_dual_slot_partition_table():
    text = PARTITIONS.read_text(encoding="utf-8")
    assert "app0" in text and "ota_0" in text
    assert "app1" in text and "ota_1" in text
    assert "0x400000" in text
    assert "ota_2" not in text
    assert "0x70000" not in text


def test_launcher_uses_direct_staged_session():
    text = APP_LAUNCHER.read_text(encoding="utf-8")
    assert "launchStagedAppSession()" in text
    assert "resolveLaunchBootPartition()" in text
    assert "launchGatewaySession()" not in text
    assert "flashEmbeddedGatewayIfNeeded()" not in text
    assert "m5os_gateway.h" not in text
    assert "otaSlotWriterBegin" in text


def test_staged_launch_in_flash_not_gateway():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert "launchStagedAppSession() { return rebootIntoStagedApp" in flash.replace("\n", " ")
    gateway = (ROOT / "src" / "m5os_gateway.cpp").read_text(encoding="utf-8")
    assert "launchStagedAppSession()" not in gateway


def test_boot_does_not_install_gateway():
    main = MAIN_CPP.read_text(encoding="utf-8")
    assert "gatewayPartitionReady" not in main
    assert "Installing session gateway" not in main


def test_platformio_plain_m5_os_build():
    ini = PLATFORMIO.read_text(encoding="utf-8")
    assert "prebuild_gateway_embed.py" not in ini
    assert "upload_all_extra.py" not in ini
    assert "custom_bootloader" in ini
    assert "prebuild_bootloader.py" in ini


def test_readme_documents_load_app_reboot():
    readme = README.read_text(encoding="utf-8")
    assert "Load app" in readme
    assert "4 MB" in readme or "4MB" in readme or "0x400000" in readme


def test_flash_recovery_leaves_app1_empty_for_load_app():
    """Dual-OTA recovery must not flash legacy session gateway over the app1 run slot."""
    script = (ROOT / "scripts" / "flash_recovery.ps1").read_text(encoding="utf-8")
    assert "flash_session_gateway.ps1" not in script
