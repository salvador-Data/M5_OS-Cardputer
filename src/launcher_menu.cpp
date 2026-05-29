#include "launcher_menu.h"

#include "burner_bridge.h"
#include "m5os_config.h"
#include "m5os_gc.h"
#include "m5os_settings.h"
#include "m5os_vfs.h"
#include "serial_log.h"
#include "power_manager.h"
#include "ui_display.h"
#include "wifi_manager.h"

#include <ArduinoJson.h>
#include <SD.h>

namespace m5os {

namespace {

void showSdRequired(const String& action = "") {
    String body = "Insert SD to save";
    if (action.length()) body = action + "\n" + body;
    const String sdDetail = vfs::lastMountError();
    if (sdDetail.length()) body += "\n" + sdDetail;
    body += "\n/contacts away from screen";
    ui::showMessage("No SD", body, TFT_YELLOW);
}

}  // namespace

LauncherMenu::LauncherMenu(FirmwareCatalog& catalog, AppLauncher& launcher)
    : catalog_(catalog), launcher_(launcher) {}

void LauncherMenu::exportCatalogSerial() {
    JsonDocument doc;
    doc["app"] = "M5_OS-Cardputer";
    doc["event"] = "catalog_export";
    JsonArray installed = doc["installed"].to<JsonArray>();
    for (const auto& pkg : catalog_.installed()) {
        JsonObject row = installed.add<JsonObject>();
        row["name"] = pkg.name;
        row["bin"] = pkg.binFile;
        row["version"] = pkg.version;
    }
    JsonArray available = doc["available"].to<JsonArray>();
    for (const auto& pkg : catalog_.available()) {
        JsonObject row = available.add<JsonObject>();
        row["name"] = pkg.name;
        row["version"] = pkg.version;
        row["installed"] = pkg.installed;
        row["description"] = pkg.description;
    }
    String out;
    serializeJson(doc, out);
    Serial.println(out);
    log::info("catalog_exported");
}

void LauncherMenu::showInstalledApps() {
    if (!settings::ensureSdMounted()) {
        showSdRequired("Launch needs SD");
        return;
    }
    catalog_.scanInstalled();
    std::vector<String> labels;
    for (const auto& pkg : catalog_.installed()) {
        String line = pkg.name;
        if (pkg.version.length()) line += " v" + pkg.version;
        labels.push_back(line);
    }
    const int pick = ui::selectFromList(labels, "Launch app");
    if (pick < 0) return;
    const auto& pkg = catalog_.installed()[pick];
    ui::drawHeader("Launch app");
    m5os::lcd().setCursor(4, 28);
    m5os::lcd().println(pkg.name);
    m5os::lcd().setCursor(4, 44);
    m5os::lcd().println(pkg.description.length() ? pkg.description : "Field firmware");
    m5os::lcd().setCursor(4, 64);
    m5os::lcd().print("Enter flash app slot");
    m5os::lcd().setCursor(4, 78);
    m5os::lcd().print("` cancel  (M5 OS on SD)");

    while (true) {
        m5os::update();
        Buttons keys = m5os::readButtons();
        if (keys.back) return;
        if (keys.ok) {
            LaunchResult result = launcher_.launchBinFile(pkg.binFile);
            if (!result.ok) ui::showMessage("Launch failed", result.message, TFT_RED);
            return;
        }
        delay(power::uiLoopDelayMs());
    }
}

void LauncherMenu::showDownloadCatalog() {
    if (!settings::ensureSdMounted()) {
        showSdRequired("Download needs SD");
        return;
    }
    std::vector<String> labels;
    for (const auto& pkg : catalog_.available()) {
        String line = pkg.name + " v" + pkg.version;
        if (pkg.installed) line += " [SD]";
        labels.push_back(line);
    }
    const int pick = ui::selectFromList(labels, "Download");
    if (pick < 0) return;
    const FirmwarePackage& pkg = catalog_.available()[pick];
    if (pkg.installed) {
        ui::showMessage("Download", pkg.name + "\nAlready on SD", TFT_YELLOW);
        return;
    }
    if (!wifiIsConnected()) {
        ui::showMessage("Download", "WiFi required", TFT_RED);
        return;
    }
    ui::drawHeader("Downloading");
    m5os::lcd().setCursor(4, 30);
    m5os::lcd().println(pkg.name);
    if (catalog_.downloadPackage(pkg)) {
        const String path = catalog_.binPathForPackage(pkg);
        String body = pkg.name;
        if (path.length()) body += "\nSaved to\n" + path;
        ui::showMessage("Installed", body, TFT_GREEN, 2200);
    } else {
        String body = "Download failed";
        if (!vfs::isMounted()) body = "Insert SD to save";
        ui::showMessage("Error", body, TFT_RED);
    }
}

void LauncherMenu::refreshCatalog() {
    bool ok = false;
    if (wifiIsConnected()) {
        ok = catalog_.refreshFromNetwork(kDefaultManifestUrl);
    }
    if (!ok) ok = catalog_.refreshFromSdManifest();
    if (ok) {
        ui::showMessage("Catalog", String(catalog_.available().size()) + " packages", TFT_GREEN);
    } else {
        ui::showMessage("Catalog", "WiFi or SD manifest\nrequired", TFT_RED);
    }
}

void LauncherMenu::showFileExplorer(const char* path) {
    if (!vfs::isMounted()) {
        const vfs::MountResult remount = vfs::mountAndInit();
        if (!remount.ok) {
            String body = remount.message.length() ? remount.message : vfs::lastMountError();
            if (!body.length()) body = "Insert FAT32 microSD";
            body += "\n/contacts away from screen";
            ui::showMessage("No SD", body, TFT_YELLOW);
            return;
        }
        catalog_.scanInstalled();
    }

    String dirPath = path && path[0] ? String(path) : "/";
    if (!dirPath.startsWith("/")) dirPath = "/" + dirPath;

    std::vector<String> entries;
    entries.push_back("[..]");
    File dir = SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) {
        String body = "Cannot open:\n" + dirPath;
        if (vfs::lastMountError().length()) body += "\n" + vfs::lastMountError();
        ui::showMessage("File explorer", body, TFT_RED);
        if (dir) dir.close();
        return;
    }
    File entry;
    while ((entry = dir.openNextFile())) {
        String leaf = vfs::entryBaseName(entry.name());
        if (!leaf.length() || leaf == "." || leaf == "..") {
            entry.close();
            continue;
        }
        if (entry.isDirectory()) leaf += "/";
        entries.push_back(leaf);
        entry.close();
    }
    dir.close();

