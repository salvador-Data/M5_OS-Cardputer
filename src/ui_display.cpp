#include "ui_display.h"

#include "m5os_vfs.h"
#include "power_manager.h"

#include <M5Unified.h>
#include <SD.h>

#include <cmath>

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

    // High-contrast field — never theme-derived (ST7789 wash-out on preset 3).
    const uint16_t fieldBg = 0x3186;  // dark blue-grey
    const uint16_t fieldFg = TFT_WHITE;
    const uint16_t labelFg = TFT_WHITE;
    const uint16_t hintFg = TFT_CYAN;

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
    d.print("Del erase  Tab AP  Enter save  ` cancel");
    d.setCursor(4, 78);
    d.printf("(%u chars)", static_cast<unsigned>(length));
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

// Optional SD boot WAV — user-provided, licensed copy only (not shipped in firmware).
// Search order: /home/default/boot/, /system/boot/, /boot/ on microSD.
static const char* kBootWavPaths[] = {
    "/home/default/boot/mr_roboto.wav",
    "/system/boot/mr_roboto.wav",
    "/boot/mr_roboto.wav",
};

struct __attribute__((packed)) BootWavHeader {
    char riff[4];
    uint32_t chunk_size;
    char waveFmt[8];
    uint32_t fmt_chunk_size;
    uint16_t audiofmt;
    uint16_t channel;
    uint32_t sample_rate;
    uint32_t byte_per_sec;
    uint16_t block_size;
    uint16_t bit_per_sample;
};

struct __attribute__((packed)) BootWavSubChunk {
    char identifier[4];
    uint32_t chunk_size;
};

class BootSdWavStream {
public:
    bool begin(const char* path) {
        if (!path || !m5os::vfs::isMounted()) return false;
        file_ = SD.open(path, FILE_READ);
        if (!file_) return false;

        BootWavHeader hdr{};
        if (file_.read(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr)) != sizeof(hdr)) {
            closeFile();
            return false;
        }
        if (memcmp(hdr.riff, "RIFF", 4) || memcmp(hdr.waveFmt, "WAVEfmt ", 8)) {
            closeFile();
            return false;
        }
        if (hdr.audiofmt != 1 || hdr.bit_per_sample < 8 || hdr.bit_per_sample > 16) {
            closeFile();
            return false;
        }
        if (hdr.channel == 0 || hdr.channel > 2) {
            closeFile();
            return false;
        }

        flg16bit_ = (hdr.bit_per_sample >> 4) != 0;
        stereo_ = hdr.channel > 1;
        sampleRate_ = hdr.sample_rate;

        file_.seek(offsetof(BootWavHeader, audiofmt) + hdr.fmt_chunk_size);
        BootWavSubChunk sub{};
        if (file_.read(reinterpret_cast<uint8_t*>(&sub), 8) != 8) {
            closeFile();
            return false;
        }
        while (memcmp(sub.identifier, "data", 4)) {
            if (!file_.seek(sub.chunk_size, SeekCur)) {
                closeFile();
                return false;
            }
            if (file_.read(reinterpret_cast<uint8_t*>(&sub), 8) != 8) {
                closeFile();
                return false;
            }
        }

        dataRemaining_ = static_cast<int32_t>(sub.chunk_size);
        active_ = dataRemaining_ > 0;
        firstChunk_ = true;
        if (!active_) {
            closeFile();
            return false;
        }

        auto& spk = M5.Speaker;
        if (!spk.isEnabled()) spk.begin();
        spk.setChannelVolume(static_cast<uint8_t>(channel_), 200);
        return true;
    }

    void pump() {
        if (!active_ || dataRemaining_ <= 0) return;

        auto& spk = M5.Speaker;
        const size_t playState = spk.isPlaying(static_cast<uint8_t>(channel_));
        if (!firstChunk_ && playState >= 2) return;

        size_t len = min(static_cast<size_t>(dataRemaining_), kBufSize);
        len = file_.read(buffer_, len);
        if (len == 0) {
            dataRemaining_ = 0;
            active_ = false;
            closeFile();
            return;
        }
        dataRemaining_ -= static_cast<int32_t>(len);

        bool ok = false;
        if (flg16bit_) {
            ok = spk.playRaw(reinterpret_cast<const int16_t*>(buffer_), len >> 1, sampleRate_, stereo_,
                             1, channel_, firstChunk_);
        } else {
            ok = spk.playRaw(buffer_, len, sampleRate_, stereo_, 1, channel_, firstChunk_);
        }
        if (!ok) {
            active_ = false;
            closeFile();
            return;
        }
        firstChunk_ = false;
        if (dataRemaining_ <= 0) {
            active_ = false;
            closeFile();
        }
    }

    void stopPlayback() {
        active_ = false;
        dataRemaining_ = 0;
        firstChunk_ = true;
        closeFile();
        M5.Speaker.stop(static_cast<uint8_t>(channel_));
    }

