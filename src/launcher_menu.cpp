#include "launcher_menu.h"

#include "burner_install.h"
#include "burner_bridge.h"
#include "m5os_config.h"
#include "M5OSDevice.h"
#include "m5os_gc.h"
#include "m5os_settings.h"
#include "m5os_vfs.h"
#include "serial_log.h"
#include "power_manager.h"
#include "ui_display.h"
#include "wifi_manager.h"

#include <ArduinoJson.h>
#include <M5Cardputer.h>
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

void LauncherMenu::showAppSwitcher() {
    m5os::keyboardDrainBack();
    if (!settings::ensureSdMounted()) {
        showSdRequired("Switch app needs SD");
        return;
    }
    catalog_.scanInstalled();
    const auto& installed = catalog_.installed();
    if (installed.empty()) {
        ui::showMessage("Switch app", "No apps on SD\nLoad from catalog", TFT_YELLOW);
        return;
    }

    int index = 0;
    int scroll = 0;
    int lastIndex = -1;
    int lastScroll = -1;
    constexpr int kVisible = 6;

    auto redrawSwitcher = [&]() {
        ui::drawHeader("Switch app");
        if (index < scroll) scroll = index;
        if (index >= scroll + kVisible) scroll = index - kVisible + 1;

        for (int row = 0; row < kVisible; ++row) {
            const int i = scroll + row;
            const int y = 24 + row * 14;
            m5os::lcd().fillRect(4, y - 2, m5os::lcd().width() - 8, 14, TFT_BLACK);
            if (i >= static_cast<int>(installed.size())) continue;
            m5os::lcd().setCursor(8, y);
            String line = installed[i].name;
            if (installed[i].version.length()) line += " v" + installed[i].version;
            if (i == index) {
                m5os::lcd().setTextColor(ui::theme().primary, ui::theme().secondary);
                m5os::lcd().printf("> %s", line.c_str());
            } else {
                m5os::lcd().setTextColor(ui::theme().secondary, TFT_BLACK);
                m5os::lcd().printf("  %s", line.c_str());
            }
        }

        m5os::lcd().setTextColor(TFT_WHITE, TFT_BLACK);
        m5os::lcd().setCursor(4, 108);
        m5os::lcd().print("Enter launch  Tab next");
        m5os::lcd().setTextColor(TFT_DARKGREY, TFT_BLACK);
        m5os::lcd().setCursor(4, 118);
        m5os::lcd().print("ESC/` confirm back");
        lastIndex = index;
        lastScroll = scroll;
    };

    redrawSwitcher();

    while (true) {
        if (index != lastIndex || scroll != lastScroll) redrawSwitcher();

        m5os::update();
        Buttons keys = ui::readButtonsExtended();
        if (keys.help) {
            ui::drawHelpOverlay();
            continue;
        }
        if (keys.up) index = (index > 0) ? index - 1 : static_cast<int>(installed.size()) - 1;
        if (keys.down) index = (index + 1) % static_cast<int>(installed.size());
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            if (M5Cardputer.Keyboard.keysState().tab) {
                index = (index + 1) % static_cast<int>(installed.size());
            }
        }
        if (keys.back || m5os::keyboardBackJustPressed()) return;
        if (keys.ok) {
            const auto& pkg = installed[index];
            ui::drawHeader("Launch app");
            m5os::lcd().setCursor(4, 28);
            m5os::lcd().println(pkg.name);
            m5os::lcd().setCursor(4, 44);
            m5os::lcd().println(pkg.description.length() ? pkg.description : "Field firmware");
            m5os::lcd().setCursor(4, 64);
            m5os::lcd().print("Enter copy to run slot");
            m5os::lcd().setCursor(4, 78);
            m5os::lcd().print("ESC/` confirm back");

            m5os::keyboardDrainBack();
            while (true) {
                m5os::update();
                if (m5os::keyboardBackJustPressed()) break;
                Buttons confirm = m5os::readButtons();
                if (confirm.back) break;
                if (confirm.ok) {
                    LaunchResult result = launcher_.launchBinFile(pkg.binFile);
                    if (!result.ok) ui::showMessage("Launch failed", result.message, TFT_RED);
                    return;
                }
                delay(power::uiLoopDelayMs());
            }
            redrawSwitcher();
            continue;
        }
        delay(power::uiLoopDelayMs());
    }
}

void LauncherMenu::showInstalledApps() { showAppSwitcher(); }

void LauncherMenu::showLoadCatalog() {
    if (!settings::ensureSdMounted()) {
        showSdRequired("Load needs SD");
        return;
    }
    std::vector<String> labels;
    for (const auto& pkg : catalog_.available()) {
        String line = pkg.name + " v" + pkg.version;
        if (pkg.installed) line += " [SD]";
        labels.push_back(line);
    }
    const int pick = ui::selectFromList(labels, "Load from catalog");
    if (pick < 0) return;
    const FirmwarePackage& pkg = catalog_.available()[pick];
    if (pkg.installed) {
        ui::showMessage("Load", pkg.name + "\nAlready on SD", TFT_YELLOW);
        return;
    }
    if (!wifiIsConnected()) {
        ui::showMessage("Load", "WiFi required", TFT_RED);
        return;
    }
    ui::showFlashProgress(0, "Loading", pkg.name);
    m5os::update();
    if (catalog_.downloadPackage(pkg)) {
        const String path = catalog_.binPathForPackage(pkg);
        String body = pkg.name;
        if (path.length()) body += "\nSaved to\n" + path;
        ui::showFlashProgress(100, "Loading", body);
        ui::showMessage("Loaded", body, TFT_GREEN, 2200);
    } else {
        String body = catalog_.lastDownloadError().length() ? catalog_.lastDownloadError()
                                                            : String("Load failed");
        if (!vfs::isMounted()) body = "Insert SD to save";
        ui::showMessage("Load failed", body, TFT_RED);
    }
}

