#pragma once

#include "firmware_catalog.h"

#include <vector>

namespace m5os::burner {

/** Boris Morcelli LauncherHub + M5Burner CDN hookup (discovery/download only). */
static const char* kLauncherHubCatalogBase = "https://api.launcherhub.net/firmwares";
static const char* kLauncherHubDownloadBase = "https://api.launcherhub.net/download";
static const char* kM5BurnerCdnBase = "https://m5burner-cdn.m5stack.com/firmware/";
static const char* kBurnerCategory = "cardputer";

/** Prefix relative M5Burner file paths with the official CDN base. */
String resolveFileUrl(const String& file);

/** Build LauncherHub download proxy URL (Boris installFirmware/downloadFirmware pattern). */
String resolveDownloadUrl(const String& fid, const String& file);

/** Fetch one catalog page from LauncherHub (category=cardputer). */
bool fetchCatalogPage(uint8_t page, std::vector<FirmwarePackage>& out);

/** Fill file, version, size on a package from LauncherHub version metadata. */
bool enrichPackageFromBurner(FirmwarePackage& pkg);

}  // namespace m5os::burner
