#include "utms_core.h"

#include "m5os_gateway_shared.h"
#include "m5os_security.h"
#include "m5os_vfs.h"
#include "m5os_watchdog.h"
#include "utms_threat_pack.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <time.h>

namespace m5os::utms {

namespace {

constexpr size_t kMaxDenyHashes = 256;
constexpr size_t kMaxAllowHashes = 64;
constexpr size_t kMaxDenyStrings = 64;
constexpr size_t kStringScanMaxBytes = 262144;
constexpr size_t kManifestMaxEntries = 128;

bool pathUnderPrefix(const String& path, const char* prefix) {
    if (!path.startsWith(prefix)) return false;
    const size_t plen = strlen(prefix);
    if (path.length() == plen) return true;
    return path.charAt(plen) == '/';
}

bool shouldSkipScanPath(const String& path) {
    if (pathUnderPrefix(path, vfs::kUtmsDir)) return true;
    if (pathUnderPrefix(path, vfs::kQuarantineDir)) return true;
    return false;
}

bool listContainsHash(const std::vector<String>& list, const String& digest) {
    if (!digest.length()) return false;
    for (const auto& h : list) {
        if (security::sha256Equal(h, digest)) return true;
    }
    return false;
}

bool fileContainsDenyString(File& file, const std::vector<String>& denyStrings) {
    if (denyStrings.empty()) return false;
    const size_t toRead = min(static_cast<size_t>(file.size()), kStringScanMaxBytes);
    if (toRead == 0) return false;
    uint8_t buffer[512];
    size_t readTotal = 0;
    while (readTotal < toRead && file.available()) {
        const size_t want = min(sizeof(buffer), toRead - readTotal);
        const size_t n = file.read(buffer, want);
        if (n == 0) break;
        readTotal += n;
        String chunk;
        chunk.reserve(n);
        for (size_t i = 0; i < n; ++i) chunk += static_cast<char>(buffer[i]);
        for (const auto& needle : denyStrings) {
            if (needle.length() && chunk.indexOf(needle) >= 0) return true;
        }
        feedWatchdog();
    }
    return false;
}

ScanVerdict classifyFile(const String& path, const std::vector<String>& denyHashes,
                         const std::vector<String>& allowHashes,
                         const std::vector<String>& denyStrings, String& outDigest,
                         String& outReason) {
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) {
        outReason = "open fail";
        return ScanVerdict::Unknown;
    }
    outDigest = security::computeFileSha256Hex(file);
    if (!outDigest.length()) {
        file.close();
        outReason = "hash fail";
        return ScanVerdict::Unknown;
    }
    if (listContainsHash(denyHashes, outDigest)) {
        file.close();
        outReason = "deny hash";
        return ScanVerdict::Infected;
    }
    if (listContainsHash(allowHashes, outDigest)) {
        file.close();
        outReason = "allow hash";
        return ScanVerdict::Clean;
    }
    if (file.seek(0) && fileContainsDenyString(file, denyStrings)) {
        file.close();
        outReason = "deny string";
        return ScanVerdict::Infected;
    }
    file.close();
    outReason = "no match";
    return ScanVerdict::Unknown;
}

void scanFilePath(const String& path, ScanSummary& summary, const std::vector<String>& denyHashes,
                  const std::vector<String>& allowHashes, const std::vector<String>& denyStrings,
                  size_t maxFiles) {
    if (summary.scanned >= maxFiles) return;
    if (shouldSkipScanPath(path)) return;
    if (!path.endsWith(".bin")) return;

    ScanHit hit;
    hit.path = path;
    hit.verdict = classifyFile(path, denyHashes, allowHashes, denyStrings, hit.sha256, hit.reason);
    ++summary.scanned;
    if (hit.verdict == ScanVerdict::Infected) {
        ++summary.infected;
    } else if (hit.verdict == ScanVerdict::Clean) {
        ++summary.clean;
    } else {
        ++summary.unknown;
    }
    summary.hits.push_back(hit);
    feedWatchdog();
}

void scanDirectoryRecursive(const String& dirPath, ScanSummary& summary,
                          const std::vector<String>& denyHashes,
                          const std::vector<String>& allowHashes,
                          const std::vector<String>& denyStrings, size_t maxFiles) {
    if (summary.scanned >= maxFiles) return;
    if (!SD.exists(dirPath.c_str())) return;
    File dir = SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }
    File entry;
    while ((entry = dir.openNextFile()) && summary.scanned < maxFiles) {
        const String name = vfs::entryBaseName(entry.name());
        String full = dirPath;
        if (!full.endsWith("/")) full += "/";
        full += name;
        if (entry.isDirectory()) {
            if (name != "." && name != ".." && !shouldSkipScanPath(full)) {
                scanDirectoryRecursive(full, summary, denyHashes, allowHashes, denyStrings, maxFiles);
            }
        } else {
            scanFilePath(full, summary, denyHashes, allowHashes, denyStrings, maxFiles);
        }
        entry.close();
    }
    dir.close();
}

