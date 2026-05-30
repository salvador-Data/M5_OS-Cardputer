#pragma once

#include <M5Cardputer.h>

namespace m5os {

constexpr uint8_t kHidEscape = 0x29;
constexpr uint8_t kHidEnter = 0x28;

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

inline bool keyboardEnterHeld() {
    if (!M5Cardputer.Keyboard.isPressed()) return false;
    const auto status = M5Cardputer.Keyboard.keysState();
    if (status.enter) return true;
    for (auto key : status.word) {
        if (key == '\n' || key == '\r') return true;
    }
    for (uint8_t hid : status.hid_keys) {
        if (hid == kHidEnter) return true;
    }
    return false;
}

/** Enter confirm — keysState().enter like WiFi password (no isPressed gate). */
inline bool keyboardEnterJustPressed() {
    if (!M5Cardputer.Keyboard.isChange()) return false;
    const auto status = M5Cardputer.Keyboard.keysState();
    if (status.enter) return true;
    for (auto key : status.word) {
        if (key == '\n' || key == '\r') return true;
    }
    for (uint8_t hid : status.hid_keys) {
        if (hid == kHidEnter) return true;
    }
    return false;
}

}  // namespace m5os
