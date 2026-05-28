#pragma once

#include "app_launcher.h"
#include "firmware_catalog.h"

namespace m5os {

class LauncherMenu {
public:
    LauncherMenu(FirmwareCatalog& catalog, AppLauncher& launcher);

    void runMainLoop();
    void showInstalledApps();
    void showDownloadCatalog();
    void refreshCatalog();
    void showFileExplorer(const char* path);
    void showThemeMenu();
    void showWifiSetup();
    void showBurnerBridge();
    void showStorageCleanup();
    void showHelp();
    void exportCatalogSerial();

private:
    FirmwareCatalog& catalog_;
    AppLauncher& launcher_;
};

}  // namespace m5os
