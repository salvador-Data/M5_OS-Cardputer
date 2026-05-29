#pragma once

#include "m5os_config.h"

#include <cstddef>

namespace m5os {

/** Inactive OTA partition size at runtime (may exceed kMaxAppBinBytes on app1). */
size_t maxOtaAppBytes();

}  // namespace m5os
