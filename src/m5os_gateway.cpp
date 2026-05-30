#include "m5os_gateway.h"

#include "m5os_config.h"
#include "m5os_flash.h"

#include "m5os_gateway_embed.h"

#include "m5os_gateway_shared.h"

#include "m5os_vfs.h"

#include "serial_log.h"



#include <SD.h>

#include <esp_ota_ops.h>

#include <nvs.h>



namespace m5os {



namespace {



constexpr size_t kGatewayMaxBytes = 0x70000;

constexpr size_t kFlashChunk = 4096;

constexpr char kGatewaySdPath[] = "/system/m5os_session_gateway.bin";



bool nvsSetFlag(const char* key, bool value) {

    nvs_handle_t handle = 0;

    if (nvs_open(gateway::kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;

    esp_err_t err = value ? nvs_set_u8(handle, key, 1) : nvs_erase_key(handle, key);

    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;

    if (err == ESP_OK) nvs_commit(handle);

    nvs_close(handle);

    return err == ESP_OK;

}



bool partitionHasAppMagic(const esp_partition_t* part) {

    if (!part) return false;

    uint8_t magic = 0;

    return esp_partition_read(part, 0, &magic, 1) == ESP_OK && magic == 0xE9;

}



bool flashGatewayFromFile(File& f, size_t len) {

    const esp_partition_t* gw = gatewayOtaPartition();

    if (!gw || len == 0 || len > gw->size || len > kGatewayMaxBytes) return false;



    if (esp_partition_erase_range(gw, 0, gw->size) != ESP_OK) return false;



    uint8_t buffer[kFlashChunk];

    size_t written = 0;

    while (written < len) {

        const size_t n = f.read(buffer, min(sizeof(buffer), len - written));

        if (n == 0) break;

        if (esp_partition_write(gw, written, buffer, n) != ESP_OK) return false;

        written += n;

    }

    if (written != len) return false;

    return partitionHasAppMagic(gw) && markPartitionOtaState(gw, ESP_OTA_IMG_VALID);

}



}  // namespace



const esp_partition_t* gatewayOtaPartition() {

    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1,

                                    nullptr);

}



const esp_partition_t* runSlotOtaPartition() {
    const esp_partition_t* app2 =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_2, nullptr);
    if (app2) return app2;
    const esp_partition_t* app1 =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (app1 && app1->size >= kMinRunSlotPartitionBytes) return app1;
    return nullptr;
}



const esp_partition_t* stagingOtaPartition() { return runSlotOtaPartition(); }



bool gatewayPartitionReady() {

    const esp_partition_t* gw = gatewayOtaPartition();

    const esp_partition_t* run = runSlotOtaPartition();

    if (!gw || !run || gw == run || gw->size < 0x70000) return false;

    return partitionHasAppMagic(gw);

}



bool flashGatewayImage(const uint8_t* data, size_t len) {

    const esp_partition_t* gw = gatewayOtaPartition();

    if (!gw || !data || len == 0 || len > gw->size || len > kGatewayMaxBytes) return false;



    if (esp_partition_erase_range(gw, 0, gw->size) != ESP_OK) return false;



    size_t written = 0;

    while (written < len) {

        const size_t chunk = min(kFlashChunk, len - written);

        if (esp_partition_write(gw, written, data + written, chunk) != ESP_OK) return false;

        written += chunk;

    }



    if (!partitionHasAppMagic(gw)) {

        log::info("gw_flash_magic_fail", String(len));

        return false;

    }

    markPartitionOtaState(gw, ESP_OTA_IMG_VALID);

    log::info("gw_flash_ok", String(len));

    return true;

}



bool flashEmbeddedGatewayIfNeeded() {

    if (gatewayPartitionReady()) return true;



    if (vfs::isMounted() && SD.exists(kGatewaySdPath)) {

        File f = SD.open(kGatewaySdPath);

        if (f) {

            const size_t len = f.size();

            const bool ok = flashGatewayFromFile(f, len);

            f.close();

            if (ok) {

                log::info("gw_flash_sd_ok", kGatewaySdPath);

                return true;

            }

        }

    }



    if (gateway_embed::kSize > 0 && gateway_embed::kData[0] == 0xE9) {

        if (flashGatewayImage(gateway_embed::kData, gateway_embed::kSize)) {

            log::info("gw_flash_embed_ok", String(gateway_embed::kSize));

            return true;

        }

        log::info("gw_flash_embed_fail", String(gateway_embed::kSize));

    }



    log::info("gw_flash_missing", "embedded+sd unavailable");

    return false;

}



bool launchGatewaySession() {

    const esp_partition_t* gw = gatewayOtaPartition();

    const esp_partition_t* run = runSlotOtaPartition();

    if (!gw || !run || gw == run) {

        log::info("gw_launch_fail", "partition_layout");

        return false;

    }

    if (!partitionHasAppMagic(run)) {

        log::info("gw_launch_fail", "run_slot_empty");

        return false;

    }

    if (!flashEmbeddedGatewayIfNeeded()) {

        log::info("gw_launch_fail", "no_gateway_image");

        return false;

    }



    saveHomeAppPartition();

    nvsSetFlag(gateway::kGatewayActiveKey, true);

    markPartitionOtaState(run, ESP_OTA_IMG_VALID);



    if (esp_ota_set_boot_partition(gw) != ESP_OK) {

        log::info("gw_launch_fail", "set_boot");

        return false;

    }



    log::info("gw_launch_reboot", gw->label);

    delay(100);

    gateway::setStagedBootHandoff();

    esp_restart();

    return true;

}



bool gatewayExitToHome() {

    nvsSetFlag(gateway::kGatewayActiveKey, false);

    setSessionExitPending(true);

    return restoreBootToHome();

}



bool gatewayLaunchRunSlot() {

    const esp_partition_t* run = runSlotOtaPartition();

    if (!run || !partitionHasAppMagic(run)) return false;

    nvsSetFlag(gateway::kGatewayActiveKey, false);

    markPartitionOtaState(run, ESP_OTA_IMG_VALID);

    if (esp_ota_set_boot_partition(run) != ESP_OK) return false;

    beginLaunchSession();

    delay(50);

    gateway::setStagedBootHandoff();

    esp_restart();

    return true;

}



}  // namespace m5os

