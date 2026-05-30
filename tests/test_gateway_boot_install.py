"""Session gateway auto-install on boot and recovery menu."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
MENU = ROOT / "src" / "launcher_menu.cpp"
GATEWAY_SHARED = ROOT / "include" / "m5os_gateway_shared.h"
GATEWAY_MAIN = ROOT / "src" / "session_gateway_main.cpp"


def test_boot_installs_gateway_when_missing():
    main = MAIN.read_text(encoding="utf-8")
    assert "gatewayPartitionReady()" in main
    assert "flashEmbeddedGatewayIfNeeded()" in main
    assert main.index("saveHomeAppPartition()") < main.index("gatewayPartitionReady()")


def test_recovery_menu_offers_gateway_install():
    menu = MENU.read_text(encoding="utf-8")
    fn = menu[menu.index("void LauncherMenu::showBurnerBridge") : menu.index("void LauncherMenu::showStorageCleanup")]
    assert "flashEmbeddedGatewayIfNeeded()" in fn
    assert "Session gateway: missing" in fn


def test_gateway_esc_ux_timings():
    shared = GATEWAY_SHARED.read_text(encoding="utf-8")
    assert "kAutoLaunchMs = 6000" in shared
    assert "kEscHoldMs = 1000" in shared
    main = GATEWAY_MAIN.read_text(encoding="utf-8")
    assert "ESC/` = M5 OS" in main
    assert "Auto-launch in" in main
    assert "keyboardBackHeld()" in main
