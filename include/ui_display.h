#pragma once

#include <M5Cardputer.h>
#include <vector>

#include "M5OSDevice.h"

namespace m5os::ui {

struct Theme {
    uint16_t primary = 0x07E0;    // Hacker Green default (matrix bright)
    uint16_t secondary = 0x0660;  // dim green readable on black
};

enum class BootStage { MountSd, LoadManifest, GcScan, Ready };

enum class PasswordPromptResult { Cancelled, Confirmed, ChangeNetwork };

Theme& theme();
void setThemePreset(int preset);
int getThemePreset();

void drawHeader(const char* title);
/** Progress bar for M5Burner flash / OTA / SD load (bytes + percent). */
void showFlashProgress(int percent, const char* label, const String& detail = "");
void showMessage(const char* title, const String& body, uint16_t color = TFT_WHITE,
                 unsigned long holdMs = 1600);
void bootIntroBegin();
void bootIntroStage(BootStage stage, const String& detail = "");
void bootIntroFinish();
void introSplash();  // legacy single-frame splash
void drawHelpOverlay();
void drawBurnerHelp();
int selectFromList(const std::vector<String>& items, const char* title, int startIndex = 0,
                   const char* backConfirmTitle = nullptr, const char* backConfirmBody = nullptr);
/** Lowercase y/n confirm — y=yes, n/`/ESC=no. Enter/space ignored. */
bool promptYesNo(const char* title, const char* question);
/** Keyboard password entry — never logged to serial. Tab = pick another AP. */
PasswordPromptResult promptPassword(char* out, size_t outLen, const char* title);
Buttons readButtonsExtended();

}  // namespace m5os::ui
