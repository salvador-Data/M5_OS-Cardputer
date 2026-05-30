#include "utms.h"

#include "m5os_config.h"
#include "m5os_settings.h"
#include "m5os_vfs.h"
#include "M5OSDevice.h"
#include "power_manager.h"
#include "ui_display.h"
#include "utms_core.h"
#include "utms_firewall.h"
#include "utms_threat_pack.h"
#include "wifi_manager.h"

#include <SD.h>

namespace m5os::utms {

namespace {

void showSdRequired() {
    String body = "Insert SD for UTMS";
    if (vfs::lastMountError().length()) body += "\n" + vfs::lastMountError();
    body += "\n/contacts away from screen";
    ui::showMessage("No SD", body, TFT_YELLOW);
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
        m5os::lcd().printf("%u deny %u allow\n", static_cast<unsigned>(pack.hashCount),
                           static_cast<unsigned>(pack.allowHashCount));
    } else {
        m5os::lcd().println("No threat pack");
        m5os::lcd().println("Update signatures");
    }
    m5os::lcd().setCursor(4, 64);
    m5os::lcd().println("Scanning SD...");
    m5os::update();

    const ScanSummary summary = runAvScan(96);

    m5os::lcd().fillRect(4, 64, m5os::lcd().width() - 8, 54, TFT_BLACK);
    m5os::lcd().setCursor(4, 64);
    m5os::lcd().setTextColor(TFT_WHITE, TFT_BLACK);
    m5os::lcd().printf("Scanned %u\n", static_cast<unsigned>(summary.scanned));
    m5os::lcd().printf("Clean %u  Bad %u\n", static_cast<unsigned>(summary.clean),
                       static_cast<unsigned>(summary.infected));
    m5os::lcd().printf("Unknown %u\n", static_cast<unsigned>(summary.unknown));

    if (summary.infected > 0) {
        m5os::lcd().setTextColor(TFT_ORANGE, TFT_BLACK);
        m5os::lcd().setCursor(4, 108);
        m5os::lcd().print("Enter quarantine all");
    } else {
        m5os::lcd().setTextColor(TFT_DARKGREY, TFT_BLACK);
        m5os::lcd().setCursor(4, 118);
        m5os::lcd().print("ESC/` back");
    }

    m5os::keyboardDrainBack();
    m5os::keyboardDrainEnter();
    while (true) {
        m5os::update();
        Buttons keys = ui::readButtonsExtended();
        if (keys.back || m5os::keyboardBackJustPressed()) return;
        if ((keys.ok || m5os::keyboardEnterJustPressed()) && summary.infected > 0) {
            size_t moved = 0;
            for (const auto& hit : summary.hits) {
                if (hit.verdict != ScanVerdict::Infected) continue;
                String err;
                if (quarantineFile(hit.path, hit.sha256, err)) ++moved;
            }
            ui::showMessage("Quarantine", String(moved) + " file(s) moved", TFT_GREEN, 2400);
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

    const ThreatPackInfo before = loadPackInfo();
    const String beforeVer = before.loaded ? before.version : "(none)";
    const String url = settings::utmsPackUrl();

    ui::showFlashProgress(0, "UTMS update", "Was: " + beforeVer + "\nFetching...");
    m5os::update();

    const UpdateResult result = fetchAndInstallPack(url);
    if (result.ok) {
        ui::showFlashProgress(100, "UTMS update", result.message);
        String body = "Before: " + beforeVer;
        body += "\nAfter: v" + result.version;
        body += "\n" + String(vfs::kThreatPackPath);
        ui::showMessage("Signatures OK", body, TFT_GREEN, 3200);
    } else {
        ui::showMessage("Update failed", "Before: " + beforeVer + "\n" + result.message, TFT_RED);
    }
}

void showIdsStatus() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();
    const IdsStatus status = loadIdsStatus();

    String body = "Last check: " + status.lastCheck;
    body += "\nNVS: " + (status.nvsVersion.length() ? status.nvsVersion : "(none)");
    body += "\nPack: " + (status.packVersion.length() ? ("v" + status.packVersion) : "(none)");
    body += "\nDeny hashes: " + String(status.denyHashes);
    body += "\nAllow hashes: " + String(status.allowHashes);
    body += "\nAlerts (log): " + String(status.alertCount);
    body += "\nQuarantined: " + String(status.quarantinedCount);

    ui::showMessage("IDS status", body, TFT_WHITE, 4500);
}

void showQuarantineDetail(const QuarantineEntry& entry) {
    static const char* actions[] = {"Restore to original", "Delete permanently", "Back"};
    std::vector<String> labels;
    for (auto* a : actions) labels.push_back(a);

    const int pick = ui::selectFromList(labels, "Quarantine file");
    if (pick < 0 || pick == 2) return;

    String err;
    bool ok = false;
    if (pick == 0) {
        ok = restoreQuarantined(entry.quarantineName, err);
        ui::showMessage(ok ? "Restored" : "Restore failed", ok ? entry.originalPath : err,
                        ok ? TFT_GREEN : TFT_RED, 2400);
    } else if (pick == 1) {
        ok = deleteQuarantined(entry.quarantineName, err);
        ui::showMessage(ok ? "Deleted" : "Delete failed", ok ? entry.quarantineName : err,
                        ok ? TFT_GREEN : TFT_RED, 2000);
    }
}

void showQuarantine() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();

