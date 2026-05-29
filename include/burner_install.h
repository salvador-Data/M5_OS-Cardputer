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
    /** SPIFFS/LittleFS on device flash (M5 OS has no SPIFFS partition). */
    bool requiresFlashAssets = false;
    std::vector<BurnerDataSlice> dataSlices;
};

struct BurnerFlashResult {
    bool ok = false;
    String message;
    bool savedExtraToSd = false;
};

/** Result for SD-only app slice download (Download from catalog). */
struct BurnerDownloadResult {
    bool ok = false;
    String message;
    int httpCode = 0;
    String stage;
    bool requiresFlashAssets = false;
};

/** List published versions for a LauncherHub firmware id. */
bool fetchVersionList(const String& fid, std::vector<BurnerVersionInfo>& out);

/** Resolve install plan from LauncherHub (manifest first, version list fallback). */
bool buildInstallPlan(const String& fid, const String& version, BurnerInstallPlan& plan);

/**
 * Stream app bytes from LauncherHub/M5Burner CDN.
 * SPIFFS/composite images: SD download only (no OTA reboot).
 * Simple app-only images: stage OTA inactive slot + SD copy; M5 OS stays bootable.
 */
BurnerFlashResult flashAppToOta(const BurnerInstallPlan& plan, const String& sdPath = "");

/** Download app slice (+ optional SPIFFS/FAT extras) from a resolved install plan. */
BurnerDownloadResult downloadPlanToSd(const BurnerInstallPlan& plan, const String& sdPath);

/**
 * True when composite M5Burner image needs SPIFFS/LittleFS or bootloader slice —
 * must not OTA-reboot on M5 OS (no SPIFFS partition in our table).
 */
bool planRequiresSdOnly(const BurnerInstallPlan& plan);

/** Resolve install plan and download app slice to sdPath (LauncherHub catalog download). */
BurnerDownloadResult downloadFidToSd(const String& fid, const String& version, const String& sdPath);

}  // namespace m5os::burner
