#include "power_manager.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace m5os::power {

namespace {

constexpr unsigned long kBatteryPollMs = 5000;
constexpr uint8_t kNormalBrightness = 128;
constexpr uint8_t kSavingBrightness = 52;

int gLastPercent = -1;
bool gSaving = false;
bool gWifiSleepApplied = false;
unsigned long gLastPollMs = 0;
uint8_t gSavedBrightness = kNormalBrightness;

int readBatteryPercent() {
    const int level = M5.Power.getBatteryLevel();
    if (level < 0) return -1;
    if (level > 100) return 100;
    return level;
}

void applySavingState(bool enable) {
    if (enable == gSaving) return;
    gSaving = enable;

    auto& d = M5Cardputer.Display;
    if (enable) {
        gSavedBrightness = d.getBrightness();
        if (gSavedBrightness == 0 || gSavedBrightness > 255) {
            gSavedBrightness = kNormalBrightness;
        }
        d.setBrightness(kSavingBrightness);
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.setSleep(WIFI_PS_MIN_MODEM);
            gWifiSleepApplied = true;
        }
    } else {
        const uint8_t restore =
            (gSavedBrightness >= 40 && gSavedBrightness <= 255) ? gSavedBrightness : kNormalBrightness;
        d.setBrightness(restore);
        if (gWifiSleepApplied) {
            WiFi.setSleep(false);
            gWifiSleepApplied = false;
        }
    }
}

void evaluateSaving(int percent) {
    if (percent < 0) return;
    if (!gSaving && percent <= kLowBatteryThresholdPct) {
        applySavingState(true);
    } else if (gSaving && percent >= kLowBatteryRecoverPct) {
        applySavingState(false);
    }
}

uint16_t barFillColor(int percent, bool charging) {
    if (charging) return TFT_GREEN;
    if (percent < 0) return TFT_DARKGREY;
    if (percent <= kLowBatteryThresholdPct) return TFT_RED;
    if (percent <= 40) return TFT_YELLOW;
    return TFT_WHITE;
}

}  // namespace

void begin() {
    gLastPercent = readBatteryPercent();
    gLastPollMs = millis();
    gSaving = false;
    gWifiSleepApplied = false;

    auto& d = M5Cardputer.Display;
    const uint8_t cur = d.getBrightness();
    gSavedBrightness = (cur >= 40 && cur <= 255) ? cur : kNormalBrightness;
    if (gSavedBrightness == 0) d.setBrightness(kNormalBrightness);

    evaluateSaving(gLastPercent);
}

void tick() {
    const unsigned long now = millis();
    if (now - gLastPollMs < kBatteryPollMs) return;
    gLastPollMs = now;
    gLastPercent = readBatteryPercent();
    evaluateSaving(gLastPercent);
}

int batteryPercent() {
    if (gLastPercent < 0) gLastPercent = readBatteryPercent();
    return gLastPercent;
}

bool isSaving() { return gSaving; }

bool allowBootChime() { return !gSaving; }

unsigned long uiLoopDelayMs() { return gSaving ? 160 : 80; }

WifiThroughputGuard::WifiThroughputGuard() {
    if (WiFi.status() != WL_CONNECTED) return;
    wifi_ps_type_t ps = WIFI_PS_NONE;
    if (esp_wifi_get_ps(&ps) != ESP_OK) return;
    prevPs_ = static_cast<int>(ps);
    if (ps == WIFI_PS_NONE) return;
    if (esp_wifi_set_ps(WIFI_PS_NONE) == ESP_OK) active_ = true;
}

WifiThroughputGuard::~WifiThroughputGuard() {
    if (!active_) return;
    esp_wifi_set_ps(static_cast<wifi_ps_type_t>(prevPs_));
}

void drawStatusBar(M5GFX& display) {
    tick();

    const int pct = batteryPercent();
    const bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging);

    const int barW = 26;
    const int barH = 7;
    const int barX = display.width() - barW - 4;
    const int barY = 5;

    display.fillRect(barX - 1, barY - 1, barW + 2, barH + 2, TFT_BLACK);
    display.drawRect(barX, barY, barW, barH, TFT_DARKGREY);

    if (pct >= 0) {
        const int fillW = max(1, (barW - 2) * pct / 100);
        display.fillRect(barX + 1, barY + 1, fillW, barH - 2, barFillColor(pct, charging));
    }

    display.setTextSize(1);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setCursor(barX - 34, barY);
    if (pct >= 0) {
        display.printf("%3d%%", pct);
    } else {
        display.print(" --%");
    }

    if (isSaving()) {
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.setTextSize(1);
        display.setCursor(barX - 58, barY);
        display.print("SAV");
    } else if (charging) {
        display.setTextColor(TFT_GREEN, TFT_BLACK);
        display.setTextSize(1);
        display.setCursor(barX - 10, barY);
        display.print("+");
    }
}

}  // namespace m5os::power
