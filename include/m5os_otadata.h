#pragma once

#include <esp_ota_ops.h>
#include <esp_partition.h>

namespace m5os::otadata {

/** Write otadata VALID (or other state) for a non-running OTA slot before set_boot. */
bool markPartitionOtaState(const esp_partition_t* staged, esp_ota_img_states_t targetState);

}  // namespace m5os::otadata
