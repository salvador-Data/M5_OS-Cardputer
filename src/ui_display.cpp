#include "ui_display.h"

#include "power_manager.h"

#include <M5Unified.h>

namespace m5os::ui {

static Theme gTheme;
static int gThemePreset = 3;
static unsigned long gBootStartMs = 0;

namespace {

uint16_t lerp565(uint16_t a, uint16_t b, uint8_t t) {
    const uint8_t ta = 255 - t;
    const uint8_t ar = (a >> 11) & 0x1F;
    const uint8_t ag = (a >> 5) & 0x3F;
    const uint8_t ab = a & 0x1F;
    const uint8_t br = (b >> 11) & 0x1F;
    const uint8_t bg = (b >> 5) & 0x3F;
    const uint8_t bb = b & 0x1F;
    const uint8_t r = (ar * ta + br * t) >> 8;
    const uint8_t g = (ag * ta + bg * t) >> 8;
    const uint8_t bl = (ab * ta + bb * t) >> 8;
    return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

bool color565IsDark(uint16_t c) {
    const int r = (c >> 11) & 0x1F;
    const int g = (c >> 5) & 0x3F;
    const int b = c & 0x1F;
    return (r * 8 + g * 4 + b * 8) < 80;
}

uint16_t themeFieldBg() {
    return lerp565(TFT_BLACK, gTheme.primary, 90);
}

uint16_t themeFieldText() {
    if (!color565IsDark(gTheme.primary)) return gTheme.primary;
    if (!color565IsDark(gTheme.secondary)) return gTheme.secondary;
    return TFT_WHITE;
}

uint16_t themeLabelOnBlack() {
    if (!color565IsDark(gTheme.primary)) return gTheme.primary;
    if (!color565IsDark(gTheme.secondary)) return gTheme.secondary;
    return TFT_WHITE;
}

uint16_t themeHintOnBlack() {
    if (!color565IsDark(gTheme.secondary)) return gTheme.secondary;
    return lerp565(gTheme.primary, TFT_WHITE, 160);
}

constexpr int kPassCharW = 6;
constexpr int kPassPrefixChars = 6;  // "Pass: "

size_t passwordVisibleChars(int fieldInnerW) {
    const int starAreaW = fieldInnerW - kPassPrefixChars * kPassCharW;
    return static_cast<size_t>(max(1, starAreaW / kPassCharW));
}

size_t passwordScrollOffset(size_t length, size_t visible) {
    if (length <= visible) return 0;
    return length - visible;
}

void drawPasswordFrame(const char* title, size_t length, size_t scrollOffset, bool fullFrame) {
    auto& d = m5os::lcd();
    if (fullFrame) {
        drawHeader(title);
    } else {
        d.fillRect(0, 20, d.width(), 72, TFT_BLACK);
    }

    // Fixed contrast — theme lerp can wash out field/text (all-white screen).
    const uint16_t fieldBg = TFT_DARKGREY;
    const uint16_t fieldFg = TFT_WHITE;
    const uint16_t labelFg = TFT_WHITE;
    const uint16_t hintFg = TFT_DARKGREY;

    d.setTextColor(labelFg, TFT_BLACK);
    d.setCursor(4, 24);
    d.print("Password:");

    const int fieldX = 4;
    const int fieldY = 38;
    const int fieldW = d.width() - 8;
    const int fieldH = 18;
    const int textX = 8;
    const int textY = fieldY + 2;
    const size_t visible = passwordVisibleChars(fieldW - (textX - fieldX));
    const size_t showStart = scrollOffset;
    const size_t showEnd = min(length, scrollOffset + visible);

    d.fillRect(fieldX, fieldY - 2, fieldW, fieldH, fieldBg);

    if (scrollOffset > 0) {
        d.setTextColor(hintFg, fieldBg);
        d.setCursor(fieldX + 2, textY);
        d.print('<');
    }
    if (length > scrollOffset + visible) {
        d.setTextColor(hintFg, fieldBg);
        d.setCursor(fieldX + fieldW - 10, textY);
        d.print('>');
    }

    d.setTextColor(fieldFg, fieldBg);
    d.setCursor(textX, textY);
    d.print("Pass: ");
    for (size_t i = showStart; i < showEnd; ++i) d.print('*');

    if (length == showEnd) {
        const int cursorX = textX + kPassPrefixChars * kPassCharW + static_cast<int>(showEnd - showStart) * kPassCharW;
        d.drawFastVLine(cursorX, textY - 1, 10, fieldFg);
    }

    if (length > visible) {
        const int barX = fieldX + 4;
        const int barY = fieldY + fieldH - 1;
        const int barW = fieldW - 8;
        d.drawRect(barX, barY, barW, 3, hintFg);
        const size_t maxScroll = length - visible;
        const int thumbW = max(4, static_cast<int>(barW * visible / length));
        const int travel = barW - thumbW;
        const int thumbX = barX + (maxScroll > 0 ? static_cast<int>(travel * scrollOffset / maxScroll) : 0);
        d.fillRect(thumbX, barY + 1, thumbW, 1, gTheme.primary);
    }

    d.setTextColor(hintFg, TFT_BLACK);
    d.setCursor(4, 66);
    d.print("Enter save  ` cancel");
}

void playBootChime() {
    if (!power::allowBootChime()) return;
    auto& spk = M5.Speaker;
    if (!spk.isEnabled()) spk.begin();
    const int notes[] = {392, 494, 587, 784};
    for (int freq : notes) {
        spk.tone(freq, 70);
        delay(85);
    }
    spk.stop();
}

const char* stageLabel(BootStage stage) {
    switch (stage) {
        case BootStage::MountSd:
            return "Mount SD";
        case BootStage::LoadManifest:
            return "Load manifest";
        case BootStage::GcScan:
            return "GC quick scan";
        case BootStage::Ready:
            return "Ready";
        default:
            return "";
    }
}

int stagePercent(BootStage stage) {
    switch (stage) {
        case BootStage::MountSd:
            return 20;
        case BootStage::LoadManifest:
            return 55;
        case BootStage::GcScan:
            return 80;
        case BootStage::Ready:
            return 100;
        default:
            return 0;
    }
}

void drawBootFrame(int percent, const char* label, const String& detail, uint8_t pulse) {
    auto& d = m5os::lcd();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(2);
    d.setTextColor(gTheme.primary, TFT_BLACK);
    d.setCursor(14, 18);
    d.println("M5 OS");
    d.setTextSize(1);
    d.setTextColor(gTheme.secondary, TFT_BLACK);
    d.setCursor(10, 42);
    d.println("Hacker Planet / WhiteHat CyberSec");
    d.setCursor(10, 56);
    d.println("salvador-Data");

    const int barX = 10;
    const int barY = 78;
    const int barW = d.width() - 20;
    const int barH = 10;
    d.drawRect(barX, barY, barW, barH, gTheme.secondary);
    const int fillW = max(2, (barW - 2) * percent / 100);
    const uint16_t fillColor = lerp565(gTheme.primary, gTheme.secondary, pulse);
    d.fillRect(barX + 1, barY + 1, fillW, barH - 2, fillColor);

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(10, 96);
    d.printf("%s", label);
    if (detail.length()) {
        d.setCursor(10, 110);
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.println(detail);
    }

    d.drawFastHLine(0, d.height() - 14, d.width(), gTheme.primary);
    d.setCursor(6, d.height() - 12);
    d.setTextColor(gTheme.secondary, TFT_BLACK);
    d.print("Authorized lab use only");
}

}  // namespace

Theme& theme() { return gTheme; }

int getThemePreset() { return gThemePreset; }

void setThemePreset(int preset) {
    if (preset < 0) preset = 0;
    if (preset > 3) preset = 3;
    gThemePreset = preset;
    switch (preset) {
        case 1:
            gTheme.primary = 0x07E0;
            gTheme.secondary = 0x0320;
            break;
        case 2:
            gTheme.primary = 0xF800;
            gTheme.secondary = 0x7800;
            break;
        case 3:
            gTheme.primary = 0x05A1;
            gTheme.secondary = 0xCE9F;
            break;
        default:
            gTheme.primary = 0xB6DF;
            gTheme.secondary = 0x0083;
            break;
    }
}

void drawHeader(const char* title) {
    auto& d = m5os::lcd();
    d.fillScreen(TFT_BLACK);
    d.setTextColor(gTheme.primary, TFT_BLACK);
    d.setCursor(4, 4);
    d.printf("%s", title);
    power::drawStatusBar(d);
    d.drawFastHLine(0, 18, d.width(), gTheme.secondary);
}

void showMessage(const char* title, const String& body, uint16_t color, unsigned long holdMs) {
    auto& d = m5os::lcd();
    d.fillScreen(TFT_BLACK);
    d.setTextColor(color, TFT_BLACK);
    d.setCursor(4, 8);
    d.println(title);
    d.setCursor(4, 28);
    d.setTextColor(gTheme.secondary, TFT_BLACK);
    d.println(body);
    delay(holdMs);
}

void bootIntroBegin() {
    setThemePreset(3);
    gBootStartMs = millis();
    drawBootFrame(5, "Booting", "", 0);
    playBootChime();
}

void bootIntroStage(BootStage stage, const String& detail) {
    const int target = stagePercent(stage);
    drawBootFrame(target, stageLabel(stage), detail, 0);
}

void bootIntroFinish() {
    bootIntroStage(BootStage::Ready, "");
    const unsigned long elapsed = millis() - gBootStartMs;
    const unsigned long minHold = power::isSaving() ? 900 : 2200;
    if (elapsed < minHold) delay(minHold - elapsed);
}

void introSplash() {
    bootIntroBegin();
    bootIntroStage(BootStage::Ready, "");
    bootIntroFinish();
}

Buttons readButtonsExtended() {
    Buttons b = m5os::readButtons();
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return b;
    auto status = M5Cardputer.Keyboard.keysState();
    for (auto key : status.word) {
        if (key == 'h' || key == 'H' || key == '?') b.help = true;
        if (key == 'e' || key == 'E') b.exportKey = true;
    }
    return b;
}

int selectFromList(const std::vector<String>& items, const char* title, int startIndex) {
    if (items.empty()) {
        showMessage(title, "No items");
        return -1;
    }
    int index = startIndex;
    if (index < 0 || index >= static_cast<int>(items.size())) index = 0;
    int scroll = 0;
    int lastIndex = -1;
    int lastScroll = -1;
    constexpr int kVisible = 8;

    auto redrawList = [&]() {
        drawHeader(title);
        if (index < scroll) scroll = index;
        if (index >= scroll + kVisible) scroll = index - kVisible + 1;

        for (int row = 0; row < kVisible; ++row) {
            const int i = scroll + row;
            const int y = 24 + row * 14;
            m5os::lcd().fillRect(4, y - 2, m5os::lcd().width() - 8, 14, TFT_BLACK);
            if (i >= static_cast<int>(items.size())) continue;
            m5os::lcd().setCursor(8, y);
            if (i == index) {
                m5os::lcd().setTextColor(gTheme.primary, gTheme.secondary);
                m5os::lcd().printf("> %s", items[i].c_str());
            } else {
                m5os::lcd().setTextColor(gTheme.secondary, TFT_BLACK);
                m5os::lcd().printf("  %s", items[i].c_str());
            }
        }
        m5os::lcd().setTextColor(TFT_DARKGREY, TFT_BLACK);
        m5os::lcd().setCursor(4, 118);
        m5os::lcd().print("h help  ` back");
        lastIndex = index;
        lastScroll = scroll;
    };

    redrawList();

    while (true) {
        if (index != lastIndex || scroll != lastScroll) redrawList();

        m5os::update();
        Buttons keys = readButtonsExtended();
        if (keys.help) {
            drawHelpOverlay();
            continue;
        }
        if (keys.up) index = (index > 0) ? index - 1 : static_cast<int>(items.size()) - 1;
        if (keys.down) index = (index + 1) % static_cast<int>(items.size());
        if (keys.ok) return index;
        if (keys.back) return -1;
        delay(power::uiLoopDelayMs());
    }
}

void drawHelpOverlay() {
    drawHeader("Keyboard shortcuts");
    auto& d = m5os::lcd();
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 24);
    d.println(";/. w/s  navigate");
    d.println("Enter     select");
    d.println("h / ?     this help");
    d.println("e         export catalog serial");
    d.println("`         back");
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 110);
    d.print("Authorized lab only");
    while (true) {
        m5os::update();
        Buttons keys = readButtonsExtended();
        if (keys.back || keys.ok || keys.help) return;
        delay(power::uiLoopDelayMs());
    }
}

