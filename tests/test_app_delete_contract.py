"""Del key delete contract for Load app / catalog pickers and SD VFS paths."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KB = ROOT / "include" / "m5os_keyboard.h"
MENU = ROOT / "src" / "launcher_menu.cpp"
APP_LAUNCHER = ROOT / "src" / "app_launcher.cpp"
VFS_CPP = ROOT / "src" / "m5os_vfs.cpp"
VFS_H = ROOT / "include" / "m5os_vfs.h"
WIFI_UI = ROOT / "src" / "ui_display.cpp"


def test_keyboard_del_maps_hid_backspace_and_status_del():
    text = KB.read_text(encoding="utf-8")
    assert "kHidBackspace = 0x2a" in text
    held = text.split("inline bool keyboardDelHeld()")[1].split("inline bool keyboardDelJustPressed")[0]
    assert "status.del" in held
    assert "kHidBackspace" in held
    assert "keyboardDrainDel" in text
    just = text.split("inline bool keyboardDelJustPressed()")[1].split("inline void keyboardDrainDel")[0]
    assert "isChange()" in just
    assert "keyboardDelHeld()" in just


def test_load_app_switcher_del_delete_footer_and_confirm():
    text = MENU.read_text(encoding="utf-8")
    switcher = text[text.index("void LauncherMenu::showAppSwitcher") : text.index(
        "void LauncherMenu::showInstalledApps"
    )]
    assert 'constexpr const char* kAppPickerFooter = "Del=delete  Enter=load  Gateway: ESC=OS"' in text
    assert "print(kAppPickerFooter)" in switcher
    assert "keyboardDelJustPressed()" in switcher
    assert "keyboardDrainDel()" in switcher
    assert "confirmAndDeleteApp" in switcher
    assert "deleteInstalledApp" in text
    assert '? y/n"' in text
    confirm = text[text.index("LoadConfirmChoice promptLoadAppConfirm") : text.index(
        "constexpr const char* kAppPickerFooter"
    )]
    assert "keyboardDelJustPressed()" not in confirm


def test_catalog_picker_del_on_installed_sd_apps():
    text = MENU.read_text(encoding="utf-8")
    catalog = text[text.index("void LauncherMenu::showLoadCatalog") : text.index(
        "void LauncherMenu::showFlashBurnerCatalog"
    )]
    assert 'constexpr const char* kAppPickerFooter = "Del=delete  Enter=load  Gateway: ESC=OS"' in text
    assert "print(kAppPickerFooter)" in catalog
    assert "keyboardDelJustPressed()" in catalog
    assert "pkg.installed" in catalog
    assert "confirmAndDeleteApp" in catalog


def test_delete_removes_apps_slug_dirs_and_clears_run_slot():
    launcher = APP_LAUNCHER.read_text(encoding="utf-8")
    delete_fn = launcher[launcher.index("AppDeleteResult AppLauncher::deleteInstalledApp") :]
    assert "vfs::removeApp(slug" in delete_fn
    assert "clearRunSlotIfCached" in delete_fn
    assert "clearLaunchCacheForKey" in delete_fn
    assert "invalidateRunSlot" in launcher

    vfs = VFS_CPP.read_text(encoding="utf-8")
    remove = vfs[vfs.index("bool removeApp") :]
    assert "appDirFor(slug)" in remove
    assert "appDataDirFor(slug)" in remove
    assert "removeDirectoryTree(appDir" in remove
    assert "removeDirectoryTree(dataDir" in remove
    assert "kLegacyFirmwareDir" in remove

    header = VFS_H.read_text(encoding="utf-8")
    assert "bool removeApp(const String& appSlug" in header


def test_wifi_password_still_uses_del_erase_not_app_delete():
    text = WIFI_UI.read_text(encoding="utf-8")
    assert "Del erase  Tab AP  Enter save  ` cancel" in text
    pwd = text[text.index("PasswordPromptResult promptPassword") : text.index("}  // namespace m5os::ui")]
    assert "keyboardWantsErase()" in pwd
    assert "deleteInstalledApp" not in pwd


def test_file_explorer_delete_actions() -> None:
    text = MENU.read_text(encoding="utf-8")
    assert "promptExplorerAction" in text
    assert "Delete file" in text
    assert "Delete folder" in text
    explorer = text[text.index("void LauncherMenu::showFileExplorer") : text.index("void LauncherMenu::showThemeMenu")]
    assert "ExplorerEntryAction::DeleteFile" in explorer
    assert "ExplorerEntryAction::DeleteFolder" in explorer
