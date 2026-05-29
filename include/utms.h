#pragma once

namespace m5os::utms {

/** Top-level UTMS submenu (called from launcher_menu). */
void showUtmsMenu();

/** Optional boot hook — checks pack when auto_check enabled in settings. */
void maybeAutoCheckOnBoot();

}  // namespace m5os::utms
