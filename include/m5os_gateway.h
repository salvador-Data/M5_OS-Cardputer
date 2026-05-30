#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include <esp_partition.h>

namespace m5os {

/** Permanent session gateway (ota_1 / app1). */
const esp_partition_t* gatewayOtaPartition();

/** True when app1 holds the current embedded session gateway image. */
bool gatewayPartitionReady();

/** Optional UI callback: percent 0–100 and short phase label. */
using GatewayFlashProgressFn = void (*)(int percent, const char* phase);

/** Write gateway firmware bytes into the gateway partition and mark VALID. */
bool flashGatewayImage(const uint8_t* data, size_t len, GatewayFlashProgressFn progress = nullptr);

/** Ensure app1 gateway: SD override, else embedded build. Fast return when magic OK. */
bool flashEmbeddedGatewayIfNeeded(GatewayFlashProgressFn progress = nullptr);

/** Install gateway when missing; no-op when partition already valid. */
bool ensureGatewayInstalled(GatewayFlashProgressFn progress = nullptr);

/** Non-blocking: flash embedded gateway once after boot if app1 empty. Call from loop(). */
void tryDeferredGatewayInstall();

/** Schedule background gateway install after boot (no setup() blocking). */
void scheduleDeferredGatewayInstall();

/** Reboot into gateway (app1) after run slot is staged. */
bool launchGatewaySession();

/** Gateway ESC path: request save prompt and restore home otadata. */
bool gatewayExitToHome();

/** Gateway Enter path: boot run slot (app2) via otadata + restart. */
bool gatewayLaunchRunSlot();

/** Last gateway flash failure detail (SD read, app1 write, verify, otadata). */
String lastGatewayFlashDetail();

}  // namespace m5os
