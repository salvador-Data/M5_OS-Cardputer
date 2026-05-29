#pragma once

#include <Arduino.h>
#include <vector>

namespace m5os {

struct FirmwarePackage {
    String name;
    String slug;  // compartment dir under /apps/<slug>/
    String version;
    String url;
    String fid;   // M5Burner / LauncherHub firmware id (32 hex)
    String file;  // M5Burner CDN filename or full https URL
    size_t size = 0;
    String description;
    String binFile;  // SD filename without path, e.g. ble_bot.bin
    String sha256;   // optional lowercase hex digest
    bool installed = false;
    /** Bruce-style composite: app on SD OK, launch needs M5Burner USB SPIFFS flash. */
    bool needsFlashSpiffs = false;
};

class FirmwareCatalog {
public:
    bool ensureStorage();
    void scanInstalled();
    bool refreshFromNetwork(const char* manifestUrl);
    bool refreshFromBurnerHub(uint8_t page = 1);
    bool refreshFromSdManifest();
    bool downloadPackage(const FirmwarePackage& pkg);
    /** User-facing detail from the last failed downloadPackage call. */
    const String& lastDownloadError() const { return lastDownloadError_; }
    const std::vector<FirmwarePackage>& installed() const { return installed_; }
    const std::vector<FirmwarePackage>& available() const { return available_; }
    FirmwarePackage* findInstalledByName(const String& name);
    const FirmwarePackage* findByBinFile(const String& binFile) const;
    String binPathFor(const String& binFile) const;
    String binPathForPackage(const FirmwarePackage& pkg) const;
    std::vector<String> whitelistedAppSlugs() const;
    static String slugToBinFile(const String& name);
    void markNeedsFlashSpiffs(const String& fid, const String& name, bool value);

private:
    void mergeInstalledFlags();
    bool loadSdManifestFrom(const char* path);

    std::vector<FirmwarePackage> installed_;
    std::vector<FirmwarePackage> available_;
    String lastDownloadError_;
};

}  // namespace m5os
