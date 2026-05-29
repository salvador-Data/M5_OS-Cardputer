"""Contract tests for app session launch, exit, and save prompt."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN_CPP = ROOT / "src" / "main.cpp"
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"
FLASH_H = ROOT / "include" / "m5os_flash.h"
SESSION_CPP = ROOT / "src" / "m5os_session.cpp"
SESSION_H = ROOT / "include" / "m5os_session.h"
APP_LAUNCHER_CPP = ROOT / "src" / "app_launcher.cpp"
LAUNCHER_CPP = ROOT / "src" / "launcher_menu.cpp"
SDKCONFIG = ROOT / "sdkconfig.defaults"
README = ROOT / "README.md"


def test_app_session_nvs_keys():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert 'kAppSessionKey[] = "app_sess"' in flash
    assert 'kSessionExitKey[] = "sess_exit"' in flash
    for sym in (
        "isAppSessionActive",
        "setAppSessionActive",
        "clearAppSession",
        "isSessionExitPending",
        "setSessionExitPending",
        "isRunningHomePartition",
    ):
        assert sym in FLASH_H.read_text(encoding="utf-8")
        assert sym in flash
    assert "markStagedPartitionPendingVerify" in flash


def test_session_marks_staged_pending_before_reboot():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool rebootIntoStagedApp") : flash.index("bool nvsSetFlag")]
    assert "markStagedPartitionPendingVerify(target)" in fn
    assert "clearLaunchPending()" in fn
    assert "setAppSessionActive(true)" in flash[flash.index("void beginLaunchSession") : flash.index("void cancelLaunchSession")]


def test_recovery_splash_and_save_exit_flag():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    recovery = flash[flash.index("void tryEarlyRecoveryBoot") : flash.index("bool consumeLaunchHandoffFailure")]
    assert "showRecoverySplashWindow()" in recovery
    assert "Release for M5 OS" in flash
    assert "setSessionExitPending(true)" in recovery
    assert "stamp::recoveryPulse()" in flash


def test_session_save_prompt_wired_in_main():
    main = MAIN_CPP.read_text(encoding="utf-8")
    assert "isSessionReturnBoot()" in main
    assert "promptSaveSessionAndRestoreHome()" in main
    assert main.index("tryEarlyRecoveryBoot()") < main.index("isSessionReturnBoot()")


def test_prepare_launch_sd_before_session():
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    helper = text[text.index("LaunchResult launchFromOpenFile") : text.index("AppLauncher::AppLauncher")]
    assert "prepareLaunchSd" in helper
    assert helper.index("prepareLaunchSd") < helper.index("beginLaunchSession()")


def test_session_json_and_save_dirs():
    session = SESSION_CPP.read_text(encoding="utf-8")
    assert "kSessionJsonPath" in SESSION_H.read_text(encoding="utf-8")
    assert "ensureAppSavesDir" in session or "ensureSavesTree" in session
    assert 'promptYesNo("Exit app"' in session
    assert "Save files before exit?" in session


def test_ota_rollback_enabled():
    assert "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y" in SDKCONFIG.read_text(encoding="utf-8")


def test_launcher_documents_reset_exit():
    text = LAUNCHER_CPP.read_text(encoding="utf-8")
    assert "Reset=exit" in text or "saves on SD" in text


def test_readme_session_recovery_docs():
    readme = README.read_text(encoding="utf-8")
    assert "Save files before exit" in readme or "save prompt" in readme.lower()
    assert "reset" in readme.lower()
