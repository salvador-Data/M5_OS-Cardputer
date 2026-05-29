#pragma once

#include <Arduino.h>
#include <FS.h>

namespace m5os::security {

/** SD/WiFi read chunk for SHA256 streaming (4 KiB — balances heap and throughput). */
constexpr size_t kSha256IoChunkBytes = 4096;

/** Progress callback interval while hashing (bytes). */
constexpr size_t kSha256ProgressBytes = 4096;

/** HTTPS URLs from github.com/salvador-Data, raw.githubusercontent.com/salvador-Data, hackerplanet.dev, LauncherHub, or M5Burner CDN */
bool isAllowedHttpsUrl(const String& url);

/** SD filename only — rejects path traversal and invalid characters. */
String sanitizeBinFilename(const String& raw);

/** Single VFS path segment — lowercase slug for /apps/<seg>/ and /home/.../<seg>/. */
String sanitizePathSegment(const String& raw);

/** Lowercase 64-char hex SHA-256 digest. Empty if invalid. */
String normalizeSha256Hex(const String& raw);

bool isValidSha256Hex(const String& hex);

String computeSha256Hex(const uint8_t* data, size_t len);

/** Hash file contents from current read position through end; rewinds to start on failure. */
String computeFileSha256Hex(File& file);

/** Same as computeFileSha256Hex but calls progress(hashed, total) every 4 KiB (TWDT-safe). */
String computeFileSha256HexWithProgress(File& file, size_t totalBytes,
                                        void (*progress)(size_t hashed, size_t total));

bool sha256Equal(const String& expected, const String& actual);

/** M5Burner / LauncherHub firmware id — 32 lowercase hex chars. */
String normalizeBurnerFid(const String& raw);

/** M5Burner CDN filename or whitelisted https URL. */
String sanitizeBurnerFile(const String& raw);

}  // namespace m5os::security
