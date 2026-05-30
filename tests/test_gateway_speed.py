"""Gateway speed and deferred install contracts."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHARED = ROOT / "include" / "m5os_gateway_shared.h"
GATEWAY_CPP = ROOT / "src" / "m5os_gateway.cpp"
GATEWAY_H = ROOT / "include" / "m5os_gateway.h"
MAIN_CPP = ROOT / "src" / "main.cpp"
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
GATEWAY_MAIN = ROOT / "src" / "session_gateway_main.cpp"


def test_gateway_auto_launch_2s():
    shared = SHARED.read_text(encoding="utf-8")
    assert "kAutoLaunchMs = 2000" in shared
    assert "kMinGatewayUiMs = 0" in shared


def test_gateway_ready_skips_flash():
    gw = GATEWAY_CPP.read_text(encoding="utf-8")
    assert "gw_ready_skip" in gw
    fn = gw[gw.index("bool flashEmbeddedGatewayIfNeeded") : gw.index("bool ensureGatewayInstalled")]
    assert fn.index("gatewayPartitionReady()") < fn.index("SD.exists")


def test_boot_defers_gateway_install():
    main = MAIN_CPP.read_text(encoding="utf-8")
    assert "scheduleDeferredGatewayInstall()" in main
    assert "tryDeferredGatewayInstall()" in main
    setup = main[main.index("void setup()") : main.index("void loop()")]
    assert "flashEmbeddedGatewayIfNeeded()" not in setup


def test_load_app_shows_session_shell_progress():
    launcher = APP_LAUNCHER.read_text(encoding="utf-8")
    fn = launcher[launcher.index("bool rebootIntoGatewaySession") : launcher.index("void paintLaunchProgress")]
    assert "Session shell" in fn
    assert "ensureGatewayInstalled" in fn
    assert "gatewayPartitionReady()" in fn
    assert "900" not in fn


def test_launch_gateway_no_blocking_flash():
    gw = GATEWAY_CPP.read_text(encoding="utf-8")
    fn = gw[gw.index("bool launchGatewaySession()") : gw.index("bool gatewayExitToHome()")]
    assert "flashEmbeddedGatewayIfNeeded" not in fn
    assert "gatewayPartitionReady()" in fn
    assert "delay(100)" not in fn


def test_gateway_shell_shows_immediately():
    main = GATEWAY_MAIN.read_text(encoding="utf-8")
    setup = main[main.index("void setup()") :]
    assert "Ready — Enter or wait" in setup
    assert "kMinGatewayUiMs" in setup