private:
    void closeFile() {
        if (file_) file_.close();
    }

    File file_;
    bool active_ = false;
    bool firstChunk_ = true;
    bool flg16bit_ = false;
    bool stereo_ = false;
    uint32_t sampleRate_ = 44100;
    int32_t dataRemaining_ = 0;
    int channel_ = 0;
    static constexpr size_t kBufSize = 1024;
    uint8_t buffer_[kBufSize]{};
};

const char* resolveBootWavPath() {
    if (!m5os::vfs::isMounted()) return nullptr;
    for (const char* path : kBootWavPaths) {
        if (SD.exists(path)) return path;
    }
    return nullptr;
}

struct BootSynthNote {
    uint8_t frame;
    uint16_t freq;
    uint8_t durationMs;
};

// Original staccato minor synth — robotic vibe, not a Styx recording.
constexpr BootSynthNote kRobotoSynthTheme[] = {
    {0, 220, 32},   {2, 277, 30},   {4, 330, 30},   {6, 392, 32},   {8, 440, 32},
    {10, 523, 30},  {12, 587, 30},  {14, 659, 32},  {16, 587, 28},  {17, 523, 28},
    {18, 440, 28},  {19, 392, 28},  {21, 330, 32},  {23, 294, 32},  {25, 262, 35},
    {28, 440, 28},  {29, 523, 28},  {30, 659, 28},  {31, 784, 32},  {33, 659, 28},
    {34, 523, 28},  {35, 440, 28},  {37, 392, 32},  {39, 440, 32},  {41, 523, 35},
    {44, 587, 32},  {46, 659, 32},  {48, 784, 38},  {51, 659, 32},  {53, 523, 32},
    {55, 440, 35},  {57, 392, 38},  {59, 330, 42},
};

unsigned long playRobotoBootThemeFrame(int frame) {
    if (!power::allowBootChime()) return 0;

    for (const BootSynthNote& note : kRobotoSynthTheme) {
        if (note.frame != static_cast<uint8_t>(frame)) continue;
        auto& spk = M5.Speaker;
        if (!spk.isEnabled()) spk.begin();
        const uint32_t dur = min(static_cast<uint32_t>(note.durationMs), 34U);
        spk.tone(static_cast<float>(note.freq), dur, 0, frame == 0);
        return dur;
    }
    return 0;
}

unsigned long introFrameDelayMs() { return power::allowBootChime() ? 58UL : 28UL; }