bool loadManifestDoc(JsonDocument& doc) {
    if (!SD.exists(vfs::kQuarantineManifestPath)) {
        doc.to<JsonObject>();
        doc["entries"].to<JsonArray>();
        return true;
    }
    File in = SD.open(vfs::kQuarantineManifestPath, FILE_READ);
    if (!in) return false;
    const DeserializationError err = deserializeJson(doc, in);
    in.close();
    return !err;
}

bool writeManifestDoc(JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    const String tmp = String(vfs::kQuarantineManifestPath) + ".tmp";
    File out = SD.open(tmp.c_str(), FILE_WRITE);
    if (!out) return false;
    const size_t written = out.print(body);
    out.close();
    if (written != body.length()) {
        SD.remove(tmp.c_str());
        return false;
    }
    if (SD.exists(vfs::kQuarantineManifestPath)) SD.remove(vfs::kQuarantineManifestPath);
    return SD.rename(tmp.c_str(), vfs::kQuarantineManifestPath);
}

String quarantineFileName(const String& sourcePath) {
    const int slash = sourcePath.lastIndexOf('/');
    String base = slash >= 0 ? sourcePath.substring(slash + 1) : sourcePath;
    base.replace("/", "_");
    return base + ".q";
}

}  // namespace

bool loadThreatSignatures(std::vector<String>& denyHashes, std::vector<String>& allowHashes,
                          std::vector<String>& denyStrings) {
    denyHashes.clear();
    allowHashes.clear();
    denyStrings.clear();
    if (!ensureUtmsDirs()) return false;
    if (!SD.exists(vfs::kThreatPackPath)) return false;

    File in = SD.open(vfs::kThreatPackPath, FILE_READ);
    if (!in) return false;
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, in);
    in.close();
    if (err) return false;

    JsonObject sigs = doc["signatures"].as<JsonObject>();
    if (sigs.isNull()) return false;

    JsonArray hashes = sigs["hashes"].as<JsonArray>();
    if (!hashes.isNull()) {
        for (JsonVariant v : hashes) {
            if (denyHashes.size() >= kMaxDenyHashes) break;
            const String h = security::normalizeSha256Hex(v.as<String>());
            if (h.length()) denyHashes.push_back(h);
        }
    }
    JsonArray allow = sigs["allow_hashes"].as<JsonArray>();
    if (!allow.isNull()) {
        for (JsonVariant v : allow) {
            if (allowHashes.size() >= kMaxAllowHashes) break;
            const String h = security::normalizeSha256Hex(v.as<String>());
            if (h.length()) allowHashes.push_back(h);
        }
    }
    JsonArray strings = sigs["strings"].as<JsonArray>();
    if (!strings.isNull()) {
        for (JsonVariant v : strings) {
            if (denyStrings.size() >= kMaxDenyStrings) break;
            const String s = v.as<String>();
            if (s.length()) denyStrings.push_back(s);
        }
    }
    return denyHashes.size() > 0 || allowHashes.size() > 0 || denyStrings.size() > 0;
}

