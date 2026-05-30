#include "utms_threat_pack.h"

#include "m5os_config.h"
#include "m5os_security.h"
#include "m5os_vfs.h"
#include "serial_log.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>

namespace m5os::utms {

namespace {

constexpr char kNvsNamespace[] = "m5os_utms";
constexpr char kLastVersionKey[] = "last_ver";
constexpr char kLastCheckKey[] = "last_chk";
constexpr uint32_t kOtaMinIntervalMs = 60000;
constexpr uint32_t kHttpTimeoutMs = 20000;
constexpr size_t kMaxPackBytes = 65536;
constexpr size_t kMaxHashes = 256;
constexpr size_t kMaxStrings = 64;
constexpr size_t kMaxStringLen = 128;

uint32_t gLastOtaAttemptMs = 0;

bool writeAtomicFile(const char* finalPath, const String& body) {
    const String tmp = String(finalPath) + ".tmp";
    File out = SD.open(tmp.c_str(), FILE_WRITE);
    if (!out) return false;
    const size_t written = out.print(body);
    out.close();
    if (written != body.length()) {
        SD.remove(tmp.c_str());
        return false;
    }
    if (SD.exists(finalPath)) SD.remove(finalPath);
    if (!SD.rename(tmp.c_str(), finalPath)) {
        SD.remove(tmp.c_str());
        return false;
    }
    return true;
}

bool verifyPackDoc(JsonDocument& doc) {
    const String version = doc["version"] | "";
    if (!version.length() || version.length() > 32) return false;

    JsonObject sigs = doc["signatures"].as<JsonObject>();
    if (sigs.isNull()) return false;

    JsonArray hashes = sigs["hashes"].as<JsonArray>();
    if (!hashes.isNull()) {
        if (hashes.size() > kMaxHashes) return false;
        for (JsonVariant v : hashes) {
            if (!security::isValidSha256Hex(v.as<String>())) return false;
        }
    }

    JsonArray strings = sigs["strings"].as<JsonArray>();
    if (!strings.isNull()) {
        if (strings.size() > kMaxStrings) return false;
        for (JsonVariant v : strings) {
            const String s = v.as<String>();
            if (!s.length() || s.length() > kMaxStringLen) return false;
        }
    }

    JsonArray allowHashes = sigs["allow_hashes"].as<JsonArray>();
    if (!allowHashes.isNull()) {
        if (allowHashes.size() > kMaxHashes) return false;
        for (JsonVariant v : allowHashes) {
            if (!security::isValidSha256Hex(v.as<String>())) return false;
        }
    }

    const String expectedSha = security::normalizeSha256Hex(doc["sha256"] | "");
    if (expectedSha.length()) {
        doc.remove("sha256");
        String canonical;
        serializeJson(doc, canonical);
        doc["sha256"] = expectedSha;
        const String actual = security::computeSha256Hex(
            reinterpret_cast<const uint8_t*>(canonical.c_str()), canonical.length());
        if (!security::sha256Equal(expectedSha, actual)) return false;
    }
    return true;
}

String httpGetPackBody(const String& url, int* httpCodeOut) {
    if (!security::isAllowedHttpsUrl(url)) {
        if (httpCodeOut) *httpCodeOut = -1;
        return "";
    }
    HTTPClient http;
    http.begin(url);
    http.setTimeout(kHttpTimeoutMs);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    if (httpCodeOut) *httpCodeOut = code;
    if (code != HTTP_CODE_OK) {
        http.end();
        return "";
    }
    const int len = http.getSize();
    if (len > static_cast<int>(kMaxPackBytes)) {
        http.end();
        if (httpCodeOut) *httpCodeOut = -2;
        return "";
    }
    const String body = http.getString();
    http.end();
    if (body.length() > kMaxPackBytes) {
        if (httpCodeOut) *httpCodeOut = -2;
        return "";
    }
    return body;
}

void saveNvsVersion(const String& version) {
    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, false)) return;
    prefs.putString(kLastVersionKey, version);
    prefs.end();
}

}  // namespace

