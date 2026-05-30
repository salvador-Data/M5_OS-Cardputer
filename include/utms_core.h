#pragma once

#include <Arduino.h>
#include <vector>

namespace m5os::utms {

enum class ScanVerdict { Clean, Infected, Unknown };

struct ScanHit {
    String path;
    String sha256;
    ScanVerdict verdict = ScanVerdict::Unknown;
    String reason;
};

struct ScanSummary {
    size_t scanned = 0;
    size_t clean = 0;
    size_t infected = 0;
    size_t unknown = 0;
    std::vector<ScanHit> hits;
};

struct IdsStatus {
    String lastCheck;
    String packVersion;
    String nvsVersion;
    size_t alertCount = 0;
    size_t quarantinedCount = 0;
    size_t denyHashes = 0;
    size_t allowHashes = 0;
};

struct QuarantineEntry {
    String quarantineName;
    String originalPath;
    String sha256;
    uint32_t epoch = 0;
};

/** Load deny/allow hashes and deny strings from SD threat pack. */
bool loadThreatSignatures(std::vector<String>& denyHashes, std::vector<String>& allowHashes,
                          std::vector<String>& denyStrings);

ScanSummary runAvScan(size_t maxFiles = 96);

bool quarantineFile(const String& sourcePath, const String& sha256, String& errOut);

bool restoreQuarantined(const String& quarantineName, String& errOut);

bool deleteQuarantined(const String& quarantineName, String& errOut);

std::vector<QuarantineEntry> listQuarantineEntries();

IdsStatus loadIdsStatus();

std::vector<String> readLogTailLines(size_t maxLines = 48);

}  // namespace m5os::utms
