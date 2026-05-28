#pragma once

#include <Arduino.h>
#include <FS.h>

namespace m5os::security {

/** HTTPS URLs from github.com/salvador-Data, raw.githubusercontent.com/salvador-Data, or hackerplanet.dev */
bool isAllowedHttpsUrl(const String& url);

/** SD filename only — rejects path traversal and invalid characters. */
String sanitizeBinFilename(const String& raw);

/** Lowercase 64-char hex SHA-256 digest. Empty if invalid. */
String normalizeSha256Hex(const String& raw);

bool isValidSha256Hex(const String& hex);

String computeSha256Hex(const uint8_t* data, size_t len);

/** Hash file contents from current read position through end; rewinds to start on failure. */
String computeFileSha256Hex(File& file);

bool sha256Equal(const String& expected, const String& actual);

}  // namespace m5os::security
