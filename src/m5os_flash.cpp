#include "m5os_flash.h"

#include "m5os_boot_policy.h"

#include "M5OSDevice.h"

#include "m5os_watchdog.h"

#include "serial_log.h"

#include "stamp_glow.h"

#include "ui_display.h"



#include <M5Cardputer.h>

#include <bootloader_common.h>

#include <driver/gpio.h>

#include <esp_flash_partitions.h>

#include <esp_image_format.h>

#include <esp_ota_ops.h>

#include <esp_system.h>

#include <nvs.h>

#include <soc/rtc_cntl_reg.h>

#include <cstring>



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



bool writeOtadataSelectEntry(const esp_partition_t* otadata, int sectorIndex,

                             const esp_ota_select_entry_t& entry) {

    if (!otadata || sectorIndex < 0 || sectorIndex > 1) return false;

    constexpr size_t kSectorSize = 4096;

    if (esp_partition_erase_range(otadata, sectorIndex * kSectorSize, kSectorSize) != ESP_OK) {

        return false;

    }

    return esp_partition_write(otadata, sectorIndex * kSectorSize, &entry, sizeof(entry)) == ESP_OK;

}



uint32_t nextOtadataSeqForIndex(int stagedIndex, uint8_t otaAppCount, uint32_t activeSeq) {

    const uint32_t indexSeq = (static_cast<uint32_t>(stagedIndex) % otaAppCount) + 1;

    if (activeSeq == 0 || activeSeq == UINT32_MAX) return indexSeq;

    uint32_t generation = 0;

    while (activeSeq >= indexSeq + generation * otaAppCount) {

        ++generation;

    }

    return indexSeq + generation * otaAppCount;

}



