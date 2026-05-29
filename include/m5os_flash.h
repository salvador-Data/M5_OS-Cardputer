#pragma once

#include "m5os_config.h"

#include <Arduino.h>
#include <cstddef>
#include <esp_partition.h>



namespace m5os {



/** Inactive OTA partition size at runtime (may exceed kMaxAppBinBytes on app1). */

size_t maxOtaAppBytes();



/** Persist label of the M5 OS launcher partition (NVS) for recovery boot. */

bool saveHomeAppPartition();



/** Point otadata at the saved M5 OS home partition. */

bool restoreBootToHome();



/** True when BtnA / G0 held at call time (recovery gesture). */

bool recoveryBootRequested();



/**

 * If recovery key held, restore home boot partition and restart into M5 OS.

 * Call immediately after m5os::begin().

 */

void tryEarlyRecoveryBoot();

/** On ESP_RST_POWERON only, point otadata at saved M5 OS home partition. */
void applyColdBootHomeRestore();

/** After panic/watchdog reset, point otadata at saved M5 OS home partition. */
void applyCrashResetHomeRestore();

/** NVS flag: skip shutdown home-restore while rebooting into staged app. */
void setLaunchPending(bool pending);
bool isLaunchPending();
void clearLaunchPending();

/** Mark an intentional load-app restart (RAM + NVS) before OTA copy or reboot. */
void beginLaunchSession();
void cancelLaunchSession();
bool launchSessionActive();

/** Inactive OTA slot with a valid ESP image header (0xE9). */
const esp_partition_t* resolveLaunchBootPartition();

/** Phase-2: after SW reset with launch pending, point otadata at staged app and restart. */
bool tryLaunchPendingHandoff();

/** True once if the last boot handoff failed; clears the flag. */
bool consumeLaunchHandoffFailure();

/** Last launch/handoff failure tag (e.g. no_target, set_boot); empty if none. Cleared on read. */
String consumeLaunchFailDetail();

/** Save home, point otadata at staged app, single SW restart into app. */
bool launchStagedAppSession();

const esp_partition_t* stagingOtaPartition();

}  // namespace m5os