void drawBurnerHelp() {
    drawHeader("M5Burner / recovery");
    auto& d = m5os::lcd();
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 24);
    d.println("1. USB-C to PC");
    d.println("2. M5Burner app");
    d.println("3. Cardputer target");
    d.println("4. Flash M5 OS base .bin");
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 96);
    d.println("Apps stay on SD /apps/");
    d.setCursor(4, 118);
    d.print("Any key back");
    while (true) {
        m5os::update();
        Buttons keys = m5os::readButtons();
        if (keys.back || keys.ok) return;
        delay(power::uiLoopDelayMs());
    }
}

bool promptPassword(char* out, size_t outLen, const char* title) {
    if (!out || outLen < 2) return false;
    out[0] = '\0';
    size_t length = 0;
    size_t scrollOffset = 0;
    size_t lastDrawToken = SIZE_MAX;
    bool needFullFrame = true;

    auto drawToken = [&]() -> size_t { return (length << 16) | (scrollOffset & 0xFFFF); };

    while (true) {
        const int fieldInnerW = m5os::lcd().width() - 12;
        const size_t visible = passwordVisibleChars(fieldInnerW);
        scrollOffset = passwordScrollOffset(length, visible);
        const size_t token = drawToken();
        if (token != lastDrawToken || needFullFrame) {
            drawPasswordFrame(title, length, scrollOffset, needFullFrame);
            lastDrawToken = token;
            needFullFrame = false;
        }

        m5os::update();
        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            delay(power::uiLoopDelayMs());
            continue;
        }

        const auto status = M5Cardputer.Keyboard.keysState();
        if (status.enter) {
            out[length] = '\0';
            return true;
        }
        if (status.del && length > 0) {
            length--;
            out[length] = '\0';
            needFullFrame = true;
            delay(power::uiLoopDelayMs());
            continue;
        }
        for (auto key : status.word) {
            if (key == '`' || key == 27) {
                out[0] = '\0';
                return false;
            }
            if (key == '\n' || key == '\r') {
                out[length] = '\0';
                return true;
            }
            if (key == '\b' || key == 127) {
                if (length > 0) {
                    length--;
                    out[length] = '\0';
                }
                needFullFrame = true;
                continue;
            }
            if (length < outLen - 1 && key >= 32 && key <= 126) {
                out[length++] = static_cast<char>(key);
                out[length] = '\0';
                needFullFrame = true;
            }
        }
        delay(power::uiLoopDelayMs());
    }
}

}  // namespace m5os::ui
