#include "m5os_vfs.h"

#include "m5os_config.h"
#include "m5os_security.h"
#include "serial_log.h"

#include <SD.h>
#include <SPI.h>

namespace m5os::vfs {

namespace {

bool g_sdMounted = false;
bool g_spiBegun = false;
String g_lastMountError;
String g_lastMountDetail;

// Match M5Cardputer examples/Basic/sdcard/sdcard.ino — global SPI, bus stays up after begin().
void ensureSdSpiBus() {
    pinMode(kSdCsPin, OUTPUT);
    digitalWrite(kSdCsPin, HIGH);
    if (!g_spiBegun) {
        SPI.begin(kSdSclkPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
        g_spiBegun = true;
    }
}

const char* cardTypeLabel(uint8_t cardType) {
    switch (cardType) {
        case CARD_MMC:
            return "MMC";
        case CARD_SD:
            return "SDSC";
        case CARD_SDHC:
            return "SDHC";
        default:
            return "NONE";
    }
}

bool trySdMountOnce(uint32_t hz, String* failDetail) {
    ensureSdSpiBus();
    if (SD.begin(kSdCsPin, SPI, hz)) {
        const uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE) {
            SD.end();
            if (failDetail) *failDetail = "no_card_type@" + String(hz);
            return false;
        }
        const uint64_t mb = SD.cardSize() / (1024ULL * 1024ULL);
        log::info("sd_mounted", String(cardTypeLabel(cardType)) + " " + String(mb) + "MB@" + String(hz));
        return true;
    }
    SD.end();
    if (failDetail) *failDetail = "begin_fail@" + String(hz);
    return false;
}

bool trySdMount() {
    const uint32_t speeds[] = {25000000, 10000000, 4000000, 1000000};
    constexpr uint8_t kRounds = 6;
    String lastDetail;

    ensureSdSpiBus();
    delay(100);

    for (uint8_t round = 0; round < kRounds; ++round) {
        if (round > 0) delay(200);
        for (uint32_t hz : speeds) {
            if (trySdMountOnce(hz, &lastDetail)) {
                g_lastMountDetail = "";
                return true;
            }
            log::info("sd_mount_retry", "r" + String(round) + " " + lastDetail);
            SD.end();
            delay(50);
        }
    }
    g_lastMountDetail = lastDetail;
    log::info("sd_mount_failed", lastDetail.length() ? lastDetail : "all_attempts");
    return false;
}

String mountFailureMessage() {
    if (g_lastMountDetail.indexOf("no_card_type") >= 0) {
        return "No microSD in slot";
    }
    if (g_lastMountDetail.indexOf("begin_fail") >= 0) {
        return "SD present, mount failed\nFAT32 + reseat card";
    }
    return "No FAT32 SD detected";
}

bool mkdirIfMissing(const char* path) {
    if (SD.exists(path)) return true;
    return SD.mkdir(path);
}

bool mkdirChain(const char* path) {
    if (!path || !path[0]) return false;
    String current = "";
    for (unsigned i = 0; path[i]; ++i) {
        current += path[i];
        if (path[i] == '/' && current.length() > 1) {
            if (!mkdirIfMissing(current.c_str())) return false;
        }
    }
    return mkdirIfMissing(path);
}

void writeMarker(const char* path) {
    if (SD.exists(path)) return;
    File marker = SD.open(path, FILE_WRITE);
    if (marker) {
        marker.println("M5 OS Cardputer — Hacker Planet LLC");
        marker.println("Authorized lab use only.");
        marker.close();
    }
}

}  // namespace

bool isMounted() { return g_sdMounted; }

String lastMountError() { return g_lastMountError; }

String entryBaseName(const String& name) {
    const int slash = max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
    return slash >= 0 ? name.substring(slash + 1) : name;
}

String joinPath(const String& parent, const String& segment) {
    String leaf = entryBaseName(segment);
    leaf.trim();
    if (!leaf.length() || leaf == "." || leaf == ".." || leaf.indexOf('/') >= 0 || leaf.indexOf('\\') >= 0) {
        return parent;
    }
    if (leaf.length() > 96) return parent;
    if (!parent.length() || parent == "/") return String("/") + leaf;
    String base = parent;
    if (base.endsWith("/")) base.remove(base.length() - 1);
    return base + "/" + leaf;
}

String slugFromName(const String& name) {
    String slug = name;
    slug.trim();
    slug.toLowerCase();
    slug.replace(' ', '_');
    slug.replace('-', '_');
    for (unsigned i = 0; i < slug.length(); ++i) {
        const char c = slug.charAt(i);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            slug.setCharAt(i, '_');
        }
    }
    while (slug.indexOf("__") >= 0) slug.replace("__", "_");
    slug.trim();
    if (slug.startsWith("_")) slug = slug.substring(1);
    if (slug.endsWith("_")) slug.remove(slug.length() - 1);
    return security::sanitizePathSegment(slug);
}

