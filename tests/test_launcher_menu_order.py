"""Contract tests for launcher menu ordering."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MENU_CPP = ROOT / "src" / "launcher_menu.cpp"


def test_wifi_setup_is_first_menu_item() -> None:
    text = MENU_CPP.read_text(encoding="utf-8")
    start = text.index("void LauncherMenu::runMainLoop()")
    block = text[start : start + 1200]
    items_start = block.index("static const char* items[] = {")
    items_end = block.index("};", items_start)
    items_block = block[items_start:items_end]
    first = items_block.split('"')[1]
    assert first == "WiFi setup"


def test_refresh_catalog_merges_burner_hub() -> None:
    text = MENU_CPP.read_text(encoding="utf-8")
    assert "refreshFromBurnerHub" in text


def test_load_from_m5burner_catalog_menu_item() -> None:
    text = MENU_CPP.read_text(encoding="utf-8")
    assert "Load from M5Burner catalog" in text
    assert "Load from catalog" in text
    assert "showFlashBurnerCatalog" in text
    assert "flashBurnerPackage" in text
