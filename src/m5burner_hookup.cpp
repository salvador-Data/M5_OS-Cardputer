#include "m5burner_hookup.h"

#include "firmware_catalog.h"
#include "m5os_security.h"
#include "m5os_vfs.h"
#include "serial_log.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace m5os::burner {

namespace {

String httpGetPayload(const String& url) {
    if (!security::isAllowedHttpsUrl(url)) {
        log::info("burner_url_rejected");
        return "";
    }
    HTTPClient http;
    http.begin(url);
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Accept-Encoding", "identity");
    applyBurnerDownloadHeaders(http, url);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        log::info("burner_http_fail", String(code));
        return "";
    }
    const String payload = http.getString();
    http.end();
    return payload;
}

}  // namespace

String urlEncodeQueryComponent(const String& raw) {
    String out;
    out.reserve(raw.length() * 3);
    for (size_t i = 0; i < raw.length(); ++i) {
        const char c = raw[i];
        const bool safe =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~';
        if (safe) {
            out += c;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
            out += hex;
        }
    }
    return out;
}

String burnerWifiHwid() {
    if (WiFi.status() != WL_CONNECTED) return "";
    String mac = WiFi.macAddress();
    mac.toUpperCase();
    return mac;
}

bool isLauncherHubDownloadUrl(const String& url) {
    return url.startsWith(String(kLauncherHubDownloadBase));
}

void applyBurnerDownloadHeaders(HTTPClient& http, const String& url) {
    if (!isLauncherHubDownloadUrl(url)) return;
    const String hwid = burnerWifiHwid();
    if (hwid.length()) http.addHeader("HWID", hwid);
}

namespace {

bool parseVersionPayload(const String& payload, FirmwarePackage& pkg) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;
    JsonArray versions = doc["versions"].as<JsonArray>();
    if (versions.isNull() || versions.size() == 0) return false;

    JsonObject latest = versions[0].as<JsonObject>();
    const String version = latest["version"] | "";
    const String file = security::sanitizeBurnerFile(latest["file"] | "");
    const size_t size = latest["Fs"] | 0;
    if (!file.length()) return false;

    if (version.length()) pkg.version = version;
    pkg.file = file;
    if (size > 0) pkg.size = size;
    if (!pkg.fid.length()) pkg.fid = security::normalizeBurnerFid(doc["fid"] | "");
    if (!pkg.name.length()) pkg.name = String(doc["name"] | "");
    return true;
}

}  // namespace

String resolveFileUrl(const String& file) {
    const String safe = security::sanitizeBurnerFile(file);
    if (!safe.length()) return "";
    if (safe.startsWith("https://")) return safe;
    return String(kM5BurnerCdnBase) + safe;
}

String resolveDownloadUrl(const String& fid, const String& file) {
    const String safeFid = security::normalizeBurnerFid(fid);
    const String fileUrl = resolveFileUrl(file);
    if (!fileUrl.length()) return "";
    if (!safeFid.length()) return fileUrl;
    String url = String(kLauncherHubDownloadBase) + "?fid=" + safeFid + "&file=" +
                 urlEncodeQueryComponent(fileUrl);
    if (!security::isAllowedHttpsUrl(url)) return "";
    return url;
}

bool fetchCatalogPage(uint8_t page, std::vector<FirmwarePackage>& out) {
    if (WiFi.status() != WL_CONNECTED) return false;
    String url = String(kLauncherHubCatalogBase) + "?category=" + kBurnerCategory +
                 "&order_by=downloads&page=" + String(page);
    const String payload = httpGetPayload(url);
    if (!payload.length()) return false;

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        log::info("burner_catalog_parse_fail");
        return false;
    }
    JsonArray items = doc["items"].as<JsonArray>();
    if (items.isNull()) return false;

    for (JsonObject item : items) {
        const String fid = security::normalizeBurnerFid(item["fid"] | "");
        const String name = String(item["name"] | "").substring(0, 64);
        if (!fid.length() || !name.length()) continue;

        FirmwarePackage pkg;
        pkg.fid = fid;
        pkg.name = name;
        pkg.slug = vfs::slugFromName(name);
        pkg.version = "burner";
        pkg.description = String(item["author"] | "");
        pkg.binFile = security::sanitizeBinFilename(FirmwareCatalog::slugToBinFile(name));
        if (!pkg.binFile.length()) continue;
        out.push_back(pkg);
    }
    log::info("burner_catalog_page", String(out.size()) + " items");
    return !out.empty();
}

bool enrichPackageFromBurner(FirmwarePackage& pkg) {
    const String fid = security::normalizeBurnerFid(pkg.fid);
    if (!fid.length()) return false;
    if (pkg.file.length() && pkg.url.length()) return true;

    const String payload = httpGetPayload(String(kLauncherHubCatalogBase) + "?fid=" + fid);
    if (!payload.length()) return false;
    if (!parseVersionPayload(payload, pkg)) return false;

    pkg.url = resolveDownloadUrl(fid, pkg.file);
    return pkg.url.length() > 0;
}

}  // namespace m5os::burner
