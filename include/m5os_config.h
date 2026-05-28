#pragma once

namespace m5os {

// SD layout (Cardputer microSD SPI)
static const int kSdCsPin = 12;
static const int kSdSclkPin = 14;
static const int kSdMisoPin = 39;
static const int kSdMosiPin = 40;

static const char* kFirmwareDir = "/firmware";
static const char* kSdManifestPath = "/manifest.json";
static const char* kLauncherMarkerPath = "/M5OS_CARDPUTER.txt";

#ifndef M5OS_MANIFEST_URL
#define M5OS_MANIFEST_URL \
    "https://raw.githubusercontent.com/salvador-Data/M5_OS-Cardputer/main/data/manifest.example.json"
#endif

static const char* kDefaultManifestUrl = M5OS_MANIFEST_URL;

static const char* kAppRemotePossibility = "remote_possibility";
static const char* kAppBleBot = "ble_bot";

}  // namespace m5os
