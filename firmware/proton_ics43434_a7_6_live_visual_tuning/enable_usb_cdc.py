"""
Force USB CDC stdio for the norandomtechie Proton picosdk fork.

The local Proton PlatformIO fork does not enable USB CDC stdio reliably when
PIO_NO_STDIO_UART is set. This mirrors the working satellite_tester setup so
printf output appears on the Proton native USB serial port while picoprobe is
used for flashing/debug.
"""

from os.path import join

Import("env")

platform = env.PioPlatform()
framework_dir = platform.get_package_dir("framework-picosdk")

env.Append(
    CPPDEFINES=[
        ("PICO_STDIO", 1),
        ("LIB_PICO_STDIO", 1),
        ("PICO_STDIO_USB", 1),
        ("LIB_PICO_STDIO_USB", 1),
        "PIO_STDIO_USB",
        ("CFG_TUSB_DEBUG", 0),
        ("CFG_TUSB_MCU", "OPT_MCU_RP2040"),
        ("CFG_TUSB_OS", "OPT_OS_PICO"),
        ("PICO_RP2040_USB_DEVICE_UFRAME_FIX", 1),
        ("PICO_RP2040_USB_DEVICE_ENUMERATION_FIX", 1),
        ("PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS", 0),
        ("PICO_STDIO_USB_CONNECTION_WITHOUT_DTR", 1),
    ],
    CPPPATH=[
        join(framework_dir, "lib", "tinyusb", "src"),
        join(
            framework_dir,
            "src",
            "rp2_common",
            "pico_fix",
            "rp2040_usb_device_enumeration",
            "include",
        ),
    ],
)

env.BuildSources(
    join("$BUILD_DIR", "PicoSDKTinyUSB"),
    join(framework_dir, "lib", "tinyusb", "src"),
    "+<*> -<portable> +<portable/raspberrypi>",
)
env.BuildSources(
    join("$BUILD_DIR", "PicoSDKPicoFix"),
    join(framework_dir, "src", "rp2_common", "pico_fix"),
)
env.BuildSources(
    join("$BUILD_DIR", "PicoSDKpico_stdio_usb"),
    join(framework_dir, "src", "rp2_common", "pico_stdio_usb"),
    "+<*>",
)
env.BuildSources(
    join("$BUILD_DIR", "PicoSDKhardware_watchdog"),
    join(framework_dir, "src", "rp2_common", "hardware_watchdog"),
    "+<*>",
)

print("[enable_usb_cdc] USB CDC stdio defines and sources enabled")
