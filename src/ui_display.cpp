#include "ui_display.h"

#include "boot_jokes.h"
#include "m5os_config.h"
#include "m5os_vfs.h"
#include "power_manager.h"
#include "stamp_glow.h"

#include <M5Unified.h>
#include <SD.h>

#include <cmath>
#include <cstring>

namespace m5os::ui {

static Theme gTheme;
static int gThemePreset = kDefaultThemePreset;
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
void drawAngledEyeSlit(Display& d, int x0, int y0, int x1, int y1, uint16_t ink) {
    const int dx = abs(x1 - x0);
    const int dy = abs(y1 - y0);
    const int steps = max(dx, dy);
    if (steps == 0) {
        d.drawPixel(x0, y0, ink);
        return;
    }
    for (int i = 0; i <= steps; ++i) {
        const int x = x0 + (x1 - x0) * i / steps;
        const int y = y0 + (y1 - y0) * i / steps;
        const int midDist = abs(i - steps / 2);
        const int thick = (midDist <= max(1, steps / 5)) ? 3 : (midDist <= max(1, steps / 3) ? 2 : 1);
        const int half = thick / 2;
        for (int t = -half; t <= half + (thick & 1) - 1; ++t) {
            d.drawPixel(x, y + t, ink);
        }
    }
}

template <typename Display>
void drawArchedBrow(Display& d, int outerX, int innerX, int baseY, bool leftSide, uint16_t ink) {
    const int xStart = leftSide ? outerX : innerX;
    const int xEnd = leftSide ? innerX : outerX;
    const int dir = (xEnd >= xStart) ? 1 : -1;
    const int span = abs(xEnd - xStart);
    for (int i = 0; i <= span; ++i) {
        const int x = xStart + i * dir;
        const int mid = span / 2;
        const int dist = abs(i - mid);
        const int arch = max(0, 2 - dist * 2 / max(1, span / 2));
        d.drawPixel(x, baseY - arch, ink);
        if (span >= 5 && arch >= 1) d.drawPixel(x, baseY - arch + 1, ink);
    }
}

template <typename Display>
void drawHandlebarMustache(Display& d, int cx, int stashY, int faceW, int faceR, uint16_t ink) {
    const int gap = max(3, faceR / 5);
    const int curlH = max(3, faceR / 5);
    const int wingReach = max(4, faceW - max(3, faceR / 4));
    const int leftOuterX = cx - wingReach;
    const int leftInnerX = cx - gap;
    const int rightInnerX = cx + gap;
    const int rightOuterX = cx + wingReach;

    auto drawWing = [&](int xStart, int xEnd, int dir) {
        const int span = abs(xEnd - xStart);
        if (span <= 0) return;
        for (int i = 0; i <= span; ++i) {
            const int x = xStart + i * dir;
            const int tipDist = min(i, span - i);
            const int bow = (tipDist <= 1) ? 2 : (tipDist == 2 ? 1 : 0);
            d.drawPixel(x, stashY - bow, ink);
            if (faceR >= 12 && bow > 0) d.drawPixel(x, stashY - bow + 1, ink);
        }
    };
    drawWing(leftOuterX, leftInnerX, 1);
    drawWing(rightOuterX, rightInnerX, -1);

    for (int i = 0; i < curlH; ++i) {
        d.drawPixel(leftOuterX + i, stashY - 2 - i, ink);
        d.drawPixel(leftOuterX + i + 1, stashY - 1 - i, ink);
        d.drawPixel(rightOuterX - i, stashY - 2 - i, ink);
        d.drawPixel(rightOuterX - i - 1, stashY - 1 - i, ink);
        if (faceR >= 14 && i + 1 < curlH) {
            d.drawPixel(leftOuterX + i, stashY - 3 - i, ink);
            d.drawPixel(rightOuterX - i, stashY - 3 - i, ink);
        }
    }
}

template <typename Display>
void drawSubtleSmirk(Display& d, int cx, int baseY, int halfW, uint16_t ink) {
    if (halfW < 2) return;
    for (int x = -halfW; x <= halfW; ++x) {
        const int ax = abs(x);
        const int curve = (ax * ax) / max(2, halfW);
        d.drawPixel(cx + x, baseY - curve, ink);
        if (halfW >= 5 && ax <= halfW / 2) d.drawPixel(cx + x, baseY - curve + 1, ink);
    }
}

struct MaskPoint {
    int x;
    int y;
};

// Guy Fawkes mask outline — flat wide forehead, cheekbones widest, pointed chin.
// Coordinates are hundredths of faceR relative to (cx, cy); not an ellipse/egg.
struct MaskOutlineVertex {
    int8_t xPer100;
    int8_t yPer100;
};

constexpr MaskOutlineVertex kGuyFawkesSilhouette[] = {
    {-78, -94},  // top-left — wide flat forehead
    {78, -94},   // top-right — horizontal crown edge
    {88, -30},   // right upper temple
    {94, 8},     // right cheekbone (widest)
    {80, 38},    // right upper jaw taper
    {44, 66},    // right lower jaw
    {0, 98},     // chin point
    {-44, 66},   // left lower jaw
    {-80, 38},   // left upper jaw taper
    {-94, 8},    // left cheekbone (widest)
    {-88, -30},  // left upper temple
};

constexpr int kGuyFawkesSilhouetteCount =
    static_cast<int>(sizeof(kGuyFawkesSilhouette) / sizeof(kGuyFawkesSilhouette[0]));

int scaleMaskCoord(int faceR, int8_t per100) {
    return (faceR * static_cast<int>(per100)) / 100;
}

int buildGuyFawkesSilhouette(int cx, int cy, int faceR, MaskPoint* out, int maxPts) {
    const int count = min(kGuyFawkesSilhouetteCount, maxPts);
    for (int i = 0; i < count; ++i) {
        out[i].x = cx + scaleMaskCoord(faceR, kGuyFawkesSilhouette[i].xPer100);
        out[i].y = cy + scaleMaskCoord(faceR, kGuyFawkesSilhouette[i].yPer100);
    }
    return count;
}

template <typename Display>
void fillClosedPolygon(Display& d, const MaskPoint* pts, int count, uint16_t color) {
    if (count < 3) return;
    for (int i = 1; i < count - 1; ++i) {
        d.fillTriangle(pts[0].x, pts[0].y, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, color);
    }
}

template <typename Display>
void strokeClosedPolygon(Display& d, const MaskPoint* pts, int count, uint16_t color) {
    if (count < 2) return;
    for (int i = 0; i < count; ++i) {
        const int next = (i + 1) % count;
        d.drawLine(pts[i].x, pts[i].y, pts[next].x, pts[next].y, color);
    }
}

template <typename Display>
void drawGuyFawkesMask(Display& d, int cx, int cy, int faceR, uint8_t pulse) {
    const uint16_t ringColor = lerp565(gTheme.primary, gTheme.secondary, pulse);
    constexpr uint16_t kFaceCream = 0xFFDF;
    constexpr uint16_t kCheekShadow = 0xDEFB;
    const uint16_t faceColor = kFaceCream;
    const uint16_t ink = TFT_BLACK;

    const int ringPulse = pulse >> 5;
    for (int ring = 0; ring < 3; ++ring) {
        const int radius = faceR + 10 + ring * 3 + ringPulse;
        const uint16_t fade = lerp565(ringColor, TFT_BLACK, static_cast<uint8_t>(ring * 70));
        d.drawCircle(cx, cy - 1, radius, fade);
    }

    MaskPoint outline[kGuyFawkesSilhouetteCount + 1]{};
    const int outlineCount = buildGuyFawkesSilhouette(cx, cy, faceR, outline, kGuyFawkesSilhouetteCount);

    const int cheekHalfW = max(7, scaleMaskCoord(faceR, 94));
    const int foreheadHalfW = max(6, scaleMaskCoord(faceR, 78));
    const int chinY = cy + scaleMaskCoord(faceR, 98);
    const int jawJoinY = cy + scaleMaskCoord(faceR, 38);
    const int cheekMidY = cy + scaleMaskCoord(faceR, 8);
    const int cheekTopY = cy + scaleMaskCoord(faceR, -30);

    // 1 px black halo, cream fill, black stroke — closed polygon silhouette.
    MaskPoint halo[kGuyFawkesSilhouetteCount + 1]{};
    for (int i = 0; i < outlineCount; ++i) {
        const int dx = outline[i].x - cx;
        const int dy = outline[i].y - cy;
        const int dist = max(abs(dx), abs(dy));
        const int expand = (dist > 0) ? max(1, faceR / 24) : 1;
        halo[i].x = outline[i].x + (dx > 0 ? expand : (dx < 0 ? -expand : 0));
        halo[i].y = outline[i].y + (dy > 0 ? expand : (dy < 0 ? -expand : 0));
    }
    fillClosedPolygon(d, halo, outlineCount, ink);
    fillClosedPolygon(d, outline, outlineCount, faceColor);
    strokeClosedPolygon(d, outline, outlineCount, ink);

    d.drawLine(cx - foreheadHalfW + 2, cheekTopY, cx - cheekHalfW + 1, cheekMidY, ink);
    d.drawLine(cx - cheekHalfW + 1, cheekMidY, cx - scaleMaskCoord(faceR, 44), jawJoinY, ink);
    d.drawLine(cx + foreheadHalfW - 2, cheekTopY, cx + cheekHalfW - 1, cheekMidY, ink);
    d.drawLine(cx + cheekHalfW - 1, cheekMidY, cx + scaleMaskCoord(faceR, 44), jawJoinY, ink);
    if (faceR >= 14) {
        d.drawPixel(cx - cheekHalfW / 2, cheekMidY, kCheekShadow);
        d.drawPixel(cx - cheekHalfW / 2 + 1, cheekMidY + 1, kCheekShadow);
        d.drawPixel(cx + cheekHalfW / 2, cheekMidY, kCheekShadow);
        d.drawPixel(cx + cheekHalfW / 2 - 1, cheekMidY + 1, kCheekShadow);
    }

    const int eyeY = cy + scaleMaskCoord(faceR, -24);
    const int eyeGap = max(2, faceR / 7);
    const int eyeLift = max(2, faceR / 4);
    const int eyeSpan = max(3, cheekHalfW - max(4, faceR / 4));
    const int leftOuterX = cx - eyeSpan;
    const int leftInnerX = cx - eyeGap;
    const int rightInnerX = cx + eyeGap;
    const int rightOuterX = cx + eyeSpan;
    drawAngledEyeSlit(d, leftInnerX, eyeY + 1, leftOuterX, eyeY - eyeLift, ink);
    drawAngledEyeSlit(d, rightInnerX, eyeY + 1, rightOuterX, eyeY - eyeLift, ink);

    const int browY = eyeY - max(3, faceR / 4);
    drawArchedBrow(d, leftOuterX, leftInnerX, browY, true, ink);
    drawArchedBrow(d, rightOuterX, rightInnerX, browY, false, ink);

    const int noseTop = eyeY + max(2, faceR / 8);
    const int noseLen = max(3, faceR / 4);
    d.drawFastVLine(cx, noseTop, noseLen, ink);
    if (faceR >= 12) {
        d.drawPixel(cx - 1, noseTop + noseLen - 1, ink);
        d.drawPixel(cx + 1, noseTop + noseLen - 1, ink);
    }

    const int stashY = cy + scaleMaskCoord(faceR, 28);
    drawHandlebarMustache(d, cx, stashY, cheekHalfW, faceR, ink);

    const int smileY = stashY + max(2, faceR / 6);
    drawSubtleSmirk(d, cx, smileY, max(3, cheekHalfW / 3), ink);

    const int goateeTop = smileY + max(2, faceR / 8);
    const int goateeW = max(1, faceR / 7);
    d.fillTriangle(cx - goateeW, goateeTop, cx + goateeW, goateeTop, cx, chinY - 1, ink);
    d.drawFastVLine(cx, goateeTop, chinY - goateeTop - 1, ink);
    d.drawPixel(cx, chinY, ink);
}

struct IntroJokeLines {
    char line1[41]{};
    char line2[41]{};
};

static IntroJokeLines gIntroJoke;

void prepareIntroJoke() {
    const char* joke = m5os::boot::randomJokeForBoot();
    m5os::boot::wrapBootJoke(joke, gIntroJoke.line1, gIntroJoke.line2, 38);
}

template <typename Display>
void drawCenteredTextLine(Display& d, int y, const char* text, uint16_t fg, uint16_t bg) {
    if (!text || text[0] == '\0') return;
    d.setTextSize(1);
    d.setTextColor(fg, bg);
    const int textW = static_cast<int>(strlen(text)) * 6;
    const int x = max(2, (static_cast<int>(d.width()) - textW) / 2);
    d.setCursor(x, y);
    d.print(text);
}

template <typename Display>
void drawIntroFrame(Display& d, int frame, int totalFrames) {
    d.fillScreen(TFT_BLACK);

    const int cx = d.width() / 2;
    const uint8_t pulse = introPulse(frame);
    const int faceR = introMaskRadius(frame);
    drawGuyFawkesMask(d, cx, 44, faceR, pulse);

    const uint8_t titleAlpha = introTextAlpha(frame, 10, 14);
    if (titleAlpha > 0) {
        d.setTextSize(2);
        d.setTextColor(lerp565(TFT_BLACK, gTheme.primary, titleAlpha), TFT_BLACK);
        d.setCursor(max(4, cx - 72), 82);
        d.print("Hacker Planet");
    }

    const uint8_t creditAlpha = introTextAlpha(frame, 22, 14);
    if (creditAlpha > 0) {
        d.setTextSize(1);
        d.setTextColor(lerp565(TFT_BLACK, gTheme.secondary, creditAlpha), TFT_BLACK);
        d.setCursor(max(4, cx - 56), 102);
        d.print("by salvadorData");
    }

    const uint8_t jokeAlpha = introTextAlpha(frame, 30, 12);
    if (jokeAlpha > 0) {
        const uint16_t jokeColor = lerp565(TFT_BLACK, TFT_CYAN, jokeAlpha);
        drawCenteredTextLine(d, 112, gIntroJoke.line1, jokeColor, TFT_BLACK);
        drawCenteredTextLine(d, 120, gIntroJoke.line2, jokeColor, TFT_BLACK);
    }

    const int barW = d.width() - 20;
    const int progress = min(100, (frame + 1) * 100 / totalFrames);
    d.drawRect(10, 128, barW, 5, gTheme.secondary);
    d.fillRect(11, 129, max(1, (barW - 2) * progress / 100), 3,
               lerp565(gTheme.primary, gTheme.secondary, pulse));
}

void playHackerPlanetIntro() {
    auto& lcd = m5os::lcd();
    constexpr int kIntroFrames = 110;  // ~6.4s with chime (~3s longer than 60 frames)
    prepareIntroJoke();
    lgfx::LGFX_Sprite canvas(&lcd);
    const bool buffered = canvas.createSprite(lcd.width(), lcd.height()) != nullptr;

    BootSdWavStream wav;
    const char* wavPath = power::allowBootChime() ? resolveBootWavPath() : nullptr;
    const bool useWav = wavPath && wav.begin(wavPath);

    for (int frame = 0; frame < kIntroFrames; ++frame) {
        m5os::update();
        if (buffered) {
            drawIntroFrame(canvas, frame, kIntroFrames);
            canvas.pushSprite(0, 0);
        } else {
            drawIntroFrame(lcd, frame, kIntroFrames);
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

static lgfx::LGFX_Sprite* gFlashProgressSprite = nullptr;
static bool gFlashProgressSpriteReady = false;

static void drawFlashProgressFrame(lgfx::LovyanGFX& d, int percent, const char* label,
                                   const String& detail) {
    Theme& t = theme();
    d.fillScreen(TFT_BLACK);
    d.setTextColor(t.primary, TFT_BLACK);
    d.setCursor(4, 4);
    d.printf("%s", label ? label : "Transfer");
    d.drawFastHLine(0, 18, d.width(), t.secondary);

    const int barX = 10;
    const int barY = 56;
    const int barW = d.width() - 20;
    const int barH = 10;
    d.drawRect(barX, barY, barW, barH, t.secondary);
    const int clamped = max(0, min(100, percent));
    if (clamped > 0) {
        const int fillW = max(1, (barW - 2) * clamped / 100);
        d.fillRect(barX + 1, barY + 1, fillW, barH - 2, t.primary);
    }

    d.setTextColor(t.primary, TFT_BLACK);
    d.setCursor(10, 72);
    d.printf("%d%%", clamped);

    if (detail.length()) {
        d.setTextColor(themeHintOnBlack(), TFT_BLACK);
        d.setCursor(10, 86);
        d.println(detail);
    }

    d.drawFastHLine(0, d.height() - 14, d.width(), t.primary);
    d.setCursor(6, d.height() - 12);
    d.setTextColor(themeHintOnBlack(), TFT_BLACK);
    d.print("Authorized lab use only");
}

void showFlashProgress(int percent, const char* label, const String& detail) {
    auto& lcd = m5os::lcd();
    if (!gFlashProgressSprite) {
        gFlashProgressSprite = new lgfx::LGFX_Sprite(&lcd);
    }
    if (!gFlashProgressSpriteReady) {
        gFlashProgressSpriteReady =
            gFlashProgressSprite->createSprite(lcd.width(), lcd.height()) != nullptr;
    }
    if (gFlashProgressSpriteReady) {
        drawFlashProgressFrame(*gFlashProgressSprite, percent, label, detail);
        gFlashProgressSprite->pushSprite(0, 0);
        return;
    }
    drawFlashProgressFrame(lcd, percent, label, detail);
}

Theme& theme() { return gTheme; }

int getThemePreset() { return gThemePreset; }

void setThemePreset(int preset) {
    if (preset < 0) preset = 0;
    if (preset >= kThemePresetCount) preset = kThemePresetCount - 1;
    gThemePreset = preset;
    switch (preset) {
        case 1:
            gTheme.primary = 0x07E0;
            gTheme.secondary = 0x0660;
            break;
        case 2:
            gTheme.primary = 0xF800;
            gTheme.secondary = 0x7800;
            break;
        case 3:
            gTheme.primary = 0x05A1;
            gTheme.secondary = 0xCE9F;
            break;
        case 4:
            gTheme.primary = 0x07E0;
            gTheme.secondary = 0x07FF;
            break;
        case 5:
            gTheme.primary = 0xFD20;
            gTheme.secondary = 0x8200;
            break;
        default:
            gTheme.primary = 0xB6DF;
            gTheme.secondary = 0x0083;
            break;
    }
    stamp::applyTheme();
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
    d.setTextColor(themeHintOnBlack(), TFT_BLACK);
    d.println(body);
    const unsigned long until = millis() + holdMs;
    while (static_cast<long>(millis() - until) < 0) {
        m5os::update();
        delay(10);
    }
}

void bootIntroBegin() {
    setThemePreset(kDefaultThemePreset);
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

namespace {

enum class YesNoKey { None, Yes, No };

YesNoKey pollYesNoKey() {
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return YesNoKey::None;
    auto status = M5Cardputer.Keyboard.keysState();
    for (auto key : status.word) {
        if (key == 'y') return YesNoKey::Yes;
        if (key == 'n') return YesNoKey::No;
        if (key == '`' || key == 27) return YesNoKey::No;
    }
    for (uint8_t hid : status.hid_keys) {
        if (hid == m5os::kHidEscape) return YesNoKey::No;
    }
    return YesNoKey::None;
}

}  // namespace

bool promptYesNo(const char* title, const char* question) {
    while (true) {
        drawHeader(title ? title : "Confirm");
        auto& d = m5os::lcd();
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        d.setCursor(8, 30);
        d.println(question ? question : "Continue?");
        d.setCursor(8, 56);
        d.setTextColor(themeHintOnBlack(), TFT_BLACK);
        d.println("y yes  n no");

        m5os::update();
        switch (pollYesNoKey()) {
            case YesNoKey::Yes:
                return true;
            case YesNoKey::No:
                return false;
            default:
                break;
        }
        delay(power::uiLoopDelayMs());
    }
}

int selectFromList(const std::vector<String>& items, const char* title, int startIndex,
                   const char* backConfirmTitle, const char* backConfirmBody) {
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
                m5os::lcd().setTextColor(gTheme.primary, lerp565(TFT_BLACK, gTheme.primary, 72));
                m5os::lcd().printf("> %s", items[i].c_str());
            } else {
                m5os::lcd().setTextColor(themeHintOnBlack(), TFT_BLACK);
                m5os::lcd().printf("  %s", items[i].c_str());
            }
        }
        m5os::lcd().setTextColor(TFT_DARKGREY, TFT_BLACK);
        m5os::lcd().setCursor(4, 118);
        if (backConfirmTitle && backConfirmBody) {
            m5os::lcd().print("h help  ` confirm back");
        } else {
            m5os::lcd().print("h help  ` back");
        }
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
        if (keys.back || m5os::keyboardBackJustPressed()) return -1;
        delay(power::uiLoopDelayMs());
    }
}

void drawHelpOverlay() {
    drawHeader("Keyboard shortcuts");
    auto& d = m5os::lcd();
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 24);
    d.println(";/. w/s  navigate");
    d.println("Enter     select / verify hash load");
    d.println("Tab       fast load (skip hash)");
    d.println("ESC/`     load app (main menu)");
    d.println("Tab       next app (in switcher list)");
    d.println("h / ?     this help");
    d.println("e         export catalog serial");
    d.println("ESC/`     back (Y/N confirm)");
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 104);
    d.println("Apps live on SD; load app");
    d.println("copies to run slot when needed.");
    d.setCursor(4, 118);
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
    d.println("Or: Load from M5Burner catalog");
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
