"""Contract tests for UTMS scan, quarantine, and firewall modules."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE_CPP = ROOT / "src" / "utms_core.cpp"
CORE_H = ROOT / "include" / "utms_core.h"
FW_CPP = ROOT / "src" / "utms_firewall.cpp"
MENU_CPP = ROOT / "src" / "utms_menu.cpp"
SECURITY_CPP = ROOT / "src" / "m5os_security.cpp"
VFS_H = ROOT / "include" / "m5os_vfs.h"
SCRIPTS = ROOT / "scripts" / "utms_threat_pack.py"


def test_utms_core_scan_and_quarantine_symbols():
    h = CORE_H.read_text(encoding="utf-8")
    cpp = CORE_CPP.read_text(encoding="utf-8")
    for sym in ("runAvScan", "quarantineFile", "restoreQuarantined", "loadThreatSignatures"):
        assert sym in h
        assert sym in cpp
    assert "allow_hashes" in cpp
    assert "kQuarantineManifestPath" in VFS_H.read_text(encoding="utf-8")


def test_scan_walks_apps_home_and_staging():
    text = CORE_CPP.read_text(encoding="utf-8")
    assert "kAppsDir" in text
    assert "kHomeDefaultDir" in text
    assert "kStagingBinPath" in text
    assert "ScanVerdict::Infected" in text
    assert "deny hash" in text


def test_firewall_hooks_https_allowlist():
    sec = SECURITY_CPP.read_text(encoding="utf-8")
    assert "utms_firewall.h" in sec
    assert "urlAllowedByFirewall" in sec


def test_firewall_rules_json_roundtrip():
    fw = FW_CPP.read_text(encoding="utf-8")
    assert "saveFirewallRules" in fw
    assert '"rules"' in fw
    assert "fw_block" in fw


def test_menu_no_stub_markers():
    menu = MENU_CPP.read_text(encoding="utf-8")
    assert "stub" not in menu.lower()
    assert "runAvScan" in menu
    assert "loadIdsStatus" in menu
    assert "readLogTailLines" in menu
    assert "launchGatewaySession" not in menu


def test_vfs_quarantine_manifest_path():
    text = VFS_H.read_text(encoding="utf-8")
    assert "kQuarantineManifestPath" in text


def test_host_script_allow_hashes():
    text = SCRIPTS.read_text(encoding="utf-8")
    assert "allow_hashes" in text
