#include "firmware_catalog.h"

#include "m5os_config.h"
#include "m5os_security.h"
#include "serial_log.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>

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
        pkg.sha256 = security::normalizeSha256Hex(item["sha256"] | "");
        pkg.binFile = security::sanitizeBinFilename(item["bin"].as<const char*>());
        if (!pkg.binFile.length()) pkg.binFile = security::sanitizeBinFilename(FirmwareCatalog::slugToBinFile(pkg.name));
        if (!pkg.binFile.length()) continue;
        if (pkg.url.length() && !security::isAllowedHttpsUrl(pkg.url)) {
            log::info("manifest_url_rejected", pkg.name);
            continue;
        }
        out.push_back(pkg);
    }
    return !out.empty();
}

String hashDownloadStream(WiFiClient* stream, File& out) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    uint8_t buffer[512];
    while (stream->connected() || stream->available()) {
        const size_t available = stream->available();
        if (!available) {
            delay(1);
            continue;
        }
        const int read = stream->readBytes(buffer, min(available, sizeof(buffer)));
        if (read <= 0) break;
        out.write(buffer, read);
        mbedtls_sha256_update(&ctx, buffer, static_cast<size_t>(read));
    }
    uint8_t digest[32];
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    return security::computeSha256Hex(digest, sizeof(digest));
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
    const String safe = security::sanitizeBinFilename(binFile);
    if (!safe.length()) return "";
    return String(kFirmwareDir) + "/" + safe;
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
            const int slash = name.lastIndexOf('/');
            const String binFile = security::sanitizeBinFilename(name.substring(slash + 1));
            if (binFile.length()) {
                FirmwarePackage pkg;
                pkg.binFile = binFile;
                pkg.name = binFile.substring(0, binFile.length() - 4);
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
    if (!security::isAllowedHttpsUrl(manifestUrl)) {
        log::info("manifest_url_rejected");
        return false;
    }
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
    if (!security::isAllowedHttpsUrl(pkg.url)) {
        log::info("download_url_rejected", pkg.name);
        return false;
    }
    const String safeBin = security::sanitizeBinFilename(pkg.binFile);
    if (!safeBin.length()) {
        log::info("download_bin_rejected", pkg.name);
        return false;
    }
    HTTPClient http;
    http.begin(pkg.url);
    http.setTimeout(20000);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        log::info("download_http_fail", String(code));
        return false;
    }
    const String path = binPathFor(safeBin);
    if (!path.length()) {
        http.end();
        return false;
    }
    File out = SD.open(path, FILE_WRITE);
    if (!out) {
        http.end();
        return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    const String digest = hashDownloadStream(stream, out);
    out.close();
    http.end();
    if (!digest.length()) {
        SD.remove(path.c_str());
        log::info("download_hash_fail", pkg.name);
        return false;
    }
    if (pkg.sha256.length()) {
        if (!security::sha256Equal(pkg.sha256, digest)) {
            SD.remove(path.c_str());
            log::info("download_checksum_fail", pkg.name);
            return false;
        }
        log::info("download_checksum_ok", pkg.name);
    } else {
        log::info("download_checksum_skip", pkg.name);
    }
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

const FirmwarePackage* FirmwareCatalog::findByBinFile(const String& binFile) const {
    const String safe = security::sanitizeBinFilename(binFile);
    if (!safe.length()) return nullptr;
    for (const auto& pkg : available_) {
        if (pkg.binFile == safe) return &pkg;
    }
    for (const auto& pkg : installed_) {
        if (pkg.binFile == safe) return &pkg;
    }
    return nullptr;
}

}  // namespace m5os
