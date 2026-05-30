"""Boot (M5Burner → app2) + keep (gateway ESC save prompt) contracts."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
FLASH_H = ROOT / "include" / "m5os_flash.h"
GATEWAY_MAIN = ROOT / "src" / "session_gateway_main.cpp"
BURNER_CPP = ROOT / "src" / "burner_install.cpp"


def test_ota_finish_marks_valid_in_otadata() -> None:
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool otaSlotWriterFinish") : flash.index("void otaSlotWriterAbort")]
    assert "markPartitionOtaState(writer.part, ESP_OTA_IMG_VALID)" in fn
    assert fn.index("esp_ota_end") < fn.index("markPartitionOtaState")


def test_run_slot_ready_helper_exported() -> None:
    assert "runSlotReadyForLaunch" in FLASH_H.read_text(encoding="utf-8")
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool runSlotReadyForLaunch") : flash.index("size_t maxOtaAppBytes")]
    assert "metadata.image_len <= expectedSize" in fn
    assert "expectedSize - metadata.image_len" in fn
    assert "validateAppImageChipTarget(slot)" in fn


def test_m5burner_fast_load_skips_recopy_when_run_slot_ready() -> None:
    launcher = APP_LAUNCHER.read_text(encoding="utf-8")
    fn = launcher[launcher.index("LaunchResult launchFromOpenFile") : launcher.index(
        "AppLauncher::AppLauncher"
    )]
    assert "runSlotReadyForLaunch(firmwareSize)" in fn
    assert "launch_run_slot_ready" in fn
    assert fn.index("runSlotReadyForLaunch") < fn.index("copySdToOta")


def test_gateway_esc_sets_sess_exit_before_home_reboot() -> None:
    gw = GATEWAY_MAIN.read_text(encoding="utf-8")
    fn = gw[gw.index("void exitToHome") : gw.index("void launchRunSlot")]
    assert "kSessionExitKey" in fn
    assert "kLaunchPendingKey" in fn
    assert fn.index("kSessionExitKey") < fn.index("esp_restart()")
    assert "true" in fn.split("kSessionExitKey")[1].split("esp_restart()")[0]
    assert "setStagedBootHandoff()" in fn
    assert fn.index("setStagedBootHandoff()") < fn.index("esp_restart()")


def test_gateway_enter_clears_sess_exit_sets_launch_pend() -> None:
    gw = GATEWAY_MAIN.read_text(encoding="utf-8")
    fn = gw[gw.index("void launchRunSlot") : gw.index("}  // namespace")]
    assert "kLaunchPendingKey" in fn
    assert "kSessionExitKey" in fn
    assert "setStagedBootHandoff()" in fn
    assert "markPartitionOtaState(run, ESP_OTA_IMG_VALID)" in fn


def test_is_running_home_falls_back_to_app0() -> None:
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool isRunningHomePartition") : flash.index("bool recoveryBootRequested")]
    assert "ESP_PARTITION_SUBTYPE_APP_OTA_0" in fn
    assert "getErr != ESP_OK" in fn


def test_burner_stream_uses_ota_finish_valid_mark() -> None:
    """Burner OTA path ends with otaSlotWriterFinish → shared VALID mark."""
    burner = BURNER_CPP.read_text(encoding="utf-8")
    assert "otaSlotWriterFinish(ctx.otaWriter" in burner


def test_m5burner_chains_gateway_when_run_slot_ready() -> None:
    launcher = APP_LAUNCHER.read_text(encoding="utf-8")
    fn = launcher[launcher.index("const burner::BurnerFlashResult flash = burner::flashAppToOta") :]
    fn = fn[: fn.index("return result;", fn.index("if (flash.ok)"))]
    assert "runSlotReadyForLaunch(plan.appSize)" in fn
    assert "burner_launch_run_ready" in fn
    assert "rebootIntoGatewaySession" in fn
    assert fn.index("runSlotReadyForLaunch") < fn.index("rebootIntoGatewaySession")
