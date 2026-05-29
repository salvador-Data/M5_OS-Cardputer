#pragma once

#include "firmware_catalog.h"

#include <Arduino.h>

namespace m5os::session {

static const char* kSessionJsonPath = "/home/default/m5os_session.json";

struct SessionRecord {
    String slug;
    String binPath;
    uint64_t launchedAt = 0;
    String spiffsSdPath;
};

/** Ensure SD app dirs + writes m5os_session.json before Load app reboot. */
bool prepareLaunchSd(const String& sdBinPath, const String& cacheKey, const FirmwarePackage* meta);

/** True when returning from a loaded app session (rollback / reset / recovery). */
bool isSessionReturnBoot();

/**
 * Prompt "Save files before exit?" (lowercase y/n).
 * On y: sync SD save dirs + update session JSON; always ends session and restores home otadata.
 * Returns true if user chose to save.
 */
bool promptSaveSessionAndRestoreHome();

/** End session NVS flags without UI (cold power-on). */
void abandonSessionQuiet();

}  // namespace m5os::session
