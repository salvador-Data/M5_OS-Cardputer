#pragma once

#include <M5GFX.h>

namespace m5os::power {

/** Battery % threshold to auto-enable power savings (Cardputer ADC via M5.Power). */
constexpr int kLowBatteryThresholdPct = 20;
/** Hysteresis: disable savings above this % to avoid flicker at the boundary. */
constexpr int kLowBatteryRecoverPct = 25;

void begin();
void tick();

/** 0–100, or -1 if PMIC/ADC read failed. */
int batteryPercent();

bool isSaving();
bool allowBootChime();
unsigned long uiLoopDelayMs();

/** Top-right battery bar + optional "SAV" when savings active. */
void drawStatusBar(M5GFX& display);

}  // namespace m5os::power
