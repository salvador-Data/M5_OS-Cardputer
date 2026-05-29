#include "app_launcher.h"

#include "burner_install.h"
#include "m5burner_hookup.h"
#include "m5os_config.h"
#include "M5OSDevice.h"
#include "m5os_flash.h"
#include "m5os_vfs.h"
#include "m5os_security.h"
#include "serial_log.h"
#include "ui_display.h"

#include <SD.h>
#include <Update.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

namespace m5os {

namespace {

constexpr char kLaunchNs[] = "m5os_launch";
constexpr char kLastBinKey[] = "last_bin";
constexpr char kLastShaKey[] = "last_sha";

struct LaunchProgressCtx {
    const char* label;
    String detailPrefix;
};

LaunchProgressCtx gHashProgressCtx;

void paintLaunchProgress(size_t done, size_t total, LaunchProgressCtx* ctx) {
    if (!ctx || !ctx->label || total == 0) return;
    const int pct = static_cast<int>(min(100ULL, (done * 100ULL) / total));
    ui::showFlashProgress(pct, ctx->label, ctx->detailPrefix + "\n" + String(done) + " / " + String(total));
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

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(staging, &state) != ESP_OK) return false;
    if (state != ESP_OTA_IMG_VALID && state != ESP_OTA_IMG_PENDING_VERIFY) return false;

    log::info("launch_skip_flash", binFile);
    return true;
}

void storeLaunchCache(const String& binFile, const String& sha256) {
    Preferences prefs;
    if (!prefs.begin(kLaunchNs, false)) return;
    prefs.putString(kLastBinKey, binFile);
    prefs.putString(kLastShaKey, sha256);
    prefs.end();
}

bool copySdToOta(File& firmware, size_t firmwareSize, const String& safeBin, LaunchResult& result) {
    LaunchProgressCtx ctx{"Copy to run slot", safeBin + "\nfrom SD (no WiFi)"};
    paintLaunchProgress(0, firmwareSize, &ctx);

    saveHomeAppPartition();
    if (!Update.begin(firmwareSize)) {
        result.message = "Update.begin failed";
        log::info("launch_begin_fail", String(Update.errorString()));
        return false;
    }

    uint8_t buffer[512];
    size_t written = 0;
    while (firmware.available()) {
        const size_t n = firmware.read(buffer, sizeof(buffer));
        if (n == 0) break;
        if (Update.write(buffer, n) != n) {
            Update.abort();
            result.message = "Write failed — launcher intact";
            log::info("launch_write_fail");
            return false;
        }
        written += n;
        if (written == n || written % 512 == 0 || !firmware.available()) {
            paintLaunchProgress(written, firmwareSize, &ctx);
        }
    }

    if (written != firmwareSize) {
        Update.abort();
        result.message = "Read incomplete " + String(written) + "/" + String(firmwareSize);
        log::info("launch_read_incomplete", String(written));
        return false;
    }

    if (!Update.end(true)) {
        result.message = "Update.end failed — launcher intact";
        log::info("launch_end_fail", String(Update.errorString()));
        return false;
    }
    return true;
}

}  // namespace

AppLauncher::AppLauncher(FirmwareCatalog& catalog) : catalog_(catalog) {}

