#include "m5os_session.h"

#include "m5os_boot_policy.h"
#include "m5os_flash.h"
#include "m5os_vfs.h"
#include "serial_log.h"
#include "ui_display.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_ota_ops.h>

namespace m5os::session {

namespace {

bool readSessionRecord(SessionRecord& out) {
    out = {};
    if (!SD.exists(kSessionJsonPath)) return false;
    File f = SD.open(kSessionJsonPath);
    if (!f) return false;
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;
    out.slug = doc["slug"].as<String>();
    out.binPath = doc["bin_path"].as<String>();
    out.launchedAt = doc["launched_at"] | 0ULL;
    out.spiffsSdPath = doc["spiffs_sd_path"].as<String>();
    return out.slug.length() > 0;
}

bool writeSessionJson(const SessionRecord& rec, bool ended, bool saved) {
    JsonDocument doc;
    doc["slug"] = rec.slug;
    doc["bin_path"] = rec.binPath;
    doc["launched_at"] = rec.launchedAt;
    if (rec.spiffsSdPath.length()) doc["spiffs_sd_path"] = rec.spiffsSdPath;
    if (ended) {
        doc["ended_at"] = static_cast<uint64_t>(millis());
        doc["saved_on_exit"] = saved;
    }
    File f = SD.open(kSessionJsonPath, FILE_WRITE);
    if (!f) return false;
    const bool ok = serializeJson(doc, f) > 0;
    f.close();
    return ok;
}

bool ensureSavesTree(const String& slug) { return vfs::ensureAppSavesDir(slug); }

void syncSdPaths(const SessionRecord& rec) {
    if (rec.slug.length()) {
        ensureSavesTree(rec.slug);
        const String appDir = vfs::appDirFor(rec.slug);
        if (appDir.length()) vfs::ensureDirectoryChain(appDir.c_str(), nullptr);
    }
    SD.end();
    vfs::mountAndInit();
}

}  // namespace

bool prepareLaunchSd(const String& sdBinPath, const String& cacheKey, const FirmwarePackage* meta) {
    if (!vfs::isMounted()) {
        const vfs::MountResult mount = vfs::mountAndInit();
        if (!mount.ok) {
            log::info("session_sd_skip", "no_sd");
            return false;
        }
    }

    String slug;
    if (meta && meta->slug.length()) {
        slug = meta->slug;
    } else {
        slug = vfs::slugFromName(cacheKey);
    }
    if (!slug.length()) return false;
    if (!ensureSavesTree(slug)) return false;

    SessionRecord rec;
    rec.slug = slug;
    rec.binPath = sdBinPath;
    rec.launchedAt = static_cast<uint64_t>(millis());

    if (meta && meta->needsFlashSpiffs) {
        const String spiffsCandidate = vfs::appDirFor(slug) + "/" + slug + "-spiffs.bin";
        if (SD.exists(spiffsCandidate.c_str())) rec.spiffsSdPath = spiffsCandidate;
    }

    if (!writeSessionJson(rec, false, false)) {
        log::info("session_json_fail", slug);
        return false;
    }
    log::info("session_prepared", slug);
    return true;
}

bool isSessionReturnBoot() {
    return boot_policy::shouldPromptSessionReturn(isAppSessionActive(), isSessionExitPending(),
                                                  isRunningHomePartition(), esp_reset_reason(),
                                                  isLaunchPending());
}

bool promptSaveSessionAndRestoreHome() {
    SessionRecord rec;
    const bool haveRecord = vfs::isMounted() && readSessionRecord(rec);

    const bool save = ui::promptYesNo("Exit app", "Save files before exit?");

    if (save && haveRecord) {
        syncSdPaths(rec);
        writeSessionJson(rec, true, true);
        log::info("session_saved", rec.slug);
    } else if (haveRecord) {
        writeSessionJson(rec, true, false);
        log::info("session_exit_no_save", rec.slug);
    }

    clearAppSession();
    clearSessionExitPending();
    cancelLaunchSession();
    restoreBootToHome();
    return save;
}

void abandonSessionQuiet() {
    clearAppSession();
    clearSessionExitPending();
    cancelLaunchSession();
}

}  // namespace m5os::session
