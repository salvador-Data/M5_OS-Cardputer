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
    assert "tryOfficialMount" in text
    assert "sd_mount_retry" in text
    assert "25000000" in text
    assert "vfs_ready" in text
    assert "mountFailureMessage" in text
    assert "delay(800)" not in text


def test_vfs_ensure_directory_chain_and_quarantine():
    text = VFS_CPP.read_text(encoding="utf-8")
    assert "ensureDirectory" in text
    assert "ensureDirectoryChain" in text
    assert "mkdirViaParent" in text
    assert ".m5os_bak" in text
    assert "pathKind" in text
    assert "isDirectory()" in text
    assert "ensureDirectoryChain(dir, &vfsStepError)" in text
    assert "errnoHint" in text
    assert 'current += path[i]' not in text


def test_vfs_root_probe_before_tree():
    text = VFS_CPP.read_text(encoding="utf-8")
    assert 'kProbe = "/.m5os_probe"' in text
    assert "/system/.sd_probe" not in text
    mount_fn = text[text.index("MountResult mountAndInit()") : text.index("}  // namespace m5os::vfs")]
    probe_call_idx = mount_fn.index("verifySdReadWrite(&vfsStepError)")
    tree_idx = mount_fn.index("ensureDirectoryChain(dir")
    assert probe_call_idx < tree_idx
    assert "mkdirIfMissing(kSystemDir" not in mount_fn


def test_boot_mounts_sd_after_display_like_official():
    text = MAIN_CPP.read_text(encoding="utf-8")
    assert "ensureStorage()" in text
    begin_idx = text.index("m5os::begin()")
    storage_idx = text.index("ensureStorage()")
    splash_idx = text.index("bootIntroBegin")
    assert begin_idx < storage_idx < splash_idx
    assert "primeSdPinsPreDisplay" not in text


def test_official_m5_example_pins_if_present():
    if not OFFICIAL_INO.is_file():
        return
    text = OFFICIAL_INO.read_text(encoding="utf-8")
    assert "SD_SPI_SCK_PIN  40" in text
    assert "SD_SPI_CS_PIN   12" in text
    assert "SPI.begin(SD_SPI_SCK_PIN" in text
    assert "SD.begin(SD_SPI_CS_PIN, SPI," in text
