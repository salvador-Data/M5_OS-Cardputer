#include "m5os_vfs.h"

#include "m5os_config.h"
#include "m5os_security.h"
#include "serial_log.h"

#include <SD.h>
#include <SPI.h>

#include <cerrno>
#include <cstring>

#include <driver/gpio.h>

namespace m5os::vfs {

namespace {

bool g_sdMounted = false;
String g_lastMountError;
String g_lastMountDetail;

void driveSdPinsStrong() {
    const gpio_num_t pins[] = {
        static_cast<gpio_num_t>(kSdCsPin),   static_cast<gpio_num_t>(kSdSclkPin),
        static_cast<gpio_num_t>(kSdMisoPin), static_cast<gpio_num_t>(kSdMosiPin),
    };
    for (gpio_num_t pin : pins) {
        gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_3);
    }
}

// Match M5Cardputer examples/Basic/sdcard/sdcard.ino — global SPI, bus stays up after begin().
void ensureSdSpiBus() {
    pinMode(kSdCsPin, OUTPUT);
    digitalWrite(kSdCsPin, HIGH);
    driveSdPinsStrong();
    SPI.begin(kSdSclkPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
    delay(5);
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

bool tryOfficialMount(String* failDetail) {
    ensureSdSpiBus();
    if (SD.begin(kSdCsPin, SPI, 25000000)) {
        const uint8_t cardType = SD.cardType();
        if (cardType != CARD_NONE) {
            const uint64_t mb = SD.cardSize() / (1024ULL * 1024ULL);
            log::info("sd_mounted", String("official ") + cardTypeLabel(cardType) + " " + String(mb) + "MB");
            return true;
        }
        SD.end();
        if (failDetail) *failDetail = "no_card_type@official";
        return false;
    }
    SD.end();
    if (failDetail) *failDetail = "begin_fail@official";
    return false;
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
    const uint32_t speeds[] = {25000000, 10000000, 4000000, 1000000, 400000};
    constexpr uint8_t kRounds = 3;
    String lastDetail;

    delay(50);

    if (tryOfficialMount(&lastDetail)) {
        g_lastMountDetail = "";
        return true;
    }
    log::info("sd_mount_retry", "official " + lastDetail);

    for (uint8_t round = 0; round < kRounds; ++round) {
        if (round > 0) {
            delay(200);
            ensureSdSpiBus();
        }
        for (uint32_t hz : speeds) {
            if (trySdMountOnce(hz, &lastDetail)) {
                g_lastMountDetail = "";
                return true;
            }
            log::info("sd_mount_retry", "r" + String(round) + " " + lastDetail);
            SD.end();
            delay(40);
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

enum class PathKind { kMissing, kDirectory, kFile, kUnknown };

String normalizeVfsPath(const char* path) {
    if (!path || !path[0]) return "";
    String normalized(path);
    while (normalized.length() > 1 && normalized.endsWith("/")) {
        normalized.remove(normalized.length() - 1);
    }
    return normalized;
}

PathKind pathKind(const char* path) {
    if (!SD.exists(path)) return PathKind::kMissing;
    File entry = SD.open(path);
    if (!entry) return PathKind::kUnknown;
    const bool isDir = entry.isDirectory();
    entry.close();
    return isDir ? PathKind::kDirectory : PathKind::kFile;
}

String errnoHint(int err) {
    if (err == 0) return "";
    switch (err) {
        case EEXIST:
            return " (exists)";
        case ENOENT:
            return " (parent missing)";
        case EACCES:
            return " (read-only?)";
        case ENOSPC:
            return " (card full)";
        default:
            return String(" errno=") + String(err);
    }
}

String pathLeaf(const String& normalized) {
    const int slash = normalized.lastIndexOf('/');
    return slash >= 0 ? normalized.substring(slash + 1) : normalized;
}

String pathParent(const String& normalized) {
    if (!normalized.startsWith("/")) return "";
    const int slash = normalized.lastIndexOf('/');
    if (slash <= 0) return "/";
    return normalized.substring(0, slash);
}

String joinParentLeaf(const String& parent, const String& leaf) {
    if (!leaf.length()) return parent;
    if (!parent.length() || parent == "/") return String("/") + leaf;
    return parent + "/" + leaf;
}

bool clearBlockingFile(const String& normalized, String* failReason) {
    if (pathKind(normalized.c_str()) != PathKind::kFile) return true;

    const String backup = normalized + ".m5os_bak";
    if (SD.exists(backup.c_str()) && !SD.remove(backup.c_str())) {
        if (failReason) *failReason = "cannot clear " + backup;
        return false;
    }
    if (!SD.rename(normalized.c_str(), backup.c_str())) {
        if (failReason) {
            *failReason = "rename " + normalized + " -> .m5os_bak" + errnoHint(errno);
        }
        return false;
    }
    log::info("vfs_file_quarantine", normalized + " -> " + backup);
    return true;
}

bool mkdirAbsolute(const String& normalized, String* failReason) {
    errno = 0;
    if (SD.mkdir(normalized.c_str())) return true;
    if (failReason) *failReason = String("mkdir ") + normalized + errnoHint(errno);
    return false;
}

bool mkdirViaParent(const String& normalized, String* failReason) {
    const String parentPath = pathParent(normalized);
    const String leaf = pathLeaf(normalized);
    if (!parentPath.length() || !leaf.length()) {
        if (failReason) *failReason = "invalid path " + normalized;
        return false;
    }

    File parent = SD.open(parentPath.c_str());
    if (!parent) {
        if (failReason) *failReason = String("open parent ") + parentPath + errnoHint(errno);
        return false;
    }
    const bool isDir = parent.isDirectory();
    parent.close();
    if (!isDir) {
        if (failReason) *failReason = parentPath + " not a directory";
        return false;
    }

    errno = 0;
    if (SD.mkdir(normalized.c_str())) return true;

    const String joined = joinParentLeaf(parentPath, leaf);
    errno = 0;
    if (SD.mkdir(joined.c_str())) return true;

    if (failReason) *failReason = String("mkdir ") + leaf + " in " + parentPath + errnoHint(errno);
    return false;
}

bool ensureDirectory(const char* path, String* failReason) {
    const String normalized = normalizeVfsPath(path);
    if (!normalized.length() || normalized == "/") return true;
    if (!normalized.startsWith("/")) {
        if (failReason) *failReason = "path must be absolute";
        return false;
    }

    switch (pathKind(normalized.c_str())) {
        case PathKind::kDirectory:
            return true;
        case PathKind::kFile:
            if (!clearBlockingFile(normalized, failReason)) return false;
            break;
        case PathKind::kUnknown:
            if (failReason) *failReason = String("cannot stat ") + normalized;
            return false;
        case PathKind::kMissing:
            break;
    }

    if (mkdirAbsolute(normalized, failReason)) return true;
    String parentFail;
    if (mkdirViaParent(normalized, &parentFail)) return true;

    if (pathKind(normalized.c_str()) == PathKind::kDirectory) return true;

    if (failReason) {
        if (parentFail.length()) {
            *failReason = parentFail;
        }
    }
    return false;
}

bool ensureDirectoryChain(const char* path, String* failReason) {
    const String normalized = normalizeVfsPath(path);
    if (!normalized.length() || normalized == "/") return true;
    if (!normalized.startsWith("/")) {
        if (failReason) *failReason = "path must be absolute";
        return false;
    }

    String built = "";
    int start = 1;
    while (start <= static_cast<int>(normalized.length())) {
        const int slash = normalized.indexOf('/', start);
        const String segment =
            slash < 0 ? normalized.substring(start) : normalized.substring(start, slash);
        start = slash < 0 ? static_cast<int>(normalized.length()) + 1 : slash + 1;
        if (!segment.length()) continue;

        built += "/" + segment;
        if (!ensureDirectory(built.c_str(), failReason)) return false;
    }
    return true;
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

bool verifySdReadWrite(String* failReason) {
    constexpr const char* kProbe = "/.m5os_probe";
    errno = 0;
    File out = SD.open(kProbe, FILE_WRITE);
    if (!out) {
        if (failReason) *failReason = String("probe write ") + kProbe + errnoHint(errno);
        return false;
    }
    out.print("ok");
    out.close();

    errno = 0;
    File in = SD.open(kProbe, FILE_READ);
    if (!in) {
        if (failReason) *failReason = String("probe read ") + kProbe + errnoHint(errno);
        SD.remove(kProbe);
        return false;
    }
    char buf[3] = {};
    const size_t n = in.readBytes(buf, 2);
    in.close();
    SD.remove(kProbe);
    if (n != 2 || buf[0] != 'o' || buf[1] != 'k') {
        if (failReason) *failReason = "probe verify mismatch";
        return false;
    }
    return true;
}

}  // namespace

void primeSdPinsPreDisplay() {
    pinMode(kSdCsPin, OUTPUT);
    digitalWrite(kSdCsPin, HIGH);
    driveSdPinsStrong();
    delay(80);
}

void reinitSpiPostDisplay() {
    ensureSdSpiBus();
    delay(120);
}

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
    return ensureDirectoryChain(appDir.c_str(), nullptr) && ensureDirectoryChain(dataDir.c_str(), nullptr);
}

MountResult mountAndInit() {
    MountResult result;
    if (g_sdMounted && SD.cardType() != CARD_NONE) {
        result.ok = true;
        result.message = "SD mounted";
        return result;
    }

    if (g_sdMounted) {
        SD.end();
        g_sdMounted = false;
        delay(50);
    }

    g_lastMountError = "";

    if (!trySdMount()) {
        result.message = mountFailureMessage();
        g_lastMountError = result.message;
        log::info("sd_missing", g_lastMountDetail.length() ? g_lastMountDetail : result.message);
        return result;
    }

    String vfsStepError;
    if (!verifySdReadWrite(&vfsStepError)) {
        SD.end();
        g_sdMounted = false;
        result.message = "SD read/write failed\nFAT32 + reseat card";
        if (vfsStepError.length()) result.message += "\n" + vfsStepError;
        g_lastMountError = result.message;
        log::info("sd_rw_fail", vfsStepError.length() ? vfsStepError : "probe");
        return result;
    }

    const char* tree[] = {kSystemDir,       kSystemBinDir,    kAppsDir,
                          kHomeDir,         kHomeDefaultDir,  kHomeAppsDir,
                          kHomeCacheDir,    kSavesDir,        kTmpDir,
                          kVarLogDir,       kLegacyFirmwareDir};
    for (const char* dir : tree) {
        vfsStepError = "";
        if (!ensureDirectoryChain(dir, &vfsStepError)) {
            result.message = String("Cannot create ") + dir;
            if (vfsStepError.length()) result.message += "\n" + vfsStepError;
            g_lastMountError = result.message;
            log::info("vfs_mkdir_fail", vfsStepError.length() ? dir + String(" ") + vfsStepError : dir);
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
