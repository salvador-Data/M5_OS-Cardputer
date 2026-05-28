#include "app_launcher.h"

#include "m5os_config.h"
#include "serial_log.h"
#include "ui_display.h"

#include <SD.h>
#include <Update.h>
#include <esp_system.h>

namespace m5os {

AppLauncher::AppLauncher(FirmwareCatalog& catalog) : catalog_(catalog) {}

LaunchResult AppLauncher::launchBinFile(const String& binFile) {
    LaunchResult result;
    const String path = catalog_.binPathFor(binFile);
    if (!SD.exists(path.c_str())) {
        result.message = "Missing " + binFile;
        log::info("launch_missing", path);
        return result;
    }

    File firmware = SD.open(path.c_str());
    if (!firmware) {
        result.message = "Cannot open bin";
        return result;
    }

    const size_t firmwareSize = firmware.size();
    ui::drawHeader("Flashing app");
    m5os::lcd().setCursor(4, 30);
    m5os::lcd().println(binFile);
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
            result.message = "Write failed";
            log::info("launch_write_fail");
            return result;
        }
    }
    firmware.close();

    if (!Update.end(true)) {
        result.message = "Update.end failed";
        log::info("launch_end_fail", String(Update.errorString()));
        return result;
    }

    result.ok = true;
    result.message = "Rebooting into app";
    log::info("launch_ok", binFile);
    ui::showMessage("Launch", binFile + "\nRebooting...", TFT_GREEN, 1200);
    delay(300);
    esp_restart();
    return result;
}

LaunchResult AppLauncher::launchByPackageName(const String& packageName) {
    if (FirmwarePackage* pkg = catalog_.findInstalledByName(packageName)) {
        return launchBinFile(pkg->binFile);
    }
    LaunchResult result;
    result.message = "Not installed: " + packageName;
    return result;
}

}  // namespace m5os