    const int pick = ui::selectFromList(entries, dirPath.c_str());
    if (pick < 0) return;
    if (pick == 0) {
        if (dirPath == "/") return;
        const int slash = dirPath.lastIndexOf('/');
        if (slash <= 0) {
            showFileExplorer("/");
        } else {
            showFileExplorer(dirPath.substring(0, slash).c_str());
        }
        return;
    }
    String chosen = entries[pick];
    if (chosen.endsWith("/")) {
        chosen.remove(chosen.length() - 1);
        showFileExplorer(vfs::joinPath(dirPath, chosen).c_str());
    } else {
        ui::showMessage("File", vfs::joinPath(dirPath, chosen));
    }
}

void LauncherMenu::showThemeMenu() {
    static const char* themes[] = {"Baby Blue", "Hacker Green", "Mr. Robot Red", "Hacker Planet"};
    std::vector<String> labels;
    for (auto* t : themes) labels.push_back(t);
    const int pick = ui::selectFromList(labels, "Theme", ui::getThemePreset());
    if (pick < 0) return;
    ui::setThemePreset(pick);
    if (settings::saveTheme(pick)) {
        ui::showMessage("Theme saved", String(themes[pick]) + "\n" + vfs::kSettingsPath, TFT_GREEN, 1400);
    } else {
        showSdRequired("Theme active until reboot");
    }
}

