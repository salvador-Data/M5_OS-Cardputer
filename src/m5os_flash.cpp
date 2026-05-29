#include "m5os_flash.h"

#include "m5os_boot_policy.h"

#include "M5OSDevice.h"

#include "serial_log.h"

#include "stamp_glow.h"

#include "ui_display.h"



#include <M5Cardputer.h>

#include <bootloader_common.h>

#include <driver/gpio.h>

#include <esp_flash_partitions.h>

#include <esp_ota_ops.h>

#include <esp_system.h>

#include <nvs.h>



namespace m5os {



namespace {



constexpr char kNvsNamespace[] = "m5os";

constexpr char kHomeLabelKey[] = "home_label";

constexpr char kLaunchPendingKey[] = "launch_pend";

constexpr char kAppSessionKey[] = "app_sess";

constexpr char kSessionExitKey[] = "sess_exit";



bool gLaunchRestartPending = false;

bool gLaunchHandoffFailed = false;

String gLaunchFailDetail;



const esp_partition_t* partitionFromLabel(const char* label) {

    if (!label || !label[0]) return nullptr;

    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);

}



bool partitionHasAppMagic(const esp_partition_t* part) {

    if (!part) return false;

    uint8_t magic = 0;

    return esp_partition_read(part, 0, &magic, 1) == ESP_OK && magic == 0xE9;

}



bool partitionIsLaunchable(const esp_partition_t* part) {

    if (!part) return false;

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;

    if (esp_ota_get_state_partition(part, &state) == ESP_OK) {

        if (state == ESP_OTA_IMG_VALID || state == ESP_OTA_IMG_PENDING_VERIFY) return true;

    }

    return partitionHasAppMagic(part);

}



uint8_t countOtaAppPartitions() {

    uint8_t count = 0;

    esp_partition_subtype_t subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;

    while (true) {

        const esp_partition_t* part =

            esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, nullptr);

        if (!part) break;

        ++count;

        if (subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MAX) break;

        subtype = static_cast<esp_partition_subtype_t>(subtype + 1);

    }

    return count ? count : 2;

}



int otaIndexFromSelectEntry(const esp_ota_select_entry_t& entry, uint8_t otaAppCount) {

    if (otaAppCount == 0 || entry.ota_seq == 0 || entry.ota_seq == UINT32_MAX) return -1;

    return static_cast<int>((entry.ota_seq - 1) % otaAppCount);

}



bool markStagedPartitionOtaState(const esp_partition_t* staged, esp_ota_img_states_t targetState) {

    if (!staged) return false;



    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;

    if (esp_ota_get_state_partition(staged, &state) == ESP_OK && state == targetState) {

        return true;

    }



    const esp_partition_t* otadata =

        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);

    if (!otadata) return false;



    constexpr size_t kSectorSize = 4096;

    esp_ota_select_entry_t entries[2]{};

    if (esp_partition_read(otadata, 0, &entries[0], sizeof(entries[0])) != ESP_OK) return false;

    if (esp_partition_read(otadata, kSectorSize, &entries[1], sizeof(entries[1])) != ESP_OK) {

        return false;

    }



    const uint8_t otaAppCount = countOtaAppPartitions();

    const int stagedIndex = static_cast<int>(staged->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0);

    bool updated = false;



    for (int i = 0; i < 2; ++i) {

        if (!bootloader_common_ota_select_valid(&entries[i])) continue;

        if (otaIndexFromSelectEntry(entries[i], otaAppCount) != stagedIndex) continue;

        entries[i].ota_state = targetState;

        entries[i].crc = bootloader_common_ota_select_crc(&entries[i]);

        if (esp_partition_erase_range(otadata, i * kSectorSize, kSectorSize) != ESP_OK) continue;

        if (esp_partition_write(otadata, i * kSectorSize, &entries[i], sizeof(entries[i])) !=

            ESP_OK) {

            continue;

        }

        updated = true;

    }



    if (updated) {

        log::info("m5os_stage_ota_state",

                  String(staged->label) + ":" + String(static_cast<int>(targetState)));

    }

    return updated;

}



void noteLaunchFail(const char* tag) {

    gLaunchHandoffFailed = true;

    gLaunchFailDetail = tag;

    log::info("m5os_launch_fail", tag);

}



void showRecoverySplashWindow() {

    const unsigned long deadline = millis() + 2000;

    while (static_cast<long>(millis() - deadline) < 0) {

        ui::drawHeader("Recovery");

        auto& d = m5os::lcd();

        d.setTextColor(TFT_WHITE, TFT_BLACK);

        d.setCursor(4, 36);

        d.println("Release for M5 OS");

        d.setCursor(4, 52);

        d.setTextColor(TFT_DARKGREY, TFT_BLACK);

        d.println("Hold ESC/` or BtnA");

        stamp::recoveryPulse();

        m5os::update();

        if (!recoveryBootRequested()) break;

        delay(20);

    }

}



