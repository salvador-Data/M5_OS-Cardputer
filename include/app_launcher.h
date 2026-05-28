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

private:
    FirmwareCatalog& catalog_;
};

}  // namespace m5os
