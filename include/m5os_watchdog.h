#pragma once

#include "m5os_config.h"

namespace m5os {

/** Init ESP task watchdog + panic reboot shutdown hook (M5 OS menu only). */
void beginWatchdog();

/** Reset TWDT; safe no-op before beginWatchdog(). */
void feedWatchdog();

}  // namespace m5os