bool ensureUtmsDirs() {
    if (!vfs::isMounted() && !vfs::mountAndInit().ok) return false;
    return vfs::ensureDirectoryChain(vfs::kUtmsDir, nullptr) &&
           vfs::ensureDirectoryChain(vfs::kQuarantineDir, nullptr);
}

void appendLog(const char* event, const String& detail) {
    if (!ensureUtmsDirs()) return;
    File log = SD.open(vfs::kUtmsLogPath, FILE_APPEND);
    if (!log) return;
    log.printf("[%lu] %s", static_cast<unsigned long>(millis()), event);
    if (detail.length()) log.printf(" %s", detail.c_str());
    log.println();
    log.close();
}

ThreatPackInfo loadPackInfo() {
    ThreatPackInfo info;
    if (!ensureUtmsDirs()) return info;
    if (!SD.exists(vfs::kThreatPackPath)) return info;

    File in = SD.open(vfs::kThreatPackPath, FILE_READ);
    if (!in) return info;
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, in);
    in.close();
    if (err) return info;

    info.version = doc["version"] | "";
    info.sha256Expected = security::normalizeSha256Hex(doc["sha256"] | "");
    JsonObject sigs = doc["signatures"].as<JsonObject>();
    if (!sigs.isNull()) {
        JsonArray hashes = sigs["hashes"].as<JsonArray>();
        JsonArray strings = sigs["strings"].as<JsonArray>();
        if (!hashes.isNull()) info.hashCount = hashes.size();
        JsonArray allow = sigs["allow_hashes"].as<JsonArray>();
        if (!allow.isNull()) info.allowHashCount = allow.size();
        if (!strings.isNull()) info.stringCount = strings.size();
    }
    info.loaded = info.version.length() > 0;
    return info;
}

String lastUpdateVersion() {
    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, true)) return "";
    const String ver = prefs.getString(kLastVersionKey, "");
    prefs.end();
    return ver;
}

uint32_t lastCheckEpoch() {
    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, true)) return 0;
    const uint32_t epoch = prefs.getUInt(kLastCheckKey, 0);
    prefs.end();
    return epoch;
}

void markCheckEpoch() {
    Preferences prefs;
    if (!prefs.begin(kNvsNamespace, false)) return;
    uint32_t epoch = static_cast<uint32_t>(time(nullptr));
    if (epoch < 1000000000U) epoch = millis() / 1000U;
    prefs.putUInt(kLastCheckKey, epoch);
    prefs.end();
}

bool canAttemptOtaFetch() {
    const uint32_t now = millis();
    if (gLastOtaAttemptMs && (now - gLastOtaAttemptMs) < kOtaMinIntervalMs) return false;
    gLastOtaAttemptMs = now;
    return true;
}

UpdateResult fetchAndInstallPack(const String& url) {
    UpdateResult result;
    if (!canAttemptOtaFetch()) {
        result.message = "Rate limited\nWait 60s";
        return result;
    }
    if (WiFi.status() != WL_CONNECTED) {
        result.message = "WiFi required";
        return result;
    }
    if (!ensureUtmsDirs()) {
        result.message = "SD required";
        return result;
    }

    int httpCode = 0;
    const String body = httpGetPackBody(url, &httpCode);
    markCheckEpoch();

    if (!body.length()) {
        result.message = httpCode > 0 ? String("HTTP ") + String(httpCode) : "Download failed";
        if (httpCode == -2) result.message = "Pack too large";
        log::info("utms_ota_fail", result.message);
        appendLog("ota_fail", result.message);
        return result;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    if (err) {
        result.message = "Invalid JSON";
        appendLog("ota_fail", result.message);
        return result;
    }
    if (!verifyPackDoc(doc)) {
        result.message = "Pack verify failed\nCheck sha256";
        appendLog("ota_fail", result.message);
        return result;
    }

    const String version = doc["version"] | "";
    if (!writeAtomicFile(vfs::kThreatPackPath, body)) {
        result.message = "SD write failed";
        appendLog("ota_fail", result.message);
        return result;
    }

    saveNvsVersion(version);
    result.ok = true;
    result.version = version;
    result.message = "Installed v" + version;
    log::info("utms_ota_ok", version);
    appendLog("ota_ok", version);
    return result;
}

}  // namespace m5os::utms
