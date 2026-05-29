#pragma once

namespace m5os::stamp {

/** Init SK6812 stamp LED (GPIO 21) — no-op on non-Cardputer boards. */
void begin();

/** Breathe/pulse glow using current ui theme color. Call from m5os::update(). */
void tick();

/** Amber pulse during early recovery window (hold ESC/` at boot). */
void recoveryPulse();

/** Refresh LED immediately after theme change. */
void applyTheme();

/** Turn stamp LED off (power save / shutdown). */
void off();

}  // namespace m5os::stamp
