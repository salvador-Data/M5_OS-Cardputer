#pragma once

#include "firmware_catalog.h"

#include <Arduino.h>

namespace m5os {

struct LaunchResult {
    bool ok = false;
    String message;
};

class AppLauncher {
public:
    explicit AppLauncher(FirmwareCatalog& catalog);
    LaunchResult launchBinFile(const String& binFile);
    LaunchResult launchByPackageName(const String& packageName);
    /** Stream LauncherHub/M5Burner firmware into OTA slot (Boris installFirmwareFromManifest subset). */
    LaunchResult flashBurnerPackage(const FirmwarePackage& pkg, const String& version = "");

private:
    FirmwareCatalog& catalog_;
};

}  // namespace m5os
