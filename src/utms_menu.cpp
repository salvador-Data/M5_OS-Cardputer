#include "utms.h"

#include "m5os_config.h"
#include "m5os_settings.h"
#include "m5os_security.h"
#include "m5os_vfs.h"
#include "M5OSDevice.h"
#include "power_manager.h"
#include "serial_log.h"
#include "ui_display.h"
#include "utms_threat_pack.h"
#include "wifi_manager.h"

#include <ArduinoJson.h>
#include <M5Cardputer.h>
#include <SD.h>

namespace m5os::utms {

namespace {

void showSdRequired() {
    String body = "Insert SD for UTMS";
    if (vfs::lastMountError().length()) body += "\n" + vfs::lastMountError();
    body += "\n/contacts away from screen";
    ui::showMessage("No SD", body, TFT_YELLOW);
}

String formatLastCheck() {
    const uint32_t epoch = lastCheckEpoch();
    if (!epoch) return "never";
    const uint32_t now = static_cast<uint32_t>(time(nullptr));
    if (now >= 1000000000U && epoch >= 1000000000U) {
        const uint32_t delta = now - epoch;
        if (delta < 3600) return String(delta / 60) + "m ago";
        if (delta < 86400) return String(delta / 3600) + "h ago";
        return String(delta / 86400) + "d ago";
    }
    return String(epoch) + "s boot";
}

void showAntivirusScan() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();
    const ThreatPackInfo pack = loadPackInfo();

    ui::drawHeader("AV scan");
    m5os::lcd().setCursor(4, 28);
    m5os::lcd().setTextColor(TFT_WHITE, TFT_BLACK);
    if (pack.loaded) {
        m5os::lcd().printf("Pack v%s\n", pack.version.c_str());
        m5os::lcd().printf("%u hashes %u strings\n",
                           static_cast<unsigned>(pack.hashCount),
                           static_cast<unsigned>(pack.stringCount));
    } else {
        m5os::lcd().println("No threat pack");
        m5os::lcd().println("Update signatures first");
    }
    m5os::lcd().setCursor(4, 64);
    m5os::lcd().println("Scanning /apps/*.bin...");

    size_t scanned = 0;
    size_t flagged = 0;
    if (SD.exists(vfs::kAppsDir)) {
        File apps = SD.open(vfs::kAppsDir);
        if (apps && apps.isDirectory()) {
            File entry;
            while ((entry = apps.openNextFile())) {
                if (!entry.isDirectory()) {
                    const String name = vfs::entryBaseName(entry.name());
                    if (name.endsWith(".bin")) {
                        ++scanned;
                        const String digest = security::computeFileSha256Hex(entry);
                        if (digest.length()) {
                            // Stub: full hash-list match deferred to future scanner module.
                            (void)digest;
                        }
                    }
                }
                entry.close();
            }
            apps.close();
        }
    }

    m5os::lcd().setCursor(4, 92);
    m5os::lcd().printf("Scanned %u bins\n", static_cast<unsigned>(scanned));
    m5os::lcd().printf("Flagged %u (stub)\n", static_cast<unsigned>(flagged));
    m5os::lcd().setTextColor(TFT_DARKGREY, TFT_BLACK);
    m5os::lcd().setCursor(4, 118);
    m5os::lcd().print("Any key back");

    appendLog("av_scan", String(scanned) + " bins");
    m5os::keyboardDrainBack();
    m5os::keyboardDrainEnter();
    while (true) {
        m5os::update();
        Buttons keys = ui::readButtonsExtended();
        if (keys.back || keys.ok || m5os::keyboardBackJustPressed() || m5os::keyboardEnterJustPressed()) {
            return;
        }
        delay(power::uiLoopDelayMs());
    }
}

void showUpdateSignatures() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    if (!wifiIsConnected()) {
        ui::showMessage("Update signatures", "WiFi required\nUse WiFi setup", TFT_RED);
        return;
    }

    const String url = settings::utmsPackUrl();
    ui::showFlashProgress(0, "UTMS update", "Fetching pack...");
    m5os::update();

    const UpdateResult result = fetchAndInstallPack(url);
    if (result.ok) {
        ui::showFlashProgress(100, "UTMS update", result.message);
        ui::showMessage("Signatures OK", result.message + "\n" + vfs::kThreatPackPath, TFT_GREEN, 2600);
    } else {
        ui::showMessage("Update failed", result.message, TFT_RED);
    }
}

void showIdsStatus() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();
    const ThreatPackInfo pack = loadPackInfo();
    const String nvsVer = lastUpdateVersion();

    String body = "Last check: " + formatLastCheck();
    body += "\nNVS ver: " + (nvsVer.length() ? nvsVer : "(none)");
    if (pack.loaded) {
        body += "\nPack: v" + pack.version;
        body += "\n" + String(pack.hashCount) + " hashes";
    } else {
        body += "\nNo pack on SD";
    }
    body += "\nAlerts: 0 (stub)";

    ui::showMessage("IDS status", body, TFT_WHITE, 4000);
}

