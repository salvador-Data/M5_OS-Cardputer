#include "firmware_catalog.h"

#include "m5os_config.h"
#include "serial_log.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

namespace m5os {

namespace {

String normalizeName(const String& name) {
    String out = name;
    out.trim();
    return out;
}

bool parseManifestPayload(const String& payload, std::vector<FirmwarePackage>& out) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;
    JsonArray arr = doc["firmware"].as<JsonArray>();
    if (arr.isNull()) return false;
    out.clear();
    for (JsonObject item : arr) {
        FirmwarePackage pkg;
        pkg.name = normalizeName(item["name"] | "");
        if (!pkg.name.length()) continue;
        pkg.version = item["version"] | "1.0.0";
        pkg.url = item["url"] | "";
        pkg.size = item["size"] | 0;
        pkg.description = item["description"] | "";
        pkg.binFile = item["bin"].as<const char*>();
        if (!pkg.binFile.length()) pkg.binFile = FirmwareCatalog::slugToBinFile(pkg.name);
        out.push_back(pkg);
    }
    return true;
}

}  // namespace

String FirmwareCatalog::slugToBinFile(const String& name) {
    String slug = name;
    slug.toLowerCase();
    slug.replace(' ', '_');
    if (slug == "remote_possibility" || slug == "remote-possibility") {
        return String(kAppRemotePossibility) + ".bin";
    }
    if (slug == "ble_bot" || slug == "ble-bot") return String(kAppBleBot) + ".bin";
    return slug + ".bin";
}

String FirmwareCatalog::binPathFor(const String& binFile) const {
    return String(kFirmwareDir) + "/" + binFile;
}

bool FirmwareCatalog::ensureStorage() {
    SPI.begin(kSdMosiPin, kSdMisoPin, kSdSclkPin, kSdCsPin);
    if (!SD.begin(kSdCsPin, SPI, 25000000)) {
        log::info("sd_missing");
        return false;
    }
    if (!SD.exists(kFirmwareDir)) SD.mkdir(kFirmwareDir);
    if (!SD.exists(kLauncherMarkerPath)) {
        File marker = SD.open(kLauncherMarkerPath, FILE_WRITE);
        if (marker) {
            marker.println("M5 OS Cardputer launcher");
            marker.close();
        }
    }
    log::info("sd_ready");
    return true;
}

void FirmwareCatalog::scanInstalled() {
    installed_.clear();
    File dir = SD.open(kFirmwareDir);
    if (!dir) return;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            if (name.endsWith(".bin")) {
                FirmwarePackage pkg;
                const int slash = name.lastIndexOf('/');
                pkg.binFile = name.substring(slash + 1);
                pkg.name = pkg.binFile.substring(0, pkg.binFile.length() - 4);
                pkg.version = "local";
                pkg.installed = true;
                installed_.push_back(pkg);
            }
        }
        entry.close();
    }
    dir.close();
    mergeInstalledFlags();
    log::info("installed_scan", String(installed_.size()) + " bins");
}

void FirmwareCatalog::mergeInstalledFlags() {
    for (auto& pkg : available_) {
        pkg.installed = SD.exists(binPathFor(pkg.binFile).c_str());
    }
}

bool FirmwareCatalog::refreshFromNetwork(const char* manifestUrl) {
    if (WiFi.status() != WL_CONNECTED) return false;
    HTTPClient http;
    http.begin(manifestUrl);
    http.setTimeout(15000);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        log::info("manifest_http_fail", String(code));
        return false;
    }
    const String payload = http.getString();
    http.end();
    if (!parseManifestPayload(payload, available_)) {
        log::info("manifest_parse_fail");
        return false;
    }
    scanInstalled();
    log::info("manifest_loaded", String(available_.size()) + " packages");
    return true;
}

bool FirmwareCatalog::refreshFromSdManifest() {
    if (!SD.exists(kSdManifestPath)) return false;
    File in = SD.open(kSdManifestPath, FILE_READ);
    if (!in) return false;
    String payload;
    while (in.available()) payload += static_cast<char>(in.read());
    in.close();
    if (!parseManifestPayload(payload, available_)) return false;
    scanInstalled();
    log::info("manifest_sd_loaded", String(available_.size()) + " packages");
    return true;
}

bool FirmwareCatalog::downloadPackage(const FirmwarePackage& pkg) {
    if (WiFi.status() != WL_CONNECTED || !pkg.url.length()) return false;
    HTTPClient http;
    http.begin(pkg.url);
    http.setTimeout(20000);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    const String path = binPathFor(pkg.binFile);
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
    scanInstalled();
    log::info("package_installed", pkg.name);
    return SD.exists(path.c_str());
}

FirmwarePackage* FirmwareCatalog::findInstalledByName(const String& name) {
    for (auto& pkg : installed_) {
        if (pkg.name.equalsIgnoreCase(name) || pkg.binFile.startsWith(name)) return &pkg;
    }
    return nullptr;
}

}  // namespace m5os
