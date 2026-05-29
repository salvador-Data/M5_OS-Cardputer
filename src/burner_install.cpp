#include "burner_install.h"

#include "m5burner_hookup.h"
#include "m5os_config.h"
#include "m5os_security.h"
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

String httpGetPayload(const String& url) {
    if (!security::isAllowedHttpsUrl(url)) {
        log::info("burner_url_rejected");
        return "";
    }
    HTTPClient http;
    http.begin(url);
    http.setTimeout(20000);
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
    plan.requiresExtraPartitions = false;

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
                    const uint32_t copySize = part["copy_size"] | 0;
                    if (copySize > 0) plan.requiresExtraPartitions = true;
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
    if (plan.appSize > kMaxAppBinBytes) return false;
    return plan.downloadUrl.length() > 0;
}

size_t parseContentRangeTotal(const String& contentRange) {
    const int slash = contentRange.lastIndexOf('/');
    if (slash < 0 || slash + 1 >= contentRange.length()) return 0;
    return static_cast<size_t>(contentRange.substring(slash + 1).toInt());
}

bool probeRemoteSize(const String& url, size_t& totalSize) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000);
    applyBurnerDownloadHeaders(http, url);
    http.addHeader("Range", "bytes=0-0");
    const int code = http.GET();
    const String contentRange = http.header("Content-Range");
    http.end();
    if (code != HTTP_CODE_PARTIAL_CONTENT) return false;
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

bool streamRangeToOta(const String& url, uint32_t offset, size_t imageSize, File* sdOut) {
    if (!security::isAllowedHttpsUrl(url)) return false;
    if (imageSize == 0 || imageSize > kMaxAppBinBytes) return false;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(30000);
    applyBurnerDownloadHeaders(http, url);
    const uint32_t endByte = offset + static_cast<uint32_t>(imageSize) - 1;
    http.addHeader("Range", "bytes=" + String(offset) + "-" + String(endByte));
    const int code = http.GET();
    if (code != HTTP_CODE_OK && code != HTTP_CODE_PARTIAL_CONTENT) {
        http.end();
        log::info("burner_range_fail", String(code));
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    RangeFlashContext ctx;
    ctx.expected = imageSize;
    ctx.sdOut = sdOut;

    uint8_t buffer[512];
    while (ctx.written < imageSize && (stream->connected() || stream->available())) {
        const size_t avail = stream->available();
        if (!avail) {
            delay(1);
            continue;
        }
        const size_t want = min(avail, min(sizeof(buffer), imageSize - ctx.written));
        const int read = stream->readBytes(buffer, want);
        if (read <= 0) break;
        if (!writeFlashChunk(ctx, buffer, static_cast<size_t>(read))) {
            Update.abort();
            http.end();
            return false;
        }
        if (ctx.written % 8192 == 0 || ctx.written == imageSize) {
            ui::drawHeader("Flashing");
            m5os::lcd().setCursor(4, 44);
            m5os::lcd().printf("%u / %u", static_cast<unsigned>(ctx.written),
                               static_cast<unsigned>(imageSize));
        }
    }
    http.end();

    if (ctx.written != imageSize) {
        if (ctx.updateStarted) Update.abort();
        log::info("burner_incomplete", String(ctx.written));
        return false;
    }
    if (!Update.end(true)) {
        log::info("burner_end_fail", String(Update.errorString()));
        return false;
    }
    return true;
}

}  // namespace

