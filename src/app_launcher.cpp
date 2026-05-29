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

bool tryBootCachedOta(const String& binFile, const String& sha256) {
    if (!sha256.length()) return false;

    Preferences prefs;
    if (!prefs.begin(kLaunchNs, true)) return false;
    const String lastBin = prefs.getString(kLastBinKey, "");
    const String lastSha = prefs.getString(kLastShaKey, "");
    prefs.end();

    if (lastBin != binFile || !security::sha256Equal(lastSha, sha256)) return false;

    const esp_partition_t* updatePart = esp_ota_get_next_update_partition(nullptr);
    if (!updatePart) return false;

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(updatePart, &state) != ESP_OK) return false;
    if (state != ESP_OTA_IMG_VALID && state != ESP_OTA_IMG_PENDING_VERIFY) return false;

    if (esp_ota_set_boot_partition(updatePart) != ESP_OK) return false;
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
    const String path = catalog_.binPathFor(safeBin);
    if (!path.length() || !SD.exists(path.c_str())) {
        result.message = "Missing " + safeBin;
        log::info("launch_missing", path);
        return result;
    }

    File firmware = SD.open(path.c_str());
    if (!firmware) {
        result.message = "Cannot open bin";
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

    String sdDigest = security::computeFileSha256Hex(firmware);
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
    firmware.close();

    ui::showFlashProgress(0, "Launch app", safeBin + "\nSD -> run slot");
    m5os::update();

    if (tryBootCachedOta(safeBin, sdDigest)) {
        ui::showFlashProgress(100, "Launch app", safeBin + "\nAlready loaded — reboot");
        result.ok = true;
        result.skippedFlash = true;
        result.message = "Rebooting into app";
        log::info("launch_cached_ok", safeBin);
        ui::showMessage("Launch", safeBin + "\nRun slot ready\nRebooting...", TFT_GREEN, 900);
        if (launchStagedAppSession()) return result;
        result.ok = false;
        result.message = "Boot staged app failed";
        return result;
    }

    firmware = SD.open(path.c_str());
    if (!firmware) {
        result.message = "Cannot reopen bin";
        return result;
    }

    ui::showFlashProgress(0, "Copy to run slot", safeBin + "\nfrom SD (no WiFi)");
    m5os::update();

    saveHomeAppPartition();
    if (!Update.begin(firmwareSize)) {
        firmware.close();
        result.message = "Update.begin failed";
        log::info("launch_begin_fail", String(Update.errorString()));
        return result;
    }

    uint8_t buffer[512];
    size_t written = 0;
    while (firmware.available()) {
        const size_t n = firmware.read(buffer, sizeof(buffer));
        if (Update.write(buffer, n) != n) {
            Update.abort();
            firmware.close();
            result.message = "Write failed — launcher intact";
            log::info("launch_write_fail");
            return result;
        }
        written += n;
        if (written == n || written % 512 == 0 || !firmware.available()) {
            const int pct = static_cast<int>(min(100ULL, (written * 100ULL) / firmwareSize));
            ui::showFlashProgress(pct, "Copy to run slot",
                                  String(written) + " / " + String(firmwareSize));
            m5os::update();
        }
    }
    firmware.close();

    if (!Update.end(true)) {
        result.message = "Update.end failed — launcher intact";
        log::info("launch_end_fail", String(Update.errorString()));
        return result;
    }

    storeLaunchCache(safeBin, sdDigest);

    result.ok = true;
    result.message = "Rebooting into app";
    log::info("launch_ok", safeBin);
    ui::showMessage("Launch", safeBin + "\nRebooting...", TFT_GREEN, 1200);
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
    if (flash.ok) catalog_.scanInstalled();
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
