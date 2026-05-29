#pragma once

#include "m5os_config.h"

#include <cstddef>

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

}  // namespace m5os
