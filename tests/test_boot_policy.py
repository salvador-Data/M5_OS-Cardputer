"""Unit tests for m5os_boot_policy.h decision logic (mirrored in Python)."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POLICY_H = ROOT / "include" / "m5os_boot_policy.h"
SESSION_CPP = ROOT / "src" / "m5os_session.cpp"
FLASH_CPP = ROOT / "src" / "m5os_flash.cpp"

# esp_reset_reason_t values used by ESP-IDF (stable across IDF 4.x/5.x)
ESP_RST_POWERON = 1
ESP_RST_EXT = 2
ESP_RST_SW = 3
ESP_RST_PANIC = 4
ESP_RST_INT_WDT = 5
ESP_RST_TASK_WDT = 6
ESP_RST_WDT = 7


def is_crash_reset_reason(reason: int) -> bool:
    return reason in (ESP_RST_PANIC, ESP_RST_INT_WDT, ESP_RST_TASK_WDT, ESP_RST_WDT)


def is_session_sw_reset_exit(reason: int) -> bool:
    return reason == ESP_RST_SW


def is_session_ext_reset_exit(reason: int) -> bool:
    return reason == ESP_RST_EXT


def should_prompt_session_return(
    app_session_active: bool,
    session_exit_pending: bool,
    running_home: bool,
    reset_reason: int,
) -> bool:
    if not running_home:
        return False
    if reset_reason == ESP_RST_POWERON:
        return False
    if session_exit_pending:
        return True
    if not app_session_active:
        return False
    return is_session_sw_reset_exit(reset_reason) or is_session_ext_reset_exit(reset_reason) or is_crash_reset_reason(
        reset_reason
    )


def should_power_on_restore_home(reset_reason: int) -> bool:
    return reset_reason == ESP_RST_POWERON


def should_ext_reset_restore_home(reset_reason: int) -> bool:
    return reset_reason == ESP_RST_EXT


def should_hardware_reset_restore_home(reset_reason: int) -> bool:
    return should_power_on_restore_home(reset_reason) or should_ext_reset_restore_home(reset_reason)


def should_cold_boot_restore_home(reset_reason: int) -> bool:
    return should_power_on_restore_home(reset_reason)


def should_crash_reset_restore_home(reset_reason: int) -> bool:
    return is_crash_reset_reason(reset_reason)


def test_policy_header_exists_and_wired():
    text = POLICY_H.read_text(encoding="utf-8")
    assert "shouldPromptSessionReturn" in text
    assert "shouldColdBootRestoreHome" in text
    assert "shouldCrashResetRestoreHome" in text
    session = SESSION_CPP.read_text(encoding="utf-8")
    assert "m5os_boot_policy.h" in session
    assert "shouldPromptSessionReturn" in session
    flash = FLASH_CPP.read_text(encoding="utf-8")
    assert "m5os_boot_policy.h" in flash
    assert "shouldHardwareResetRestoreHome" in flash
    assert "shouldCrashResetRestoreHome" in flash


def test_session_launch_marks_valid_not_pending_verify():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("bool rebootIntoStagedApp") : flash.index("bool nvsSetFlag")]
    assert "ESP_OTA_IMG_VALID" in fn
    assert "markStagedPartitionPendingVerify(target)" not in fn
    assert "markPartitionOtaState" in fn


def test_no_pending_verify_on_session_launch():
    flash = FLASH_CPP.read_text(encoding="utf-8")
    reboot = flash[flash.index("bool rebootIntoStagedApp") : flash.index("bool nvsSetFlag")]
    assert "ESP_OTA_IMG_PENDING_VERIFY" not in reboot


def test_cold_power_never_prompts():
    assert not should_prompt_session_return(True, False, True, ESP_RST_POWERON)
    assert not should_prompt_session_return(True, True, True, ESP_RST_POWERON)


def test_sw_reset_with_active_session_on_home_prompts():
    assert should_prompt_session_return(True, False, True, ESP_RST_SW)


def test_sw_reset_without_session_no_prompt():
    assert not should_prompt_session_return(False, False, True, ESP_RST_SW)


def test_session_exit_pending_prompts_on_sw_reset():
    assert should_prompt_session_return(False, True, True, ESP_RST_SW)


def test_not_home_never_prompts():
    assert not should_prompt_session_return(True, False, False, ESP_RST_SW)


def test_crash_on_home_with_session_prompts():
    for reason in (ESP_RST_PANIC, ESP_RST_TASK_WDT, ESP_RST_INT_WDT, ESP_RST_WDT):
        assert should_prompt_session_return(True, False, True, reason)


def test_ext_reset_with_active_session_on_home_prompts():
    assert should_prompt_session_return(True, False, True, ESP_RST_EXT)


def test_ext_reset_without_session_no_prompt():
    assert not should_prompt_session_return(False, False, True, ESP_RST_EXT)


def test_hardware_reset_restore_poweron_and_ext():
    assert should_hardware_reset_restore_home(ESP_RST_POWERON)
    assert should_hardware_reset_restore_home(ESP_RST_EXT)
    assert not should_hardware_reset_restore_home(ESP_RST_SW)


def test_power_on_clears_session_ext_keeps_for_prompt():
    policy = POLICY_H.read_text(encoding="utf-8")
    assert "shouldPowerOnRestoreHome" in policy
    assert "shouldExtResetRestoreHome" in policy
    assert "shouldHardwareResetRestoreHome" in policy
    flash = FLASH_CPP.read_text(encoding="utf-8")
    fn = flash[flash.index("void applyColdBootHomeRestore") : flash.index("void applyCrashResetHomeRestore")]
    assert "shouldHardwareResetRestoreHome" in fn
    assert "shouldPowerOnRestoreHome" in fn
    assert 'log::info("m5os_hw_reset_home", "ext")' in fn


def test_custom_bootloader_forces_app0_on_hw_reset():
    boot_c = ROOT / "bootloader_components" / "main" / "bootloader_start.c"
    assert boot_c.is_file()
    text = boot_c.read_text(encoding="utf-8")
    assert "M5OS_RTC_BOOT_STAGED_MAGIC" in text
    assert "RTC_CNTL_STORE0_REG" in text
    assert "M5OS_RECOVERY_GPIO" in text


def test_cold_boot_restore_only_on_poweron():
    assert should_cold_boot_restore_home(ESP_RST_POWERON)
    assert not should_cold_boot_restore_home(ESP_RST_SW)


def test_crash_restore_reasons():
    assert should_crash_reset_restore_home(ESP_RST_PANIC)
    assert not should_crash_reset_restore_home(ESP_RST_SW)
