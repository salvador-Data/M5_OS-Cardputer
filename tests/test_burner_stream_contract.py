"""Contract tests for M5Burner HTTP stream retry and chunk sizing."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BURNER_CPP = ROOT / "src" / "burner_install.cpp"
SEC_H = ROOT / "include" / "m5os_security.h"


def test_burner_stream_uses_4k_chunks():
    text = BURNER_CPP.read_text(encoding="utf-8")
    sec = SEC_H.read_text(encoding="utf-8")
    assert "kSha256IoChunkBytes = 4096" in sec
    assert "kStreamChunkBytes = security::kSha256IoChunkBytes" in text
    chunk = text[text.index("RangeStreamResult streamRangeChunk") : text.index(
        "RangeStreamResult streamRangeToFile"
    )]
    assert "buffer[kStreamChunkBytes]" in chunk
    assert "feedWatchdog()" in chunk


def test_burner_stream_throttles_progress_paint():
    text = BURNER_CPP.read_text(encoding="utf-8")
    assert "kStreamProgressPaintBytes = 32768" in text
    fn = text[text.index("void reportStreamProgress") : text.index("RangeStreamResult streamRangeChunk")]
    assert "kStreamProgressPaintBytes" in fn
    assert "prevPct" in fn


def test_burner_download_disables_wifi_sleep():
    text = BURNER_CPP.read_text(encoding="utf-8")
    flash_fn = text[text.index("BurnerFlashResult flashAppToOta") : text.index(
        "BurnerDownloadResult downloadPlanToSd"
    )]
    sd_fn = text[text.index("BurnerDownloadResult downloadPlanToSd") : text.index(
        "BurnerDownloadResult downloadFidToSd"
    )]
    assert "WifiThroughputGuard" in flash_fn
    assert "WifiThroughputGuard" in sd_fn


def test_burner_file_download_retries_incomplete():
    text = BURNER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("RangeStreamResult streamRangeToFile") : text.index(
        "bool streamRangeToOtaOnce"
    )]
    assert "kHttpMaxAttempts" in fn
    assert "streamRangeChunk" in fn
    assert "out.seek(totalWritten)" in fn


def test_burner_ota_retries_on_failure():
    text = BURNER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("bool streamRangeToOta(") : text.index("String sdExtraPathFromApp")]
    assert "streamRangeToOtaOnce" in fn
    assert "kHttpMaxAttempts" in fn
    assert "burner_len_mismatch" in text
