"""
Reset the RP2350 after upload so the test firmware starts immediately.
"""

import os
import subprocess

Import("env")


def after_upload(source, target, env):
    openocd = os.path.join(
        env.PioPlatform().get_package_dir("tool-openocd-rp2040-earlephilhower"),
        "bin",
        "openocd.exe",
    )
    scripts = os.path.join(
        env.PioPlatform().get_package_dir("tool-openocd-rp2040-earlephilhower"),
        "share",
        "openocd",
        "scripts",
    )
    subprocess.run(
        [
            openocd,
            "-s",
            scripts,
            "-f",
            "interface/cmsis-dap.cfg",
            "-f",
            "target/rp2350.cfg",
            "-c",
            "adapter speed 5000; init; reset run; shutdown",
        ],
        capture_output=True,
        timeout=10,
        check=False,
    )
    print("*** Post-upload: MCU reset & running ***")


env.AddPostAction("upload", after_upload)