    const std::vector<QuarantineEntry> entries = listQuarantineEntries();
    if (entries.empty()) {
        ui::showMessage("Quarantine", "Empty\n" + String(vfs::kQuarantineDir), TFT_DARKGREY, 2200);
        return;
    }

    std::vector<String> labels;
    for (const auto& e : entries) {
        String line = e.quarantineName;
        if (e.sha256.length() >= 8) line += " " + e.sha256.substring(0, 8);
        labels.push_back(line);
    }
    const int pick = ui::selectFromList(labels, "Quarantine");
    if (pick < 0) return;

    String body = entries[pick].quarantineName;
    body += "\n" + entries[pick].originalPath;
    if (entries[pick].sha256.length()) body += "\n" + entries[pick].sha256;
    ui::showMessage("Quarantined", body, TFT_YELLOW, 3500);
    showQuarantineDetail(entries[pick]);
}

void showFirewallRules() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();

    std::vector<FirewallRule> rules = loadFirewallRules();
    if (rules.empty()) {
        ui::showMessage("Firewall rules",
                        "No rules file\nCreate:\n" + String(vfs::kFirewallRulesPath) +
                            "\n{\"rules\":[{\"id\":\"1\",\"action\":\"deny\","
                            "\"pattern\":\"evil.example\",\"enabled\":true}]}",
                        TFT_WHITE, 5500);
        return;
    }

    while (true) {
        std::vector<String> labels;
        for (auto& r : rules) {
            String line = (r.enabled ? "[on] " : "[off] ");
            line += r.action + " " + r.pattern;
            if (line.length() > 38) line = line.substring(0, 38);
            labels.push_back(line);
        }
        labels.push_back("+ Back to UTMS");

        const int pick = ui::selectFromList(labels, "Firewall rules");
        if (pick < 0 || pick >= static_cast<int>(labels.size()) - 1) return;

        rules[pick].enabled = !rules[pick].enabled;
        if (saveFirewallRules(rules)) {
            ui::showMessage("Firewall", rules[pick].enabled ? "Rule enabled" : "Rule disabled",
                            TFT_GREEN, 900);
        } else {
            ui::showMessage("Firewall", "Save failed", TFT_RED);
            return;
        }
    }
}

void showUtmsLogs() {
    if (!settings::ensureSdMounted()) {
        showSdRequired();
        return;
    }
    ensureUtmsDirs();

    const std::vector<String> lines = readLogTailLines(64);
    if (lines.empty()) {
        ui::showMessage("UTMS logs", "(empty)\n" + String(vfs::kUtmsLogPath), TFT_DARKGREY, 2000);
        return;
    }

    int index = static_cast<int>(lines.size()) - 1;
    if (index < 0) index = 0;
    int scroll = 0;
    constexpr int kVisible = 7;

    auto redraw = [&]() {
        ui::drawHeader("UTMS logs");
        if (index < scroll) scroll = index;
        if (index >= scroll + kVisible) scroll = index - kVisible + 1;
        for (int row = 0; row < kVisible; ++row) {
            const int i = scroll + row;
            const int y = 24 + row * 13;
            m5os::lcd().fillRect(4, y - 1, m5os::lcd().width() - 8, 13, TFT_BLACK);
            if (i < 0 || i >= static_cast<int>(lines.size())) continue;
            m5os::lcd().setCursor(4, y);
            String line = lines[i];
            if (line.length() > 38) line = line.substring(0, 38);
            if (i == index) {
                m5os::lcd().setTextColor(TFT_CYAN, TFT_BLACK);
                m5os::lcd().printf("> %s", line.c_str());
            } else {
                m5os::lcd().setTextColor(TFT_DARKGREY, TFT_BLACK);
                m5os::lcd().printf("  %s", line.c_str());
            }
        }
        m5os::lcd().setTextColor(TFT_DARKGREY, TFT_BLACK);
        m5os::lcd().setCursor(4, 118);
        m5os::lcd().print("up/dn scroll  ` back");
    };

    redraw();
    while (true) {
        m5os::update();
        Buttons keys = ui::readButtonsExtended();
        if (keys.back || m5os::keyboardBackJustPressed()) return;
        if (keys.up) index = (index > 0) ? index - 1 : static_cast<int>(lines.size()) - 1;
        if (keys.down) index = (index + 1) % static_cast<int>(lines.size());
        redraw();
        delay(power::uiLoopDelayMs());
    }
}

void showUtmsSettings() {
    static const char* items[] = {
        "Auto-check on boot: OFF",
        "Auto-check on boot: ON",
        "Reset pack URL to default",
        "Test pack download (WiFi)",
    };
    std::vector<String> labels;
    const bool autoCheck = settings::utmsAutoCheckOnBoot();
    for (auto* item : items) labels.push_back(item);
    if (autoCheck)
        labels[1] += " *";
    else
        labels[0] += " *";

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
    } else if (pick == 3) {
        if (!wifiIsConnected()) {
            ui::showMessage("UTMS settings", "WiFi required", TFT_RED);
            return;
        }
        const UpdateResult result = fetchAndInstallPack(settings::utmsPackUrl());
        ui::showMessage(result.ok ? "Test OK" : "Test failed", result.message,
                        result.ok ? TFT_GREEN : TFT_RED, 2600);
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
