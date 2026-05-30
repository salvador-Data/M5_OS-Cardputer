"""Contract tests for SD bin save path resolution (M5Burner / Load app)."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VFS_CPP = ROOT / "src" / "m5os_vfs.cpp"
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
CATALOG_CPP = ROOT / "src" / "firmware_catalog.cpp"


def test_bin_path_for_uses_apps_compartment_when_dir_exists() -> None:
    """After ensureAppDirs, new downloads must not target missing /firmware/."""
    text = VFS_CPP.read_text(encoding="utf-8")
    fn = text[text.index("String binPathFor") : text.index("bool ensureAppDirs")]
    assert "SD.exists(compartment.c_str())" in fn
    assert "SD.exists(appPath.c_str())" in fn
    assert "kLegacyFirmwareDir" in fn
    assert fn.index("compartment") < fn.index("kLegacyFirmwareDir")


def test_m5burner_flash_uses_bin_path_after_ensure_app_dirs() -> None:
    text = APP_LAUNCHER.read_text(encoding="utf-8")
    fn = text[text.index("LaunchResult AppLauncher::flashBurnerPackage") : text.index(
        "LaunchResult AppLauncher::launchByPackageName"
    )]
    assert "ensureAppDirs" in fn
    assert "binPathForPackage" in fn
    assert fn.index("ensureAppDirs") < fn.index("binPathForPackage")


def test_catalog_download_builds_apps_compartment_path() -> None:
    text = CATALOG_CPP.read_text(encoding="utf-8")
    fn = text[text.index("bool FirmwareCatalog::downloadPackage") : text.index(
        "FirmwarePackage* FirmwareCatalog::findInstalledByName"
    )]
    assert "appDirFor(slug)" in fn
    assert "ensureAppDirs" in fn
