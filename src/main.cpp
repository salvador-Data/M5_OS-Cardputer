/**
 * M5 OS — Cardputer edition (modular launcher)
 * Hacker Planet LLC / salvador-Data
 */

#include <Arduino.h>

#include "app_launcher.h"
#include "firmware_catalog.h"
#include "launcher_menu.h"
#include "m5os_config.h"
#include "serial_log.h"
#include "ui_display.h"
#include "wifi_manager.h"

#include "M5OSDevice.h"

static m5os::FirmwareCatalog gCatalog;
static m5os::AppLauncher gLauncher(gCatalog);
static m5os::LauncherMenu gMenu(gCatalog, gLauncher);

void setup() {
    m5os::log::begin();
    m5os::begin();
    m5os::ui::introSplash();

    if (!gCatalog.ensureStorage()) {
        m5os::ui::showMessage("Error", "SD card missing", TFT_RED, 2200);
    } else {
        gCatalog.scanInstalled();
        if (m5os::wifiIsConnected()) {
            gCatalog.refreshFromNetwork(m5os::kDefaultManifestUrl);
        } else if (!gCatalog.refreshFromSdManifest()) {
            m5os::log::info("catalog_deferred", "Connect WiFi or add /manifest.json");
        }
    }

    gMenu.runMainLoop();
}

void loop() {
    m5os::update();
    delay(50);
}