bool setBootPartitionForLaunch(const esp_partition_t* target) {

    if (!target) return false;

    if (esp_ota_get_boot_partition() == target) return true;

    const esp_err_t err = esp_ota_set_boot_partition(target);

    if (err == ESP_OK) return true;

    log::info("m5os_set_boot_fail", String(static_cast<int>(err)));

    return esp_ota_get_boot_partition() == target;

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



void noteLaunchFail(const char* tag) {

    gLaunchHandoffFailed = true;

    gLaunchFailDetail = tag;

    log::info("m5os_launch_fail", tag);

}



void setRtcBootStagedHandoff() { REG_WRITE(RTC_CNTL_STORE0_REG, kRtcBootStagedMagic); }



void clearRtcBootStagedHandoff() { REG_WRITE(RTC_CNTL_STORE0_REG, 0); }



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



    if (!verifyPartitionAppImage(target)) {

        cancelLaunchSession();

        noteLaunchFail("image_verify");

        return false;

    }



    if (!saveHomeAppPartition()) log::info("m5os_launch_warn", "home_save_failed");



    beginLaunchSession();

    // Boot partition first, then mark VALID (rollback enabled: set_boot alone leaves NEW).

    if (!setBootPartitionForLaunch(target)) {

        cancelLaunchSession();

        noteLaunchFail("set_boot");

        return false;

    }



    if (!markPartitionOtaState(target, ESP_OTA_IMG_VALID)) {

        cancelLaunchSession();

        noteLaunchFail("otadata");

        return false;

    }



    const esp_partition_t* boot = esp_ota_get_boot_partition();

    if (boot != target) {

        cancelLaunchSession();

        noteLaunchFail("set_boot");

        return false;

    }



    setLaunchPending(true);



    log::info("m5os_launch_reboot", String(phaseTag) + ":" + target->label);

    for (int i = 0; i < 20; ++i) {
        feedWatchdog();
        delay(10);
    }

    setRtcBootStagedHandoff();

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



const esp_partition_t* stagingOtaPartition() {
    return esp_ota_get_next_update_partition(nullptr);
}

bool verifyPartitionAppImage(const esp_partition_t* part) {
    if (!part) return false;
    uint8_t magic = 0;
    if (esp_partition_read(part, 0, &magic, 1) != ESP_OK || magic != 0xE9) return false;
    esp_app_desc_t desc{};
    if (esp_ota_get_partition_description(part, &desc) == ESP_OK) return true;
    esp_partition_pos_t pos{};
    pos.offset = part->address;
    pos.size = part->size;
    esp_image_metadata_t metadata{};
    return esp_image_verify(ESP_IMAGE_VERIFY, &pos, &metadata) == ESP_OK;
}

size_t maxOtaAppBytes() {
    const esp_partition_t* staging = stagingOtaPartition();
    if (staging && staging->size > 0) return staging->size;
    return kMaxAppBinBytes;
}

String formatFlashSizeMb(size_t bytes) {
    char buf[24];
    const float mb = static_cast<float>(bytes) / (1024.0f * 1024.0f);
    snprintf(buf, sizeof(buf), "%.2f MB", mb);
    return String(buf);
}

String formatAppTooLargeMessage(size_t appBytes, size_t slotBytes) {
    String msg = "App " + formatFlashSizeMb(appBytes);
    msg += " / slot " + formatFlashSizeMb(slotBytes);
    msg += " — too large\nUse smaller bin or";
    msg += "\nM5Burner USB full flash";
    msg += "\n(no SPIFFS composite)";
    return msg;
}



bool markPartitionOtaState(const esp_partition_t* staged, esp_ota_img_states_t targetState) {

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

    if (stagedIndex < 0 || stagedIndex >= static_cast<int>(otaAppCount)) return false;

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



    if (!updated) {

        const int active = bootloader_common_get_active_otadata(entries);

        const int nextSector = (active == -1) ? 0 : ((~active) & 1);

        const uint32_t activeSeq = (active == -1) ? 0 : entries[active].ota_seq;

        const uint32_t nextSeq = nextOtadataSeqForIndex(stagedIndex, otaAppCount, activeSeq);

        esp_ota_select_entry_t selected{};

        std::memset(&selected, 0xFF, sizeof(selected));

        selected.ota_seq = nextSeq;

        selected.ota_state = targetState;

        selected.crc = bootloader_common_ota_select_crc(&selected);

        updated = writeOtadataSelectEntry(otadata, nextSector, selected);

        if (updated) {

            log::info("m5os_stage_ota_boot", String(staged->label) + ":" + String(nextSeq));

        }

    }



    if (updated) {

        log::info("m5os_stage_ota_state",

                  String(staged->label) + ":" + String(static_cast<int>(targetState)));

    }

    return updated;

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



const esp_partition_t* resolveLaunchBootPartition() {

    const esp_partition_t* running = esp_ota_get_running_partition();

    const esp_partition_t* boot = esp_ota_get_boot_partition();

    if (boot && running && boot != running && partitionIsLaunchable(boot)) return boot;



    const esp_partition_t* inactive = stagingOtaPartition();

    if (inactive && inactive != running && partitionIsLaunchable(inactive)) return inactive;

    return nullptr;

}



void applyColdBootHomeRestore() {

    const esp_reset_reason_t reason = esp_reset_reason();

    if (!boot_policy::shouldHardwareResetRestoreHome(reason)) return;

    clearRtcBootStagedHandoff();



    restoreBootToHome();

    clearLaunchPending();



    if (boot_policy::shouldPowerOnRestoreHome(reason)) {

        clearAppSession();

        clearSessionExitPending();

        log::info("m5os_hw_reset_home", "poweron");

    } else {

        log::info("m5os_hw_reset_home", "ext");

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

    /* Single-reboot launch (9135c9e+): phase-2 handoff removed to avoid double esp_restart. */

    if (isLaunchPending()) log::info("m5os_handoff_legacy", "noop");

    return false;

}



bool tryHandleLaunchSnapBack() {

    if (!isLaunchPending() || !isRunningHomePartition()) return false;

    const esp_reset_reason_t reason = esp_reset_reason();

    if (reason == ESP_RST_SW) {

        const esp_partition_t* boot = esp_ota_get_boot_partition();

        log::info("m5os_launch_snapback", boot && boot->label[0] ? boot->label : "unknown");

        log::info("m5os_launch_snapback_dbg", formatOtaSlotDebug());

        noteLaunchFail("snapback");

        cancelLaunchSession();

        return true;

    }

    clearLaunchPending();

    return false;

}



void logBootPartitionContext() {

    const esp_partition_t* running = esp_ota_get_running_partition();

    const esp_partition_t* boot = esp_ota_get_boot_partition();

    const String runLabel = running && running->label[0] ? running->label : String("?");

    const String bootLabel = boot && boot->label[0] ? boot->label : String("?");

    log::info("m5os_boot_part", runLabel + "/" + bootLabel);

    log::info("m5os_reset_reason", String(static_cast<int>(esp_reset_reason())));

}



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



String peekLaunchFailDetail() { return gLaunchFailDetail; }



String formatLaunchFailMessage(const String& tag) {

    if (tag == "no_target") return "No valid app in run slot\nRe-copy from SD";

    if (tag == "set_boot") return "Boot switch failed\nImage may be invalid";

    if (tag == "already_running") return "Already on run slot\nRe-copy from SD";

    if (tag == "otadata") return "otadata update failed\nReflash M5 OS";

    if (tag == "image_verify") return "App image invalid\nWrong chip or corrupt bin";

    if (tag == "snapback") return "App did not start\nBootloader stayed on M5 OS\nNeed ESP32-S3 app-only bin\n" + formatOtaSlotDebug();

    if (tag.length()) return tag;

    return "Reboot failed\nCheck serial log";

}



bool launchStagedAppSession() { return rebootIntoStagedApp("direct"); }



namespace {

constexpr uint8_t kChipIdEsp32S3 = 0x09;

String formatPartitionLine(const char* tag, const esp_partition_t* part) {

    if (!part || !part->label[0]) return String(tag) + ":?";

    char buf[56];

    snprintf(buf, sizeof(buf), "%s:%s 0x%x %u", tag, part->label, part->address,

             static_cast<unsigned>(part->size));

    return String(buf);

}

}  // namespace



bool otaSlotWriterBegin(OtaSlotWriter& writer, const esp_partition_t* part, size_t imageSize) {

    otaSlotWriterAbort(writer);

    if (!part || imageSize == 0 || imageSize > part->size) return false;

    const esp_err_t err = esp_ota_begin(part, imageSize, &writer.handle);

    if (err != ESP_OK) {

        log::info("m5os_ota_begin_fail", String(static_cast<int>(err)));

        return false;

    }

    writer.part = part;

    writer.expected = imageSize;

    writer.written = 0;

    writer.active = true;

    return true;

}



bool otaSlotWriterAppend(OtaSlotWriter& writer, const uint8_t* data, size_t len) {

    if (!writer.active || !data || len == 0) return false;

    if (writer.written + len > writer.expected) return false;

    const esp_err_t err = esp_ota_write(writer.handle, data, len);

    if (err != ESP_OK) {

        log::info("m5os_ota_write_fail", String(static_cast<int>(err)));

        otaSlotWriterAbort(writer);

        return false;

    }

    writer.written += len;

    return true;

}



bool otaSlotWriterFinish(OtaSlotWriter& writer, String* errOut) {

    if (!writer.active) {

        if (errOut) *errOut = "OTA writer inactive";

        return false;

    }

    if (writer.written != writer.expected) {

        if (errOut) *errOut = "Incomplete OTA write";

        otaSlotWriterAbort(writer);

        return false;

    }

    const esp_err_t err = esp_ota_end(writer.handle);

    writer.active = false;

    writer.handle = 0;

    if (err != ESP_OK) {

        log::info("m5os_ota_end_fail", String(static_cast<int>(err)));

        if (errOut) {

            if (err == ESP_ERR_OTA_VALIDATE_FAILED) {

                *errOut = "Image verify failed\nWrong chip or corrupt bin";

            } else {

                *errOut = "OTA finalize failed";

            }

        }

        return false;

    }

    if (!verifyPartitionAppImage(writer.part)) {

        if (errOut) *errOut = "Post-OTA verify failed";

        return false;

    }

    return true;

}



void otaSlotWriterAbort(OtaSlotWriter& writer) {

    if (writer.active && writer.handle) esp_ota_abort(writer.handle);

    writer = OtaSlotWriter{};

}



String formatOtaSlotDebug() {

    return formatPartitionLine("run", esp_ota_get_running_partition()) + "\n" +

           formatPartitionLine("boot", esp_ota_get_boot_partition()) + "\n" +

           formatPartitionLine("slot", stagingOtaPartition());

}



bool validateAppImageChipTarget(const esp_partition_t* part, String* chipDetailOut) {

    if (!part) return false;

    uint8_t hdr[16]{};

    if (esp_partition_read(part, 0, hdr, sizeof(hdr)) != ESP_OK) return false;

    if (hdr[0] != 0xE9) return false;

    const uint8_t chipId = hdr[12];

    if (chipDetailOut) *chipDetailOut = String("0x") + String(chipId, HEX);

    return chipId == kChipIdEsp32S3;

}



}  // namespace m5os

