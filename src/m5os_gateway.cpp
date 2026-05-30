#include "m5os_gateway.h"

#include "m5os_config.h"
#include "m5os_flash.h"
#include "m5os_gateway_embed.h"
#include "m5os_gateway_shared.h"
#include "m5os_vfs.h"
#include "m5os_watchdog.h"
#include "serial_log.h"

#include <SD.h>
#include <esp_image_format.h>
#include <esp_ota_ops.h>
#include <nvs.h>

namespace m5os {

namespace {

constexpr size_t kGatewayMaxBytes = kGatewayPartitionBytes;
constexpr size_t kFlashChunk = 4096;
constexpr char kGatewaySdPath[] = "/system/m5os_session_gateway.bin";

bool gDeferredGatewayInstallPending = false;
bool gDeferredGatewayInstallDone = false;
String gLastGatewayFlashDetail;

void noteGatewayFlashDetail(const char* detail) {
    gLastGatewayFlashDetail = detail ? detail : "";
}

void reportProgress(GatewayFlashProgressFn progress, int percent, const char* phase) {
    if (progress) progress(percent, phase);
}

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

/** Reject stale app1 images left by USB partial flash or pre-embed gateway builds. */
bool gatewayImageMatchesEmbed(const esp_partition_t* gw) {
    if (!gw || gateway_embed::kSize == 0) return true;
    esp_partition_pos_t pos{};
    pos.offset = gw->address;
    pos.size = gw->size;
    esp_image_metadata_t metadata{};
    if (esp_image_verify(ESP_IMAGE_VERIFY, &pos, &metadata) != ESP_OK) return false;
    return metadata.image_len == gateway_embed::kSize;
}

bool flashGatewayFromFile(File& f, size_t len, GatewayFlashProgressFn progress) {
    const esp_partition_t* gw = gatewayOtaPartition();
    if (!gw || len == 0 || len > gw->size || len > kGatewayMaxBytes) {
        noteGatewayFlashDetail("Gateway layout/size invalid");
        return false;
    }
    if (gateway_embed::kSize > 0 && len != gateway_embed::kSize) {
        noteGatewayFlashDetail("Gateway SD bin wrong size");
        log::info("gw_sd_size_mismatch", String(len) + "/" + String(gateway_embed::kSize));
        return false;
    }

    reportProgress(progress, 0, "Erase app1");
    if (esp_partition_erase_range(gw, 0, gw->size) != ESP_OK) {
        noteGatewayFlashDetail("Gateway erase (app1) failed");
        return false;
    }

    uint8_t buffer[kFlashChunk];
    size_t written = 0;
    while (written < len) {
        const size_t n = f.read(buffer, min(sizeof(buffer), len - written));
        if (n == 0) break;
        if (esp_partition_write(gw, written, buffer, n) != ESP_OK) {
            noteGatewayFlashDetail("Gateway flash write (app1)");
            return false;
        }
        written += n;
        feedWatchdog();
        if (progress && (written == n || written % kFlashChunk == 0 || written == len)) {
            const int pct = static_cast<int>(min(99ULL, (written * 100ULL) / len));
            reportProgress(progress, pct, "Write gateway");
        }
    }
    if (written != len) {
        noteGatewayFlashDetail("Gateway SD read incomplete");
        return false;
    }
    reportProgress(progress, 100, "Verify gateway");
    if (!partitionHasAppMagic(gw) || !gatewayImageMatchesEmbed(gw)) {
        noteGatewayFlashDetail("Gateway verify failed (app1)");
        log::info("gw_sd_embed_mismatch", String(len));
        return false;
    }
    if (!markPartitionOtaState(gw, ESP_OTA_IMG_VALID)) {
        noteGatewayFlashDetail("Gateway otadata (app1) failed");
        log::info("gw_sd_otadata_fail", String(len));
        return false;
    }
    gLastGatewayFlashDetail = "";
    return true;
}

}  // namespace

const esp_partition_t* gatewayOtaPartition() {
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
}

bool gatewayPartitionReady() {
    const esp_partition_t* gw = gatewayOtaPartition();
    const esp_partition_t* run = runSlotOtaPartition();
    if (!gw || !run || gw == run || gw->size < kGatewayPartitionBytes) return false;
    if (!partitionHasAppMagic(gw)) return false;
    if (!gatewayImageMatchesEmbed(gw)) {
        log::info("gw_stale_embed", String(gateway_embed::kSize));
        return false;
    }
    return true;
}

bool flashGatewayImage(const uint8_t* data, size_t len, GatewayFlashProgressFn progress) {
    const esp_partition_t* gw = gatewayOtaPartition();
    if (!gw || !data || len == 0 || len > gw->size || len > kGatewayMaxBytes) {
        noteGatewayFlashDetail("Gateway embed size invalid");
        return false;
    }

    reportProgress(progress, 0, "Erase app1");
    if (esp_partition_erase_range(gw, 0, gw->size) != ESP_OK) {
        noteGatewayFlashDetail("Gateway erase (app1) failed");
        return false;
    }

    size_t written = 0;
    while (written < len) {
        const size_t chunk = min(kFlashChunk, len - written);
        if (esp_partition_write(gw, written, data + written, chunk) != ESP_OK) {
            noteGatewayFlashDetail("Gateway embed write (app1)");
            return false;
        }
        written += chunk;
        feedWatchdog();
        if (progress && (written == chunk || written % kFlashChunk == 0 || written == len)) {
            const int pct = static_cast<int>(min(99ULL, (written * 100ULL) / len));
            reportProgress(progress, pct, "Write gateway");
        }
    }

    if (!partitionHasAppMagic(gw) || !gatewayImageMatchesEmbed(gw)) {
        noteGatewayFlashDetail("Gateway verify failed (app1)");
        log::info("gw_flash_magic_fail", String(len));
        return false;
    }
    if (!markPartitionOtaState(gw, ESP_OTA_IMG_VALID)) {
        noteGatewayFlashDetail("Gateway otadata (app1) failed");
        log::info("gw_flash_otadata_fail", String(len));
        return false;
    }
    gLastGatewayFlashDetail = "";
    reportProgress(progress, 100, "Gateway ready");
    log::info("gw_flash_ok", String(len));
    return true;
}

bool flashEmbeddedGatewayIfNeeded(GatewayFlashProgressFn progress) {
    gLastGatewayFlashDetail = "";
    if (gatewayPartitionReady()) {
        log::info("gw_ready_skip", "magic");
        reportProgress(progress, 100, "Gateway ready");
        return true;
    }

    if (vfs::isMounted() && SD.exists(kGatewaySdPath)) {
        File f = SD.open(kGatewaySdPath);
        if (f) {
            const size_t len = f.size();
            if (gateway_embed::kSize > 0 && len != gateway_embed::kSize) {
                log::info("gw_sd_skip_stale", String(len) + "/" + String(gateway_embed::kSize));
            } else {
                const bool ok = flashGatewayFromFile(f, len, progress);
                if (ok && gatewayPartitionReady()) {
                    log::info("gw_flash_sd_ok", kGatewaySdPath);
                    f.close();
                    return true;
                }
                log::info("gw_flash_sd_reject", kGatewaySdPath);
            }
            f.close();
        }
    }

    if (gateway_embed::kSize > 0 && gateway_embed::kData[0] == 0xE9) {
        if (flashGatewayImage(gateway_embed::kData, gateway_embed::kSize, progress) &&
            gatewayPartitionReady()) {
            log::info("gw_flash_embed_ok", String(gateway_embed::kSize));
            return true;
        }
        log::info("gw_flash_embed_fail", String(gateway_embed::kSize));
    }

    noteGatewayFlashDetail("Gateway image missing");
    log::info("gw_flash_missing", "embedded+sd unavailable");
    return false;
}

String lastGatewayFlashDetail() { return gLastGatewayFlashDetail; }

bool ensureGatewayInstalled(GatewayFlashProgressFn progress) {
    return flashEmbeddedGatewayIfNeeded(progress);
}

void tryDeferredGatewayInstall() {
    if (gDeferredGatewayInstallDone || !gDeferredGatewayInstallPending) return;
    gDeferredGatewayInstallDone = true;
    gDeferredGatewayInstallPending = false;
    if (gatewayPartitionReady()) {
        log::info("gw_deferred_skip", "ready");
        return;
    }
    log::info("gw_deferred_install", "start");
    flashEmbeddedGatewayIfNeeded(nullptr);
}

void scheduleDeferredGatewayInstall() {
    if (gatewayPartitionReady()) {
        gDeferredGatewayInstallPending = false;
        gDeferredGatewayInstallDone = true;
        return;
    }
    gDeferredGatewayInstallPending = true;
    gDeferredGatewayInstallDone = false;
}

bool launchGatewaySession() {
    clearLaunchPending();

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
    if (!gatewayPartitionReady()) {
        log::info("gw_launch_fail", "no_gateway_image");
        return false;
    }

    saveHomeAppPartition();
    nvsSetFlag(gateway::kGatewayActiveKey, true);
    markPartitionOtaState(run, ESP_OTA_IMG_VALID);
    if (!markPartitionOtaState(gw, ESP_OTA_IMG_VALID)) {
        log::info("gw_launch_fail", "gw_otadata");
        return false;
    }

    if (esp_ota_set_boot_partition(gw) != ESP_OK) {
        log::info("gw_launch_fail", "set_boot");
        return false;
    }

    log::info("gw_launch_reboot", gw->label);
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
    setLaunchPending(true);
    gateway::setStagedBootHandoff();
    esp_restart();
    return true;
}

}  // namespace m5os