bool fetchVersionList(const String& fid, std::vector<BurnerVersionInfo>& out) {
    out.clear();
    const String safeFid = security::normalizeBurnerFid(fid);
    if (!safeFid.length() || WiFi.status() != WL_CONNECTED) return false;

    const String payload = httpGetPayload(String(kLauncherHubCatalogBase) + "?fid=" + safeFid);
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

bool buildInstallPlan(const String& fid, const String& version, BurnerInstallPlan& plan) {
    plan = BurnerInstallPlan{};
    plan.fid = security::normalizeBurnerFid(fid);
    if (!plan.fid.length() || WiFi.status() != WL_CONNECTED) return false;

    bool parsed = false;
    if (version.length()) {
        const String encodedVersion = urlEncodeQueryComponent(version);
        const String detailUrl =
            String(kLauncherHubCatalogBase) + "?fid=" + plan.fid + "&version=" + encodedVersion;
        const String detailPayload = httpGetPayload(detailUrl);

        if (detailPayload.length()) {
            JsonDocument detail;
            if (!deserializeJson(detail, detailPayload)) {
                JsonObject root = detail.as<JsonObject>();
                parsed = parseInstallManifest(root, version, plan);
            }
        }
    }

    if (!parsed) {
        std::vector<BurnerVersionInfo> versions;
        if (!fetchVersionList(plan.fid, versions)) return false;
        for (const auto& info : versions) {
            if (version.length() && info.version != version) continue;
            plan.version = info.version;
            plan.file = info.file;
            plan.appOffset = info.appOffset;
            plan.appSize = info.appSize;
            plan.noBootloader = info.noBootloader;
            parsed = true;
            break;
        }
        if (!parsed && !versions.empty()) {
            const auto& latest = versions[0];
            plan.version = latest.version;
            plan.file = latest.file;
            plan.appOffset = latest.appOffset;
            plan.appSize = latest.appSize;
            plan.noBootloader = latest.noBootloader;
            parsed = true;
        }
    }

    if (!parsed || !fillPlanDownloadUrl(plan)) return false;

    if (plan.appSize == 0 && plan.noBootloader) {
        size_t remoteSize = 0;
        if (!probeRemoteSize(plan.downloadUrl, remoteSize)) return false;
        if (remoteSize > kMaxAppBinBytes) return false;
        plan.appSize = static_cast<uint32_t>(remoteSize);
    } else if (plan.appSize == 0 && plan.appOffset > 0) {
        size_t remoteSize = 0;
        if (!probeRemoteSize(plan.downloadUrl, remoteSize)) return false;
        if (remoteSize <= plan.appOffset) return false;
        const size_t slice = remoteSize - plan.appOffset;
        if (slice > kMaxAppBinBytes) return false;
        plan.appSize = static_cast<uint32_t>(slice);
    }

    return finalizePlanSize(plan);
}

BurnerFlashResult flashAppToOta(const BurnerInstallPlan& plan, const String& sdPath) {
    BurnerFlashResult result;
    if (plan.requiresExtraPartitions) {
        result.message = "SPIFFS/FAT install unsupported";
        log::info("burner_extra_parts");
        return result;
    }
    if (WiFi.status() != WL_CONNECTED) {
        result.message = "WiFi required";
        return result;
    }
    if (!plan.downloadUrl.length() || plan.appSize == 0 || plan.appSize > kMaxAppBinBytes) {
        result.message = "Invalid install plan";
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

    ui::drawHeader("M5Burner flash");
    m5os::lcd().setCursor(4, 28);
    m5os::lcd().println(plan.version.length() ? plan.version : plan.file);
    m5os::lcd().setCursor(4, 44);
    m5os::lcd().printf("%u bytes", static_cast<unsigned>(plan.appSize));

    const uint32_t offset = plan.noBootloader ? 0 : plan.appOffset;
    const bool ok = streamRangeToOta(plan.downloadUrl, offset, plan.appSize, sdOut ? &sdOut : nullptr);
    if (sdOut) sdOut.close();

    if (!ok) {
        if (sdPath.length()) SD.remove(sdPath.c_str());
        result.message = "Flash failed — launcher intact";
        return result;
    }

    result.ok = true;
    result.message = "Rebooting into app";
    log::info("burner_flash_ok", plan.version);
    ui::showMessage("M5Burner", result.message, TFT_GREEN, 1200);
    delay(300);
    esp_restart();
    return result;
}

}  // namespace m5os::burner
