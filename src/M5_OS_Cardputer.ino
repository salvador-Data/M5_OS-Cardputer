/**
 * M5 OS — Cardputer edition
 * Multi-app launcher / firmware manager for M5Stack Cardputer (ESP32-S3)
 * Hacker Planet LLC / salvador-Data
 *
 * Controls: ;/w up · ./s down · Enter/Space select · ` back
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <vector>

#include "M5OSDevice.h"

static const char* kFirmwareDir = "/firmware";
static const char* kManifestUrl =
    "https://raw.githubusercontent.com/salvador-Data/M5_OS-Cardputer/main/data/manifest.example.json";

static uint16_t themePrimary = 0xB6DF;
static uint16_t themeSecondary = 0x0083;

struct FirmwareInfo {
    String name;
    String version;
    String url;
    size_t size = 0;
    String description;
};

static std::vector<FirmwareInfo> availableFirmware;
static std::vector<FirmwareInfo> installedFirmware;
static char wifiSSID[33] = "";
static char wifiPassword[65] = "";

static void showMessage(const String& title, const String& body, uint16_t color = TFT_WHITE) {
    m5os::lcd().fillScreen(TFT_BLACK);
    m5os::lcd().setTextColor(color, TFT_BLACK);
    m5os::lcd().setCursor(4, 8);
    m5os::lcd().println(title);
    m5os::lcd().setCursor(4, 28);
    m5os::lcd().setTextColor(themeSecondary, TFT_BLACK);
    m5os::lcd().println(body);
    delay(1600);
}

static void errorHandler(const String& msg) {
    showMessage("Error", msg, TFT_RED);
}

static bool ensureSd() {
    SPI.begin(40, 39, 14, 12);
    if (SD.begin(12, SPI, 25000000)) {
        if (!SD.exists(kFirmwareDir)) {
            SD.mkdir(kFirmwareDir);
        }
        return true;
    }
    return false;
}

static void drawHeader(const char* title) {
    m5os::lcd().fillScreen(TFT_BLACK);
    m5os::lcd().setTextColor(themePrimary, TFT_BLACK);
    m5os::lcd().setCursor(4, 4);
    m5os::lcd().printf("%s", title);
    m5os::lcd().drawFastHLine(0, 18, m5os::lcd().width(), themeSecondary);
}

static int selectFromList(const std::vector<String>& items, const char* title) {
    if (items.empty()) {
        showMessage(title, "No items");
        return -1;
    }
    int index = 0;
    while (true) {
        drawHeader(title);
        for (size_t i = 0; i < items.size() && i < 8; ++i) {
            const int y = 24 + static_cast<int>(i) * 14;
            m5os::lcd().setCursor(8, y);
            if (static_cast<int>(i) == index) {
                m5os::lcd().setTextColor(themePrimary, themeSecondary);
                m5os::lcd().printf("> %s", items[i].c_str());
            } else {
                m5os::lcd().setTextColor(themeSecondary, TFT_BLACK);
                m5os::lcd().printf("  %s", items[i].c_str());
            }
        }
        m5os::update();
        auto keys = m5os::readButtons();
        if (keys.up) index = (index > 0) ? index - 1 : static_cast<int>(items.size()) - 1;
        if (keys.down) index = (index + 1) % static_cast<int>(items.size());
        if (keys.ok) return index;
        if (keys.back) return -1;
        delay(80);
    }
}

static void scanInstalledFirmware() {
    installedFirmware.clear();
    File dir = SD.open(kFirmwareDir);
    if (!dir) return;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            if (name.endsWith(".bin")) {
                FirmwareInfo info;
                info.name = name.substring(name.lastIndexOf('/') + 1, name.length() - 4);
                info.version = "local";
                installedFirmware.push_back(info);
            }
        }
        entry.close();
    }
    dir.close();
}

static bool downloadToFile(const String& url, const String& path) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    File out = SD.open(path, FILE_WRITE);
    if (!out) {
        http.end();
        return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[512];
    while (http.connected()) {
        const size_t available = stream->available();
        if (!available) {
            delay(1);
            continue;
        }
        const int read = stream->readBytes(buffer, min(available, sizeof(buffer)));
        if (read <= 0) break;
        out.write(buffer, read);
    }
    out.close();
    http.end();
    return true;
}

static void refreshCatalog() {
    availableFirmware.clear();
    if (WiFi.status() != WL_CONNECTED) {
        showMessage("Catalog", "Connect WiFi first");
        return;
    }
    HTTPClient http;
    http.begin(kManifestUrl);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        showMessage("Catalog", "Manifest download failed");
        return;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        showMessage("Catalog", "Invalid manifest JSON");
        return;
    }
    for (JsonObject item : doc["firmware"].as<JsonArray>()) {
        FirmwareInfo info;
        info.name = item["name"].as<const char*>();
        info.version = item["version"] | "1.0";
        info.url = item["url"].as<const char*>();
        info.size = item["size"] | 0;
        info.description = item["description"] | "";
        availableFirmware.push_back(info);
    }
    showMessage("Catalog", String(availableFirmware.size()) + " packages");
}

static void installSelected(const FirmwareInfo& info) {
    if (WiFi.status() != WL_CONNECTED) {
        showMessage("Install", "WiFi required");
        return;
    }
    const String path = String(kFirmwareDir) + "/" + info.name + ".bin";
    drawHeader("Downloading");
    m5os::lcd().setCursor(4, 30);
    m5os::lcd().println(info.name);
    if (downloadToFile(info.url, path)) {
        showMessage("Installed", info.name, TFT_GREEN);
    } else {
        errorHandler("Download failed");
    }
}

static void listInstalledMenu() {
    scanInstalledFirmware();
    std::vector<String> labels;
    for (const auto& fw : installedFirmware) labels.push_back(fw.name);
    const int pick = selectFromList(labels, "Installed");
    if (pick >= 0) {
        showMessage("Launch", labels[pick] + "\n(run .bin loader TBD)");
    }
}

static void downloadMenu() {
    std::vector<String> labels;
    for (const auto& fw : availableFirmware) {
        labels.push_back(fw.name + " v" + fw.version);
    }
    const int pick = selectFromList(labels, "Download");
    if (pick >= 0) installSelected(availableFirmware[pick]);
}

static void fileExplorer(const char* path) {
    std::vector<String> entries;
    entries.push_back("[..]");
    File dir = SD.open(path);
    if (!dir) {
        errorHandler("Cannot open path");
        return;
    }
    File entry;
    while ((entry = dir.openNextFile())) {
        String entryName = entry.name();
        if (entry.isDirectory()) entryName += "/";
        entries.push_back(entryName);
        entry.close();
    }
    dir.close();
    const int pick = selectFromList(entries, path);
    if (pick <= 0) return;
    String chosen = entries[pick];
    if (chosen.endsWith("/")) {
        fileExplorer((String(path) + "/" + chosen.substring(0, chosen.length() - 1)).c_str());
    } else {
        showMessage("File", chosen);
    }
}

static void themeMenu() {
    static const char* themes[] = {"Baby Blue", "Hacker Green", "Mr. Robot Red"};
    std::vector<String> labels;
    for (auto* t : themes) labels.push_back(t);
    const int pick = selectFromList(labels, "Theme");
    if (pick == 0) {
        themePrimary = 0xB6DF;
        themeSecondary = 0x0083;
    } else if (pick == 1) {
        themePrimary = 0x07E0;
        themeSecondary = 0x0320;
    } else if (pick == 2) {
        themePrimary = 0xF800;
        themeSecondary = 0x7800;
    }
}

static void wifiSetup() {
    drawHeader("WiFi scan");
    m5os::lcd().println("Scanning...");
    const int n = WiFi.scanNetworks();
    std::vector<String> ssids;
    for (int i = 0; i < n; ++i) ssids.push_back(WiFi.SSID(i));
    const int pick = selectFromList(ssids, "WiFi");
    if (pick < 0) return;
    strncpy(wifiSSID, ssids[pick].c_str(), sizeof(wifiSSID) - 1);
    showMessage("WiFi", "Using open/P SK in code\nSet password in IDE for now");
    WiFi.begin(wifiSSID, wifiPassword);
    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500);
        tries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        showMessage("WiFi", WiFi.localIP().toString(), TFT_GREEN);
    } else {
        errorHandler("WiFi connect failed");
    }
}

static void introSplash() {
    m5os::lcd().fillScreen(TFT_BLACK);
    m5os::lcd().setTextSize(2);
    m5os::lcd().setTextColor(themePrimary, TFT_BLACK);
    m5os::lcd().setCursor(20, 40);
    m5os::lcd().println("M5 OS");
    m5os::lcd().setTextSize(1);
    m5os::lcd().setCursor(10, 70);
    m5os::lcd().println("Cardputer Edition");
    m5os::lcd().setCursor(10, 90);
    m5os::lcd().println("salvador-Data / Hacker Planet");
    delay(2000);
}

static void mainMenu() {
    static const char* items[] = {
        "Installed firmware",
        "Download catalog",
        "Refresh manifest",
        "File explorer",
        "Theme",
        "WiFi setup",
    };
    std::vector<String> labels;
    for (auto* item : items) labels.push_back(item);

    while (true) {
        const int pick = selectFromList(labels, "M5 OS Main");
        if (pick < 0) return;
        switch (pick) {
            case 0: listInstalledMenu(); break;
            case 1: downloadMenu(); break;
            case 2: refreshCatalog(); break;
            case 3: fileExplorer("/"); break;
            case 4: themeMenu(); break;
            case 5: wifiSetup(); break;
            default: break;
        }
    }
}

void setup() {
    m5os::begin();
    introSplash();
    if (!ensureSd()) {
        errorHandler("SD card missing");
    }
    scanInstalledFirmware();
    refreshCatalog();
    mainMenu();
}

void loop() {
    m5os::update();
    delay(50);
}
