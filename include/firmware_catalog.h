#pragma once

#include <Arduino.h>
#include <vector>

namespace m5os {

struct FirmwarePackage {
    String name;
    String version;
    String url;
    size_t size = 0;
    String description;
    String binFile;  // SD filename without path, e.g. ble_bot.bin
    String sha256;   // optional lowercase hex digest
    bool installed = false;
};

class FirmwareCatalog {
public:
    bool ensureStorage();
    void scanInstalled();
    bool refreshFromNetwork(const char* manifestUrl);
    bool refreshFromSdManifest();
    bool downloadPackage(const FirmwarePackage& pkg);
    const std::vector<FirmwarePackage>& installed() const { return installed_; }
    const std::vector<FirmwarePackage>& available() const { return available_; }
    FirmwarePackage* findInstalledByName(const String& name);
    const FirmwarePackage* findByBinFile(const String& binFile) const;
    String binPathFor(const String& binFile) const;
    static String slugToBinFile(const String& name);

private:
    void mergeInstalledFlags();

    std::vector<FirmwarePackage> installed_;
    std::vector<FirmwarePackage> available_;
};

}  // namespace m5os
