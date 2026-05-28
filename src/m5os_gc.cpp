#include "m5os_gc.h"

#include "m5os_vfs.h"
#include "m5os_security.h"
#include "serial_log.h"

#include <SD.h>
#include <time.h>

namespace m5os::gc {

namespace {

static const unsigned long kTmpTtlSeconds = 24UL * 3600UL;
static const unsigned long kCacheTtlSeconds = 7UL * 24UL * 3600UL;
static const size_t kMaxLogFiles = 5;
static const size_t kLogRotateBytes = 32768;

unsigned long fileAgeSeconds(File& file) {
    const time_t modified = file.getLastWrite();
    if (modified <= 0) return 0;
    const time_t now = time(nullptr);
    if (now <= 0 || now < modified) return 0;
    return static_cast<unsigned long>(now - modified);
}

bool isWhitelistedSlug(const String& slug, const std::vector<String>& whitelist) {
    for (const auto& allowed : whitelist) {
        if (allowed.equalsIgnoreCase(slug)) return true;
    }
    return false;
}

void sweepDirTtl(const char* dirPath, unsigned long ttlSeconds, GcReport& report, bool active) {
    if (!active) return;
    File dir = SD.open(dirPath);
    if (!dir || !dir.isDirectory()) return;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }
        const unsigned long age = fileAgeSeconds(entry);
        const size_t size = entry.size();
        String name = entry.name();
        entry.close();
        if (age >= ttlSeconds && age > 0) {
            const int slash = name.lastIndexOf('/');
            const String base = (slash >= 0) ? name.substring(slash + 1) : name;
            const String fullPath = String(dirPath) + "/" + base;
            if (SD.remove(fullPath.c_str())) {
                report.tmpRemoved++;
                report.bytesReclaimed += size;
            }
        }
    }
    dir.close();
}

void rotateLogs(GcReport& report) {
    File dir = SD.open(vfs::kVarLogDir);
    if (!dir || !dir.isDirectory()) return;

    struct LogEntry {
        String path;
        size_t size;
        time_t mtime;
    };
    std::vector<LogEntry> logs;

    File entry;
    while ((entry = dir.openNextFile())) {
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }
        LogEntry row;
        row.path = entry.name();
        row.size = entry.size();
        row.mtime = entry.getLastWrite();
        logs.push_back(row);
        entry.close();
    }
    dir.close();

    for (const auto& log : logs) {
        if (log.size <= kLogRotateBytes) continue;
        const int slash = log.path.lastIndexOf('/');
        const String base = (slash >= 0) ? log.path.substring(slash + 1) : log.path;
        const String rotated = String(vfs::kVarLogDir) + "/" + base + ".old";
        if (SD.exists(rotated.c_str())) SD.remove(rotated.c_str());
        File src = SD.open((String(vfs::kVarLogDir) + "/" + base).c_str(), FILE_READ);
        if (!src) continue;
        File dst = SD.open(rotated.c_str(), FILE_WRITE);
        if (dst) {
            uint8_t buf[256];
            size_t kept = 0;
            const size_t skip = log.size > 8192 ? log.size - 8192 : 0;
            if (skip) src.seek(skip);
            while (src.available()) {
                const size_t n = src.read(buf, sizeof(buf));
                if (!n) break;
                dst.write(buf, n);
                kept += n;
            }
            dst.close();
            report.logsRotated++;
            report.bytesReclaimed += (log.size > kept) ? (log.size - kept) : 0;
        }
        src.close();
        SD.remove((String(vfs::kVarLogDir) + "/" + base).c_str());
        SD.rename(rotated.c_str(), (String(vfs::kVarLogDir) + "/" + base).c_str());
    }

    if (logs.size() <= kMaxLogFiles) return;
    while (logs.size() > kMaxLogFiles) {
        size_t oldestIdx = 0;
        for (size_t i = 1; i < logs.size(); ++i) {
            if (logs[i].mtime < logs[oldestIdx].mtime) oldestIdx = i;
        }
        const String path = logs[oldestIdx].path;
        const int slash = path.lastIndexOf('/');
        const String full = (slash >= 0 && path.startsWith("/")) ? path : String(vfs::kVarLogDir) + "/" + path;
        report.bytesReclaimed += logs[oldestIdx].size;
        SD.remove(full.c_str());
        logs.erase(logs.begin() + static_cast<long>(oldestIdx));
        report.logsRotated++;
    }
}

void reclaimCacheOrphans(GcReport& report, const std::vector<String>& whitelist, bool active) {
    if (!active) return;
    File dir = SD.open(vfs::kHomeCacheDir);
    if (!dir || !dir.isDirectory()) return;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (entry.isDirectory()) {
            entry.close();
            continue;
        }
        const size_t size = entry.size();
        String name = entry.name();
        const unsigned long age = fileAgeSeconds(entry);
        entry.close();
        const int slash = name.lastIndexOf('/');
        const String base = (slash >= 0) ? name.substring(slash + 1) : name;
        const String slug = vfs::slugFromName(base.substring(0, base.lastIndexOf('.')));
        if (isWhitelistedSlug(slug, whitelist) && age < kCacheTtlSeconds) continue;
        const String fullPath = String(vfs::kHomeCacheDir) + "/" + base;
        if (SD.remove(fullPath.c_str())) {
            report.cacheRemoved++;
            report.bytesReclaimed += size;
        }
    }
    dir.close();
}

}  // namespace

GcReport quickBootScan() {
    GcReport report;
    sweepDirTtl(vfs::kTmpDir, kTmpTtlSeconds, report, true);
    rotateLogs(report);
    log::info("gc_boot", String(report.tmpRemoved) + " tmp " + String(report.logsRotated) + " logs");
    return report;
}

GcReport fullCleanup(bool userConfirmed, const std::vector<String>& whitelistedAppSlugs) {
    GcReport report = quickBootScan();
    reclaimCacheOrphans(report, whitelistedAppSlugs, userConfirmed);
    log::info("gc_full", String(report.cacheRemoved) + " cache");
    return report;
}

}  // namespace m5os::gc
