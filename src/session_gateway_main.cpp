/**

 * M5 OS session gateway — runs from app1 (ota_1).

 * ESC/` → M5 OS home + save prompt; Enter → run slot (app2).

 * Built only with env:m5os-session-gateway (see platformio.ini build_src_filter).

 */

#include "m5os_gateway_shared.h"



#include <M5Cardputer.h>



#include <esp_ota_ops.h>

#include <esp_system.h>

#include <nvs.h>



namespace {



constexpr uint8_t kHidEscape = 0x29;



bool nvsSetFlag(const char* key, bool value) {

    nvs_handle_t handle = 0;

    if (nvs_open(m5os::gateway::kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;

    esp_err_t err = value ? nvs_set_u8(handle, key, 1) : nvs_erase_key(handle, key);

    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;

    if (err == ESP_OK) nvs_commit(handle);

    nvs_close(handle);

    return err == ESP_OK;

}



bool keyboardBackHeld() {

    if (!M5Cardputer.Keyboard.isPressed()) return false;

    const auto status = M5Cardputer.Keyboard.keysState();

    for (uint8_t hid : status.hid_keys) {

        if (hid == kHidEscape) return true;

    }

    for (auto key : status.word) {

        if (key == '`' || key == 27) return true;

    }

    return false;

}



bool keyboardBackJustPressed() {

    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return false;

    return keyboardBackHeld();

}



bool keyboardEnterHeld() {

    if (!M5Cardputer.Keyboard.isPressed()) return false;

    const auto status = M5Cardputer.Keyboard.keysState();

    if (status.enter) return true;

    for (auto key : status.word) {

        if (key == '\n' || key == '\r') return true;

    }

    return false;

}



bool keyboardEnterJustPressed() {

    if (!M5Cardputer.Keyboard.isChange()) return false;

    return keyboardEnterHeld();

}



void drawFrame(const char* statusLine) {

    auto& d = M5Cardputer.Display;

    d.fillScreen(TFT_BLACK);

    d.setTextColor(TFT_CYAN, TFT_BLACK);

    d.setCursor(4, 4);

    d.println("Session gateway");

    d.setTextColor(TFT_WHITE, TFT_BLACK);

    d.setCursor(4, 24);

    d.println("ESC = M5 OS");

    d.setCursor(4, 40);

    d.println("Enter = launch app");

    d.setTextColor(TFT_DARKGREY, TFT_BLACK);

    d.setCursor(4, 56);

    d.println(statusLine);

}



const esp_partition_t* homePartition() {

    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0,

                                    nullptr);

}



const esp_partition_t* runPartition() {

    const esp_partition_t* app2 =

        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_2,

                                 nullptr);

    if (app2) return app2;

    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1,

                                    nullptr);

}



bool partitionHasAppMagic(const esp_partition_t* part) {

    if (!part) return false;

    uint8_t magic = 0;

    return esp_partition_read(part, 0, &magic, 1) == ESP_OK && magic == 0xE9;

}



void exitToHome() {

    nvsSetFlag(m5os::gateway::kGatewayActiveKey, false);

    nvsSetFlag("app_sess", true);

    nvsSetFlag("sess_exit", true);

    const esp_partition_t* home = homePartition();

    if (home) esp_ota_set_boot_partition(home);

    delay(50);

    /* No RTC handoff — bootloader must force M5 OS (app0) on this SW reset. */

    esp_restart();

}



void launchRunSlot() {

    const esp_partition_t* run = runPartition();

    if (!run || !partitionHasAppMagic(run)) {

        drawFrame("No app in run slot");

        delay(1200);

        exitToHome();

        return;

    }

    nvsSetFlag(m5os::gateway::kGatewayActiveKey, false);

    nvsSetFlag("app_sess", true);

    nvsSetFlag("sess_exit", false);

    esp_ota_set_boot_partition(run);

    delay(50);

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

    drawFrame("Waiting...");



    while (true) {

        M5Cardputer.update();



        if (keyboardBackJustPressed()) {

            drawFrame("Returning to M5 OS...");

            M5Cardputer.update();

            exitToHome();

        }



        const unsigned long now = millis();

        if (now >= uiStart + m5os::gateway::kMinGatewayUiMs &&

            (keyboardEnterJustPressed() || now >= autoLaunchAt)) {

            drawFrame("Launching app...");

            M5Cardputer.update();

            launchRunSlot();

        }



        if ((now / 400) % 2 == 0) {

            drawFrame(now < autoLaunchAt ? "ESC or Enter" : "Launching soon...");

        }

        delay(20);

    }

}



void loop() {}

