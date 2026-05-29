#include "m5os_watchdog.h"

#include "m5os_flash.h"
#include "serial_log.h"

#include <esp_idf_version.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

namespace m5os {

namespace {

bool gWatchdogReady = false;

void shutdownRestoreHome() {
    if (isLaunchPending()) return;
    restoreBootToHome();
    log::info("m5os_shutdown_home", "restore");
}

void initTaskWatchdog() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_task_wdt_config_t cfg = {
        .timeout_ms = static_cast<int>(kWatchdogTimeoutSec * 1000),
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&cfg));
#else
    ESP_ERROR_CHECK(esp_task_wdt_init(kWatchdogTimeoutSec, true));
#endif
    ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
    gWatchdogReady = true;
}

}  // namespace

void beginWatchdog() {
    if (gWatchdogReady) return;
    esp_register_shutdown_handler(shutdownRestoreHome);
    initTaskWatchdog();
    feedWatchdog();
    log::info("m5os_watchdog", String(kWatchdogTimeoutSec) + "s");
}

void feedWatchdog() {
    if (!gWatchdogReady) return;
    esp_task_wdt_reset();
}

}  // namespace m5os
