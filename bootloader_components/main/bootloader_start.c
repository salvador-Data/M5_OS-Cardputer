/*
 * M5 OS Cardputer — second-stage bootloader override.
 *
 * Loaded apps run from app1 (ota_1) with otadata pointing there after Load app.
 * Recovery logic lives in app0; without forcing app0 on hardware reset, users stay
 * in foreign firmware indefinitely.
 *
 * SW reset with RTC handoff magic (set by M5 OS before Load app reboot) boots app1.
 * Any other reset (power-on, side reset, foreign-app esp_restart) boots app0.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <sys/reent.h>

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "bootloader_config.h"
#include "bootloader_init.h"
#include "bootloader_utility.h"
#include "bootloader_common.h"
#include "soc/reset_reasons.h"
#include "soc/rtc_cntl_reg.h"

static const char* TAG = "boot";

/** Cardputer BtnA / G0 — hold at power-on to force M5 OS home. */
#define M5OS_RECOVERY_GPIO 0
/** ~80 ms debounce (bootloader tick granularity). */
#define M5OS_RECOVERY_HOLD_MS 80
/** Written to RTC_CNTL_STORE0_REG by M5 OS immediately before Load app esp_restart(). */
#define M5OS_RTC_BOOT_STAGED_MAGIC 0x4D354153u

static int select_partition_number(bootloader_state_t* bs);

static int m5os_home_partition_index(const bootloader_state_t* bs)
{
    if (bs->factory.offset != 0) {
        return FACTORY_INDEX;
    }
    if (bs->app_count == 0) {
        return INVALID_INDEX;
    }
    return 0;
}

static bool m5os_recovery_gpio_held(void)
{
    return bootloader_common_check_long_hold_gpio_level(M5OS_RECOVERY_GPIO, M5OS_RECOVERY_HOLD_MS,
                                                      false) == GPIO_LONG_HOLD;
}

static bool m5os_sw_reset_is_intentional_launch(void)
{
    const soc_reset_reason_t reason = esp_rom_get_reset_reason(0);
    if (reason != RESET_REASON_CPU0_SW && reason != RESET_REASON_CORE_SW) {
        return false;
    }
    if (REG_READ(RTC_CNTL_STORE0_REG) != M5OS_RTC_BOOT_STAGED_MAGIC) {
        return false;
    }
    REG_WRITE(RTC_CNTL_STORE0_REG, 0);
    ESP_LOGI(TAG, "M5 OS staged launch handoff");
    return true;
}

static bool m5os_should_boot_home(const bootloader_state_t* bs, int* home_index_out)
{
    if (m5os_sw_reset_is_intentional_launch()) {
        return false;
    }
    if (!m5os_recovery_gpio_held() && esp_rom_get_reset_reason(0) == RESET_REASON_CORE_DEEP_SLEEP) {
        return false;
    }
    if (!m5os_recovery_gpio_held()) {
        /* Default: always reach M5 OS except intentional Load app SW handoff. */
    }
    const int home = m5os_home_partition_index(bs);
    if (home == INVALID_INDEX) {
        return false;
    }
    *home_index_out = home;
    if (m5os_recovery_gpio_held()) {
        ESP_LOGI(TAG, "M5 OS recovery: BtnA held, boot ota_0");
    } else {
        ESP_LOGI(TAG, "M5 OS home boot: reset reason %lu", (unsigned long)esp_rom_get_reset_reason(0));
    }
    return true;
}

void __attribute__((noreturn)) call_start_cpu0(void)
{
    if (bootloader_init() != ESP_OK) {
        bootloader_reset();
    }

#ifdef CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP
    bootloader_utility_load_boot_image_from_deep_sleep();
#endif

    bootloader_state_t bs = {0};
    const int boot_index = select_partition_number(&bs);
    if (boot_index == INVALID_INDEX) {
        bootloader_reset();
    }

    bootloader_utility_load_boot_image(&bs, boot_index);
}

static int select_partition_number(bootloader_state_t* bs)
{
    if (!bootloader_utility_load_partition_table(bs)) {
        ESP_LOGE(TAG, "load partition table error!");
        return INVALID_INDEX;
    }

    int home_index = INVALID_INDEX;
    if (m5os_should_boot_home(bs, &home_index)) {
        return home_index;
    }

    return bootloader_utility_get_selected_boot_partition(bs);
}

struct _reent* __getreent(void) { return _GLOBAL_REENT; }