void LauncherMenu::showSaveExportMenu() {
    static const char* items[] = {
        "Export /var/log snapshot",
        "Backup settings.json",
    };
    std::vector<String> labels;
    for (auto* item : items) labels.push_back(item);
    const int pick = ui::selectFromList(labels, "Save / export");
    if (pick < 0) return;
    if (!settings::ensureSdMounted()) {
        showSdRequired("Save / export");
        return;
    }
    String outPath;
    bool ok = false;
    if (pick == 0) {
        ok = settings::exportLogSnapshot(&outPath);
    } else {
        ok = settings::exportSettingsSnapshot(&outPath);
    }
    if (ok && outPath.length()) {
        ui::showMessage("Saved", outPath, TFT_GREEN, 2000);
    } else if (ok) {
        ui::showMessage("Saved", vfs::kSavesDir, TFT_GREEN);
    } else {
        ui::showMessage("Save failed", "Check SD free space", TFT_RED);
    }
}

void LauncherMenu::showWifiSetup() {
    static char ssid[33] = "";
    static char pass[65] = "";
    wifiConnectInteractive(ssid, sizeof(ssid), pass, sizeof(pass));
}

void LauncherMenu::showBurnerBridge() {
    burner::showHelpScreen();
    ui::showMessage("Recovery", burner::recoveryInstructions());
}

void LauncherMenu::showStorageCleanup() {
    ui::drawHeader("Storage cleanup");
    m5os::lcd().setCursor(4, 28);
    m5os::lcd().setTextColor(TFT_WHITE, TFT_BLACK);
    m5os::lcd().println("Sweep /tmp, rotate logs,");
    m5os::lcd().println("reclaim cache orphans.");
    m5os::lcd().setCursor(4, 64);
    m5os::lcd().setTextColor(TFT_DARKGREY, TFT_BLACK);
    m5os::lcd().print("Enter confirm  ` cancel");

    while (true) {
        m5os::update();
        Buttons keys = m5os::readButtons();
        if (keys.back) return;
        if (keys.ok) {
            const auto slugs = catalog_.whitelistedAppSlugs();
            const m5os::gc::GcReport report = m5os::gc::fullCleanup(true, slugs);
            String body = String(report.tmpRemoved) + " tmp, ";
            body += String(report.cacheRemoved) + " cache, ";
            body += String(report.logsRotated) + " logs";
            body += "\n~" + String(report.bytesReclaimed / 1024) + " KB";
            ui::showMessage("Cleanup done", body, TFT_GREEN, 2400);
            return;
        }
        delay(power::uiLoopDelayMs());
    }
}

void LauncherMenu::showHelp() { ui::drawHelpOverlay(); }

void LauncherMenu::runMainLoop() {
    static const char* items[] = {
        "Launch installed app",
        "Download from catalog",
        "Refresh manifest",
        "Storage cleanup",
        "Export catalog (serial)",
        "File explorer",
        "Save / export to SD",
        "Theme",
        "WiFi setup",
        "M5Burner / recovery",
        "Keyboard shortcuts",
    };
    std::vector<String> labels;
    for (auto* item : items) labels.push_back(item);

    while (true) {
        const int pick = ui::selectFromList(labels, "M5 OS Main");
        if (pick < 0) return;
        switch (pick) {
            case 0:
                showInstalledApps();
                break;
            case 1:
                showDownloadCatalog();
                break;
            case 2:
                refreshCatalog();
                break;
            case 3:
                showStorageCleanup();
                break;
            case 4:
                exportCatalogSerial();
                ui::showMessage("Exported", "Catalog on USB serial", TFT_GREEN, 900);
                break;
            case 5:
                showFileExplorer("/");
                break;
            case 6:
                showSaveExportMenu();
                break;
            case 7:
                showThemeMenu();
                break;
            case 8:
                showWifiSetup();
                break;
            case 9:
                showBurnerBridge();
                break;
            case 10:
                showHelp();
                break;
            default:
                break;
        }
    }
}

}  // namespace m5os