String appDirFor(const String& appSlug) {
    const String safe = security::sanitizePathSegment(appSlug);
    if (!safe.length()) return "";
    return String(kAppsDir) + "/" + safe;
}

String appDataDirFor(const String& appSlug) {
    const String safe = security::sanitizePathSegment(appSlug);
    if (!safe.length()) return "";
    return String(kHomeAppsDir) + "/" + safe;
}

String binPathFor(const String& appSlug, const String& binFile) {
    const String safeBin = security::sanitizeBinFilename(binFile);
    if (!safeBin.length()) return "";
    const String slug = slugFromName(appSlug.length() ? appSlug : safeBin.substring(0, safeBin.length() - 4));
    const String appPath = appDirFor(slug);
    if (appPath.length()) {
        const String compartment = appPath + "/" + safeBin;
        if (SD.exists(compartment.c_str())) return compartment;
    }
    return String(kLegacyFirmwareDir) + "/" + safeBin;
}

bool ensureAppDirs(const String& appSlug) {
    const String slug = slugFromName(appSlug);
    if (!slug.length()) return false;
    const String appDir = appDirFor(slug);
    const String dataDir = appDataDirFor(slug);
    return mkdirChain(appDir.c_str()) && mkdirChain(dataDir.c_str());
}

MountResult mountAndInit() {
    MountResult result;
    if (g_sdMounted && SD.cardType() != CARD_NONE) {
        result.ok = true;
        result.message = "SD mounted";
        return result;
    }
    g_sdMounted = false;
    g_lastMountError = "";
    SD.end();
    delay(600);

    if (!trySdMount()) {
        result.message = mountFailureMessage();
        g_lastMountError = result.message;
        log::info("sd_missing", g_lastMountDetail.length() ? g_lastMountDetail : result.message);
        return result;
    }

    const char* tree[] = {kSystemDir,       kSystemBinDir,    kAppsDir,
                          kHomeDir,         kHomeDefaultDir,  kHomeAppsDir,
                          kHomeCacheDir,    kSavesDir,        kTmpDir,
                          kVarLogDir,       kLegacyFirmwareDir};
    for (const char* dir : tree) {
        if (!mkdirChain(dir)) {
            result.message = String("Cannot create ") + dir;
            g_lastMountError = result.message;
            log::info("vfs_mkdir_fail", dir);
            SD.end();
            g_sdMounted = false;
            return result;
        }
    }

    writeMarker(kSystemMarkerPath);
    writeMarker(kLegacyMarkerPath);

    File gcMarker = SD.open("/system/bin/gc", FILE_WRITE);
    if (gcMarker) {
        gcMarker.println("M5 OS GcService — boot + menu cleanup");
        gcMarker.close();
    }

    result.ok = true;
    result.message = "SD mounted";
    g_sdMounted = true;
    g_lastMountError = "";
    log::info("vfs_ready");
    return result;
}

}  // namespace m5os::vfs
