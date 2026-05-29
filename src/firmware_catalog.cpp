#include "firmware_catalog.h"

#include "m5burner_hookup.h"
#include "m5os_config.h"
#include "m5os_security.h"
#include "m5os_vfs.h"
#include "serial_log.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
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
        pkg.slug = vfs::slugFromName(pkg.name);
        pkg.version = item["version"] | "1.0.0";
        pkg.url = item["url"] | "";
        pkg.fid = security::normalizeBurnerFid(item["fid"] | "");
        pkg.file = security::sanitizeBurnerFile(item["file"] | "");
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
        if (!pkg.url.length() && pkg.fid.length() && pkg.file.length()) {
            pkg.url = burner::resolveDownloadUrl(pkg.fid, pkg.file);
        }
        if (!pkg.url.length() && pkg.fid.length()) {
            // Burner catalog entry — resolve download URL when user installs.
            pkg.version = pkg.version.length() ? pkg.version : "burner";
        } else if (!pkg.url.length()) {
            log::info("manifest_entry_rejected", pkg.name);
            continue;
        }
        out.push_back(pkg);
    }
    return !out.empty();
}

String hashDownloadStreamWithLimit(WiFiClient* stream, File& out, size_t maxBytes) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    uint8_t buffer[512];
    size_t total = 0;
    while (stream->connected() || stream->available()) {
        const size_t available = stream->available();
        if (!available) {
            delay(1);
            continue;
        }
        const int read = stream->readBytes(buffer, min(available, sizeof(buffer)));
        if (read <= 0) break;
        if (total + static_cast<size_t>(read) > maxBytes) {
            mbedtls_sha256_free(&ctx);
            return "";
        }
        total += static_cast<size_t>(read);
        out.write(buffer, read);
        mbedtls_sha256_update(&ctx, buffer, static_cast<size_t>(read));
    }
    uint8_t digest[32];
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    return security::computeSha256Hex(digest, sizeof(digest));
}

void scanDirBins(const char* dirPath, std::vector<FirmwarePackage>& out) {
    File dir = SD.open(dirPath);
    if (!dir) return;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (entry.isDirectory()) {
            String subName = entry.name();
            entry.close();
            const int slash = subName.lastIndexOf('/');
            const String slug = security::sanitizePathSegment(subName.substring(slash + 1));
            if (slug.length()) {
                File sub = SD.open((String(dirPath) + "/" + slug).c_str());
                if (sub && sub.isDirectory()) {
                    File binEntry;
                    while ((binEntry = sub.openNextFile())) {
                        if (!binEntry.isDirectory()) {
                            String binName = binEntry.name();
                            const int binSlash = binName.lastIndexOf('/');
                            const String binFile = security::sanitizeBinFilename(binName.substring(binSlash + 1));
                            if (binFile.length()) {
                                bool duplicate = false;
                                for (const auto& existing : out) {
                                    if (existing.binFile == binFile) duplicate = true;
                                }
                                if (!duplicate) {
                                    FirmwarePackage pkg;
                                    pkg.slug = slug;
                                    pkg.binFile = binFile;
                                    pkg.name = slug;
                                    pkg.version = "local";
                                    pkg.installed = true;
                                    out.push_back(pkg);
                                }
                            }
                        }
                        binEntry.close();
                    }
                }
                if (sub) sub.close();
            }
            continue;
        }
        String name = entry.name();
        const int slash = name.lastIndexOf('/');
        const String binFile = security::sanitizeBinFilename(name.substring(slash + 1));
        if (binFile.length()) {
            bool duplicate = false;
            for (const auto& existing : out) {
                if (existing.binFile == binFile) duplicate = true;
            }
            if (!duplicate) {
                FirmwarePackage pkg;
                pkg.binFile = binFile;
                pkg.slug = vfs::slugFromName(binFile.substring(0, binFile.length() - 4));
                pkg.name = pkg.slug;
                pkg.version = "local";
                pkg.installed = true;
                out.push_back(pkg);
            }
        }
        entry.close();
    }
    dir.close();
}

}  // namespace

String FirmwareCatalog::slugToBinFile(const String& name) {
    String slug = vfs::slugFromName(name);
    if (slug == "remote_possibility") return String(kAppRemotePossibility) + ".bin";
    if (slug == "ble_bot") return String(kAppBleBot) + ".bin";
    return slug + ".bin";
}

String FirmwareCatalog::binPathFor(const String& binFile) const {
    const String safe = security::sanitizeBinFilename(binFile);
    if (!safe.length()) return "";
    const String slug = vfs::slugFromName(safe.substring(0, safe.length() - 4));
    return vfs::binPathFor(slug, safe);
}

String FirmwareCatalog::binPathForPackage(const FirmwarePackage& pkg) const {
    const String safe = security::sanitizeBinFilename(pkg.binFile);
    if (!safe.length()) return "";
    const String slug = pkg.slug.length() ? pkg.slug : vfs::slugFromName(pkg.name);
    return vfs::binPathFor(slug, safe);
}

