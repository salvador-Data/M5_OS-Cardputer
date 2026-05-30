"""M5Burner OTA stream targets app1 staging slot; chains Load app without gateway."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BURNER_CPP = ROOT / "src" / "burner_install.cpp"
APP_LAUNCHER_CPP = ROOT / "src" / "app_launcher.cpp"
UI_CPP = ROOT / "src" / "ui_display.cpp"


def test_burner_stream_uses_staging_slot_not_update_api() -> None:
    text = BURNER_CPP.read_text(encoding="utf-8")
    assert "stagingOtaPartition()" in text
    assert "Update.begin" not in text
    fn = text[text.index("bool streamRangeToOtaOnce") : text.index("bool streamRangeToOta(")]
    assert "esp_partition_write" in text
    assert "markPartitionOtaState(ctx.staged" in fn
    assert "m5os_gateway.h" not in text


def test_flash_burner_package_chains_load_app() -> None:
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::flashBurnerPackage") : text.index(
        "LaunchResult AppLauncher::launchByPackageName"
    )]
    assert "needsSpiffs" in fn
    assert "launchBinFile(safeBin, opts)" in fn
    assert "opts.skipHash = true" in fn
    assert "launchGatewaySession" not in fn


def test_select_from_list_accepts_enter() -> None:
    text = UI_CPP.read_text(encoding="utf-8")
    fn = text[text.index("int selectFromList") : text.index("void drawHelpOverlay")]
    assert "keyboardEnterJustPressed()" in fn
