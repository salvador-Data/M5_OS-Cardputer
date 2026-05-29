#pragma once

#include <Arduino.h>
#include <vector>

namespace m5os::burner {

/** Boris LauncherHub version metadata (ao/as/nb fields or install manifest). */
struct BurnerVersionInfo {
    String version;
    String file;
    uint32_t appOffset = 0;
    uint32_t appSize = 0;
    bool noBootloader = false;
};

/** Resolved download + app slice for OTA flash (app partition only — no SPIFFS/FAT). */
struct BurnerInstallPlan {
    String fid;
    String version;
    String file;
    String downloadUrl;
    uint32_t appOffset = 0;
    uint32_t appSize = 0;
    bool noBootloader = false;
    bool requiresExtraPartitions = false;
};

struct BurnerFlashResult {
    bool ok = false;
    String message;
};

/** List published versions for a LauncherHub firmware id. */
bool fetchVersionList(const String& fid, std::vector<BurnerVersionInfo>& out);

/** Resolve install plan from LauncherHub (manifest first, version list fallback). */
bool buildInstallPlan(const String& fid, const String& version, BurnerInstallPlan& plan);

/**
 * Stream app bytes from LauncherHub/M5Burner CDN into ESP32 OTA slot.
 * When sdPath is set, also writes the same app slice to SD (multi-app model).
 */
BurnerFlashResult flashAppToOta(const BurnerInstallPlan& plan, const String& sdPath = "");

}  // namespace m5os::burner
