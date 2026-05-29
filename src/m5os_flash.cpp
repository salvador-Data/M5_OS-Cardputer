#include "m5os_flash.h"



#include "M5OSDevice.h"

#include "serial_log.h"



#include <M5Cardputer.h>

#include <driver/gpio.h>

#include <esp_ota_ops.h>

#include <esp_system.h>

#include <nvs.h>



namespace m5os {



namespace {



constexpr char kNvsNamespace[] = "m5os";

constexpr char kHomeLabelKey[] = "home_label";

constexpr char kLaunchPendingKey[] = "launch_pend";



bool gLaunchRestartPending = false;
bool gLaunchHandoffFailed = false;



const esp_partition_t* partitionFromLabel(const char* label) {

    if (!label || !label[0]) return nullptr;

    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);

}



bool partitionHasAppMagic(const esp_partition_t* part) {

    if (!part) return false;

    uint8_t magic = 0;

    return esp_partition_read(part, 0, &magic, 1) == ESP_OK && magic == 0xE9;

}



}  // namespace



size_t maxOtaAppBytes() {

    const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);

    if (part != nullptr && part->size > 0) return part->size;

    return kMaxAppBinBytes;

}



bool saveHomeAppPartition() {

    const esp_partition_t* running = esp_ota_get_running_partition();

    if (!running || !running->label[0]) return false;



    nvs_handle_t handle = 0;

    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;

    const esp_err_t err = nvs_set_str(handle, kHomeLabelKey, running->label);

    if (err == ESP_OK) nvs_commit(handle);

    nvs_close(handle);

    if (err == ESP_OK) log::info("m5os_home_saved", running->label);

    return err == ESP_OK;

}



bool restoreBootToHome() {

    nvs_handle_t handle = 0;

    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) return false;



    char label[17]{};

    size_t len = sizeof(label);

    const esp_err_t getErr = nvs_get_str(handle, kHomeLabelKey, label, &len);

    nvs_close(handle);

    if (getErr != ESP_OK) return false;



    const esp_partition_t* home = partitionFromLabel(label);

    if (!home) return false;



    const esp_err_t setErr = esp_ota_set_boot_partition(home);

    if (setErr == ESP_OK) log::info("m5os_home_restore", label);

    return setErr == ESP_OK;

}



bool recoveryBootRequested() {

    if (gpio_get_level(GPIO_NUM_0) == 0) return true;

    M5Cardputer.update();

    if (!M5Cardputer.Keyboard.isPressed()) return false;

    const auto status = M5Cardputer.Keyboard.keysState();

    for (uint8_t hid : status.hid_keys) {

        if (hid == kHidEscape) return true;

    }

    for (auto key : status.word) {

        if (key == '`' || key == 27) return true;

    }

    return false;

}



const esp_partition_t* stagingOtaPartition() { return esp_ota_get_next_update_partition(nullptr); }



const esp_partition_t* resolveLaunchBootPartition() {

    const esp_partition_t* running = esp_ota_get_running_partition();

    const esp_partition_t* boot = esp_ota_get_boot_partition();

    if (boot && running && boot != running && partitionHasAppMagic(boot)) return boot;



    const esp_partition_t* inactive = stagingOtaPartition();

    if (inactive && inactive != running && partitionHasAppMagic(inactive)) return inactive;

    return nullptr;

}



void applyColdBootHomeRestore() {

    if (esp_reset_reason() == ESP_RST_POWERON) {

        restoreBootToHome();

        clearLaunchPending();

        log::info("m5os_cold_boot_home", "poweron");

    }

}



void applyCrashResetHomeRestore() {

    switch (esp_reset_reason()) {

        case ESP_RST_PANIC:

        case ESP_RST_INT_WDT:

        case ESP_RST_TASK_WDT:

        case ESP_RST_WDT:

            restoreBootToHome();

            clearLaunchPending();

            log::info("m5os_crash_home", String(static_cast<int>(esp_reset_reason())));

            break;

        default:

            break;

    }

}



void setLaunchPending(bool pending) {

    nvs_handle_t handle = 0;

    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return;

    if (pending) {

        nvs_set_u8(handle, kLaunchPendingKey, 1);

    } else {

        nvs_erase_key(handle, kLaunchPendingKey);

    }

    nvs_commit(handle);

    nvs_close(handle);

}



bool isLaunchPending() {

    nvs_handle_t handle = 0;

    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) return false;

    uint8_t val = 0;

    const esp_err_t err = nvs_get_u8(handle, kLaunchPendingKey, &val);

    nvs_close(handle);

    return err == ESP_OK && val != 0;

}



void clearLaunchPending() {

    gLaunchRestartPending = false;

    setLaunchPending(false);

}



void beginLaunchSession() {

    gLaunchRestartPending = true;

    setLaunchPending(true);

}



void cancelLaunchSession() { clearLaunchPending(); }



bool launchSessionActive() { return gLaunchRestartPending || isLaunchPending(); }



bool tryLaunchPendingHandoff() {

    if (!isLaunchPending()) return false;

    if (esp_reset_reason() != ESP_RST_SW) return false;



    const esp_partition_t* running = esp_ota_get_running_partition();

    const esp_partition_t* target = resolveLaunchBootPartition();

    if (!running || !target) {

        cancelLaunchSession();

        gLaunchHandoffFailed = true;

        log::info("m5os_launch_handoff_fail", "no_target");

        return false;

    }

    if (running == target) {

        cancelLaunchSession();

        gLaunchHandoffFailed = true;

        log::info("m5os_launch_handoff_fail", "already_target");

        return false;

    }



    saveHomeAppPartition();

    beginLaunchSession();

    if (esp_ota_set_boot_partition(target) != ESP_OK) {

        cancelLaunchSession();

        gLaunchHandoffFailed = true;

        log::info("m5os_launch_handoff_fail", "set_boot");

        return false;

    }

    log::info("m5os_launch_handoff", target->label);

    for (int i = 0; i < 20; ++i) {

        delay(10);

    }

    esp_restart();

    return true;

}



bool launchStagedAppSession() {

    saveHomeAppPartition();

    restoreBootToHome();

    beginLaunchSession();

    log::info("m5os_launch_staged", "home_then_handoff");

    for (int i = 0; i < 20; ++i) {

        delay(10);

    }

    esp_restart();

    return true;

}



void tryEarlyRecoveryBoot() {

    if (!recoveryBootRequested()) return;

    log::info("m5os_recovery_req", "key");

    cancelLaunchSession();

    if (restoreBootToHome()) esp_restart();

}



bool consumeLaunchHandoffFailure() {

    const bool failed = gLaunchHandoffFailed;

    gLaunchHandoffFailed = false;

    return failed;

}



}  // namespace m5os


