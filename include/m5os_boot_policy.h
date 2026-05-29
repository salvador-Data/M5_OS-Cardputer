#pragma once

/** Pure boot-policy helpers — mirrored in tests/test_boot_policy.py */

#include <esp_system.h>

namespace m5os::boot_policy {

inline bool isCrashResetReason(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return true;
        default:
            return false;
    }
}

/** SW reset while a loaded app session is active (OTA rollback exit path). */
inline bool isSessionSwResetExit(esp_reset_reason_t reason) {
    return reason == ESP_RST_SW;
}

/** Cardputer side reset button (EN) — external reset, not cold power-on. */
inline bool isSessionExtResetExit(esp_reset_reason_t reason) {
    return reason == ESP_RST_EXT;
}

/**
 * True when M5 OS home is booting after an intentional return from a loaded app.
 * Excludes cold power-on (always M5 OS home without save prompt).
 * Does not run while the loaded app partition is executing.
 */
inline bool shouldPromptSessionReturn(bool appSessionActive, bool sessionExitPending,
                                      bool runningHome, esp_reset_reason_t resetReason) {
    if (!runningHome) return false;
    if (resetReason == ESP_RST_POWERON) return false;
    if (sessionExitPending) return true;
    if (!appSessionActive) return false;
    return isSessionSwResetExit(resetReason) || isSessionExtResetExit(resetReason) ||
           isCrashResetReason(resetReason);
}

/** Cold power-on: restore otadata and drop session NVS without save prompt. */
inline bool shouldPowerOnRestoreHome(esp_reset_reason_t resetReason) {
    return resetReason == ESP_RST_POWERON;
}

/** Side reset / EN button: restore otadata; session may prompt save afterward. */
inline bool shouldExtResetRestoreHome(esp_reset_reason_t resetReason) {
    return resetReason == ESP_RST_EXT;
}

/** Either hardware reset path that must reach M5 OS app0 (mirrored in custom bootloader). */
inline bool shouldHardwareResetRestoreHome(esp_reset_reason_t resetReason) {
    return shouldPowerOnRestoreHome(resetReason) || shouldExtResetRestoreHome(resetReason);
}

/** @deprecated alias — use shouldPowerOnRestoreHome / shouldHardwareResetRestoreHome */
inline bool shouldColdBootRestoreHome(esp_reset_reason_t resetReason) {
    return shouldPowerOnRestoreHome(resetReason);
}

/** Panic/watchdog while M5 OS or a loaded app was running — point otadata at home. */
inline bool shouldCrashResetRestoreHome(esp_reset_reason_t resetReason) {
    return isCrashResetReason(resetReason);
}

}  // namespace m5os::boot_policy
