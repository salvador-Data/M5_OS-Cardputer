#pragma once



#include "firmware_catalog.h"

#include <HTTPClient.h>
#include <vector>



namespace m5os::burner {



/** Boris Morcelli LauncherHub + M5Burner CDN hookup (discovery/download only). */

static const char* kLauncherHubCatalogBase = "https://api.launcherhub.net/firmwares";

static const char* kLauncherHubDownloadBase = "https://api.launcherhub.net/download";

static const char* kM5BurnerCdnBase = "https://m5burner-cdn.m5stack.com/firmware/";

static const char* kBurnerCategory = "cardputer";



/** Prefix relative M5Burner file paths with the official CDN base. */

String resolveFileUrl(const String& file);



/** Percent-encode a LauncherHub query value (version, file URL, etc.). */
String urlEncodeQueryComponent(const String& raw);

/** WiFi STA MAC for LauncherHub download proxy (Boris HWID header). */
String burnerWifiHwid();

/** True when url targets api.launcherhub.net/download (HWID required). */
bool isLauncherHubDownloadUrl(const String& url);

/** Attach HWID header for LauncherHub download requests. */
void applyBurnerDownloadHeaders(HTTPClient& http, const String& url);

/** Build LauncherHub download proxy URL (Boris installFirmware/downloadFirmware pattern). */
String resolveDownloadUrl(const String& fid, const String& file);

/** Fetch one catalog page from LauncherHub (category=cardputer). */

bool fetchCatalogPage(uint8_t page, std::vector<FirmwarePackage>& out);



/** Fill file, version, size on a package from LauncherHub version metadata. */

bool enrichPackageFromBurner(FirmwarePackage& pkg);



}  // namespace m5os::burner


