"""Point the build at a single firmware sketch.

Each AVR env sets `custom_firmware = <Name>` (a folder under firmware/). This
pre-script redirects PROJECT_SRC_DIR at firmware/<Name>/ so PlatformIO's Arduino
builder finds that sketch's .ino in the source root and compiles only that
firmware's sources (including its save_state.cpp) - no cross-firmware link order.
"""
import os

Import("env")  # noqa: F821  (injected by PlatformIO)

firmware = env.GetProjectOption("custom_firmware")  # noqa: F821
src = os.path.join(env["PROJECT_DIR"], "firmware", firmware)  # noqa: F821
env.Replace(PROJECT_SRC_DIR=src)  # noqa: F821
