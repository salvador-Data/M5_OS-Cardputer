#include "m5os_vfs.h"

#include "m5os_config.h"
#include "m5os_security.h"
#include "serial_log.h"

#include <SD.h>
#include <SPI.h>

namespace m5os::vfs {

namespace {

bool mkdirIfMissing(const char* path) {
    if (SD.exists(path)) return true;
    return SD.mkdir(path);
}

bool mkdirChain(const char* path) {
    if (!path || !path[0]) return false;
    String current = "";
    for (unsigned i = 0; path[i]; ++i) {
        current += path[i];
        if (path[i] == '/' && current.length() > 1) {
            if (!mkdirIfMissing(current.c_str())) return false;
        }
    }
    return mkdirIfMissing(path);
}

void writeMarker(const char* path) {
    if (SD.exists(path)) return;
    File marker = SD.open(path, FILE_WRITE);
    if (marker) {
        marker.println("M5 OS Cardputer — Hacker Planet LLC");
        marker.println("Authorized lab use only.");
        marker.close();
    }
}

}  // namespace

String slugFromName(const String& name) {
    String slug = name;
    slug.trim();
    slug.toLowerCase();
    slug.replace(' ', '_');
    slug.replace('-', '_');
    for (unsigned i = 0; i < slug.length(); ++i) {
        const char c = slug.charAt(i);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            slug.setCharAt(i, '_');
        }
    }
    while (slug.indexOf("__") >= 0) slug.replace("__", "_");
    slug.trim();
    if (slug.startsWith("_")) slug = slug.substring(1);
    if (slug.endsWith("_")) slug.remove(slug.length() - 1);
    return security::sanitizePathSegment(slug);
}

String appDirFor(const String& appSlug) {
    const String safe = security::sanitizePathSegment(appSlug);
    if (!safe.length()) return "";
    return String(kAppsDir) + "/" + safe;
}

String appDataDirFor(const String& appSlug) {
    const String safe = security::sanitizePathSegment(appSlug);
    if (!safe.length()) return "";
    return String(kHomeAppsDir) + "/" + safe;
}

String binPathFor(const String& appSlug, const String& binFile) {
    const String safeBin = security::sanitizeBinFilename(binFile);
    if (!safeBin.length()) return "";
    const String slug = slugFromName(appSlug.length() ? appSlug : safeBin.substring(0, safeBin.length() - 4));
    const String appPath = appDirFor(slug);
    if (appPath.length()) {
        const String compartment = appPath + "/" + safeBin;
        if (SD.exists(compartment.c_str())) return compartment;
    }
    return String(kLegacyFirmwareDir) + "/" + safeBin;
}

bool ensureAppDirs(const String& appSlug) {
    const String slug = slugFromName(appSlug);
    if (!slug.length()) return false;
    const String appDir = appDirFor(slug);
    const String dataDir = appDataDirFor(slug);
    return mkdirChain(appDir.c_str()) && mkdirChain(dataDir.c_str());
}

MountResult mountAndInit() {
    MountResult result;
    SPI.begin(kSdMosiPin, kSdMisoPin, kSdSclkPin, kSdCsPin);
    if (!SD.begin(kSdCsPin, SPI, 25000000)) {
        result.message = "SD card missing";
        log::info("sd_missing");
        return result;
    }

    const char* tree[] = {kSystemDir,       kSystemBinDir,    kAppsDir,
                          kHomeDir,         kHomeDefaultDir,  kHomeAppsDir,
                          kHomeCacheDir,    kTmpDir,          kVarLogDir,
                          kLegacyFirmwareDir};
    for (const char* dir : tree) {
        if (!mkdirChain(dir)) {
            result.message = String("Cannot create ") + dir;
            log::info("vfs_mkdir_fail", dir);
            return result;
        }
    }

    writeMarker(kSystemMarkerPath);
    writeMarker(kLegacyMarkerPath);

    File gcMarker = SD.open("/system/bin/gc", FILE_WRITE);
    if (gcMarker) {
        gcMarker.println("M5 OS GcService — boot + menu cleanup");
        gcMarker.close();
    }

    result.ok = true;
    result.message = "SD mounted";
    log::info("vfs_ready");
    return result;
}

}  // namespace m5os::vfs
