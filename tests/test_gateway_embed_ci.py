"""CI must generate gateway embed from tracked data bin before PlatformIO build."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_gateway_data_bin_present():
    assert (ROOT / "data" / "m5os_session_gateway.bin").is_file()


def test_gen_gateway_embed_script_present():
    assert (ROOT / "scripts" / "gen_gateway_embed_from_data.py").is_file()


def test_ci_workflow_generates_embed_before_build():
    workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
    assert "gen_gateway_embed_from_data.py" in workflow
    assert workflow.index("gen_gateway_embed_from_data.py") < workflow.index("pio run")