ScanSummary runAvScan(size_t maxFiles) {
    ScanSummary summary;
    std::vector<String> denyHashes;
    std::vector<String> allowHashes;
    std::vector<String> denyStrings;
    loadThreatSignatures(denyHashes, allowHashes, denyStrings);

    scanDirectoryRecursive(String(vfs::kAppsDir), summary, denyHashes, allowHashes, denyStrings,
                           maxFiles);
    scanDirectoryRecursive(String(vfs::kHomeDefaultDir), summary, denyHashes, allowHashes,
                           denyStrings, maxFiles);
    if (SD.exists(vfs::kStagingBinPath)) {
        scanFilePath(String(vfs::kStagingBinPath), summary, denyHashes, allowHashes, denyStrings,
                     maxFiles);
    }
    if (SD.exists(m5os::gateway::kStagingPath)) {
        scanFilePath(String(m5os::gateway::kStagingPath), summary, denyHashes, allowHashes,
                     denyStrings, maxFiles);
    }

    markCheckEpoch();
    appendLog("av_scan",
              String(summary.scanned) + " clean=" + String(summary.clean) + " inf=" +
                  String(summary.infected) + " unk=" + String(summary.unknown));
    return summary;
}

bool quarantineFile(const String& sourcePath, const String& sha256, String& errOut) {
    errOut = "";
    if (!ensureUtmsDirs()) {
        errOut = "SD required";
        return false;
    }
    if (!SD.exists(sourcePath.c_str())) {
        errOut = "Missing file";
        return false;
    }
    const String qname = quarantineFileName(sourcePath);
    String dest = String(vfs::kQuarantineDir) + "/" + qname;
    if (SD.exists(dest.c_str())) SD.remove(dest.c_str());
    if (!SD.rename(sourcePath.c_str(), dest.c_str())) {
        errOut = "Move failed";
        return false;
    }

    JsonDocument doc;
    if (!loadManifestDoc(doc)) {
        errOut = "Manifest read fail";
        return false;
    }
    JsonArray entries = doc["entries"].to<JsonArray>();
    if (entries.size() >= kManifestMaxEntries) {
        errOut = "Manifest full";
        return false;
    }
    JsonObject row = entries.add<JsonObject>();
    row["file"] = qname;
    row["orig"] = sourcePath;
    row["sha256"] = sha256;
    uint32_t epoch = static_cast<uint32_t>(time(nullptr));
    if (epoch < 1000000000U) epoch = millis() / 1000U;
    row["ts"] = epoch;
    if (!writeManifestDoc(doc)) {
        errOut = "Manifest write fail";
        return false;
    }
    appendLog("quarantine", sourcePath + " -> " + qname);
    return true;
}

std::vector<QuarantineEntry> listQuarantineEntries() {
    std::vector<QuarantineEntry> out;
    if (!ensureUtmsDirs()) return out;

    JsonDocument doc;
    if (!loadManifestDoc(doc)) return out;
    JsonArray entries = doc["entries"].as<JsonArray>();
    if (entries.isNull()) return out;

    for (JsonObject row : entries) {
        QuarantineEntry e;
        e.quarantineName = row["file"] | "";
        e.originalPath = row["orig"] | "";
        e.sha256 = security::normalizeSha256Hex(row["sha256"] | "");
        e.epoch = row["ts"] | 0;
        if (e.quarantineName.length()) {
            const String path = String(vfs::kQuarantineDir) + "/" + e.quarantineName;
            if (SD.exists(path.c_str())) out.push_back(e);
        }
    }
    return out;
}

bool restoreQuarantined(const String& quarantineName, String& errOut) {
    errOut = "";
    String safe = quarantineName;
    safe.trim();
    if (!safe.length() || safe.indexOf('/') >= 0 || safe.indexOf("..") >= 0) {
        errOut = "Invalid name";
        return false;
    }

    JsonDocument doc;
    if (!loadManifestDoc(doc)) {
        errOut = "Manifest read fail";
        return false;
    }
    JsonArray entries = doc["entries"].as<JsonArray>();
    if (entries.isNull()) {
        errOut = "No manifest";
        return false;
    }

    int found = -1;
    String orig;
    for (size_t i = 0; i < entries.size(); ++i) {
        const String file = entries[i]["file"] | "";
        if (file == safe) {
            found = static_cast<int>(i);
            orig = entries[i]["orig"] | "";
            break;
        }
    }
    if (found < 0 || !orig.length()) {
        errOut = "Not in manifest";
        return false;
    }

    const int slash = orig.lastIndexOf('/');
    if (slash > 0) {
        const String parent = orig.substring(0, slash);
        vfs::ensureDirectoryChain(parent.c_str(), nullptr);
    }

    const String from = String(vfs::kQuarantineDir) + "/" + safe;
    if (!SD.exists(from.c_str())) {
        errOut = "Missing in quarantine";
        return false;
    }
    if (SD.exists(orig.c_str())) SD.remove(orig.c_str());
    if (!SD.rename(from.c_str(), orig.c_str())) {
        errOut = "Restore failed";
        return false;
    }
    entries.remove(found);
    if (!writeManifestDoc(doc)) {
        errOut = "Manifest write fail";
        return false;
    }
    appendLog("restore", orig);
    return true;
}

