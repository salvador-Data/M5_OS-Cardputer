Import("env")
import os
import subprocess
import sys
from pathlib import Path

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
VARIANT_DIR = PROJECT_DIR / "variants" / "m5os-cardputer"
BOOTLOADER_ENV = "m5stack-cardputer-bootloader"
BOOTLOADER_SRC = PROJECT_DIR / ".pio" / "build" / BOOTLOADER_ENV / "bootloader.bin"
BOOTLOADER_DST = VARIANT_DIR / "bootloader.bin"
BOOTLOADER_C = PROJECT_DIR / "bootloader_components" / "main" / "bootloader_start.c"


def _bootloader_needs_rebuild() -> bool:
    if not BOOTLOADER_DST.is_file():
        return True
    if not BOOTLOADER_C.is_file():
        return False
    return BOOTLOADER_C.stat().st_mtime > BOOTLOADER_DST.stat().st_mtime


def _run_bootloader_build(source, target, env):
    if env.subst("$PIOENV") == BOOTLOADER_ENV:
        return
    if os.environ.get("M5OS_SKIP_BOOTLOADER_BUILD", "").strip() in ("1", "true", "yes"):
        print("M5 OS: skip custom bootloader build (M5OS_SKIP_BOOTLOADER_BUILD)")
        return
    VARIANT_DIR.mkdir(parents=True, exist_ok=True)
    if BOOTLOADER_DST.is_file() and not _bootloader_needs_rebuild():
        print("M5 OS: using existing custom bootloader -> %s" % BOOTLOADER_DST)
        return
    if not _bootloader_needs_rebuild():
        return
    cmd = [sys.executable, "-m", "platformio", "run", "-e", BOOTLOADER_ENV, "-t", "bootloader"]
    print("M5 OS: building custom bootloader (%s)" % BOOTLOADER_ENV)
    try:
        subprocess.check_call(cmd, cwd=str(PROJECT_DIR))
    except subprocess.CalledProcessError:
        if BOOTLOADER_DST.is_file():
            print("M5 OS: bootloader build failed; keeping existing %s" % BOOTLOADER_DST)
            return
        raise
    if not BOOTLOADER_SRC.is_file():
        if BOOTLOADER_DST.is_file():
            return
        raise RuntimeError("bootloader build missing: %s" % BOOTLOADER_SRC)
    BOOTLOADER_DST.write_bytes(BOOTLOADER_SRC.read_bytes())
    print("M5 OS: installed custom bootloader -> %s" % BOOTLOADER_DST)


env.AddPreAction("buildprog", _run_bootloader_build)
