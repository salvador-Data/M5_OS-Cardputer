#pragma once

#include "firmware_catalog.h"

#include <Arduino.h>

namespace m5os {

struct LaunchResult {
    bool ok = false;
    bool skippedFlash = false;
    String message;
};

struct LaunchOptions {
    /** Skip SHA256 before SD→OTA copy (opt-in on confirm screen). ESP magic still checked. */
    bool skipHash = false;
};

struct AppDeleteResult {
    bool ok = false;
    bool clearedRunSlot = false;
    String message;
};

class AppLauncher {
public:
    explicit AppLauncher(FirmwareCatalog& catalog);
    LaunchResult launchBinFile(const String& binFile, LaunchOptions opts = {});
    /** Load from full SD path (file explorer or arbitrary .bin location). */
    LaunchResult launchBinPath(const String& sdPath, LaunchOptions opts = {});
    LaunchResult launchByPackageName(const String& packageName);
    /** Stream LauncherHub/M5Burner firmware into OTA slot (Boris installFirmwareFromManifest subset). */
    LaunchResult flashBurnerPackage(const FirmwarePackage& pkg, const String& version = "");
    /** Delete SD app compartment + data; clears launch NVS / run slot when it matches. */
    AppDeleteResult deleteInstalledApp(const FirmwarePackage& pkg);

private:
    FirmwareCatalog& catalog_;
};

}  // namespace m5os
