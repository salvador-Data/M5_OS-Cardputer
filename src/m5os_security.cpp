#include "m5os_security.h"

#include "m5os_watchdog.h"

#include <mbedtls/sha256.h>

namespace m5os::security {

namespace {

String extractHostAndPath(const String& url, String& pathOut) {
    pathOut = "";
    if (!url.startsWith("https://")) return "";
    const int hostStart = 8;
    const int pathStart = url.indexOf('/', hostStart);
    if (pathStart < 0) {
        pathOut = "/";
        return url.substring(hostStart);
    }
    pathOut = url.substring(pathStart);
    return url.substring(hostStart, pathStart);
}

bool isAllowedHostPath(const String& host, const String& path) {
    if (host == "hackerplanet.dev" || host == "www.hackerplanet.dev") return true;
    if (host == "github.com") return path.startsWith("/salvador-Data/");
    if (host == "raw.githubusercontent.com") return path.startsWith("/salvador-Data/");
    if (host == "api.launcherhub.net") {
        return path.startsWith("/firmwares") || path.startsWith("/download");
    }
    if (host == "m5burner-cdn.m5stack.com") return path.startsWith("/firmware/");
    return false;
}

bool isSafeFilenameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == '-';
}

}  // namespace

bool isAllowedHttpsUrl(const String& url) {
    if (!url.startsWith("https://")) return false;
    String path;
    const String host = extractHostAndPath(url, path);
    if (!host.length()) return false;
    return isAllowedHostPath(host, path);
}

String sanitizePathSegment(const String& raw) {
    String seg = raw;
    seg.trim();
    seg.toLowerCase();
    if (!seg.length() || seg.indexOf("..") >= 0 || seg.indexOf('/') >= 0 || seg.indexOf('\\') >= 0) {
        return "";
    }
    for (unsigned i = 0; i < seg.length(); ++i) {
        const char c = seg.charAt(i);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return "";
    }
    if (seg.length() > 48) return "";
    return seg;
}

String sanitizeBinFilename(const String& raw) {
    String name = raw;
    name.trim();
    if (name.indexOf("..") >= 0) return "";
    const int slash = max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
    if (slash >= 0) name = name.substring(slash + 1);
    if (!name.length() || name.indexOf("..") >= 0) return "";
    if (!name.endsWith(".bin")) return "";
    for (unsigned i = 0; i < name.length(); ++i) {
        if (!isSafeFilenameChar(name.charAt(i))) return "";
    }
    if (name.length() > 64) return "";
    return name;
}

String normalizeSha256Hex(const String& raw) {
    String hex = raw;
    hex.trim();
    hex.toLowerCase();
    if (hex.length() != 64) return "";
    for (unsigned i = 0; i < hex.length(); ++i) {
        const char c = hex.charAt(i);
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return "";
    }
    return hex;
}

bool isValidSha256Hex(const String& hex) { return normalizeSha256Hex(hex).length() == 64; }

String computeSha256Hex(const uint8_t* data, size_t len) {
    uint8_t digest[32];
    mbedtls_sha256(data, len, digest, 0);
    static const char* kHex = "0123456789abcdef";
    String out;
    out.reserve(64);
    for (uint8_t byte : digest) {
        out += kHex[(byte >> 4) & 0x0F];
        out += kHex[byte & 0x0F];
    }
    return out;
}

String computeFileSha256Hex(File& file) {
    return computeFileSha256HexWithProgress(file, file ? file.size() : 0, nullptr);
}

String computeFileSha256HexWithProgress(File& file, size_t totalBytes,
                                        void (*progress)(size_t hashed, size_t total)) {
    if (!file) return "";
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    uint8_t buffer[kSha256IoChunkBytes];
    size_t hashed = 0;
    while (file.available()) {
        const size_t n = file.read(buffer, sizeof(buffer));
        if (!n) break;
        mbedtls_sha256_update(&ctx, buffer, n);
        hashed += n;
        feedWatchdog();
        if (progress &&
            (hashed == n || hashed % kSha256ProgressBytes == 0 || !file.available())) {
            progress(hashed, totalBytes);
        }
    }
    uint8_t digest[32];
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    file.seek(0);
    return computeSha256Hex(digest, sizeof(digest));
}

bool sha256Equal(const String& expected, const String& actual) {
    const String a = normalizeSha256Hex(expected);
    const String b = normalizeSha256Hex(actual);
    if (a.length() != 64 || b.length() != 64) return false;
    uint8_t diff = 0;
    for (int i = 0; i < 64; ++i) diff |= static_cast<uint8_t>(a.charAt(i) ^ b.charAt(i));
    return diff == 0;
}

String normalizeBurnerFid(const String& raw) {
    String fid = raw;
    fid.trim();
    fid.toLowerCase();
    if (fid.length() != 32) return "";
    for (unsigned i = 0; i < fid.length(); ++i) {
        const char c = fid.charAt(i);
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return "";
    }
    return fid;
}

String sanitizeBurnerFile(const String& raw) {
    String file = raw;
    file.trim();
    if (!file.length()) return "";
    if (file.startsWith("https://")) {
        return isAllowedHttpsUrl(file) ? file : "";
    }
    if (file.indexOf("..") >= 0 || file.indexOf('/') >= 0 || file.indexOf('\\') >= 0) return "";
    if (!file.endsWith(".bin")) return "";
    for (unsigned i = 0; i < file.length(); ++i) {
        if (!isSafeFilenameChar(file.charAt(i))) return "";
    }
    if (file.length() > 96) return "";
    return file;
}

}  // namespace m5os::security
