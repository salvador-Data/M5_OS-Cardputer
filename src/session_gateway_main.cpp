/**
 * M5 OS session gateway — runs from app1 (ota_1).
 * ESC/` → M5 OS home + save prompt; Enter → run slot (app2).
 * Built only with env:m5os-session-gateway (see platformio.ini build_src_filter).
 */
#include "m5os_gateway_shared.h"
#include "m5os_keyboard.h"
#include "m5os_otadata.h"

#include <M5Cardputer.h>

#include <esp_ota_ops.h>
#include <esp_system.h>
#include <nvs.h>

namespace {

bool nvsSetFlag(const char* key, bool value) {
    nvs_handle_t handle = 0;
    if (nvs_open(m5os::gateway::kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t err = value ? nvs_set_u8(handle, key, 1) : nvs_erase_key(handle, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

void drawFrame(const char* statusLine, int countdownSec = -1) {
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setCursor(4, 4);
    d.println("Session gateway");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 24);
    d.println("ESC/` = M5 OS");
    d.setCursor(4, 40);
    d.println("Enter = launch app");
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 56);
    d.println(statusLine);
    if (countdownSec >= 0) {
        d.setCursor(4, 72);
        d.printf("Auto-launch %ds", countdownSec);
    }
}

const esp_partition_t* homePartition() {
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                    nullptr);
}

const esp_partition_t* runPartition() {
    const esp_partition_t* app2 =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_2, nullptr);
    if (app2) return app2;
    const esp_partition_t* app1 =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (app1 && app1->size >= m5os::kMinRunSlotPartitionBytes) return app1;
    return nullptr;
}

bool partitionHasAppMagic(const esp_partition_t* part) {
    if (!part) return false;
    uint8_t magic = 0;
    return esp_partition_read(part, 0, &magic, 1) == ESP_OK && magic == 0xE9;
}

void exitToHome() {
    nvsSetFlag(m5os::gateway::kGatewayActiveKey, false);
    nvsSetFlag(m5os::gateway::kLaunchPendingKey, false);
    nvsSetFlag(m5os::gateway::kAppSessionKey, true);
    nvsSetFlag(m5os::gateway::kSessionExitKey, true);
    const esp_partition_t* home = homePartition();
    if (home) esp_ota_set_boot_partition(home);
    esp_restart();
}

void launchRunSlot() {
    const esp_partition_t* run = runPartition();
    if (!run || !partitionHasAppMagic(run)) {
        drawFrame("No app in run slot");
        delay(400);
        exitToHome();
        return;
    }
    nvsSetFlag(m5os::gateway::kGatewayActiveKey, false);
    if (!m5os::otadata::markPartitionOtaState(run, ESP_OTA_IMG_VALID)) {
        drawFrame("otadata failed");
        delay(400);
        exitToHome();
        return;
    }
    if (esp_ota_set_boot_partition(run) != ESP_OK) {
        drawFrame("Boot switch failed");
        delay(400);
        exitToHome();
        return;
    }
    nvsSetFlag(m5os::gateway::kLaunchPendingKey, true);
    nvsSetFlag(m5os::gateway::kAppSessionKey, true);
    nvsSetFlag(m5os::gateway::kSessionExitKey, false);
    m5os::gateway::setStagedBootHandoff();
    esp_restart();
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);

    nvsSetFlag(m5os::gateway::kGatewayActiveKey, true);

    const unsigned long uiStart = millis();
    const unsigned long autoLaunchAt = uiStart + m5os::gateway::kAutoLaunchMs;
    drawFrame("Ready — Enter or wait");
    unsigned long escHoldStart = 0;
    int lastCountdown = -1;
    bool autoLaunchDone = false;

    while (true) {
        M5Cardputer.update();

        if (m5os::keyboardBackJustPressed()) {
            drawFrame("Returning to M5 OS...");
            M5Cardputer.update();
            exitToHome();
        }

        const unsigned long now = millis();

        if (m5os::keyboardBackHeld()) {
            if (escHoldStart == 0) escHoldStart = now;
            if (now - escHoldStart >= m5os::gateway::kEscHoldMs) {
                drawFrame("Returning to M5 OS...");
                M5Cardputer.update();
                exitToHome();
            }
        } else {
            escHoldStart = 0;
        }

        if (!autoLaunchDone && now >= autoLaunchAt) {
            autoLaunchDone = true;
            drawFrame("Launching app...");
            M5Cardputer.update();
            launchRunSlot();
        } else if (now >= uiStart + m5os::gateway::kMinGatewayUiMs &&
                   m5os::keyboardEnterJustPressed()) {
            drawFrame("Launching app...");
            M5Cardputer.update();
            launchRunSlot();
        }

        const int countdownSec =
            now < autoLaunchAt ? static_cast<int>((autoLaunchAt - now + 999) / 1000) : 0;
        if (countdownSec != lastCountdown) {
            lastCountdown = countdownSec;
            drawFrame(now < autoLaunchAt ? "Enter=launch ESC=M5 OS" : "Launching...", countdownSec);
        }
        delay(20);
    }
}

void loop() {}
