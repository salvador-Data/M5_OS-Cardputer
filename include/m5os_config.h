#pragma once

#include <cstddef>
#include <cstdint>

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

#ifndef M5OS_UTMS_PACK_URL
#define M5OS_UTMS_PACK_URL \
    "https://raw.githubusercontent.com/salvador-Data/M5_OS-Cardputer/main/data/threat_pack.example.json"
#endif

static const char* kDefaultUtmsPackUrl = M5OS_UTMS_PACK_URL;

static const char* kAppRemotePossibility = "remote_possibility";
static const char* kAppBleBot = "ble_bot";

/** Max app .bin for SD + OTA run slot (app2 in partitions/m5os_cardputer_8MB.csv). */
static const size_t kMaxAppBinBytes = 0x390000;

/** Session gateway on app1 is 448 KiB; run slot (app2) is 3.5625 MiB. Legacy 2-slot tables use app1 >= 2 MiB. */
static const size_t kMinRunSlotPartitionBytes = 0x200000;

/** Permanent session gateway partition size (app1 / ota_1). */
static const size_t kGatewayPartitionBytes = 0x70000;

/** M5 OS menu freeze recovery — TWDT timeout then restore home + reboot. */
static const uint32_t kWatchdogTimeoutSec = 30;

/** UI theme presets (see ui::setThemePreset). */
static const int kThemePresetCount = 6;
static const int kDefaultThemePreset = 1;  // Hacker Green

/** Cardputer Stamp-S3 SK6812 data pin (M5Unified rgb_led table). */
static const int kStampLedGpio = 21;

/** RTC_CNTL_STORE0 handoff token — mirrored in bootloader_components/main/bootloader_start.c */
static const uint32_t kRtcBootStagedMagic = 0x4D354153u;

}  // namespace m5os
