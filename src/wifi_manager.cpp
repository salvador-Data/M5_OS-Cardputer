#include "wifi_manager.h"

#include "m5os_settings.h"
#include "m5os_vfs.h"
#include "power_manager.h"
#include "serial_log.h"
#include "ui_display.h"

#include <WiFi.h>
#include <vector>

namespace m5os {

namespace {

bool wifiAttemptConnect(const char* ssid, const char* pass) {
    ui::drawHeader("WiFi connect");
    m5os::lcd().setCursor(4, 30);
    m5os::lcd().setTextColor(ui::theme().primary, TFT_BLACK);
    m5os::lcd().println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 24) {
        delay(500);
        tries++;
    }
    if (WiFi.status() != WL_CONNECTED) return false;
    if (power::isSaving()) WiFi.setSleep(WIFI_PS_MIN_MODEM);
    log::info("wifi_connected", wifiIpAddress());
    return true;
}

bool wifiSaveCredentials(const char* ssid, char* passOut) {
    String body = wifiIpAddress();
    const bool saved = settings::saveWifi(ssid, passOut);
    passOut[0] = '\0';
    if (saved) {
        body += "\nSaved to\n" + String(vfs::kSettingsPath);
        ui::showMessage("WiFi", body, TFT_GREEN, 2000);
    } else {
        body += "\nInsert SD to save\nWiFi (session only)";
        ui::showMessage("WiFi", body, TFT_YELLOW, 2000);
    }
    return true;
}

bool wifiPickNetwork(char* ssidOut, size_t ssidLen) {
    ui::drawHeader("WiFi scan");
    m5os::lcd().setCursor(4, 24);
    m5os::lcd().setTextColor(ui::theme().primary, TFT_BLACK);
    m5os::lcd().println("Scanning...");
    const int n = WiFi.scanNetworks();
    std::vector<String> ssids;
    for (int i = 0; i < n; ++i) ssids.push_back(WiFi.SSID(i));
    if (ssids.empty()) {
        ui::showMessage("WiFi", "No networks found", TFT_YELLOW);
        return false;
    }
    const int pick = ui::selectFromList(ssids, "Pick network");
    if (pick < 0) return false;
    strncpy(ssidOut, ssids[pick].c_str(), ssidLen - 1);
    ssidOut[ssidLen - 1] = '\0';
    return true;
}

}  // namespace

bool wifiIsConnected() { return WiFi.status() == WL_CONNECTED; }

bool wifiTrySavedConnect() {
    const String ssid = settings::savedWifiSsid();
    if (!ssid.length()) return false;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), settings::savedWifiPass().c_str());
    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 12) {
        delay(500);
        tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        if (power::isSaving()) WiFi.setSleep(WIFI_PS_MIN_MODEM);
        log::info("wifi_saved_connect", wifiIpAddress());
        return true;
    }
    log::info("wifi_saved_fail", ssid);
    return false;
}

String wifiIpAddress() {
    if (!wifiIsConnected()) return "";
    return WiFi.localIP().toString();
}

bool wifiConnectInteractive(char* ssidOut, size_t ssidLen, char* passOut, size_t passLen) {
    for (;;) {
        if (wifiIsConnected()) {
            std::vector<String> actions;
            actions.push_back("Change network");
            actions.push_back("Disconnect");
            actions.push_back("Status: " + wifiIpAddress());
            actions.push_back("Back");
            const int action = ui::selectFromList(actions, "WiFi connected");
            if (action < 0 || action == 3) return wifiIsConnected();
            if (action == 1) {
                WiFi.disconnect(true);
                ui::showMessage("WiFi", "Disconnected", TFT_YELLOW, 1200);
                continue;
            }
            if (action == 2) {
                ui::showMessage("WiFi", WiFi.SSID() + "\n" + wifiIpAddress(), TFT_GREEN, 2000);
                continue;
            }
        }

        if (!wifiPickNetwork(ssidOut, ssidLen)) return wifiIsConnected();

        for (;;) {
            passOut[0] = '\0';
            ui::drawHeader("WiFi password");
            m5os::lcd().setCursor(4, 14);
            m5os::lcd().setTextColor(TFT_CYAN, TFT_BLACK);
            m5os::lcd().println(ssidOut);

            const ui::PasswordPromptResult pw =
                ui::promptPassword(passOut, passLen, "WiFi password");

            if (pw == ui::PasswordPromptResult::ChangeNetwork) break;

            if (pw == ui::PasswordPromptResult::Cancelled) {
                ui::showMessage("WiFi", "Password cancelled\nTab = pick another AP", TFT_YELLOW, 1500);
                return wifiIsConnected();
            }

            if (!wifiAttemptConnect(ssidOut, passOut)) {
                passOut[0] = '\0';
                log::info("wifi_failed");
                ui::showMessage("Error", "Connect failed\nTab = pick another AP", TFT_RED, 1800);
                continue;
            }
            return wifiSaveCredentials(ssidOut, passOut);
        }
    }
}

}  // namespace m5os
