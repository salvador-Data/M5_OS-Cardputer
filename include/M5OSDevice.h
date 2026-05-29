#pragma once

#include <M5Cardputer.h>

#include "power_manager.h"

namespace m5os {

inline void begin() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

inline void update() {
    M5Cardputer.update();
    power::tick();
}


inline M5GFX& lcd() { return M5Cardputer.Display; }

inline int freeHeap() { return ESP.getFreeHeap(); }

struct Buttons {
    bool up = false;
    bool down = false;
    bool ok = false;
    bool back = false;
    bool help = false;
    bool exportKey = false;
};

constexpr uint8_t kHidEscape = 0x29;

inline bool keyboardBackHeld() {
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

inline bool keyboardBackJustPressed() {
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return false;
    return keyboardBackHeld();
}

/** Drop ESC/` held from a prior screen (e.g. main menu back opens switcher). */
inline void keyboardDrainBack() {
    for (int i = 0; i < 24; ++i) {
        update();
        if (!M5Cardputer.Keyboard.isChange() && !keyboardBackHeld()) break;
        delay(5);
    }
    while (keyboardBackHeld()) {
        update();
        delay(10);
    }
}

inline Buttons readButtons() {
    Buttons b;
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return b;
    }
    const auto status = M5Cardputer.Keyboard.keysState();
    if (status.enter || status.space) b.ok = true;
    if (keyboardBackHeld()) b.back = true;
    for (auto key : status.word) {
        if (key == ';' || key == 'w' || key == 'W') b.up = true;
        if (key == '.' || key == 's' || key == 'S') b.down = true;
        if (key == '\n' || key == '\r') b.ok = true;
    }
    return b;
}

}  // namespace m5os
