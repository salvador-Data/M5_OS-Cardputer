#include "ui_display.h"

namespace m5os::ui {

static Theme gTheme;

Theme& theme() { return gTheme; }

void setThemePreset(int preset) {
    switch (preset) {
        case 1:
            gTheme.primary = 0x07E0;
            gTheme.secondary = 0x0320;
            break;
        case 2:
            gTheme.primary = 0xF800;
            gTheme.secondary = 0x7800;
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

void introSplash() {
    auto& d = m5os::lcd();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(2);
    d.setTextColor(gTheme.primary, TFT_BLACK);
    d.setCursor(20, 40);
    d.println("M5 OS");
    d.setTextSize(1);
    d.setCursor(10, 70);
    d.println("Cardputer Edition");
    d.setCursor(10, 90);
    d.println("salvador-Data / Hacker Planet");
    delay(2000);
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
    constexpr int kVisible = 8;

    while (true) {
        drawHeader(title);
        if (index < scroll) scroll = index;
        if (index >= scroll + kVisible) scroll = index - kVisible + 1;

        for (int row = 0; row < kVisible; ++row) {
            const int i = scroll + row;
            if (i >= static_cast<int>(items.size())) break;
            const int y = 24 + row * 14;
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
        delay(80);
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
        delay(80);
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
    d.println("4. Flash M5_OS .bin");
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 96);
    d.println("docs.m5stack.com/m5burner");
    d.setCursor(4, 118);
    d.print("Any key back");
    while (true) {
        m5os::update();
        Buttons keys = m5os::readButtons();
        if (keys.back || keys.ok) return;
        delay(80);
    }
}

}  // namespace m5os::ui
