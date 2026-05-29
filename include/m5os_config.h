#pragma once

namespace m5os {

// SD layout (Cardputer microSD SPI — docs.m5stack.com microSD socket)
static const int kSdCsPin = 12;
static const int kSdSclkPin = 40;
static const int kSdMisoPin = 39;
static const int kSdMosiPin = 14;

/** Primary manifest on SD "hard drive"; legacy root manifest still accepted. */
static const char* kSdManifestPath = "/apps/manifest.json";
static const char* kLegacyManifestPath = "/manifest.json";
static const char* kFirmwareDir = "/apps";
static const char* kLegacyFirmwareDir = "/firmware";
static const char* kLauncherMarkerPath = "/system/M5OS_CARDPUTER.txt";

#ifndef M5OS_MANIFEST_URL
#define M5OS_MANIFEST_URL \
    "https://raw.githubusercontent.com/salvador-Data/M5_OS-Cardputer/main/data/manifest.example.json"
#endif

static const char* kDefaultManifestUrl = M5OS_MANIFEST_URL;

static const char* kAppRemotePossibility = "remote_possibility";
static const char* kAppBleBot = "ble_bot";

/** Max app .bin size for SD download + Update flash (default_8MB OTA slot ≈ 3 MiB). */
static const size_t kMaxAppBinBytes = 3145728;

}  // namespace m5os
