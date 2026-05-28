#pragma once

#include <Arduino.h>
#include <vector>

namespace m5os::gc {

struct GcReport {
    unsigned tmpRemoved = 0;
    unsigned cacheRemoved = 0;
    unsigned logsRotated = 0;
    unsigned bytesReclaimed = 0;
};

/** Quick boot scan — sweeps /tmp TTL, rotates oversized logs. Safe without user confirm. */
GcReport quickBootScan();

/**
 * Full cleanup — includes cache orphan reclaim.
 * @param userConfirmed required true before deleting orphaned cache entries.
 */
GcReport fullCleanup(bool userConfirmed, const std::vector<String>& whitelistedAppSlugs);

}  // namespace m5os::gc
