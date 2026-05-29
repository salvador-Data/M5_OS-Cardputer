#pragma once

#include <stddef.h>

namespace m5os::boot {

/** Number of boot intro jokes in the catalog. */
size_t bootJokeCount();

/** Pick a random joke for this boot (esp_random). Same call returns same pointer until reboot. */
const char* randomJokeForBoot();

/** Split joke into up to two display lines (maxChars per line, typically 38 on Cardputer). */
void wrapBootJoke(const char* joke, char* line1, char* line2, size_t maxChars);

}  // namespace m5os::boot