bool FirmwareCatalog::ensureStorage() {
    const vfs::MountResult mount = vfs::mountAndInit();
    if (!mount.ok) return false;
    return true;
}

void FirmwareCatalog::scanInstalled() {
    installed_.clear();
    scanDirBins(kFirmwareDir, installed_);
    scanDirBins(kLegacyFirmwareDir, installed_);
    mergeInstalledFlags();
    log::info("installed_scan", String(installed_.size()) + " bins");
}

void FirmwareCatalog::mergeInstalledFlags() {
    for (auto& pkg : available_) {
        pkg.installed = SD.exists(binPathForPackage(pkg).c_str());
    }
}

std::vector<String> FirmwareCatalog::whitelistedAppSlugs() const {
    std::vector<String> slugs;
    for (const auto& pkg : available_) {
        if (pkg.slug.length()) slugs.push_back(pkg.slug);
    }
    for (const auto& pkg : installed_) {
        if (pkg.slug.length()) {
            bool found = false;
            for (const auto& s : slugs) {
                if (s == pkg.slug) found = true;
            }
            if (!found) slugs.push_back(pkg.slug);
        }
    }
    return slugs;
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

bool FirmwareCatalog::loadSdManifestFrom(const char* path) {
    if (!SD.exists(path)) return false;
    File in = SD.open(path, FILE_READ);
    if (!in) return false;
    String payload;
    while (in.available()) payload += static_cast<char>(in.read());
    in.close();
    if (!parseManifestPayload(payload, available_)) return false;
    scanInstalled();
    log::info("manifest_sd_loaded", String(available_.size()) + " packages");
    return true;
}

bool FirmwareCatalog::refreshFromBurnerHub(uint8_t page) {
    if (WiFi.status() != WL_CONNECTED) return false;
    std::vector<FirmwarePackage> burner;
    if (!burner::fetchCatalogPage(page, burner)) return false;
    for (const auto& pkg : burner) {
        bool duplicate = false;
        for (const auto& existing : available_) {
            if ((pkg.fid.length() && existing.fid == pkg.fid) ||
                existing.binFile == pkg.binFile) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) available_.push_back(pkg);
    }
    scanInstalled();
    log::info("burner_catalog_loaded", String(burner.size()) + " packages");
    return true;
}

bool FirmwareCatalog::refreshFromSdManifest() {
    if (loadSdManifestFrom(kSdManifestPath)) return true;
    return loadSdManifestFrom(kLegacyManifestPath);
}

bool FirmwareCatalog::downloadPackage(const FirmwarePackage& pkgIn) {
    if (!vfs::isMounted() && !ensureStorage()) return false;
    FirmwarePackage pkg = pkgIn;
    if (!pkg.url.length() && pkg.fid.length()) {
        if (!burner::enrichPackageFromBurner(pkg)) {
            log::info("burner_enrich_fail", pkg.name);
            return false;
        }
    } else if (!pkg.url.length() && pkg.fid.length() && pkg.file.length()) {
        pkg.url = burner::resolveDownloadUrl(pkg.fid, pkg.file);
    }
    if (WiFi.status() != WL_CONNECTED || !pkg.url.length()) return false;
    if (!security::isAllowedHttpsUrl(pkg.url)) {
        log::info("download_url_rejected", pkg.name);
        return false;
    }
    if (pkg.size > kMaxAppBinBytes) {
        log::info("download_size_rejected", pkg.name);
        return false;
    }
    const String safeBin = security::sanitizeBinFilename(pkg.binFile);
    if (!safeBin.length()) {
        log::info("download_bin_rejected", pkg.name);
        return false;
    }
    const String slug = pkg.slug.length() ? pkg.slug : vfs::slugFromName(pkg.name);
    if (!vfs::ensureAppDirs(slug)) {
        log::info("download_dir_fail", pkg.name);
        return false;
    }
    HTTPClient http;
    http.begin(pkg.url);
    http.setTimeout(20000);
    burner::applyBurnerDownloadHeaders(http, pkg.url);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        log::info("download_http_fail", String(code));
        return false;
    }
    const int contentLength = http.getSize();
    if (contentLength > 0 && static_cast<size_t>(contentLength) > kMaxAppBinBytes) {
        http.end();
        log::info("download_size_rejected", pkg.name);
        return false;
    }
    const String path = String(vfs::appDirFor(slug)) + "/" + safeBin;
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
    const String digest = hashDownloadStreamWithLimit(stream, out, kMaxAppBinBytes);
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
    const String slug = vfs::slugFromName(name);
    for (auto& pkg : installed_) {
        if (pkg.name.equalsIgnoreCase(name) || pkg.slug == slug || pkg.binFile.startsWith(name)) return &pkg;
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