void showQuarantine() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();

    std::vector<String> entries;
    if (SD.exists(vfs::kQuarantineDir)) {
        File dir = SD.open(vfs::kQuarantineDir);
        if (dir && dir.isDirectory()) {
            File entry;
            while ((entry = dir.openNextFile())) {
                if (!entry.isDirectory()) {
                    entries.push_back(vfs::entryBaseName(entry.name()));
                }
                entry.close();
            }
            dir.close();
        }
    }

    if (entries.empty()) {
        ui::showMessage("Quarantine", "Empty\n" + String(vfs::kQuarantineDir), TFT_DARKGREY, 2200);
        return;
    }

    std::vector<String> labels;
    for (const auto& e : entries) labels.push_back(e);
    const int pick = ui::selectFromList(labels, "Quarantine");
    if (pick < 0) return;
    ui::showMessage("Quarantined", entries[pick] + "\n" + String(vfs::kQuarantineDir), TFT_YELLOW);
}

void showFirewallRules() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();

    String body = "Lab soft-AP rules\n(stub — edit on SD)\n";
    body += String(vfs::kFirewallRulesPath);
    if (SD.exists(vfs::kFirewallRulesPath)) {
        File in = SD.open(vfs::kFirewallRulesPath, FILE_READ);
        if (in) {
            body += "\n---\n";
            int lines = 0;
            while (in.available() && lines < 6) {
                body += in.readStringUntil('\n');
                body += "\n";
                ++lines;
            }
            in.close();
        }
    } else {
        body += "\n\nExample:\n{\"allow\":[\"AA:BB:CC\"],\"deny\":[]}";
    }
    ui::showMessage("Firewall rules", body, TFT_WHITE, 5000);
}

void showUtmsLogs() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();

    String body;
    if (SD.exists(vfs::kUtmsLogPath)) {
        File in = SD.open(vfs::kUtmsLogPath, FILE_READ);
        if (in) {
            const size_t size = in.size();
            const size_t tail = size > 768 ? size - 768 : 0;
            if (tail) in.seek(tail);
            while (in.available() && body.length() < 700) {
                body += static_cast<char>(in.read());
            }
            in.close();
        }
    }
    if (!body.length()) body = "(no UTMS events yet)";
    ui::showMessage("UTMS logs", body, TFT_WHITE, 6000);
}

void showUtmsSettings() {
    static const char* items[] = {
        "Auto-check on boot: OFF",
        "Auto-check on boot: ON",
        "Reset pack URL to default",
    };
    std::vector<String> labels;
    const bool autoCheck = settings::utmsAutoCheckOnBoot();
    for (auto* item : items) labels.push_back(item);
    if (autoCheck) labels[0] += " *";
    else labels[1] += " *";

    const int pick = ui::selectFromList(labels, "UTMS settings");
    if (pick < 0) return;
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    if (pick == 0) {
        settings::saveUtmsAutoCheck(false);
        ui::showMessage("UTMS settings", "Auto-check OFF", TFT_GREEN, 1200);
    } else if (pick == 1) {
        settings::saveUtmsAutoCheck(true);
        ui::showMessage("UTMS settings", "Auto-check ON\nBoot + WiFi only", TFT_GREEN, 1600);
    } else if (pick == 2) {
        settings::saveUtmsPackUrl(kDefaultUtmsPackUrl);
        ui::showMessage("UTMS settings", "URL reset to default", TFT_GREEN, 1400);
    }
}

}  // namespace

void showUtmsMenu() {
    static const char* items[] = {
        "Antivirus scan",
        "Update signatures",
        "IDS status",
        "Quarantine",
        "Firewall rules",
        "Logs",
        "UTMS settings",
    };
    std::vector<String> labels;
    for (auto* item : items) labels.push_back(item);

    while (true) {
        const int pick = ui::selectFromList(labels, "UTMS / Security");
        if (pick < 0) return;
        switch (pick) {
            case 0:
                showAntivirusScan();
                break;
            case 1:
                showUpdateSignatures();
                break;
            case 2:
                showIdsStatus();
                break;
            case 3:
                showQuarantine();
                break;
            case 4:
                showFirewallRules();
                break;
            case 5:
                showUtmsLogs();
                break;
            case 6:
                showUtmsSettings();
                break;
            default:
                break;
        }
    }
}

void maybeAutoCheckOnBoot() {
    if (!settings::utmsAutoCheckOnBoot()) return;
    if (!wifiIsConnected()) return;
    if (!settings::ensureSdMounted()) return;

    const ThreatPackInfo local = loadPackInfo();
    const UpdateResult result = fetchAndInstallPack(settings::utmsPackUrl());
    if (result.ok && local.version != result.version) {
        ui::showMessage("UTMS auto-update", "v" + result.version, TFT_GREEN, 1800);
    }
}

}  // namespace m5os::utms