bool deleteQuarantined(const String& quarantineName, String& errOut) {
    errOut = "";
    String safe = quarantineName;
    safe.trim();
    if (!safe.length() || safe.indexOf('/') >= 0 || safe.indexOf("..") >= 0) {
        errOut = "Invalid name";
        return false;
    }

    JsonDocument doc;
    if (!loadManifestDoc(doc)) {
        errOut = "Manifest read fail";
        return false;
    }
    JsonArray entries = doc["entries"].as<JsonArray>();
    int found = -1;
    for (size_t i = 0; i < entries.size(); ++i) {
        if ((entries[i]["file"] | "") == safe) {
            found = static_cast<int>(i);
            break;
        }
    }
    if (found < 0) {
        errOut = "Not in manifest";
        return false;
    }

    const String path = String(vfs::kQuarantineDir) + "/" + safe;
    if (SD.exists(path.c_str())) SD.remove(path.c_str());
    entries.remove(found);
    if (!writeManifestDoc(doc)) {
        errOut = "Manifest write fail";
        return false;
    }
    appendLog("q_delete", safe);
    return true;
}

IdsStatus loadIdsStatus() {
    IdsStatus status;
    const uint32_t epoch = lastCheckEpoch();
    if (!epoch) {
        status.lastCheck = "never";
    } else {
        const uint32_t now = static_cast<uint32_t>(time(nullptr));
        if (now >= 1000000000U && epoch >= 1000000000U) {
            const uint32_t delta = now - epoch;
            if (delta < 3600)
                status.lastCheck = String(delta / 60) + "m ago";
            else if (delta < 86400)
                status.lastCheck = String(delta / 3600) + "h ago";
            else
                status.lastCheck = String(delta / 86400) + "d ago";
        } else {
            status.lastCheck = String(epoch) + "s boot";
        }
    }
    status.nvsVersion = lastUpdateVersion();
    const ThreatPackInfo pack = loadPackInfo();
    if (pack.loaded) {
        status.packVersion = pack.version;
        status.denyHashes = pack.hashCount;
    }
    std::vector<String> allow;
    std::vector<String> deny;
    std::vector<String> strings;
    if (loadThreatSignatures(deny, allow, strings)) status.allowHashes = allow.size();

    const auto q = listQuarantineEntries();
    status.quarantinedCount = q.size();

    const std::vector<String> lines = readLogTailLines(200);
    for (const auto& line : lines) {
        if (line.indexOf("fw_block") >= 0 || line.indexOf("infected") >= 0 ||
            line.indexOf("deny") >= 0 || line.indexOf("ota_fail") >= 0) {
            ++status.alertCount;
        }
    }
    return status;
}

std::vector<String> readLogTailLines(size_t maxLines) {
    std::vector<String> lines;
    if (!ensureUtmsDirs()) return lines;
    if (!SD.exists(vfs::kUtmsLogPath)) return lines;

    File in = SD.open(vfs::kUtmsLogPath, FILE_READ);
    if (!in) return lines;
    String tail;
    const size_t size = in.size();
    const size_t seek = size > 4096 ? size - 4096 : 0;
    if (seek) in.seek(seek);
    while (in.available()) tail += static_cast<char>(in.read());
    in.close();

    int start = 0;
    while (lines.size() < maxLines) {
        const int nl = tail.indexOf('\n', start);
        if (nl < 0) {
            if (start < tail.length()) lines.push_back(tail.substring(start));
            break;
        }
        String line = tail.substring(start, nl);
        line.trim();
        if (line.length()) lines.push_back(line);
        start = nl + 1;
    }
    if (lines.size() > maxLines) {
        lines.erase(lines.begin(), lines.end() - static_cast<int>(maxLines));
    }
    return lines;
}

}  // namespace m5os::utms
