#include "burner_install.h"

#include "m5burner_hookup.h"
#include "m5os_config.h"
#include "M5OSDevice.h"
#include "m5os_flash.h"
#include "m5os_security.h"
#include "m5os_watchdog.h"
#include "serial_log.h"
#include "ui_display.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_system.h>

namespace m5os::burner {

namespace {

constexpr uint32_t kCatalogHttpTimeoutMs = 20000;
constexpr uint32_t kDownloadHttpTimeoutMs = 60000;
constexpr uint32_t kStreamIdleMinMs = 120000;
constexpr uint32_t kStreamIdleMaxMs = 300000;
constexpr uint8_t kHttpMaxAttempts = 3;
constexpr size_t kStreamChunkBytes = 64;

uint32_t streamIdleTimeoutMs(size_t imageSize) {
    // ~1 ms idle budget per 32 bytes — Bruce ~4 MB needs several minutes on slow WiFi.
    const uint32_t scaled = static_cast<uint32_t>(imageSize / 32U);
    if (scaled < kStreamIdleMinMs) return kStreamIdleMinMs;
    if (scaled > kStreamIdleMaxMs) return kStreamIdleMaxMs;
    return scaled;
}

String formatHttpStageError(const char* stage, int code) {
    String msg = stage;
    if (code > 0) msg += " HTTP " + String(code);
    else if (code < 0) msg += " err " + String(code);
    return msg;
}

void setPlanError(BurnerPlanError* errOut, const char* stage, int httpCode, const char* detail) {
    if (!errOut) return;
    errOut->stage = stage;
    errOut->httpCode = httpCode;
    errOut->message = formatHttpStageError(stage, httpCode);
    if (detail && detail[0] != '\0') errOut->message += "\n" + String(detail);
}

struct HttpGetResult {
    int code = -1;
    bool ok = false;
};

HttpGetResult httpGetWithRetry(HTTPClient& http, const String& url, uint32_t timeoutMs,
                               const String& rangeHeader = "") {
    HttpGetResult result;
    for (uint8_t attempt = 0; attempt < kHttpMaxAttempts; ++attempt) {
        if (WiFi.status() != WL_CONNECTED) {
            delay(400);
            continue;
        }
        http.begin(url);
        http.setTimeout(timeoutMs);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.addHeader("Accept-Encoding", "identity");
        applyBurnerDownloadHeaders(http, url);
        if (rangeHeader.length()) http.addHeader("Range", rangeHeader);
        result.code = http.GET();
        if (result.code == HTTP_CODE_OK || result.code == HTTP_CODE_PARTIAL_CONTENT) {
            result.ok = true;
            return result;
        }
        http.end();
        delay(400 * (attempt + 1));
    }
    return result;
}

String httpGetPayload(const String& url) {
    if (!security::isAllowedHttpsUrl(url)) {
        log::info("burner_url_rejected");
        return "";
    }
    HTTPClient http;
    const HttpGetResult result = httpGetWithRetry(http, url, kCatalogHttpTimeoutMs);
    if (!result.ok) {
        log::info("burner_http_fail", String(result.code));
        return "";
    }
    const String payload = http.getString();
    http.end();
    return payload;
}

bool parseVersionObject(JsonObject versionObj, BurnerVersionInfo& info) {
    const String version = versionObj["version"] | "";
    const String file = security::sanitizeBurnerFile(versionObj["file"] | "");
    if (!version.length() || !file.length()) return false;
    info.version = version;
    info.file = file;
    info.appOffset = versionObj["ao"] | 0;
    info.appSize = versionObj["as"] | 0;
    if (versionObj["nb"].isNull()) {
        info.noBootloader = info.appOffset == 0;
    } else {
        info.noBootloader = versionObj["nb"] | false;
    }
    return true;
}

void appendDataSlice(BurnerInstallPlan& plan, JsonObject part) {
    const uint32_t copySize = part["copy_size"] | 0;
    const String subtype = String(part["subtype"] | "");
    const bool required = part["required"] | false;
    if (copySize == 0) {
        if (required && (subtype == "spiffs" || subtype == "fat")) {
            plan.requiresFlashAssets = true;
        }
        return;
    }
    BurnerDataSlice slice;
    slice.subtype = subtype;
    slice.label = String(part["label"] | "");
    slice.sourceOffset = part["source_offset"] | 0;
    slice.copySize = copySize;
    plan.dataSlices.push_back(slice);
    if (subtype == "spiffs" || subtype == "fat") plan.requiresFlashAssets = true;
}

bool parseInstallManifest(JsonObject detail, const String& version, BurnerInstallPlan& plan) {
    JsonObject versionObj = detail["version"].as<JsonObject>();
    if (versionObj.isNull()) return false;
    const String file = security::sanitizeBurnerFile(versionObj["file"] | "");
    if (!file.length()) return false;

    plan.version = version.length() ? version : String(versionObj["version"] | "");
    plan.file = file;
    plan.appOffset = 0;
    plan.appSize = 0;
    plan.noBootloader = true;
    plan.dataSlices.clear();
    plan.requiresFlashAssets = false;
    plan.fromManifest = true;

    JsonObject install = versionObj["install"].as<JsonObject>();
    if (!install.isNull()) {
        JsonObject app = install["app"].as<JsonObject>();
        if (!app.isNull()) {
            plan.appOffset = app["source_offset"] | 0;
            plan.appSize = app["image_size"] | 0;
            plan.noBootloader = plan.appOffset == 0;
        }
        JsonArray partitions = install["partitions"].as<JsonArray>();
        if (!partitions.isNull()) {
            for (JsonObject part : partitions) {
                const String type = part["type"] | "";
                const String subtype = part["subtype"] | "";
                if (type == "app" && subtype == "ota") {
                    plan.appOffset = part["source_offset"] | plan.appOffset;
                    plan.appSize = part["copy_size"] | plan.appSize;
                    plan.noBootloader = plan.appOffset == 0;
                } else if (type == "data" &&
                           (subtype == "spiffs" || subtype == "fat")) {
                    appendDataSlice(plan, part);
                }
            }
        }
    }
    return true;
}

bool fillPlanDownloadUrl(BurnerInstallPlan& plan) {
    if (!plan.file.length()) return false;
    plan.downloadUrl = m5os::burner::resolveDownloadUrl(plan.fid, plan.file);
    return plan.downloadUrl.length() > 0;
}

bool finalizePlanSize(BurnerInstallPlan& plan) {
    if (plan.appSize == 0) {
        if (!plan.noBootloader && plan.appOffset == 0) return false;
        plan.noBootloader = true;
    }
    if (plan.appSize > maxOtaAppBytes()) return false;
    return plan.downloadUrl.length() > 0;
}

size_t parseContentRangeTotal(const String& contentRange) {
    const int slash = contentRange.lastIndexOf('/');
    if (slash < 0 || slash + 1 >= contentRange.length()) return 0;
    return static_cast<size_t>(contentRange.substring(slash + 1).toInt());
}

bool probeRemoteSize(const String& url, size_t& totalSize, int* httpCodeOut = nullptr) {
    HTTPClient http;
    const HttpGetResult result = httpGetWithRetry(http, url, kCatalogHttpTimeoutMs, "bytes=0-0");
    if (httpCodeOut) *httpCodeOut = result.code;
    if (!result.ok || result.code != HTTP_CODE_PARTIAL_CONTENT) {
        http.end();
        return false;
    }
    const String contentRange = http.header("Content-Range");
    http.end();
    totalSize = parseContentRangeTotal(contentRange);
    return totalSize > 0;
}

struct RangeFlashContext {
    size_t expected = 0;
    size_t written = 0;
    File* sdOut = nullptr;
    bool updateStarted = false;
};

bool writeFlashChunk(RangeFlashContext& ctx, const uint8_t* data, size_t len) {
    if (!ctx.updateStarted) {
        if (!Update.begin(ctx.expected)) {
            log::info("burner_begin_fail", String(Update.errorString()));
            return false;
        }
        ctx.updateStarted = true;
    }
    if (Update.write(const_cast<uint8_t*>(data), len) != len) {
        log::info("burner_write_fail");
        return false;
    }
    if (ctx.sdOut && (*ctx.sdOut)) {
        if (ctx.sdOut->write(data, len) != len) return false;
    }
    ctx.written += len;
    return true;
}

struct RangeStreamResult {
    bool ok = false;
    int httpCode = 0;
    size_t written = 0;
};

void reportStreamProgress(size_t written, size_t total, const char* label,
                          const char* detailLine, size_t* lastPainted) {
    if (!label || total == 0) return;
    if (lastPainted && *lastPainted == written) return;
    if (lastPainted) *lastPainted = written;

    const int percent = static_cast<int>(min(100ULL, (written * 100ULL) / total));
    String detail;
    if (detailLine && detailLine[0] != '\0') detail = detailLine;
    const String bytes =
        String(static_cast<unsigned>(written)) + " / " + String(static_cast<unsigned>(total));
    if (detail.length()) detail += "\n";
    detail += bytes;
    ui::showFlashProgress(percent, label, detail);
    m5os::update();
}

RangeStreamResult streamRangeChunk(const String& url, uint32_t offset, size_t imageSize, File& out,
                                   const char* progressLabel, const char* detailLine,
                                   size_t progressBase, size_t progressTotal) {
    RangeStreamResult result;
    if (!security::isAllowedHttpsUrl(url)) return result;
    if (imageSize == 0) return result;

    const uint32_t endByte = offset + static_cast<uint32_t>(imageSize) - 1;
    const String rangeHeader = "bytes=" + String(offset) + "-" + String(endByte);

    HTTPClient http;
    const HttpGetResult httpResult =
        httpGetWithRetry(http, url, kDownloadHttpTimeoutMs, rangeHeader);
    result.httpCode = httpResult.code;
    if (!httpResult.ok) {
        http.end();
        log::info("burner_range_fail", String(httpResult.code));
        return result;
    }

    const int headerLen = http.getSize();
    if (headerLen > 0 && static_cast<size_t>(headerLen) != imageSize) {
        log::info("burner_len_mismatch", String(headerLen) + "/" + String(imageSize));
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    size_t lastPainted = SIZE_MAX;
    uint8_t buffer[kStreamChunkBytes];
    uint32_t idleMs = 0;
    reportStreamProgress(progressBase, progressTotal, progressLabel, detailLine, &lastPainted);
    const uint32_t idleLimitMs = streamIdleTimeoutMs(imageSize);
    while (written < imageSize) {
        if (WiFi.status() != WL_CONNECTED) break;
        const size_t avail = stream->available();
        if (!avail) {
            if (!stream->connected() && !stream->available()) break;
            delay(1);
            idleMs += 1;
            if (idleMs % 16 == 0) {
                m5os::update();
                feedWatchdog();
            }
            if (idleMs > idleLimitMs) break;
            continue;
        }
        idleMs = 0;
        const size_t want = min(avail, min(sizeof(buffer), imageSize - written));
        const int read = stream->readBytes(buffer, want);
        if (read <= 0) break;
        if (out.write(buffer, static_cast<size_t>(read)) != static_cast<size_t>(read)) {
            http.end();
            result.written = written;
            return result;
        }
        written += static_cast<size_t>(read);
        feedWatchdog();
        reportStreamProgress(progressBase + written, progressTotal, progressLabel, detailLine,
                             &lastPainted);
    }
    http.end();
    result.written = written;
    result.ok = written == imageSize;
    if (!result.ok) log::info("burner_incomplete", String(written) + "/" + String(imageSize));
    return result;
}

RangeStreamResult streamRangeToFile(const String& url, uint32_t offset, size_t imageSize, File& out,
                                    const char* progressLabel, const char* detailLine = nullptr) {
    RangeStreamResult result;
    if (!out) return result;

    size_t totalWritten = 0;
    uint32_t remoteOffset = offset;
    size_t remaining = imageSize;

    for (uint8_t attempt = 0; attempt < kHttpMaxAttempts && remaining > 0; ++attempt) {
        if (totalWritten > 0 && !out.seek(totalWritten)) {
            log::info("burner_sd_seek_fail", String(totalWritten));
            break;
        }
        const RangeStreamResult chunk =
            streamRangeChunk(url, remoteOffset, remaining, out, progressLabel, detailLine,
                             totalWritten, imageSize);
        result.httpCode = chunk.httpCode;
        totalWritten += chunk.written;
        remoteOffset += static_cast<uint32_t>(chunk.written);
        remaining -= chunk.written;
        if (chunk.ok) {
            result.ok = true;
            result.written = totalWritten;
            return result;
        }
        if (chunk.written == 0) {
            delay(400 * (attempt + 1));
            continue;
        }
        out.flush();
        delay(400 * (attempt + 1));
    }

    result.written = totalWritten;
    result.ok = totalWritten == imageSize;
    if (!result.ok) log::info("burner_incomplete", String(totalWritten) + "/" + String(imageSize));
    return result;
}

bool streamRangeToOtaOnce(const String& url, uint32_t offset, size_t imageSize, File* sdOut,
                          int* httpCodeOut, const char* progressLabel, const char* detailLine,
                          RangeFlashContext& ctx) {
    if (!security::isAllowedHttpsUrl(url)) return false;
    if (imageSize == 0 || imageSize > maxOtaAppBytes()) return false;

    const uint32_t endByte = offset + static_cast<uint32_t>(imageSize) - 1;
    const String rangeHeader = "bytes=" + String(offset) + "-" + String(endByte);

    HTTPClient http;
    const HttpGetResult httpResult =
        httpGetWithRetry(http, url, kDownloadHttpTimeoutMs, rangeHeader);
    if (httpCodeOut) *httpCodeOut = httpResult.code;
    if (!httpResult.ok) {
        http.end();
        log::info("burner_range_fail", String(httpResult.code));
        return false;
    }

    const int headerLen = http.getSize();
    if (headerLen > 0 && static_cast<size_t>(headerLen) != imageSize) {
        log::info("burner_len_mismatch", String(headerLen) + "/" + String(imageSize));
    }

    WiFiClient* stream = http.getStreamPtr();
    ctx.expected = imageSize;
    ctx.sdOut = sdOut;
    if (ctx.updateStarted) Update.abort();
    ctx.updateStarted = false;
    ctx.written = 0;

    uint8_t buffer[kStreamChunkBytes];
    uint32_t idleMs = 0;
    size_t lastPainted = SIZE_MAX;
    reportStreamProgress(0, imageSize, progressLabel, detailLine, &lastPainted);
    const uint32_t idleLimitMs = streamIdleTimeoutMs(imageSize);
    while (ctx.written < imageSize) {
        if (WiFi.status() != WL_CONNECTED) break;
        const size_t avail = stream->available();
        if (!avail) {
            if (!stream->connected() && !stream->available()) break;
            delay(1);
            idleMs += 1;
            if (idleMs % 16 == 0) {
                m5os::update();
                feedWatchdog();
            }
            if (idleMs > idleLimitMs) break;
            continue;
        }
        idleMs = 0;
        const size_t want = min(avail, min(sizeof(buffer), imageSize - ctx.written));
        const int read = stream->readBytes(buffer, want);
        if (read <= 0) break;
        if (!writeFlashChunk(ctx, buffer, static_cast<size_t>(read))) {
            if (ctx.updateStarted) Update.abort();
            http.end();
            return false;
        }
        feedWatchdog();
        reportStreamProgress(ctx.written, imageSize, progressLabel, detailLine, &lastPainted);
    }
    http.end();

    if (ctx.written != imageSize) {
        if (ctx.updateStarted) Update.abort();
        ctx.updateStarted = false;
        log::info("burner_incomplete", String(ctx.written) + "/" + String(imageSize));
        return false;
    }
    if (!Update.end(true)) {
        log::info("burner_end_fail", String(Update.errorString()));
        return false;
    }
    restoreBootToHome();
    return true;
}

bool streamRangeToOta(const String& url, uint32_t offset, size_t imageSize, File* sdOut,
                      int* httpCodeOut = nullptr, const char* progressLabel = "Loading app",
                      const char* detailLine = nullptr) {
    RangeFlashContext ctx;
    ctx.sdOut = sdOut;
    for (uint8_t attempt = 0; attempt < kHttpMaxAttempts; ++attempt) {
        if (streamRangeToOtaOnce(url, offset, imageSize, sdOut, httpCodeOut, progressLabel,
                                 detailLine, ctx)) {
            return true;
        }
        if (ctx.updateStarted) Update.abort();
        ctx.updateStarted = false;
        ctx.written = 0;
        if (sdOut && (*sdOut)) {
            if (!sdOut->seek(0)) {
                log::info("burner_sd_seek_fail", "0");
                return false;
            }
        }
        delay(400 * (attempt + 1));
    }
    return false;
}

String sdExtraPathFromApp(const String& appSdPath, const BurnerDataSlice& slice) {
    if (!appSdPath.length() || slice.copySize == 0) return "";
    const int slash = appSdPath.lastIndexOf('/');
    if (slash < 0) return "";
    String dir = appSdPath.substring(0, slash);
    String name = slice.subtype;
    if (slice.label.length()) name += "_" + slice.label;
    return dir + "/" + name + ".bin";
}

bool applyVersionInfo(const BurnerVersionInfo& info, BurnerInstallPlan& plan) {
    plan.version = info.version;
    plan.file = info.file;
    plan.appOffset = info.appOffset;
    plan.appSize = info.appSize;
    plan.noBootloader = info.noBootloader;
    plan.fromManifest = false;
    plan.requiresFlashAssets = false;
    plan.dataSlices.clear();
    return true;
}

bool fetchInstallManifest(const String& fid, const String& version, BurnerInstallPlan& plan) {
    if (!version.length()) return false;
    const String encodedVersion = urlEncodeQueryComponent(version);
    const String detailUrl =
        String(kLauncherHubCatalogBase) + "?fid=" + fid + "&version=" + encodedVersion;
    const String detailPayload = httpGetPayload(detailUrl);
    if (!detailPayload.length()) return false;

    JsonDocument detail;
    if (deserializeJson(detail, detailPayload)) return false;
    BurnerInstallPlan manifestPlan = plan;
    if (!parseInstallManifest(detail.as<JsonObject>(), version, manifestPlan)) return false;
    plan = manifestPlan;
    plan.fid = fid;
    return true;
}

}  // namespace

String formatBurnerStageError(const char* stage, int httpCode, const char* detail) {
    return formatHttpStageError(stage, httpCode) +
           ((detail && detail[0] != '\0') ? String("\n") + detail : String(""));
}

bool planRequiresSdOnly(const BurnerInstallPlan& plan) {
    if (plan.requiresFlashAssets) return true;
    if (!plan.dataSlices.empty()) return true;
    if (plan.appOffset > 0 && !plan.noBootloader) return true;
    return false;
}

bool fetchVersionList(const String& fid, std::vector<BurnerVersionInfo>& out, int* httpCodeOut) {
    out.clear();
    const String safeFid = security::normalizeBurnerFid(fid);
    if (!safeFid.length() || WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    const HttpGetResult httpResult =
        httpGetWithRetry(http, String(kLauncherHubCatalogBase) + "?fid=" + safeFid, kCatalogHttpTimeoutMs);
    if (httpCodeOut) *httpCodeOut = httpResult.code;
    if (!httpResult.ok) {
        http.end();
        return false;
    }
    const String payload = http.getString();
    http.end();
    if (!payload.length()) return false;

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;
    JsonArray versions = doc["versions"].as<JsonArray>();
    if (versions.isNull()) return false;

    for (JsonObject item : versions) {
        BurnerVersionInfo info;
        if (parseVersionObject(item, info)) out.push_back(info);
    }
    return !out.empty();
}

bool buildInstallPlan(const String& fid, const String& version, BurnerInstallPlan& plan,
                      BurnerPlanError* errOut) {
    plan = BurnerInstallPlan{};
    plan.fid = security::normalizeBurnerFid(fid);
    if (!plan.fid.length()) {
        setPlanError(errOut, "Plan", 0, "Invalid firmware id");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        setPlanError(errOut, "WiFi", 0, "WiFi required");
        return false;
    }

    int versionHttpCode = 0;
    std::vector<BurnerVersionInfo> versions;
    if (!fetchVersionList(plan.fid, versions, &versionHttpCode)) {
        setPlanError(errOut, "Catalog", versionHttpCode, "Version list failed");
        return false;
    }

    String targetVersion = version;
    const BurnerVersionInfo* picked = nullptr;
    for (const auto& info : versions) {
        if (version.length() && info.version != version) continue;
        picked = &info;
        break;
    }
    if (!picked) picked = &versions[0];
    if (!targetVersion.length()) targetVersion = picked->version;

    applyVersionInfo(*picked, plan);

    if (fetchInstallManifest(plan.fid, targetVersion, plan) && fillPlanDownloadUrl(plan)) {
        // Manifest is authoritative for app slice + SPIFFS/FAT metadata.
    } else {
        applyVersionInfo(*picked, plan);
        if (!fillPlanDownloadUrl(plan)) {
            setPlanError(errOut, "Plan", 0, "Download URL failed");
            return false;
        }
    }

    const size_t otaLimit = maxOtaAppBytes();

    if (plan.appSize == 0) {
        size_t remoteSize = 0;
        int probeHttpCode = 0;
        if (!probeRemoteSize(plan.downloadUrl, remoteSize, &probeHttpCode)) {
            setPlanError(errOut, "Download", probeHttpCode, "Size probe failed");
            return false;
        }

        if (plan.appOffset > 0) {
            if (remoteSize <= plan.appOffset) {
                setPlanError(errOut, "Plan", 0, "Image smaller than offset");
                return false;
            }
            const size_t slice = remoteSize - plan.appOffset;
            if (slice > otaLimit) {
                plan.appSize = static_cast<uint32_t>(slice);
                setPlanError(errOut, "Plan", 0, "App too large for OTA slot");
                return false;
            }
            plan.appSize = static_cast<uint32_t>(slice);
            plan.noBootloader = false;
        } else if (plan.noBootloader) {
            if (remoteSize > otaLimit) {
                plan.appSize = static_cast<uint32_t>(remoteSize);
                setPlanError(errOut, "Plan", 0, "App too large for OTA slot");
                return false;
            }
            plan.appSize = static_cast<uint32_t>(remoteSize);
        } else {
            setPlanError(errOut, "Plan", 0, "Missing app size");
            return false;
        }
    }

    if (!finalizePlanSize(plan)) {
        setPlanError(errOut, "Plan", 0,
                     plan.appSize > otaLimit ? "App too large for OTA slot" : "Invalid install plan");
        return false;
    }
    return true;
}

BurnerFlashResult flashAppToOta(const BurnerInstallPlan& plan, const String& sdPath) {
    BurnerFlashResult result;
    if (WiFi.status() != WL_CONNECTED) {
        result.message = "WiFi required";
        return result;
    }
    const size_t otaLimit = maxOtaAppBytes();
    if (!plan.downloadUrl.length() || plan.appSize == 0 || plan.appSize > otaLimit) {
        result.message = plan.appSize > otaLimit ? "App too large for OTA slot" : "Invalid install plan";
        return result;
    }

    saveHomeAppPartition();

    if (planRequiresSdOnly(plan)) {
        if (!sdPath.length()) {
            result.message = "Insert SD — SPIFFS apps save to SD only";
            return result;
        }
        const BurnerDownloadResult saved = downloadPlanToSd(plan, sdPath);
        result.ok = saved.ok;
        if (saved.ok) {
            result.savedExtraToSd = !plan.dataSlices.empty();
            if (plan.requiresFlashAssets) {
                result.message =
                    "App saved to SD.\nNeeds SPIFFS on flash —\nuse M5Burner USB full flash.\n"
                    "Load app blocked here.";
            } else {
                result.message =
                    "Saved to SD. Needs SPIFFS/LittleFS — use M5Burner USB full flash to run. "
                    "M5 OS stays active.";
            }
        } else {
            result.message = saved.message.length() ? saved.message : "Load failed";
        }
        return result;
    }

    File sdOut;
    if (sdPath.length()) {
        sdOut = SD.open(sdPath.c_str(), FILE_WRITE);
        if (!sdOut) {
            result.message = "Cannot open SD path";
            log::info("burner_sd_open_fail", sdPath);
            return result;
        }
    }

    const String flashDetail =
        (plan.version.length() ? plan.version : plan.file) + "\nApp " + String(plan.appSize) +
        " bytes";
    ui::showFlashProgress(0, "M5Burner load", flashDetail);
    m5os::update();

    const uint32_t offset = plan.noBootloader ? 0 : plan.appOffset;
    int flashHttpCode = 0;
    const bool ok =
        streamRangeToOta(plan.downloadUrl, offset, plan.appSize, sdOut ? &sdOut : nullptr,
                         &flashHttpCode, "Loading app", flashDetail.c_str());
    if (sdOut) sdOut.close();

    if (!ok) {
        if (sdPath.length()) SD.remove(sdPath.c_str());
        if (WiFi.status() != WL_CONNECTED) {
            result.message = "WiFi lost during load";
        } else if (flashHttpCode > 0) {
            result.message = formatHttpStageError("Load HTTP", flashHttpCode);
        } else {
            result.message = "Load failed — launcher intact";
        }
        return result;
    }

    result.ok = true;
    result.message = sdPath.length()
                         ? "Staged on SD. Launch from Apps menu to run."
                         : "Staged in OTA slot. Launch from Apps menu to run.";
    log::info("burner_flash_ok", plan.version);
    ui::showFlashProgress(100, "M5Burner load", result.message);
    return result;
}

BurnerDownloadResult downloadPlanToSd(const BurnerInstallPlan& plan, const String& sdPath) {
    BurnerDownloadResult result;
    if (WiFi.status() != WL_CONNECTED) {
        result.stage = "WiFi";
        result.message = "WiFi required";
        return result;
    }
    if (!plan.downloadUrl.length() || plan.appSize == 0) {
        result.stage = "Plan";
        result.message = "Invalid install plan";
        return result;
    }
    if (plan.appSize > maxOtaAppBytes()) {
        result.stage = "Plan";
        result.message = "App too large for OTA slot";
        return result;
    }
    if (!sdPath.length()) {
        result.stage = "SD";
        result.message = "Missing SD path";
        return result;
    }

    File out = SD.open(sdPath.c_str(), FILE_WRITE);
    if (!out) {
        result.stage = "SD";
        result.message = "Cannot open SD path";
        log::info("burner_sd_open_fail", sdPath);
        return result;
    }

    const String downloadDetail =
        (plan.version.length() ? plan.version : plan.file) + "\nApp " + String(plan.appSize) +
        " bytes";
    ui::showFlashProgress(0, "Loading", downloadDetail);
    m5os::update();

    const uint32_t offset = plan.noBootloader ? 0 : plan.appOffset;
    const RangeStreamResult streamed = streamRangeToFile(
        plan.downloadUrl, offset, plan.appSize, out, "Loading", downloadDetail.c_str());
    out.close();
    result.httpCode = streamed.httpCode;
    result.stage = "Download";

    if (!streamed.ok) {
        SD.remove(sdPath.c_str());
        if (WiFi.status() != WL_CONNECTED) {
            result.message = "WiFi lost during load";
        } else if (streamed.httpCode > 0) {
            result.message = formatHttpStageError("Load HTTP", streamed.httpCode);
        } else if (streamed.written > 0) {
            result.message = "Load incomplete " + String(streamed.written) + "/" +
                             String(plan.appSize);
        } else {
            result.message = "Load failed";
        }
        return result;
    }

    for (const auto& slice : plan.dataSlices) {
        if (slice.copySize == 0) continue;
        const String extraPath = sdExtraPathFromApp(sdPath, slice);
        if (!extraPath.length()) continue;

        const String sliceDetail =
            slice.subtype + (slice.label.length() ? ":" + slice.label : "") + "\n" +
            String(slice.copySize) + " bytes";

        File extraOut = SD.open(extraPath.c_str(), FILE_WRITE);
        if (!extraOut) {
            log::info("burner_extra_sd_fail", extraPath);
            continue;
        }
        const RangeStreamResult saved = streamRangeToFile(
            plan.downloadUrl, slice.sourceOffset, slice.copySize, extraOut, "Saving assets",
            sliceDetail.c_str());
        extraOut.close();
        if (!saved.ok) {
            SD.remove(extraPath.c_str());
            log::info("burner_extra_incomplete", extraPath);
            continue;
        }
        log::info("burner_extra_saved", extraPath);
    }

    result.ok = true;
    result.requiresFlashAssets = plan.requiresFlashAssets;
    result.message = plan.requiresFlashAssets
                         ? "App saved to SD.\nNeeds SPIFFS on flash —\nuse M5Burner USB full flash."
                         : "Saved to SD";
    log::info("burner_download_ok", plan.version);
    ui::showFlashProgress(100, "Loading", result.message);
    return result;
}

BurnerDownloadResult downloadFidToSd(const String& fid, const String& version,
                                       const String& sdPath) {
    BurnerDownloadResult result;
    BurnerInstallPlan plan;
    BurnerPlanError planErr;
    if (!buildInstallPlan(fid, version, plan, &planErr)) {
        result.stage = planErr.stage.length() ? planErr.stage : String("Plan");
        result.httpCode = planErr.httpCode;
        result.message =
            planErr.message.length() ? planErr.message : String("Install info failed");
        return result;
    }
    return downloadPlanToSd(plan, sdPath);
}

}  // namespace m5os::burner
