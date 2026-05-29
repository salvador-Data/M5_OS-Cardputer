#pragma once

#include <Arduino.h>

namespace m5os::settings {

/** Mount SD if needed; false shows caller should prompt user to insert card. */
bool ensureSdMounted(String* detailOut = nullptr);

/** Load /home/default/settings.json when SD is mounted. */
bool load();

/** Persist theme preset (0–3) to settings.json. */
bool saveTheme(int preset);

/** Persist Wi-Fi credentials (lab SD only — plaintext on FAT32). */
bool saveWifi(const char* ssid, const char* pass);

/** Copy rotated logs from /var/log into /home/default/saves/. */
bool exportLogSnapshot(String* outPath = nullptr);

/** Copy settings.json into /home/default/saves/ with timestamp suffix. */
bool exportSettingsSnapshot(String* outPath = nullptr);

int themePreset();
const char* themePresetName(int preset);

/** Loaded from settings.json (empty until load()). */
String savedWifiSsid();
String savedWifiPass();

}  // namespace m5os::settings
