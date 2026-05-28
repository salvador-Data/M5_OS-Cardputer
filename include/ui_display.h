#pragma once

#include <M5Cardputer.h>
#include <vector>

#include "M5OSDevice.h"

namespace m5os::ui {

struct Theme {
    uint16_t primary = 0x05A1;   // Hacker Planet teal #00b48c
    uint16_t secondary = 0xCE9F;  // magenta #d2a8ff
};

enum class BootStage { MountSd, LoadManifest, GcScan, Ready };

Theme& theme();
void setThemePreset(int preset);

void drawHeader(const char* title);
void showMessage(const char* title, const String& body, uint16_t color = TFT_WHITE,
                 unsigned long holdMs = 1600);
void bootIntroBegin();
void bootIntroStage(BootStage stage, const String& detail = "");
void bootIntroFinish();
void introSplash();  // legacy single-frame splash
void drawHelpOverlay();
void drawBurnerHelp();
int selectFromList(const std::vector<String>& items, const char* title, int startIndex = 0);
/** Keyboard password entry — never logged to serial. Returns false if cancelled. */
bool promptPassword(char* out, size_t outLen, const char* title);
Buttons readButtonsExtended();

}  // namespace m5os::ui
