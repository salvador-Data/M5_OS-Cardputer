#pragma once

#include <Arduino.h>

namespace m5os::utms {

struct ThreatPackInfo {
    String version;
    String sha256Expected;
    size_t hashCount = 0;
    size_t allowHashCount = 0;
    size_t stringCount = 0;
    bool loaded = false;
};

struct UpdateResult {
    bool ok = false;
    String message;
    String version;
};

/** Load pack metadata from SD (no full signature list in RAM). */
ThreatPackInfo loadPackInfo();

/** Fetch pack from HTTPS URL, verify sha256 if present, atomic write to SD. */
UpdateResult fetchAndInstallPack(const String& url);

/** Last installed version from NVS. */
String lastUpdateVersion();

/** Unix epoch of last check (0 if never). */
uint32_t lastCheckEpoch();

/** Persist check timestamp (call after manual or auto check). */
void markCheckEpoch();

/** Rate-limit guard — returns false if called too soon. */
bool canAttemptOtaFetch();

/** Ensure /home/default/utms/ tree exists. */
bool ensureUtmsDirs();

/** Append one line to UTMS log on SD. */
void appendLog(const char* event, const String& detail = "");

}  // namespace m5os::utms
