#pragma once



#include "m5os_config.h"



#include <Arduino.h>

#include <cstddef>

#include <esp_ota_ops.h>
#include <esp_partition.h>



namespace m5os {



/** Foreign-app run slot size (app2, or app1 on legacy 2-slot tables). Not the gateway slot. */

size_t maxOtaAppBytes();

/** Human-readable size for Load app errors (e.g. "3.75 MB"). */
String formatFlashSizeMb(size_t bytes);

/** Pre-check message when an app bin exceeds the run slot. */
String formatAppTooLargeMessage(size_t appBytes, size_t slotBytes);



/** Persist label of the M5 OS launcher partition (NVS) for recovery boot. */

bool saveHomeAppPartition();



/** Point otadata at the saved M5 OS home partition. */

bool restoreBootToHome();

/** True when the running OTA partition matches saved home_label. */
bool isRunningHomePartition();



/** True when BtnA / G0 / ESC / ` held at call time (recovery gesture). */

bool recoveryBootRequested();



/**

 * If recovery key held, show splash then flag session exit for save prompt.

 * Call immediately after m5os::begin().

 */

void tryEarlyRecoveryBoot();



/** On cold power-on or external reset, point otadata at saved M5 OS home partition. */

void applyColdBootHomeRestore();



/** After panic/watchdog reset, point otadata at saved M5 OS home partition. */

void applyCrashResetHomeRestore();



/** NVS flag: legacy handoff (kept for tests / shutdown hook). */

void setLaunchPending(bool pending);

bool isLaunchPending();

void clearLaunchPending();



/** NVS app_session_active — set when Load app reboots into app1. */

bool isAppSessionActive();

void setAppSessionActive(bool active);

void clearAppSession();



/** NVS sess_exit — user requested exit; prompt save on next M5 OS boot. */

bool isSessionExitPending();

void setSessionExitPending(bool pending);

void clearSessionExitPending();



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

/** Mark a non-running OTA slot state in otadata (e.g. gateway or run slot). */
bool markPartitionOtaState(const esp_partition_t* part, esp_ota_img_states_t targetState);

}  // namespace m5os

