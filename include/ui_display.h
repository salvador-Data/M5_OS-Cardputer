#pragma once

#include <M5Cardputer.h>
#include <vector>

#include "M5OSDevice.h"

namespace m5os::ui {

struct Theme {
    uint16_t primary = 0xB6DF;
    uint16_t secondary = 0x0083;
};

Theme& theme();
void setThemePreset(int preset);

void drawHeader(const char* title);
void showMessage(const char* title, const String& body, uint16_t color = TFT_WHITE,
                 unsigned long holdMs = 1600);
void introSplash();
void drawHelpOverlay();
void drawBurnerHelp();
int selectFromList(const std::vector<String>& items, const char* title, int startIndex = 0);
Buttons readButtonsExtended();

}  // namespace m5os::ui
