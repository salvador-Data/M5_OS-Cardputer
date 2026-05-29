"""Contract tests for M5Burner HTTP stream retry and chunk sizing."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BURNER_CPP = ROOT / "src" / "burner_install.cpp"


def test_burner_stream_uses_64_byte_chunks():
    text = BURNER_CPP.read_text(encoding="utf-8")
    assert "kStreamChunkBytes = 64" in text
    chunk = text[text.index("RangeStreamResult streamRangeChunk") : text.index(
        "RangeStreamResult streamRangeToFile"
    )]
    assert "buffer[kStreamChunkBytes]" in chunk
    assert "feedWatchdog()" in chunk


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