LaunchResult AppLauncher::launchBinFile(const String& binFile) {
    LaunchResult result;
    const String safeBin = security::sanitizeBinFilename(binFile);
    if (!safeBin.length()) {
        result.message = "Invalid bin filename";
        log::info("launch_bin_rejected");
        return result;
    }

    ui::showFlashProgress(0, "Load app", safeBin + "\nPreparing...");
    m5os::update();

    String path;
    if (const FirmwarePackage* meta = catalog_.findByBinFile(safeBin)) {
        path = catalog_.binPathForPackage(*meta);
    } else {
        path = catalog_.binPathFor(safeBin);
    }
    if (!path.length() || !SD.exists(path.c_str())) {
        result.message = "Missing " + safeBin;
        log::info("launch_missing", path);
        return result;
    }

    if (const FirmwarePackage* meta = catalog_.findByBinFile(safeBin)) {
        if (meta->needsFlashSpiffs) {
            result.message =
                "Needs SPIFFS on flash.\nUse M5Burner USB\nfull flash to run.\n(App on SD OK)";
            log::info("launch_spiffs_blocked", safeBin);
            return result;
        }
    }

    File firmware = SD.open(path.c_str());
    if (!firmware) {
        result.message = "Cannot open bin";
        log::info("launch_open_fail", path);
        return result;
    }

    const size_t firmwareSize = firmware.size();
    const size_t otaLimit = maxOtaAppBytes();
    if (firmwareSize == 0 || firmwareSize > otaLimit) {
        firmware.close();
        result.message = firmwareSize ? "App too large for OTA slot" : "Empty bin file";
        log::info("launch_size_rejected", safeBin);
        return result;
    }

    ui::showFlashProgress(0, "Hashing", safeBin + "\nVerifying SD file");
    m5os::update();
    gHashProgressCtx = {"Hashing", safeBin + "\nVerifying SD file"};
    const String sdDigest =
        security::computeFileSha256HexWithProgress(firmware, firmwareSize, hashProgressShim);
    if (!sdDigest.length()) {
        firmware.close();
        result.message = "Hash failed";
        return result;
    }

    if (const FirmwarePackage* meta = catalog_.findByBinFile(safeBin)) {
        if (meta->sha256.length()) {
            if (!security::sha256Equal(meta->sha256, sdDigest)) {
                firmware.close();
                result.message = "SHA256 mismatch";
                log::info("launch_checksum_fail", safeBin);
                return result;
            }
            log::info("launch_checksum_ok", safeBin);
        } else {
            log::info("launch_checksum_skip", safeBin);
        }
    } else {
        log::info("launch_checksum_skip", safeBin);
    }

    if (canSkipFlashToCachedOta(safeBin, sdDigest)) {
        ui::showFlashProgress(100, "Load app", safeBin + "\nAlready loaded — reboot");
        m5os::update();
        result.ok = true;
        result.skippedFlash = true;
        result.message = "Rebooting into app";
        log::info("launch_cached_ok", safeBin);
        ui::showMessage("Load app", safeBin + "\nRun slot ready\nRebooting...", TFT_GREEN, 900);
        if (launchStagedAppSession()) return result;
        result.ok = false;
        result.message = "Boot staged app failed";
        return result;
    }

    if (!copySdToOta(firmware, firmwareSize, safeBin, result)) {
        firmware.close();
        return result;
    }
    firmware.close();

    storeLaunchCache(safeBin, sdDigest);

    result.ok = true;
    result.message = "Rebooting into app";
    log::info("launch_ok", safeBin);
    ui::showMessage("Load app", safeBin + "\nRebooting...", TFT_GREEN, 1200);
    if (launchStagedAppSession()) return result;
    result.ok = false;
    result.message = "Boot staged app failed";
    return result;
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
    if (!burner::enrichPackageFromBurner(pkg)) {
        result.message = "Version info failed";
        log::info("burner_enrich_fail", pkg.name);
        return result;
    }

    burner::BurnerInstallPlan plan;
    const String pickVersion = version.length() ? version : pkg.version;
    if (!burner::buildInstallPlan(pkg.fid, pickVersion == "burner" ? "" : pickVersion, plan)) {
        if (plan.appSize > maxOtaAppBytes()) {
            result.message = "App too large for OTA slot";
        } else {
            result.message = "Install info failed";
        }
        log::info("burner_plan_fail", pkg.name);
        return result;
    }

    const String slug = pkg.slug.length() ? pkg.slug : vfs::slugFromName(pkg.name);
    const String safeBin = security::sanitizeBinFilename(pkg.binFile);
    String sdPath;
    if (safeBin.length() && vfs::ensureAppDirs(slug)) {
        sdPath = catalog_.binPathForPackage(pkg);
    }

    const burner::BurnerFlashResult flash = burner::flashAppToOta(plan, sdPath);
    result.ok = flash.ok;
    result.message = flash.message;
    if (flash.ok) {
        catalog_.markNeedsFlashSpiffs(pkg.fid, pkg.name,
                                      plan.requiresFlashAssets || !plan.dataSlices.empty());
        catalog_.scanInstalled();
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
