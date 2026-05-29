#include "m5os_flash.h"

#include <esp_ota_ops.h>

namespace m5os {

size_t maxOtaAppBytes() {
    const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
    if (part != nullptr && part->size > 0) return part->size;
    return kMaxAppBinBytes;
}

}  // namespace m5os
