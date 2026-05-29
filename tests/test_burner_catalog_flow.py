"""Contract tests for on-device M5Burner catalog load flow."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MENU_CPP = ROOT / "src" / "launcher_menu.cpp"
APP_LAUNCHER_CPP = ROOT / "src" / "app_launcher.cpp"
BURNER_CPP = ROOT / "src" / "burner_install.cpp"
HOOKUP_CPP = ROOT / "src" / "m5burner_hookup.cpp"


def test_m5burner_menu_auto_refreshes_launcherhub() -> None:
    text = MENU_CPP.read_text(encoding="utf-8")
    fn = text[text.index("void LauncherMenu::showFlashBurnerCatalog") : text.index(
        "void LauncherMenu::refreshCatalog"
    )]
    assert "refreshFromBurnerHub" in fn
    assert "Fetching LauncherHub" in fn
    assert "Refresh manifest first" not in fn


def test_m5burner_confirm_accepts_keyboard_enter() -> None:
    text = MENU_CPP.read_text(encoding="utf-8")
    fn = text[text.index("void LauncherMenu::showFlashBurnerCatalog") : text.index(
        "void LauncherMenu::refreshCatalog"
    )]
    assert "keyboardEnterJustPressed()" in fn
    assert "readButtonsExtended()" in fn
    assert "keyboardDrainEnter()" in fn


def test_flash_burner_package_skips_enrich() -> None:
    text = APP_LAUNCHER_CPP.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::flashBurnerPackage") : text.index(
        "LaunchResult AppLauncher::launchByPackageName"
    )]
    assert "buildInstallPlan" in fn
    assert "enrichPackageFromBurner" not in fn
    assert "ensureAppDirs" in fn
    assert "scanInstalled()" in fn


def test_build_install_plan_reports_stage_errors() -> None:
    text = BURNER_CPP.read_text(encoding="utf-8")
    assert "BurnerPlanError" in text
    assert "setPlanError" in text
    assert "formatBurnerStageError" in text
    fn = text[text.index("bool buildInstallPlan") : text.index("BurnerFlashResult flashAppToOta")]
    assert "Version list failed" in fn
    assert "Size probe failed" in fn


def test_hookup_catalog_http_retries() -> None:
    text = HOOKUP_CPP.read_text(encoding="utf-8")
    assert "kHttpMaxAttempts" in text
    assert "delay(400 * (attempt + 1))" in text
