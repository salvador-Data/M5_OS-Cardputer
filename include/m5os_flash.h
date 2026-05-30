#pragma once



#include "m5os_config.h"



#include <Arduino.h>
#include <cstddef>

#include <esp_ota_ops.h>
#include <esp_partition.h>



namespace m5os {



/** Foreign-app run slot size (app1 on dual-OTA partition table). */

size_t maxOtaAppBytes();

/** Human-readable size for Load app errors (e.g. "3.75 MB"). */
String formatFlashSizeMb(size_t bytes);

/** Pre-check message when an app bin exceeds the run slot. */
String formatAppTooLargeMessage(size_t appBytes, size_t slotBytes);

/** Short esp_err label for OTA write failures (e.g. "err 0x102"). */
String formatEspOtaErr(esp_err_t err);



/** Persist label of the M5 OS launcher partition (NVS) for recovery boot. */

bool saveHomeAppPartition();



/** Point otadata at the saved M5 OS home partition. */

bool restoreBootToHome();

/** NVS home first, else running partition — after failed staging / launch. */
bool ensureOtadataBootsHome();

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



/** After Load app, M5 OS still on home via SW reset (run slot did not stay loaded). */

bool tryHandleLaunchSnapBack();



/** Serial markers: running/boot otadata partition labels + reset reason. */

void logBootPartitionContext();



/** True once if the last boot handoff failed; clears the flag. */

bool consumeLaunchHandoffFailure();



/** Last launch/handoff failure tag (e.g. no_target, set_boot); empty if none. Cleared on read. */

String consumeLaunchFailDetail();

/** Same as consumeLaunchFailDetail but does not clear (for immediate UI after failed launch). */
String peekLaunchFailDetail();

/** Map launch fail tag to user-facing message. */
String formatLaunchFailMessage(const String& tag);



/** Save home, point otadata at staged app, single SW restart into app. */

bool launchStagedAppSession();



/** Foreign-app run slot (app2, or legacy app1 when >= kMinRunSlotPartitionBytes). */
const esp_partition_t* runSlotOtaPartition();

/** @deprecated alias — use runSlotOtaPartition(). */
const esp_partition_t* stagingOtaPartition();

/** ESP image header + esp_image_verify on a partition (after SD copy). */
bool verifyPartitionAppImage(const esp_partition_t* part);

/** True when app2 already holds a launchable ESP32-S3 image matching expectedSize (bytes). */
bool runSlotReadyForLaunch(size_t expectedSize);

/** Mark a non-running OTA slot state in otadata (e.g. gateway or run slot). */
bool markPartitionOtaState(const esp_partition_t* part, esp_ota_img_states_t targetState);

/** Mark app2 run slot INVALID — does not touch app0/app1. */
bool invalidateRunSlot();

/** Incremental SD/network → inactive OTA slot writer (esp_ota_begin/write/end). */
struct OtaSlotWriter {
    esp_ota_handle_t handle = 0;
    const esp_partition_t* part = nullptr;
    size_t expected = 0;
    size_t written = 0;
    bool active = false;
    esp_err_t lastErr = ESP_OK;
};

bool otaSlotWriterBegin(OtaSlotWriter& writer, const esp_partition_t* part, size_t imageSize);
bool otaSlotWriterAppend(OtaSlotWriter& writer, const uint8_t* data, size_t len);
/** Validates image via esp_ota_end; does not switch boot partition. */
bool otaSlotWriterFinish(OtaSlotWriter& writer, String* errOut = nullptr);
void otaSlotWriterAbort(OtaSlotWriter& writer);

/** On-screen / serial debug: running, boot, staging slot labels and sizes. */
String formatOtaSlotDebug();

/** After write: chip_id in image header must match ESP32-S3 (0x09). */
bool validateAppImageChipTarget(const esp_partition_t* part, String* chipDetailOut = nullptr);

}  // namespace m5os

