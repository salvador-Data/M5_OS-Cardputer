#include "wifi_manager.h"

#include "m5os_settings.h"
#include "m5os_vfs.h"
#include "power_manager.h"
#include "serial_log.h"
#include "ui_display.h"

#include <WiFi.h>
#include <vector>

namespace m5os {

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
    ui::drawHeader("WiFi scan");
    m5os::lcd().setCursor(4, 24);
    m5os::lcd().setTextColor(ui::theme().primary, TFT_BLACK);
    m5os::lcd().println("Scanning...");
    const int n = WiFi.scanNetworks();
    std::vector<String> ssids;
    for (int i = 0; i < n; ++i) ssids.push_back(WiFi.SSID(i));
    const int pick = ui::selectFromList(ssids, "WiFi");
    if (pick < 0) return false;

    strncpy(ssidOut, ssids[pick].c_str(), ssidLen - 1);
    ssidOut[ssidLen - 1] = '\0';

    passOut[0] = '\0';
    if (!ui::promptPassword(passOut, passLen, "WiFi password")) {
        ui::showMessage("WiFi", "Password cancelled", TFT_YELLOW, 1200);
        return false;
    }

    ui::drawHeader("WiFi connect");
    m5os::lcd().setCursor(4, 30);
    m5os::lcd().setTextColor(ui::theme().primary, TFT_BLACK);
    m5os::lcd().println(ssidOut);
    WiFi.begin(ssidOut, passOut);
    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 24) {
        delay(500);
        tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        if (power::isSaving()) WiFi.setSleep(WIFI_PS_MIN_MODEM);
        log::info("wifi_connected", wifiIpAddress());
        String body = wifiIpAddress();
        const bool saved = settings::saveWifi(ssidOut, passOut);
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
    passOut[0] = '\0';
    log::info("wifi_failed");
    ui::showMessage("Error", "WiFi connect failed", TFT_RED);
    return false;
}

}  // namespace m5os
