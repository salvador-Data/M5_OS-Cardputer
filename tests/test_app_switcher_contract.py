"""Contract tests for ESC app switcher and Load UI wording."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MENU_CPP = ROOT / "src" / "launcher_menu.cpp"
DOCS = ROOT / "docs" / "APP_INSTALL.md"


def test_app_switcher_implemented():
    text = MENU_CPP.read_text(encoding="utf-8")
    assert "showAppSwitcher" in text
    assert "Load app (ESC/`)" in text
    assert "showAppSwitcher();" in text
    assert "Tab next" in text or "keysState().tab" in text


def test_esc_from_main_menu_opens_switcher():
    text = MENU_CPP.read_text(encoding="utf-8")
    start = text.index("void LauncherMenu::runMainLoop()")
    block = text[start : start + 800]
    assert "if (pick < 0)" in block
    assert "showAppSwitcher()" in block
    assert "Exit app?" not in block


def test_app_switcher_esc_direct_to_main_menu():
    text = MENU_CPP.read_text(encoding="utf-8")
    switcher = text[text.index("void LauncherMenu::showAppSwitcher") : text.index("void LauncherMenu::showInstalledApps")]
    assert "keyboardBackJustPressed()" in switcher
    assert 'promptYesNo("Exit app?"' not in switcher


def test_load_wording_not_download_in_menu():
    text = MENU_CPP.read_text(encoding="utf-8")
    assert "Load from catalog" in text
    assert "showLoadCatalog" in text
    assert "Download from catalog" not in text
    assert '"Download failed"' not in text
    assert "Load failed" in text


def test_docs_use_load_and_esc_switcher():
    text = DOCS.read_text(encoding="utf-8")
    assert "Load from catalog" in text
    assert "Load app" in text
    assert "ESC" in text
