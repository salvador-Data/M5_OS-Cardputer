#pragma once



#include <cstddef>

#include <cstdint>



#include <esp_partition.h>



namespace m5os {



/** Permanent 64K session gateway (ota_1 / app1). */

const esp_partition_t* gatewayOtaPartition();



/** Foreign app run slot (ota_2 / app2, or ota_1 on legacy 2-slot tables). */

const esp_partition_t* runSlotOtaPartition();



/** @deprecated Use runSlotOtaPartition — kept for tests and call sites. */

const esp_partition_t* stagingOtaPartition();



/** True when gateway partition holds a valid ESP image (0xE9). */

bool gatewayPartitionReady();



/** Write gateway firmware bytes into the gateway partition and mark VALID. */

bool flashGatewayImage(const uint8_t* data, size_t len);



/** Ensure app1 gateway: SD override, else embedded build (see prebuild_gateway_embed.py). */

bool flashEmbeddedGatewayIfNeeded();



/** Reboot into gateway (app1) after run slot is staged. */

bool launchGatewaySession();



/** Gateway ESC path: request save prompt and restore home otadata. */

bool gatewayExitToHome();



/** Gateway Enter path: boot run slot (app2) via otadata + restart. */

bool gatewayLaunchRunSlot();



}  // namespace m5os

