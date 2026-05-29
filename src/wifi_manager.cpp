#include "wifi_manager.h"

#include "serial_log.h"
#include "ui_display.h"

#include <WiFi.h>
#include <vector>

namespace m5os {

bool wifiIsConnected() { return WiFi.status() == WL_CONNECTED; }

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
    ui::promptPassword(passOut, passLen, "WiFi password");

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
    passOut[0] = '\0';
    if (WiFi.status() == WL_CONNECTED) {
        log::info("wifi_connected", wifiIpAddress());
        ui::showMessage("WiFi", wifiIpAddress(), TFT_GREEN);
        return true;
    }
    log::info("wifi_failed");
    ui::showMessage("Error", "WiFi connect failed", TFT_RED);
    return false;
}

}  // namespace m5os
