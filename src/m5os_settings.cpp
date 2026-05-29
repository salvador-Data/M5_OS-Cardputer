#include "m5os_settings.h"

#include "m5os_config.h"
#include "m5os_vfs.h"
#include "serial_log.h"

#include <ArduinoJson.h>
#include <SD.h>

namespace m5os::settings {

namespace {

constexpr int kSettingsVersion = 1;
int gThemePreset = kDefaultThemePreset;
String gWifiSsid;
String gWifiPass;

bool writeSettingsDoc(JsonDocument& doc) {
    if (!vfs::isMounted()) {
        const vfs::MountResult mount = vfs::mountAndInit();
        if (!mount.ok) return false;
    }
    if (!SD.exists(vfs::kHomeDefaultDir)) {
        if (!SD.mkdir(vfs::kHomeDefaultDir)) return false;
    }
    if (!SD.exists(vfs::kSavesDir)) {
        if (!SD.mkdir(vfs::kSavesDir)) return false;
    }
    File out = SD.open(vfs::kSettingsPath, FILE_WRITE);
    if (!out) return false;
    serializeJson(doc, out);
    out.println();
    out.close();
    log::info("settings_saved", vfs::kSettingsPath);
    return true;
}

bool readSettingsFile(JsonDocument& doc) {
    if (!SD.exists(vfs::kSettingsPath)) return false;
    File in = SD.open(vfs::kSettingsPath, FILE_READ);
    if (!in) return false;
    const DeserializationError err = deserializeJson(doc, in);
    in.close();
    return !err;
}

String timestampSuffix() {
    // millis-based stamp — no RTC on base M5 OS
    return String(millis());
}

bool copyFileToSaves(const char* srcPath, const String& destName) {
    if (!ensureSdMounted()) return false;
    if (!SD.exists(srcPath)) return false;
    File src = SD.open(srcPath, FILE_READ);
    if (!src) return false;
    const String dest = String(vfs::kSavesDir) + "/" + destName;
    File dst = SD.open(dest.c_str(), FILE_WRITE);
    if (!dst) {
        src.close();
        return false;
    }
    uint8_t buf[256];
    while (src.available()) {
        const int n = src.read(buf, sizeof(buf));
        if (n > 0) dst.write(buf, n);
    }
    src.close();
    dst.close();
    return true;
}

}  // namespace

bool ensureSdMounted(String* detailOut) {
    if (vfs::isMounted()) return true;
    const vfs::MountResult mount = vfs::mountAndInit();
    if (!mount.ok) {
        String msg = mount.message.length() ? mount.message : vfs::lastMountError();
        if (!msg.length()) msg = "Insert FAT32 microSD";
        if (detailOut) *detailOut = msg;
        return false;
    }
    return true;
}

bool load() {
    if (!ensureSdMounted()) return false;
    JsonDocument doc;
    if (!readSettingsFile(doc)) return false;
    const int theme = doc["theme"] | gThemePreset;
    if (theme >= 0 && theme < kThemePresetCount) gThemePreset = theme;
    gWifiSsid = doc["wifi"]["ssid"] | "";
    gWifiPass = doc["wifi"]["pass"] | "";
    gWifiSsid.trim();
    log::info("settings_loaded", String(gThemePreset));
    return true;
}

bool saveTheme(int preset) {
    if (preset < 0 || preset >= kThemePresetCount) return false;
    gThemePreset = preset;
    if (!ensureSdMounted()) return false;
    JsonDocument doc;
    readSettingsFile(doc);
    doc["version"] = kSettingsVersion;
    doc["theme"] = preset;
    if (gWifiSsid.length()) {
        doc["wifi"]["ssid"] = gWifiSsid;
        doc["wifi"]["pass"] = gWifiPass;
    }
    return writeSettingsDoc(doc);
}

bool saveWifi(const char* ssid, const char* pass) {
    if (!ssid || !ssid[0]) return false;
    gWifiSsid = ssid;
    gWifiPass = pass ? pass : "";
    if (!ensureSdMounted()) return false;
    JsonDocument doc;
    readSettingsFile(doc);
    doc["version"] = kSettingsVersion;
    doc["theme"] = gThemePreset;
    doc["wifi"]["ssid"] = gWifiSsid;
    doc["wifi"]["pass"] = gWifiPass;
    return writeSettingsDoc(doc);
}

bool exportLogSnapshot(String* outPath) {
    if (!ensureSdMounted()) return false;
    if (!SD.exists(vfs::kSavesDir) && !SD.mkdir(vfs::kSavesDir)) return false;

    const String dest = String(vfs::kSavesDir) + "/log_export_" + timestampSuffix() + ".txt";
    File out = SD.open(dest.c_str(), FILE_WRITE);
    if (!out) return false;

    File dir = SD.open(vfs::kVarLogDir);
    if (dir && dir.isDirectory()) {
        File entry;
        while ((entry = dir.openNextFile())) {
            if (!entry.isDirectory()) {
                String leaf = vfs::entryBaseName(entry.name());
                out.printf("--- %s ---\n", leaf.c_str());
                while (entry.available()) out.write(static_cast<uint8_t>(entry.read()));
                out.println();
            }
            entry.close();
        }
        dir.close();
    } else {
        out.println("(no /var/log files yet)");
    }
    out.close();
    if (outPath) *outPath = dest;
    log::info("log_exported", dest);
    return true;
}

bool exportSettingsSnapshot(String* outPath) {
    if (!ensureSdMounted()) return false;
    if (!SD.exists(vfs::kSettingsPath)) return false;
    const String destName = String("settings_") + timestampSuffix() + ".json";
    if (!copyFileToSaves(vfs::kSettingsPath, destName)) return false;
    const String dest = String(vfs::kSavesDir) + "/" + destName;
    if (outPath) *outPath = dest;
    log::info("settings_snapshot", dest);
    return true;
}

int themePreset() { return gThemePreset; }

const char* themePresetName(int preset) {
    static const char* names[] = {
        "Baby Blue", "Hacker Green", "Mr. Robot Red", "Hacker Planet", "Matrix Neon", "Amber Terminal",
    };
    if (preset < 0 || preset >= kThemePresetCount) preset = kDefaultThemePreset;
    return names[preset];
}

String savedWifiSsid() { return gWifiSsid; }

String savedWifiPass() { return gWifiPass; }

}  // namespace m5os::settings
