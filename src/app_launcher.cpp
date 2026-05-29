#include "app_launcher.h"

#include "burner_install.h"
#include "m5burner_hookup.h"
#include "m5os_config.h"
#include "m5os_vfs.h"
#include "m5os_security.h"
#include "serial_log.h"
#include "ui_display.h"

#include <SD.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_system.h>

namespace m5os {

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
    if (firmwareSize == 0 || firmwareSize > kMaxAppBinBytes) {
        firmware.close();
        result.message = firmwareSize ? "App too large for OTA slot" : "Empty bin file";
        log::info("launch_size_rejected", safeBin);
        return result;
    }
    if (const FirmwarePackage* meta = catalog_.findByBinFile(safeBin)) {
        if (meta->sha256.length()) {
            const String digest = security::computeFileSha256Hex(firmware);
            if (!security::sha256Equal(meta->sha256, digest)) {
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

    ui::drawHeader("Flashing app");
    m5os::lcd().setCursor(4, 30);
    m5os::lcd().println(safeBin);
    m5os::lcd().setCursor(4, 44);
    m5os::lcd().printf("%u bytes", static_cast<unsigned>(firmwareSize));

    if (!Update.begin(firmwareSize)) {
        firmware.close();
        result.message = "Update.begin failed";
        log::info("launch_begin_fail", String(Update.errorString()));
        return result;
    }

    uint8_t buffer[512];
    while (firmware.available()) {
        const size_t n = firmware.read(buffer, sizeof(buffer));
        if (Update.write(buffer, n) != n) {
            Update.abort();
            firmware.close();
            result.message = "Write failed — launcher intact";
            log::info("launch_write_fail");
            return result;
        }
    }
    firmware.close();

    if (!Update.end(true)) {
        result.message = "Update.end failed — launcher intact";
        log::info("launch_end_fail", String(Update.errorString()));
        return result;
    }

    result.ok = true;
    result.message = "Rebooting into app";
    log::info("launch_ok", safeBin);
    ui::showMessage("Launch", safeBin + "\nRebooting...", TFT_GREEN, 1200);
    delay(300);
    esp_restart();
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
        result.message = "Install info failed";
        log::info("burner_plan_fail", pkg.name);
        return result;
    }
    if (plan.requiresExtraPartitions) {
        result.message = "SPIFFS/FAT not supported";
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
    result.message = "App not on SD:\n" + packageName + "\nDownload or copy .bin";
    return result;
}

}  // namespace m5os
