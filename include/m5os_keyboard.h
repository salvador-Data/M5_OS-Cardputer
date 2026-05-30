#pragma once

#include <M5Cardputer.h>

namespace m5os {

constexpr uint8_t kHidEscape = 0x29;
constexpr uint8_t kHidGrave = 0x35;
constexpr uint8_t kHidEnter = 0x28;

inline bool keyboardBackHeld() {
    if (!M5Cardputer.Keyboard.isPressed()) return false;
    const auto status = M5Cardputer.Keyboard.keysState();
    for (uint8_t hid : status.hid_keys) {
        if (hid == kHidEscape || hid == kHidGrave) return true;
    }
    for (auto key : status.word) {
        if (key == '`' || key == '~' || key == 27) return true;
    }
    if (M5Cardputer.Keyboard.isKeyPressed('`') || M5Cardputer.Keyboard.isKeyPressed('~')) {
        return true;
    }
    return false;
}

/** Rising edge for ESC/` — avoids isChange() missing key swaps at constant key count. */
inline bool keyboardBackEdge(bool& wasHeld) {
    const bool held = keyboardBackHeld();
    const bool edge = held && !wasHeld;
    wasHeld = held;
    return edge;
}

inline bool keyboardBackJustPressed() {
    static bool wasHeld = false;
    return keyboardBackEdge(wasHeld);
}

inline void keyboardDrainBack() {
    bool wasHeld = false;
    for (int i = 0; i < 24; ++i) {
        M5Cardputer.update();
        keyboardBackEdge(wasHeld);
        if (!M5Cardputer.Keyboard.isChange() && !keyboardBackHeld()) break;
        delay(5);
    }
    while (keyboardBackHeld()) {
        M5Cardputer.update();
        delay(10);
    }
    wasHeld = false;
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

inline void keyboardDrainEnter() {
    for (int i = 0; i < 24; ++i) {
        M5Cardputer.update();
        if (!M5Cardputer.Keyboard.isChange() && !keyboardEnterHeld()) break;
        delay(5);
    }
    while (keyboardEnterHeld()) {
        M5Cardputer.update();
        delay(10);
    }
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

inline bool keyboardTabHeld() {
    if (!M5Cardputer.Keyboard.isPressed()) return false;
    return M5Cardputer.Keyboard.keysState().tab;
}

inline bool keyboardTabJustPressed() {
    if (!M5Cardputer.Keyboard.isChange()) return false;
    return M5Cardputer.Keyboard.keysState().tab;
}

inline void keyboardDrainTab() {
    for (int i = 0; i < 24; ++i) {
        M5Cardputer.update();
        if (!M5Cardputer.Keyboard.isChange() && !keyboardTabHeld()) break;
        delay(5);
    }
    while (keyboardTabHeld()) {
        M5Cardputer.update();
        delay(10);
    }
}

constexpr uint8_t kHidBackspace = 0x2a;

inline bool keyboardDelHeld() {
    if (!M5Cardputer.Keyboard.isPressed()) return false;
    const auto status = M5Cardputer.Keyboard.keysState();
    if (status.del) return true;
    for (uint8_t hid : status.hid_keys) {
        if (hid == kHidBackspace) return true;
    }
    return false;
}

inline bool keyboardDelJustPressed() {
    if (!M5Cardputer.Keyboard.isChange()) return false;
    return keyboardDelHeld();
}

inline void keyboardDrainDel() {
    for (int i = 0; i < 24; ++i) {
        M5Cardputer.update();
        if (!M5Cardputer.Keyboard.isChange() && !keyboardDelHeld()) break;
        delay(5);
    }
    while (keyboardDelHeld()) {
        M5Cardputer.update();
        delay(10);
    }
}

}  // namespace m5os
