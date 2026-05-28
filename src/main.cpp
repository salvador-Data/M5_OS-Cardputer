/**
 * M5 OS — Cardputer edition (modular launcher)
 * Hacker Planet LLC / salvador-Data
 */

#include <Arduino.h>

#include "app_launcher.h"
#include "firmware_catalog.h"
#include "launcher_menu.h"
#include "m5os_config.h"
#include "m5os_gc.h"
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
    m5os::ui::bootIntroBegin();

    if (!gCatalog.ensureStorage()) {
        m5os::ui::bootIntroStage(m5os::ui::BootStage::MountSd, "SD missing");
        m5os::ui::bootIntroFinish();
        m5os::ui::showMessage("Error", "Insert FAT32 SD\n/system /apps /home", TFT_RED, 3200);
    } else {
        m5os::ui::bootIntroStage(m5os::ui::BootStage::MountSd, "VFS ready");

        gCatalog.scanInstalled();
        bool manifestOk = false;
        if (m5os::wifiIsConnected()) {
            manifestOk = gCatalog.refreshFromNetwork(m5os::kDefaultManifestUrl);
        }
        if (!manifestOk) manifestOk = gCatalog.refreshFromSdManifest();
        m5os::ui::bootIntroStage(m5os::ui::BootStage::LoadManifest,
                                 manifestOk ? String(gCatalog.available().size()) + " apps" : "offline mode");

        const m5os::gc::GcReport gc = m5os::gc::quickBootScan();
        m5os::ui::bootIntroStage(m5os::ui::BootStage::GcScan,
                                 String(gc.tmpRemoved) + " tmp " + String(gc.logsRotated) + " logs");

        if (!manifestOk) {
            m5os::log::info("catalog_deferred", "Connect WiFi or add /apps/manifest.json");
        }
        m5os::ui::bootIntroFinish();
    }

    gMenu.runMainLoop();
}

void loop() {
    m5os::update();
    delay(50);
}
