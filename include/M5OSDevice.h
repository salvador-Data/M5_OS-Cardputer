#pragma once

#include <M5Cardputer.h>

#include "m5os_keyboard.h"
#include "m5os_watchdog.h"
#include "power_manager.h"

namespace m5os::stamp {
void tick();
}

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
    feedWatchdog();
    stamp::tick();
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

inline Buttons readButtons() {
    Buttons b;
    if (!M5Cardputer.Keyboard.isChange()) return b;
    if (keyboardEnterJustPressed()) b.ok = true;
    if (keyboardBackJustPressed()) b.back = true;
    if (!M5Cardputer.Keyboard.isPressed()) return b;
    const auto status = M5Cardputer.Keyboard.keysState();
    if (status.space) b.ok = true;
    for (auto key : status.word) {
        if (key == ';' || key == 'w' || key == 'W') b.up = true;
        if (key == '.' || key == 's' || key == 'S') b.down = true;
    }
    return b;
}

}  // namespace m5os
