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

/** SPIFFS/FAT slice from composite image — saved to SD, not flashed on M5 OS. */
struct BurnerDataSlice {
    String subtype;
    String label;
    uint32_t sourceOffset = 0;
    uint32_t copySize = 0;
};

/** Resolved download + app slice for OTA flash (app partition only). */
struct BurnerInstallPlan {
    String fid;
    String version;
    String file;
    String downloadUrl;
    uint32_t appOffset = 0;
    uint32_t appSize = 0;
    bool noBootloader = false;
    bool fromManifest = false;
    std::vector<BurnerDataSlice> dataSlices;
};

struct BurnerFlashResult {
    bool ok = false;
    String message;
    bool savedExtraToSd = false;
};

/** List published versions for a LauncherHub firmware id. */
bool fetchVersionList(const String& fid, std::vector<BurnerVersionInfo>& out);

/** Resolve install plan from LauncherHub (manifest first, version list fallback). */
bool buildInstallPlan(const String& fid, const String& version, BurnerInstallPlan& plan);

/**
 * Stream app bytes from LauncherHub/M5Burner CDN into ESP32 OTA slot.
 * When sdPath is set, also writes the same app slice to SD (multi-app model).
 * SPIFFS/FAT slices (copy_size > 0) are saved alongside the app on SD when sdPath is set.
 */
BurnerFlashResult flashAppToOta(const BurnerInstallPlan& plan, const String& sdPath = "");

}  // namespace m5os::burner
