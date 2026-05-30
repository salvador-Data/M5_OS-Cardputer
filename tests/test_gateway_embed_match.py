"""Gateway app1 must match embedded firmware size (stale USB flash guard)."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GATEWAY_CPP = ROOT / "src" / "m5os_gateway.cpp"


def test_gateway_ready_checks_embed_image_len() -> None:
    text = GATEWAY_CPP.read_text(encoding="utf-8")
    assert "gatewayImageMatchesEmbed" in text
    fn = text[text.index("bool gatewayPartitionReady") : text.index("bool flashGatewayImage")]
    assert "gatewayImageMatchesEmbed(gw)" in fn
    assert "gw_stale_embed" in fn
    match_fn = text[text.index("bool gatewayImageMatchesEmbed") : text.index("bool flashGatewayFromFile")]
    assert "metadata.image_len == gateway_embed::kSize" in match_fn
    assert "esp_image_verify" in match_fn


def test_flash_embed_image_checks_otadata_mark() -> None:
    text = GATEWAY_CPP.read_text(encoding="utf-8")
    fn = text[text.index("bool flashGatewayImage") : text.index("bool flashEmbeddedGatewayIfNeeded")]
    assert "gw_flash_otadata_fail" in fn
    assert "markPartitionOtaState(gw, ESP_OTA_IMG_VALID)" in fn
