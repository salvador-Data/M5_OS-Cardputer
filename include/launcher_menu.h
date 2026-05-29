#pragma once

#include "app_launcher.h"
#include "firmware_catalog.h"

namespace m5os {

class LauncherMenu {
public:
    LauncherMenu(FirmwareCatalog& catalog, AppLauncher& launcher);

    void runMainLoop();
    /** ESC/` app picker — pick installed SD app to launch (multi-app switch). */
    void showAppSwitcher();
    void showInstalledApps();
    void showLoadCatalog();
    void showFlashBurnerCatalog();
    void refreshCatalog();
    void showFileExplorer(const char* path);
    void showThemeMenu();
    void showWifiSetup();
    void showSaveExportMenu();
    void showBurnerBridge();
    void showStorageCleanup();
    void showHelp();
    void showUtmsMenu();
    void exportCatalogSerial();

private:
    FirmwareCatalog& catalog_;
    AppLauncher& launcher_;
};

}  // namespace m5os
