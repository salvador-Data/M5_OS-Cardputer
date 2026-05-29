#include "stamp_glow.h"

#include "m5os_config.h"
#include "power_manager.h"
#include "ui_display.h"

#include <M5Cardputer.h>
#include <M5Unified.h>
#include <cmath>

namespace m5os::stamp {

namespace {

bool gReady = false;
unsigned long gLastTickMs = 0;

bool isCardputerBoard() {
    const m5::board_t board = M5.getBoard();
    return board == m5::board_t::board_M5Cardputer || board == m5::board_t::board_M5CardputerADV;
}

void rgb565ToRgb888(uint16_t color565, uint8_t& red, uint8_t& green, uint8_t& blue) {
    red = static_cast<uint8_t>(((color565 >> 11) & 0x1F) * 255 / 31);
    green = static_cast<uint8_t>(((color565 >> 5) & 0x3F) * 255 / 63);
    blue = static_cast<uint8_t>((color565 & 0x1F) * 255 / 31);
}

float breatheLevel(unsigned long nowMs) {
    constexpr unsigned long kPeriodMs = 3200;
    const float phase = static_cast<float>(nowMs % kPeriodMs) / static_cast<float>(kPeriodMs);
    return 0.28f + 0.72f * (0.5f + 0.5f * sinf(phase * 6.2831853f));
}

}  // namespace

void begin() {
    if (!isCardputerBoard()) return;

    // RGB LED shares LCD backlight power rail on Cardputer (M5 docs).
    auto& display = M5Cardputer.Display;
    if (display.getBrightness() < 200) display.setBrightness(200);

    if (!M5.Led.begin()) return;
    M5.Led.setBrightness(64);
    gReady = true;
    gLastTickMs = 0;
    tick();
}

void off() {
    if (!gReady) return;
    M5.Led.setAllColor(0, 0, 0);
    gReady = false;
}

void tick() {
    if (!gReady) return;

    const unsigned long now = millis();
    if (now - gLastTickMs < 45) return;
    gLastTickMs = now;

    if (power::isSaving()) {
        M5.Led.setAllColor(0, 0, 0);
        return;
    }

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    rgb565ToRgb888(ui::theme().primary, red, green, blue);

    const float level = breatheLevel(now);
    red = static_cast<uint8_t>(red * level);
    green = static_cast<uint8_t>(green * level);
    blue = static_cast<uint8_t>(blue * level);

    M5.Led.setColor(0, red, green, blue);
}

void applyTheme() {
    gLastTickMs = 0;
    tick();
}

}  // namespace m5os::stamp