bool rebootIntoStagedApp(const char* phaseTag) {

    const esp_partition_t* running = esp_ota_get_running_partition();

    const esp_partition_t* target = resolveLaunchBootPartition();

    if (!running || !target) {

        cancelLaunchSession();

        noteLaunchFail("no_target");

        return false;

    }

    if (running == target) {

        cancelLaunchSession();

        noteLaunchFail("already_running");

        return false;

    }



    if (!saveHomeAppPartition()) log::info("m5os_launch_warn", "home_save_failed");



    beginLaunchSession();

    // VALID (not PENDING_VERIFY): foreign apps often esp_restart() during init; rollback would

    // immediately revert to M5 OS before the user can use the loaded firmware.

    markStagedPartitionOtaState(target, ESP_OTA_IMG_VALID);



    const esp_partition_t* boot = esp_ota_get_boot_partition();

    if (boot != target) {

        if (esp_ota_set_boot_partition(target) != ESP_OK) {

            cancelLaunchSession();

            noteLaunchFail("set_boot");

            return false;

        }

    }



    clearLaunchPending();



    log::info("m5os_launch_reboot", String(phaseTag) + ":" + target->label);

    for (int i = 0; i < 20; ++i) delay(10);

    esp_restart();

    return true;

}



bool nvsSetFlag(const char* key, bool value) {

    nvs_handle_t handle = 0;

    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;

    esp_err_t err = ESP_OK;

    if (value) {

        err = nvs_set_u8(handle, key, 1);

    } else {

        err = nvs_erase_key(handle, key);

        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;

    }

    if (err == ESP_OK) nvs_commit(handle);

    nvs_close(handle);

    return err == ESP_OK;

}



bool nvsGetFlag(const char* key) {

    nvs_handle_t handle = 0;

    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) return false;

    uint8_t val = 0;

    const esp_err_t err = nvs_get_u8(handle, key, &val);

    nvs_close(handle);

    return err == ESP_OK && val != 0;

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



bool isRunningHomePartition() {

    const esp_partition_t* running = esp_ota_get_running_partition();

    if (!running) return false;

    nvs_handle_t handle = 0;

    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) return false;

    char label[17]{};

    size_t len = sizeof(label);

    const esp_err_t getErr = nvs_get_str(handle, kHomeLabelKey, label, &len);

    nvs_close(handle);

    if (getErr != ESP_OK) return false;

    const esp_partition_t* home = partitionFromLabel(label);

    return home && running == home;

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

    if (boot && running && boot != running && partitionIsLaunchable(boot)) return boot;



    const esp_partition_t* inactive = stagingOtaPartition();

    if (inactive && inactive != running && partitionIsLaunchable(inactive)) return inactive;

    return nullptr;

}



void applyColdBootHomeRestore() {

    if (boot_policy::shouldColdBootRestoreHome(esp_reset_reason())) {

        restoreBootToHome();

        clearAppSession();

        clearSessionExitPending();

        clearLaunchPending();

        log::info("m5os_cold_boot_home", "poweron");

    }

}



void applyCrashResetHomeRestore() {

    if (!boot_policy::shouldCrashResetRestoreHome(esp_reset_reason())) return;

    restoreBootToHome();

    clearLaunchPending();

    log::info("m5os_crash_home", String(static_cast<int>(esp_reset_reason())));

}



void setLaunchPending(bool pending) { nvsSetFlag(kLaunchPendingKey, pending); }



bool isLaunchPending() { return nvsGetFlag(kLaunchPendingKey); }



void clearLaunchPending() {

    gLaunchRestartPending = false;

    setLaunchPending(false);

}



bool isAppSessionActive() { return nvsGetFlag(kAppSessionKey); }



void setAppSessionActive(bool active) { nvsSetFlag(kAppSessionKey, active); }



void clearAppSession() { setAppSessionActive(false); }



bool isSessionExitPending() { return nvsGetFlag(kSessionExitKey); }



void setSessionExitPending(bool pending) { nvsSetFlag(kSessionExitKey, pending); }



void clearSessionExitPending() { setSessionExitPending(false); }



void beginLaunchSession() {

    gLaunchRestartPending = true;

    setAppSessionActive(true);

}



void cancelLaunchSession() {

    gLaunchRestartPending = false;

    clearAppSession();

    clearLaunchPending();

}



bool launchSessionActive() { return gLaunchRestartPending || isAppSessionActive() || isLaunchPending(); }



bool tryLaunchPendingHandoff() {

    if (!isLaunchPending()) return false;

    if (esp_reset_reason() != ESP_RST_SW) {

        log::info("m5os_handoff_skip", String(static_cast<int>(esp_reset_reason())));

        return false;

    }

    log::info("m5os_handoff_legacy", "phase2");

    return rebootIntoStagedApp("handoff");

}



bool launchStagedAppSession() { return rebootIntoStagedApp("direct"); }



void tryEarlyRecoveryBoot() {

    if (!recoveryBootRequested()) return;



    showRecoverySplashWindow();

    if (!recoveryBootRequested()) {

        log::info("m5os_recovery_abort", "released");

        return;

    }



    log::info("m5os_recovery_req", "key");

    if (isAppSessionActive()) setSessionExitPending(true);

    cancelLaunchSession();

    restoreBootToHome();

}



bool consumeLaunchHandoffFailure() {

    const bool failed = gLaunchHandoffFailed;

    gLaunchHandoffFailed = false;

    return failed;

}



String consumeLaunchFailDetail() {

    const String detail = gLaunchFailDetail;

    gLaunchFailDetail = "";

    return detail;

}



}  // namespace m5os