void feedBootWavStream(BootSdWavStream& wav) {
    for (int i = 0; i < 3; ++i) {
        wav.pump();
        if (M5.Speaker.isPlaying(0) >= 2) break;
    }
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

void drawBootFrame(lgfx::LovyanGFX& d, int percent, const char* label, const String& detail,
                   uint8_t pulse);

int smoothstep256(int t, int duration) {
    if (duration <= 0) return 256;
    if (t <= 0) return 0;
    if (t >= duration) return 256;
    const int x = (t << 8) / duration;
    return (x * x * (768 - (x << 1))) >> 16;
}

uint8_t introPulse(int frame) {
    return static_cast<uint8_t>(127.0f + 127.0f * sinf(static_cast<float>(frame) * 0.22f));
}

int introMaskRadius(int frame) {
    constexpr int kMinR = 10;
    constexpr int kMaxR = 26;
    constexpr int kGrowFrames = 32;
    if (frame <= 0) return kMinR;
    const int grow = smoothstep256(frame, kGrowFrames);
    return kMinR + (kMaxR - kMinR) * grow / 256;
}

uint8_t introTextAlpha(int frame, int startFrame, int fadeFrames) {
    if (frame < startFrame) return 0;
    const int t = frame - startFrame;
    if (t >= fadeFrames) return 255;
    return static_cast<uint8_t>((t * 255) / fadeFrames);
}

template <typename Display>
void drawGuyFawkesMask(Display& d, int cx, int cy, int faceR, uint8_t pulse) {
    const uint16_t ringColor = lerp565(gTheme.primary, gTheme.secondary, pulse);
    const uint16_t faceColor = TFT_WHITE;
    const uint16_t ink = TFT_BLACK;

    const int ringPulse = pulse >> 5;
    for (int ring = 0; ring < 3; ++ring) {
        const int radius = faceR + 10 + ring * 3 + ringPulse;
        const uint16_t fade = lerp565(ringColor, TFT_BLACK, static_cast<uint8_t>(ring * 70));
        d.drawCircle(cx, cy - 1, radius, fade);
    }

    const int faceW = faceR + 1;
    const int faceH = faceR + 5;
    const int faceTop = cy - faceH / 2;

    d.fillEllipse(cx, cy - 2, faceW, faceH, ink);
    d.fillEllipse(cx, cy - 2, max(1, faceW - 1), max(1, faceH - 1), faceColor);

    const int chinTop = cy + faceH / 4;
    const int chinTip = cy + faceH / 2 + faceR / 3;
    d.fillTriangle(cx - faceW + 2, chinTop, cx + faceW - 2, chinTop, cx, chinTip, ink);
    d.fillTriangle(cx - faceW + 4, chinTop + 1, cx + faceW - 4, chinTop + 1, cx, chinTip - 1,
                   faceColor);

    const int browY = faceTop + faceH / 5;
    const int browW = max(3, faceR / 3);
    d.drawFastHLine(cx - faceW / 2, browY, browW, ink);
    d.drawFastHLine(cx + faceW / 2 - browW, browY, browW, ink);
    d.drawPixel(cx - faceW / 2 + 1, browY - 1, ink);
    d.drawPixel(cx + faceW / 2 - 2, browY - 1, ink);

    const int eyeY = cy - faceR / 10;
    const int slitW = max(4, faceR / 3);
    const int slitH = max(2, faceR / 9);
    d.fillRect(cx - faceW + 3, eyeY, slitW, slitH, ink);
    d.fillRect(cx + faceW - slitW - 3, eyeY, slitW, slitH, ink);
    d.drawPixel(cx - faceW + slitW + 2, eyeY - 1, ink);
    d.drawPixel(cx + faceW - slitW - 3, eyeY - 1, ink);

    d.drawLine(cx - faceW + 4, cy + faceR / 12, cx - faceR / 4, cy + faceR / 2, ink);
    d.drawLine(cx + faceW - 4, cy + faceR / 12, cx + faceR / 4, cy + faceR / 2, ink);

    const int stashY = cy + faceR / 5;
    const int stashW = max(3, faceR / 3);
    d.fillRect(cx - stashW - 2, stashY, stashW, 2, ink);
    d.fillRect(cx + 2, stashY, stashW, 2, ink);
    d.drawFastVLine(cx, stashY - 1, 5, ink);

    const int beardTop = stashY + 3;
    d.fillTriangle(cx - faceW / 2 + 2, beardTop, cx + faceW / 2 - 2, beardTop, cx, chinTip - 1,
                   ink);
    d.fillTriangle(cx - faceW / 2 + 4, beardTop + 1, cx + faceW / 2 - 4, beardTop + 1, cx,
                   chinTip - 3, faceColor);
    d.drawFastVLine(cx, beardTop, chinTip - beardTop - 2, ink);

    d.drawEllipse(cx, cy - 2, faceW, faceH, ink);
}

template <typename Display>
void drawIntroFrame(Display& d, int frame, int totalFrames) {
    d.fillScreen(TFT_BLACK);

    const int cx = d.width() / 2;
    const uint8_t pulse = introPulse(frame);
    const int faceR = introMaskRadius(frame);
    drawGuyFawkesMask(d, cx, 48, faceR, pulse);

    const uint8_t titleAlpha = introTextAlpha(frame, 10, 14);
    if (titleAlpha > 0) {
        d.setTextSize(2);
        d.setTextColor(lerp565(TFT_BLACK, gTheme.primary, titleAlpha), TFT_BLACK);
        d.setCursor(max(4, cx - 72), 88);
        d.print("Hacker Planet");
    }

    const uint8_t creditAlpha = introTextAlpha(frame, 22, 14);
    if (creditAlpha > 0) {
        d.setTextSize(1);
        d.setTextColor(lerp565(TFT_BLACK, gTheme.secondary, creditAlpha), TFT_BLACK);
        d.setCursor(max(4, cx - 56), 112);
        d.print("by salvadorData");
    }

    const int barW = d.width() - 20;
    const int progress = min(100, (frame + 1) * 100 / totalFrames);
    d.drawRect(10, 126, barW, 6, gTheme.secondary);
    d.fillRect(11, 127, max(1, (barW - 2) * progress / 100), 4,
               lerp565(gTheme.primary, gTheme.secondary, pulse));
}

void playHackerPlanetIntro() {
    auto& lcd = m5os::lcd();
    constexpr int kFrames = 60;
    lgfx::LGFX_Sprite canvas(&lcd);
    const bool buffered = canvas.createSprite(lcd.width(), lcd.height()) != nullptr;

    BootSdWavStream wav;
    const char* wavPath = power::allowBootChime() ? resolveBootWavPath() : nullptr;
    const bool useWav = wavPath && wav.begin(wavPath);

    for (int frame = 0; frame < kFrames; ++frame) {
        m5os::update();
        if (buffered) {
            drawIntroFrame(canvas, frame, kFrames);
            canvas.pushSprite(0, 0);
        } else {
            drawIntroFrame(lcd, frame, kFrames);
        }

        const unsigned long frameMs = introFrameDelayMs();
        unsigned long spentMs = 0;
        if (useWav) {
            feedBootWavStream(wav);
        } else {
            spentMs = playRobotoBootThemeFrame(frame);
        }
        if (spentMs < frameMs) delay(frameMs - spentMs);
    }

    if (useWav) {
        wav.stopPlayback();
    } else if (power::allowBootChime()) {
        M5.Speaker.stop();
    }

    if (buffered) canvas.deleteSprite();
}

void drawBootFrame(lgfx::LovyanGFX& d, int percent, const char* label, const String& detail,
                   uint8_t pulse) {
    d.fillScreen(TFT_BLACK);
    d.setTextSize(2);
    d.setTextColor(gTheme.primary, TFT_BLACK);
    d.setCursor(14, 18);
    d.println("M5 OS");
    d.setTextSize(1);
    d.setTextColor(gTheme.secondary, TFT_BLACK);
    d.setCursor(10, 42);
    d.println("Hacker Planet LLC");

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

void showBootFrame(int percent, const char* label, const String& detail, uint8_t pulse) {
    auto& lcd = m5os::lcd();
    lgfx::LGFX_Sprite canvas(&lcd);
    if (canvas.createSprite(lcd.width(), lcd.height())) {
        drawBootFrame(canvas, percent, label, detail, pulse);
        canvas.pushSprite(0, 0);
        canvas.deleteSprite();
        return;
    }
    drawBootFrame(lcd, percent, label, detail, pulse);
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
    playHackerPlanetIntro();
    showBootFrame(5, "Booting", "", 0);
}

void bootIntroStage(BootStage stage, const String& detail) {
    const int target = stagePercent(stage);
    showBootFrame(target, stageLabel(stage), detail, 0);
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
    d.println("Or: Flash from M5Burner catalog");
    d.setCursor(4, 110);
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

constexpr uint8_t kHidBackspace = 0x2a;

bool keyboardWantsErase() {
    if (!M5Cardputer.Keyboard.isPressed()) return false;
    const auto status = M5Cardputer.Keyboard.keysState();
    if (status.del) return true;
    for (uint8_t key : status.hid_keys) {
        if (key == kHidBackspace) return true;
    }
    return false;
}

void erasePasswordChar(char* out, size_t& length) {
    if (length > 0) {
        length--;
        out[length] = '\0';
    }
}

PasswordPromptResult promptPassword(char* out, size_t outLen, const char* title) {
    if (!out || outLen < 2) return PasswordPromptResult::Cancelled;
    out[0] = '\0';
    size_t length = 0;
    size_t scrollOffset = 0;
    size_t lastDrawToken = SIZE_MAX;
    bool needFullFrame = true;
    unsigned long lastEraseMs = 0;
    bool eraseHeld = false;

    for (int i = 0; i < 30; ++i) {
        m5os::update();
        if (!M5Cardputer.Keyboard.isPressed()) break;
        delay(10);
    }

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

        if (keyboardWantsErase()) {
            const unsigned long now = millis();
            const unsigned long gap = eraseHeld ? 80UL : 0UL;
            if (now - lastEraseMs >= gap) {
                erasePasswordChar(out, length);
                needFullFrame = true;
                lastEraseMs = now;
                eraseHeld = true;
            }
            delay(20);
            continue;
        }
        eraseHeld = false;

        if (!M5Cardputer.Keyboard.isChange()) {
            delay(power::uiLoopDelayMs());
            continue;
        }

        const auto status = M5Cardputer.Keyboard.keysState();
        if (status.tab) {
            out[0] = '\0';
            return PasswordPromptResult::ChangeNetwork;
        }
        if (status.enter) {
            out[length] = '\0';
            return PasswordPromptResult::Confirmed;
        }
        for (auto key : status.word) {
            if (key == '`' || key == 27) {
                out[0] = '\0';
                return PasswordPromptResult::Cancelled;
            }
            if (key == '\n' || key == '\r') {
                out[length] = '\0';
                return PasswordPromptResult::Confirmed;
            }
            if (key == '\b' || key == 127) {
                erasePasswordChar(out, length);
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
