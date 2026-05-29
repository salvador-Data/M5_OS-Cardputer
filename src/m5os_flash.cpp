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

const esp_partition_t* partitionFromLabel(const char* label) {
    if (!label || !label[0]) return nullptr;
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
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

void applyColdBootHomeRestore() {
    if (esp_reset_reason() == ESP_RST_POWERON) {
        restoreBootToHome();
        log::info("m5os_cold_boot_home", "poweron");
    }
}

bool launchStagedAppSession() {
    saveHomeAppPartition();
    const esp_partition_t* staging = stagingOtaPartition();
    if (!staging) return false;
    if (esp_ota_set_boot_partition(staging) != ESP_OK) return false;
    log::info("m5os_launch_staged", staging->label);
    delay(200);
    esp_restart();
    return true;
}

void tryEarlyRecoveryBoot() {
    if (!recoveryBootRequested()) return;
    log::info("m5os_recovery_req", "key");
    if (restoreBootToHome()) esp_restart();
}

}  // namespace m5os
