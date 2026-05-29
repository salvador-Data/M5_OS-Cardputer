"""Static contract tests for Cardputer SD mount (matches M5 sdcard.ino)."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VFS_CPP = ROOT / "src" / "m5os_vfs.cpp"
CONFIG_H = ROOT / "include" / "m5os_config.h"
MAIN_CPP = ROOT / "src" / "main.cpp"
OFFICIAL_INO = (
    ROOT
    / ".pio"
    / "libdeps"
    / "m5stack-cardputer"
    / "M5Cardputer"
    / "examples"
    / "Basic"
    / "sdcard"
    / "sdcard.ino"
)


def test_sd_gpio_matches_m5_docs():
    text = CONFIG_H.read_text(encoding="utf-8")
    assert "kSdCsPin = 12" in text
    assert "kSdSclkPin = 40" in text
    assert "kSdMisoPin = 39" in text
    assert "kSdMosiPin = 14" in text


def test_vfs_uses_global_spi_like_official_example():
    text = VFS_CPP.read_text(encoding="utf-8")
    assert "SPI.begin(kSdSclkPin" in text
    assert "SD.begin(kSdCsPin, SPI," in text
    assert "SPIClass" not in text
    assert "SPI.end();" not in text
    assert "FSPI" not in text
    assert "HSPI" not in text


def test_vfs_mount_retries_and_never_tears_down_spi():
    text = VFS_CPP.read_text(encoding="utf-8")
    assert "sd_mount_retry" in text
    assert "25000000" in text
    assert "1000000" in text
    assert "vfs_ready" in text
    assert "mountFailureMessage" in text


def test_boot_mounts_sd_before_splash():
    text = MAIN_CPP.read_text(encoding="utf-8")
    assert "ensureStorage()" in text
    idx = text.index("ensureStorage()")
    splash = text.index("bootIntroBegin")
    assert idx < splash


def test_official_m5_example_pins_if_present():
    if not OFFICIAL_INO.is_file():
        return
    text = OFFICIAL_INO.read_text(encoding="utf-8")
    assert "SD_SPI_SCK_PIN  40" in text
    assert "SD_SPI_CS_PIN   12" in text
    assert "SPI.begin(SD_SPI_SCK_PIN" in text
    assert "SD.begin(SD_SPI_CS_PIN, SPI," in text