void LauncherMenu::showFlashBurnerCatalog() {
    if (!settings::ensureSdMounted()) {
        showSdRequired("Flash saves app to SD");
        return;
    }
    if (!wifiIsConnected()) {
        ui::showMessage("M5Burner flash", "WiFi required", TFT_RED);
        return;
    }

    std::vector<FirmwarePackage> burnerEntries;
    for (const auto& pkg : catalog_.available()) {
        if (pkg.fid.length()) burnerEntries.push_back(pkg);
    }
    if (burnerEntries.empty()) {
        ui::showMessage("M5Burner flash", "Refresh manifest first\n(no LauncherHub entries)", TFT_YELLOW);
        return;
    }

    std::vector<String> labels;
    for (const auto& pkg : burnerEntries) {
        String line = pkg.name;
        if (pkg.version.length() && pkg.version != "burner") line += " v" + pkg.version;
        if (pkg.installed) line += " [SD]";
        labels.push_back(line);
    }
    const int pick = ui::selectFromList(labels, "M5Burner flash");
    if (pick < 0) return;

    const FirmwarePackage& pkg = burnerEntries[pick];
    String version = pkg.version;
    if (version == "burner" || !version.length()) version = "";

    std::vector<burner::BurnerVersionInfo> versions;
    if (burner::fetchVersionList(pkg.fid, versions) && versions.size() > 1) {
        std::vector<String> versionLabels;
        for (const auto& info : versions) versionLabels.push_back(info.version);
        const int versionPick = ui::selectFromList(versionLabels, "Pick version");
        if (versionPick < 0) return;
        version = versions[versionPick].version;
    } else if (versions.size() == 1) {
        version = versions[0].version;
    }

    ui::drawHeader("M5Burner flash");
    m5os::lcd().setCursor(4, 28);
    m5os::lcd().println(pkg.name);
    m5os::lcd().setCursor(4, 44);
    m5os::lcd().println(version.length() ? version : "latest");
    m5os::lcd().setCursor(4, 64);
    m5os::lcd().print("Enter save to SD");
    m5os::lcd().setCursor(4, 78);
    m5os::lcd().print("No auto-boot (SPIFFS safe)");
    m5os::lcd().setCursor(4, 92);
    m5os::lcd().print("` cancel");

    while (true) {
        m5os::update();
        Buttons keys = m5os::readButtons();
        if (keys.back) return;
        if (keys.ok) {
            LaunchResult result = launcher_.flashBurnerPackage(pkg, version);
            if (result.ok) {
                ui::showMessage("M5Burner", result.message, TFT_GREEN, 2600);
            } else {
                ui::showMessage("Flash failed", result.message, TFT_RED);
            }
            return;
        }
        delay(power::uiLoopDelayMs());
    }
}

void LauncherMenu::refreshCatalog() {
    bool ok = false;
    if (wifiIsConnected()) {
        ok = catalog_.refreshFromNetwork(kDefaultManifestUrl);
        if (catalog_.refreshFromBurnerHub(1)) ok = true;
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
        "WiFi setup",
        "Switch app (ESC/`)",
        "Load from catalog",
        "Flash from M5Burner catalog",
        "Refresh manifest",
        "Storage cleanup",
        "Export catalog (serial)",
        "File explorer",
        "Save / export to SD",
        "Theme",
        "M5Burner / recovery",
        "Keyboard shortcuts",
    };
    std::vector<String> labels;
    for (auto* item : items) labels.push_back(item);

    while (true) {
        const int pick = ui::selectFromList(labels, "M5 OS Main");
        if (pick < 0) {
            showAppSwitcher();
            continue;
        }
        switch (pick) {
            case 0:
                showWifiSetup();
                break;
            case 1:
                showAppSwitcher();
                break;
            case 2:
                showLoadCatalog();
                break;
            case 3:
                showFlashBurnerCatalog();
                break;
            case 4:
                refreshCatalog();
                break;
            case 5:
                showStorageCleanup();
                break;
            case 6:
                exportCatalogSerial();
                ui::showMessage("Exported", "Catalog on USB serial", TFT_GREEN, 900);
                break;
            case 7:
                showFileExplorer("/");
                break;
            case 8:
                showSaveExportMenu();
                break;
            case 9:
                showThemeMenu();
                break;
            case 10:
                showBurnerBridge();
                break;
            case 11:
                showHelp();
                break;
            default:
                break;
        }
    }
}

}  // namespace m5os
