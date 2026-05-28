#pragma once

#include <Arduino.h>

namespace m5os::vfs {

/** SD "hard drive" layout — FAT32 path conventions on microSD. */
static const char* kSystemDir = "/system";
static const char* kSystemBinDir = "/system/bin";
static const char* kAppsDir = "/apps";
static const char* kAppsManifestPath = "/apps/manifest.json";
static const char* kHomeDir = "/home";
static const char* kDefaultUser = "default";
static const char* kHomeDefaultDir = "/home/default";
static const char* kHomeAppsDir = "/home/default/apps";
static const char* kHomeCacheDir = "/home/default/cache";
static const char* kTmpDir = "/tmp";
static const char* kVarLogDir = "/var/log";
static const char* kSystemMarkerPath = "/system/M5OS_CARDPUTER.txt";

/** Legacy paths (still scanned for backward compatibility). */
static const char* kLegacyFirmwareDir = "/firmware";
static const char* kLegacyManifestPath = "/manifest.json";
static const char* kLegacyMarkerPath = "/M5OS_CARDPUTER.txt";

struct MountResult {
    bool ok = false;
    String message;
};

/** Mount SD and create the OS directory tree. */
MountResult mountAndInit();

/** Resolve app compartment dir: /apps/<slug>/ */
String appDirFor(const String& appSlug);

/** Per-app data dir: /home/default/apps/<slug>/ */
String appDataDirFor(const String& appSlug);

/** Resolve firmware .bin path — prefers /apps/<slug>/<bin>, falls back to /firmware/<bin>. */
String binPathFor(const String& appSlug, const String& binFile);

/** Slugify manifest name → safe directory segment. */
String slugFromName(const String& name);

/** Ensure app compartment + data dirs exist. */
bool ensureAppDirs(const String& appSlug);

}  // namespace m5os::vfs
