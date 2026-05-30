/**

 * M5 OS — Cardputer edition (modular launcher)

 * Hacker Planet LLC / salvador-Data

 */



#include <Arduino.h>



#include "app_launcher.h"

#include "firmware_catalog.h"

#include "launcher_menu.h"

#include "m5os_config.h"

#include "m5os_settings.h"

#include "m5os_vfs.h"

#include "m5os_gc.h"

#include "m5os_session.h"

#include "serial_log.h"

#include "ui_display.h"

#include "utms.h"

#include "wifi_manager.h"



#include "M5OSDevice.h"

#include "m5os_flash.h"

#include "m5os_gateway.h"

#include "stamp_glow.h"



static m5os::FirmwareCatalog gCatalog;

static m5os::AppLauncher gLauncher(gCatalog);

static m5os::LauncherMenu gMenu(gCatalog, gLauncher);



void setup() {

    m5os::log::begin();

    m5os::begin();

    m5os::stamp::begin();

    m5os::tryEarlyRecoveryBoot();

    m5os::logBootPartitionContext();

    m5os::applyColdBootHomeRestore();

    m5os::applyCrashResetHomeRestore();

    m5os::tryHandleLaunchSnapBack();



    const bool sessionReturn = m5os::session::isSessionReturnBoot();

    if (sessionReturn) {

        gCatalog.ensureStorage();

        m5os::session::promptSaveSessionAndRestoreHome();

        m5os::ui::showMessage("M5 OS", "Session ended\nBack in launcher", TFT_GREEN, 1400);

    }



    m5os::saveHomeAppPartition();

    if (!m5os::gatewayPartitionReady()) {
        m5os::flashEmbeddedGatewayIfNeeded();
    }

    m5os::beginWatchdog();

    m5os::power::begin();



    const bool sdOk = gCatalog.ensureStorage();

    m5os::ui::bootIntroBegin();



    if (m5os::consumeLaunchHandoffFailure()) {

        String detail = m5os::consumeLaunchFailDetail();

        String msg = "Run slot invalid\nRe-copy from SD";

        if (detail == "no_target") msg = "No valid app\nin run slot (app2)";

        else if (detail == "set_boot") msg = "Boot partition\nswitch failed";

        else if (detail == "already_running") msg = "Already on run slot\nRe-copy from SD";

        else if (detail == "otadata") msg = "otadata update failed\nReflash M5 OS";

        else if (detail == "image_verify") msg = "App image invalid\nWrong chip or corrupt bin";

        else if (detail == "snapback") msg = "App did not start\nNeed ESP32-S3 bin";

        else if (detail.length()) msg = detail;

        m5os::ui::showMessage("Load app failed", msg, TFT_RED, 2500);

    }



    if (!sdOk) {

        const String sdDetail = m5os::vfs::lastMountError().length() ? m5os::vfs::lastMountError() : "SD offline";

        m5os::log::info("boot_sd_offline", sdDetail);

        m5os::ui::bootIntroStage(m5os::ui::BootStage::MountSd, sdDetail);

        m5os::ui::bootIntroFinish();

        m5os::ui::showMessage("SD offline", sdDetail + "\nMenu works without it", TFT_YELLOW, 1500);

    } else {

        m5os::ui::bootIntroStage(m5os::ui::BootStage::MountSd, "VFS ready");



        if (m5os::settings::load()) {

            m5os::ui::setThemePreset(m5os::settings::themePreset());

            m5os::wifiTrySavedConnect();

        }



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

        m5os::utms::maybeAutoCheckOnBoot();

    }



    gMenu.runMainLoop();

}



void loop() {

    m5os::update();

    delay(m5os::power::uiLoopDelayMs() / 2);

}

