#include "m5os_otadata.h"

#include <bootloader_common.h>
#include <esp_log.h>
#include <cstring>

static const char* kOtadataTag = "m5os_otadata";

namespace m5os::otadata {

namespace {

bool writeOtadataSelectEntry(const esp_partition_t* otadata, int sectorIndex,
                             const esp_ota_select_entry_t& entry) {
    if (!otadata || sectorIndex < 0 || sectorIndex > 1) return false;
    constexpr size_t kSectorSize = 4096;
    if (esp_partition_erase_range(otadata, sectorIndex * kSectorSize, kSectorSize) != ESP_OK) {
        return false;
    }
    return esp_partition_write(otadata, sectorIndex * kSectorSize, &entry, sizeof(entry)) == ESP_OK;
}

uint32_t nextOtadataSeqForIndex(int stagedIndex, uint8_t otaAppCount, uint32_t activeSeq) {
    const uint32_t indexSeq = (static_cast<uint32_t>(stagedIndex) % otaAppCount) + 1;
    if (activeSeq == 0 || activeSeq == UINT32_MAX) return indexSeq;
    uint32_t generation = 0;
    while (activeSeq >= indexSeq + generation * otaAppCount) {
        ++generation;
    }
    return indexSeq + generation * otaAppCount;
}

uint8_t countOtaAppPartitions() {
    uint8_t count = 0;
    esp_partition_subtype_t subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    while (true) {
        const esp_partition_t* part =
            esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, nullptr);
        if (!part) break;
        ++count;
        if (subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MAX) break;
        subtype = static_cast<esp_partition_subtype_t>(subtype + 1);
    }
    return count ? count : 2;
}

int otaIndexFromSelectEntry(const esp_ota_select_entry_t& entry, uint8_t otaAppCount) {
    if (otaAppCount == 0 || entry.ota_seq == 0 || entry.ota_seq == UINT32_MAX) return -1;
    return static_cast<int>((entry.ota_seq - 1) % otaAppCount);
}

}  // namespace

bool markPartitionOtaState(const esp_partition_t* staged, esp_ota_img_states_t targetState) {
    if (!staged) return false;

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(staged, &state) == ESP_OK && state == targetState) {
        return true;
    }

    const esp_partition_t* otadata =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    if (!otadata) return false;

    constexpr size_t kSectorSize = 4096;
    esp_ota_select_entry_t entries[2]{};
    if (esp_partition_read(otadata, 0, &entries[0], sizeof(entries[0])) != ESP_OK) return false;
    if (esp_partition_read(otadata, kSectorSize, &entries[1], sizeof(entries[1])) != ESP_OK) {
        return false;
    }

    const uint8_t otaAppCount = countOtaAppPartitions();
    const int stagedIndex = static_cast<int>(staged->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0);
    if (stagedIndex < 0 || stagedIndex >= static_cast<int>(otaAppCount)) return false;

    bool updated = false;

    for (int i = 0; i < 2; ++i) {
        if (!bootloader_common_ota_select_valid(&entries[i])) continue;
        if (otaIndexFromSelectEntry(entries[i], otaAppCount) != stagedIndex) continue;
        entries[i].ota_state = targetState;
        entries[i].crc = bootloader_common_ota_select_crc(&entries[i]);
        if (esp_partition_erase_range(otadata, i * kSectorSize, kSectorSize) != ESP_OK) continue;
        if (esp_partition_write(otadata, i * kSectorSize, &entries[i], sizeof(entries[i])) != ESP_OK) {
            continue;
        }
        updated = true;
    }

    if (!updated) {
        const int active = bootloader_common_get_active_otadata(entries);
        const int nextSector = (active == -1) ? 0 : ((~active) & 1);
        const uint32_t activeSeq = (active == -1) ? 0 : entries[active].ota_seq;
        const uint32_t nextSeq = nextOtadataSeqForIndex(stagedIndex, otaAppCount, activeSeq);
        esp_ota_select_entry_t selected{};
        std::memset(&selected, 0xFF, sizeof(selected));
        selected.ota_seq = nextSeq;
        selected.ota_state = targetState;
        selected.crc = bootloader_common_ota_select_crc(&selected);
        updated = writeOtadataSelectEntry(otadata, nextSector, selected);
        if (updated) {
            ESP_LOGI(kOtadataTag, "stage boot %s seq %lu", staged->label,
                     static_cast<unsigned long>(nextSeq));
        }
    }

    if (updated) {
        ESP_LOGI(kOtadataTag, "stage state %s -> %d", staged->label,
                 static_cast<int>(targetState));
    }
    return updated;
}

}  // namespace m5os::otadata
