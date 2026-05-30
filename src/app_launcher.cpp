#include "app_launcher.h"



#include "burner_install.h"

#include "m5burner_hookup.h"

#include "m5os_config.h"

#include "M5OSDevice.h"

#include "m5os_flash.h"

#include "m5os_gateway.h"

#include "m5os_session.h"

#include "m5os_vfs.h"

#include "m5os_security.h"

#include "m5os_watchdog.h"

#include "serial_log.h"

#include "ui_display.h"



#include <SD.h>

#include <Preferences.h>

#include <WiFi.h>

#include <esp_ota_ops.h>

#include <esp_system.h>



namespace m5os {



namespace {



constexpr char kLaunchNs[] = "m5os_launch";

constexpr char kLastBinKey[] = "last_bin";

constexpr char kLastShaKey[] = "last_sha";

constexpr char kLastSizeKey[] = "last_size";

constexpr char kLastMtimeKey[] = "last_mtime";

constexpr size_t kIoChunkBytes = 4096;



struct LaunchProgressCtx {
    String appLabel;
    const char* phase;
};

LaunchProgressCtx gHashProgressCtx;

void paintLoadAppPhase(int percent, const String& appLabel, const char* phase, const String& extra = "") {
    String detail = appLabel + "\n" + String(phase);
    if (extra.length()) detail += "\n" + extra;
    ui::showFlashProgress(percent, "Load app", detail);
    m5os::update();
}

void paintLoadAppDebugPhase(int percent, const String& appLabel, const char* phase) {
    paintLoadAppPhase(percent, appLabel, phase, formatOtaSlotDebug());
}

bool detectMergedFlashBin(File& firmware, size_t fileSize) {
    if (fileSize < 0x10000) return false;
    const size_t savedPos = firmware.position();
    uint8_t magic[2]{};
    if (firmware.seek(0x8000) && firmware.read(magic, 2) == 2) {
        if (magic[0] == 0xAA && magic[1] == 0x50) {
            firmware.seek(savedPos);
            return true;
        }
    }
    uint8_t head = 0;
    uint8_t appHead = 0;
    if (firmware.seek(0) && firmware.read(&head, 1) == 1 && firmware.seek(0x10000) &&
        firmware.read(&appHead, 1) == 1 && head == 0xE9 && appHead == 0xE9) {
        firmware.seek(savedPos);
        return true;
    }
    firmware.seek(savedPos);
    return false;
}

void surfaceLaunchFailure(const String& appLabel, LaunchResult& result) {
    paintLoadAppPhase(0, appLabel, "Failed", result.message);
    ui::showMessage("Load app failed", result.message, TFT_RED);
}

void surfaceLaunchRebootFailure(const String& appLabel, LaunchResult& result) {
    result.message = formatLaunchFailMessage(peekLaunchFailDetail());
    surfaceLaunchFailure(appLabel, result);
}

bool rebootIntoGatewaySession(const String& appLabel, LaunchResult& result) {
    auto gwProgress = [](int percent, const char* phase) {
        (void)phase;
        ui::showFlashProgress(percent, "Load app", String("Session shell\n") + String(phase));
        m5os::update();
    };

    if (gatewayPartitionReady()) {
        paintLoadAppPhase(100, appLabel, "Session shell", "Ready — rebooting");
        ui::showMessage("Load app", appLabel + "\nSession shell\nRebooting...", TFT_GREEN, 200);
    } else {
        paintLoadAppPhase(0, appLabel, "Session shell", "Installing gateway...");
        if (!ensureGatewayInstalled(gwProgress)) {
            result.ok = false;
            result.message = "Gateway install failed\nRun flash_all.ps1";
            log::info("launch_gateway_install_fail", appLabel);
            surfaceLaunchFailure(appLabel, result);
            return false;
        }
        paintLoadAppPhase(100, appLabel, "Session shell", "Ready — rebooting");
    }

    if (launchGatewaySession()) return true;
    result.ok = false;
    result.message = "Gateway launch failed\nReflash M5 OS + gateway";
    log::info("launch_gateway_fail", appLabel);
    surfaceLaunchFailure(appLabel, result);
    return false;
}

void paintLaunchProgress(size_t done, size_t total, LaunchProgressCtx* ctx) {
    if (!ctx || total == 0) return;
    const int pct = static_cast<int>(min(100ULL, (done * 100ULL) / total));
    String detail = ctx->appLabel + "\n" + String(ctx->phase) + "\n" + String(done) + " / " + String(total);
    ui::showFlashProgress(pct, "Load app", detail);
    m5os::update();
}



void hashProgressShim(size_t hashed, size_t total) { paintLaunchProgress(hashed, total, &gHashProgressCtx); }



bool canSkipFlashToCachedOta(const String& binFile, const String& sha256) {

    if (!sha256.length()) return false;



    Preferences prefs;

    if (!prefs.begin(kLaunchNs, true)) return false;

    const String lastBin = prefs.getString(kLastBinKey, "");

    const String lastSha = prefs.getString(kLastShaKey, "");

    prefs.end();



    if (lastBin != binFile || !security::sha256Equal(lastSha, sha256)) return false;



    const esp_partition_t* staging = stagingOtaPartition();

    if (!staging) return false;



    if (!verifyPartitionAppImage(staging)) return false;

    if (!validateAppImageChipTarget(staging)) return false;



    log::info("launch_skip_flash", binFile);

    return true;

}



bool tryLoadCachedDigest(const String& binFile, size_t firmwareSize, uint32_t mtime,

                         String& outDigest) {

    outDigest = "";

    Preferences prefs;

    if (!prefs.begin(kLaunchNs, true)) return false;

    if (prefs.getString(kLastBinKey, "") != binFile) {

        prefs.end();

        return false;

    }

    if (prefs.getULong(kLastSizeKey, 0) != firmwareSize) {

        prefs.end();

        return false;

    }

    if (prefs.getULong(kLastMtimeKey, 0) != mtime) {

        prefs.end();

        return false;

    }

    outDigest = prefs.getString(kLastShaKey, "");

    prefs.end();

    return security::isValidSha256Hex(outDigest);

}



void storeLaunchCache(const String& binFile, const String& sha256, size_t firmwareSize,

                      uint32_t mtime) {

    Preferences prefs;

    if (!prefs.begin(kLaunchNs, false)) return;

    prefs.putString(kLastBinKey, binFile);

    prefs.putString(kLastShaKey, sha256);

    prefs.putULong(kLastSizeKey, firmwareSize);

    prefs.putULong(kLastMtimeKey, mtime);

    prefs.end();

}



bool copySdToOta(File& firmware, size_t firmwareSize, const String& appLabel, LaunchResult& result) {
    LaunchProgressCtx ctx{appLabel, "Copy to run slot"};
    paintLoadAppDebugPhase(0, appLabel, ctx.phase);

    const esp_partition_t* staged = stagingOtaPartition();
    if (!staged) {
        result.message = "Run slot unavailable\nReflash M5 OS partition table";
        log::info("launch_slot_fail", "layout");
        return false;
    }
    if (firmwareSize > staged->size) {
        result.message = formatAppTooLargeMessage(firmwareSize, staged->size);
        log::info("launch_size_rejected", String(firmwareSize) + "/" + String(staged->size));
        return false;
    }
    if (detectMergedFlashBin(firmware, firmwareSize)) {
        result.message =
            "Merged flash bin\nUse app-only .bin\n(not 0x10000 composite)\nOr M5Burner catalog";
        log::info("launch_composite_reject", appLabel);
        return false;
    }

    OtaSlotWriter writer;
    if (!otaSlotWriterBegin(writer, staged, firmwareSize)) {
        result.message = "OTA begin failed\nReflash M5 OS";
        log::info("launch_ota_begin_fail", staged->label);
        return false;
    }

    uint8_t buffer[kIoChunkBytes];
    size_t written = 0;
    firmware.seek(0);
    while (firmware.available()) {
        const size_t n = firmware.read(buffer, sizeof(buffer));
        if (n == 0) break;
        if (!otaSlotWriterAppend(writer, buffer, n)) {
            otaSlotWriterAbort(writer);
            result.message = "Write failed — M5 OS intact";
            log::info("launch_write_fail");
            return false;
        }
        written += n;
        feedWatchdog();
        if (written == n || written % kIoChunkBytes == 0 || !firmware.available()) {
            paintLaunchProgress(written, firmwareSize, &ctx);
        }
    }

    if (written != firmwareSize) {
        otaSlotWriterAbort(writer);
        result.message = "Read incomplete " + String(written) + "/" + String(firmwareSize);
        log::info("launch_read_incomplete", String(written));
        return false;
    }

    String otaErr;
    if (!otaSlotWriterFinish(writer, &otaErr)) {
        result.message = otaErr.length() ? otaErr : "OTA finalize failed";
        log::info("launch_ota_end_fail", otaErr);
        return false;
    }

    String chipDetail;
    if (!validateAppImageChipTarget(staged, &chipDetail)) {
        result.message = "Wrong chip " + chipDetail + "\nNeed ESP32-S3 bin";
        log::info("launch_chip_fail", chipDetail);
        return false;
    }

    paintLoadAppDebugPhase(100, appLabel, "Copy OK");
    log::info("launch_copy_ok", String(staged->label) + " chip" + chipDetail);
    return true;
}



LaunchResult launchFromOpenFile(const String& path, const String& cacheKey, const String& label,
                                const FirmwarePackage* meta, bool userSkipHash) {
    LaunchResult result;

    ui::showFlashProgress(0, "Load app", label + "\nStarting...");
    m5os::update();

    if (meta && meta->needsFlashSpiffs) {
        result.message =
            "Needs SPIFFS on flash.\nUse M5Burner USB\nfull flash to run.\n(App on SD OK)";
        log::info("launch_spiffs_blocked", cacheKey);
        surfaceLaunchFailure(label, result);
        return result;
    }

    File firmware = SD.open(path.c_str());
    if (!firmware) {
        result.message = "Cannot open bin\n" + path;
        log::info("launch_open_fail", path);
        surfaceLaunchFailure(label, result);
        return result;
    }

    const size_t firmwareSize = firmware.size();
    const size_t otaLimit = maxOtaAppBytes();
    if (firmwareSize == 0 || firmwareSize > otaLimit) {
        firmware.close();
        if (firmwareSize == 0) {
            result.message = "Empty bin file";
        } else {
            result.message = formatAppTooLargeMessage(firmwareSize, otaLimit);
        }
        log::info("launch_size_rejected", cacheKey + " " + String(firmwareSize) + "/" + String(otaLimit));
        surfaceLaunchFailure(label, result);
        return result;
    }

    if (detectMergedFlashBin(firmware, firmwareSize)) {
        firmware.close();
        result.message =
            "Merged flash bin\nUse app-only .bin\n(not 0x10000 composite)\nOr M5Burner catalog";
        log::info("launch_composite_reject", cacheKey);
        surfaceLaunchFailure(label, result);
        return result;
    }

    const uint32_t fileMtime = static_cast<uint32_t>(firmware.getLastWrite());
    String sdDigest;
    bool hashSkipped = false;

    if (userSkipHash) {
        paintLoadAppPhase(100, label, "Hashing", "Fast load — skip hash");
        log::info("launch_fast_load", cacheKey);
        hashSkipped = true;
    } else {
        hashSkipped = tryLoadCachedDigest(cacheKey, firmwareSize, fileMtime, sdDigest);

        if (hashSkipped) {
            paintLoadAppPhase(100, label, "Hashing", "Unchanged — skip hash");
            log::info("launch_hash_skip", cacheKey);
        } else {
            paintLoadAppPhase(0, label, "Hashing", "Verifying SD file");
            gHashProgressCtx = {label, "Hashing"};
            sdDigest =
                security::computeFileSha256HexWithProgress(firmware, firmwareSize, hashProgressShim);
            if (!sdDigest.length()) {
                firmware.close();
                result.message = "Hash failed";
                surfaceLaunchFailure(label, result);
                return result;
            }
        }
    }

    if (!userSkipHash) {
        if (meta && meta->sha256.length()) {
            if (!security::sha256Equal(meta->sha256, sdDigest)) {
                firmware.close();
                result.message = "SHA256 mismatch";
                log::info("launch_checksum_fail", cacheKey);
                surfaceLaunchFailure(label, result);
                return result;
            }
            log::info("launch_checksum_ok", cacheKey);
        } else if (meta) {
            log::info("launch_checksum_skip", cacheKey);
        } else {
            log::info("launch_checksum_skip", cacheKey);
        }
    } else {
        log::info("launch_checksum_user_skip", cacheKey);
    }

    session::prepareLaunchSd(path, cacheKey, meta);

    if (!userSkipHash && canSkipFlashToCachedOta(cacheKey, sdDigest)) {
        firmware.close();
        paintLoadAppPhase(100, label, "Rebooting", "Already loaded");
        result.ok = true;
        result.skippedFlash = true;
        result.message = "Rebooting into gateway";
        log::info("launch_cached_ok", cacheKey);
        if (rebootIntoGatewaySession(label, result)) return result;
        return result;
    }

    firmware.close();

    firmware = SD.open(path.c_str());
    if (!firmware) {
        result.message = "Cannot reopen bin for copy\n" + path;
        log::info("launch_reopen_fail", path);
        surfaceLaunchFailure(label, result);
        return result;
    }

    if (!copySdToOta(firmware, firmwareSize, label, result)) {
        firmware.close();
        surfaceLaunchFailure(label, result);
        return result;
    }
    firmware.close();

    if (!userSkipHash) storeLaunchCache(cacheKey, sdDigest, firmwareSize, fileMtime);

    paintLoadAppDebugPhase(100, label, "Rebooting");
    result.ok = true;
    result.message = "Rebooting into gateway";
    log::info("launch_ok", cacheKey);
    log::info("launch_boot_dbg", formatOtaSlotDebug());
    if (rebootIntoGatewaySession(label, result)) return result;
    return result;
}



}  // namespace



AppLauncher::AppLauncher(FirmwareCatalog& catalog) : catalog_(catalog) {}



LaunchResult AppLauncher::launchBinFile(const String& binFile, LaunchOptions opts) {
    LaunchResult result;
    const String safeBin = security::sanitizeBinFilename(binFile);
    const String uiLabel = safeBin.length() ? safeBin : binFile;

    ui::showFlashProgress(0, "Load app", uiLabel + "\nStarting...");
    m5os::update();

    if (!safeBin.length()) {
        result.message = "Invalid bin filename";
        log::info("launch_bin_rejected");
        surfaceLaunchFailure(uiLabel, result);
        return result;
    }

    String path;
    const FirmwarePackage* meta = catalog_.findByBinFile(safeBin);
    if (meta) {
        path = catalog_.binPathForPackage(*meta);
    } else {
        path = catalog_.binPathFor(safeBin);
    }

    if (!path.length() || !SD.exists(path.c_str())) {
        result.message = "Missing " + safeBin;
        if (path.length()) result.message += "\n" + path;
        log::info("launch_missing", path);
        surfaceLaunchFailure(safeBin, result);
        return result;
    }

    return launchFromOpenFile(path, safeBin, safeBin, meta, opts.skipHash);
}



LaunchResult AppLauncher::launchBinPath(const String& sdPath, LaunchOptions opts) {
    LaunchResult result;
    String path = sdPath;
    path.trim();
    if (!path.startsWith("/")) path = "/" + path;

    const int slash = path.lastIndexOf('/');
    const String leaf = slash >= 0 ? path.substring(slash + 1) : path;

    ui::showFlashProgress(0, "Load app", leaf + "\nStarting...");
    m5os::update();

    if (!path.endsWith(".bin")) {
        result.message = "Not a .bin file";
        surfaceLaunchFailure(leaf, result);
        return result;
    }

    if (!SD.exists(path.c_str())) {
        result.message = "Missing file\n" + path;
        log::info("launch_path_missing", path);
        surfaceLaunchFailure(leaf, result);
        return result;
    }

    const String safeBin = security::sanitizeBinFilename(leaf);
    if (!safeBin.length()) {
        result.message = "Invalid bin filename";
        surfaceLaunchFailure(leaf, result);
        return result;
    }

    const FirmwarePackage* meta = catalog_.findByBinFile(safeBin);
    return launchFromOpenFile(path, path, leaf, meta, opts.skipHash);
}



LaunchResult AppLauncher::flashBurnerPackage(const FirmwarePackage& pkgIn, const String& version) {

    LaunchResult result;

    FirmwarePackage pkg = pkgIn;

    if (!pkg.fid.length()) {

        result.message = "Not a M5Burner entry";

        return result;

    }

    if (WiFi.status() != WL_CONNECTED) {

        result.message = "WiFi required";

        return result;

    }

    if (!catalog_.ensureStorage()) {

        result.message = "Insert SD to save app copy";

        return result;

    }



    burner::BurnerInstallPlan plan;

    burner::BurnerPlanError planErr;

    String pickVersion = version.length() ? version : pkg.version;

    if (pickVersion == "burner") pickVersion = "";

    if (!burner::buildInstallPlan(pkg.fid, pickVersion, plan, &planErr)) {

        result.message = planErr.message.length() ? planErr.message : String("Install info failed");

        log::info("burner_plan_fail", pkg.name);

        return result;

    }



    const String slug = pkg.slug.length() ? pkg.slug : vfs::slugFromName(pkg.name);

    if (!pkg.binFile.length()) {

        pkg.binFile = security::sanitizeBinFilename(FirmwareCatalog::slugToBinFile(pkg.name));

    }

    const String safeBin = security::sanitizeBinFilename(pkg.binFile);

    if (!safeBin.length()) {

        result.message = "Invalid bin filename";

        return result;

    }

    pkg.binFile = safeBin;

    if (!pkg.slug.length()) pkg.slug = slug;

    if (!vfs::ensureAppDirs(slug)) {

        result.message = "Cannot create /apps dir";

        return result;

    }

    const String sdPath = catalog_.binPathForPackage(pkg);



    const burner::BurnerFlashResult flash = burner::flashAppToOta(plan, sdPath);

    result.ok = flash.ok;

    result.message = flash.message;

    if (flash.ok) {
        const bool needsSpiffs = plan.requiresFlashAssets || !plan.dataSlices.empty();
        catalog_.markNeedsFlashSpiffs(pkg.fid, pkg.name, needsSpiffs);
        catalog_.scanInstalled();
        if (needsSpiffs) {
            if (SD.exists(sdPath.c_str())) {
                result.message = "Saved to\n" + sdPath + "\nUSB full flash to run";
            }
        } else if (SD.exists(sdPath.c_str())) {
            LaunchOptions opts;
            opts.skipHash = true;
            LaunchResult launch = launchBinFile(safeBin, opts);
            result.ok = launch.ok;
            result.message = launch.message.length() ? launch.message : flash.message;
        }
    }

    return result;

}



LaunchResult AppLauncher::launchByPackageName(const String& packageName) {

    if (FirmwarePackage* pkg = catalog_.findInstalledByName(packageName)) {

        return launchBinFile(pkg->binFile);

    }

    LaunchResult result;

    result.message = "App not on SD:\n" + packageName + "\nLoad from catalog or copy .bin";

    return result;

}



}  // namespace m5os

