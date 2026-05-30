"""PlatformIO custom target: upload M5 OS (app0) then session gateway (app1)."""
Import("env")
import subprocess
import sys
from pathlib import Path

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
GATEWAY_ENV = "m5os-session-gateway"
GATEWAY_BIN = PROJECT_DIR / ".pio" / "build" / GATEWAY_ENV / "firmware.bin"
APP1_OFFSET = "0x3D0000"


def _gateway_bin():
    if not GATEWAY_BIN.is_file():
        subprocess.check_call(
            [sys.executable, "-m", "platformio", "run", "-e", GATEWAY_ENV],
            cwd=str(PROJECT_DIR),
        )
    if not GATEWAY_BIN.is_file():
        env.Exit(1)
    return GATEWAY_BIN


def _upload_gateway(source, target, env):
    gw = _gateway_bin()
    esptool = env.subst("$PYTHONEXE")
    pkg = Path(env.subst("$PROJECT_PACKAGES_DIR")) / "tool-esptoolpy" / "esptool.py"
    port = env.subst("$UPLOAD_PORT")
    if not port:
        env.Exit(1)
    cmd = [
        esptool,
        str(pkg),
        "--chip",
        "esp32s3",
        "--port",
        port,
        "write_flash",
        APP1_OFFSET,
        str(gw),
    ]
    print("M5 OS upload-all: writing gateway to app1 @ %s" % APP1_OFFSET)
    subprocess.check_call(cmd)


env.AddCustomTarget(
    name="upload-all",
    dependencies="${BUILD_DIR}/${PROGNAME}.bin",
    actions=[
        env.VerboseAction("$UPLOADCMD", "Uploading M5 OS"),
        env.VerboseAction(_upload_gateway, "Uploading session gateway to app1"),
    ],
    title="Upload All",
    description="Upload M5 OS to app0 and session gateway to app1",
)

env.AddCustomTarget(
    name="upload-factory",
    dependencies="${BUILD_DIR}/${PROGNAME}.bin",
    actions=[
        env.VerboseAction("$UPLOADCMD", "Uploading M5 OS"),
        env.VerboseAction(_upload_gateway, "Uploading session gateway to app1"),
    ],
    title="Upload Factory",
    description="Alias for upload-all — flash app0 + gateway app1",
)
