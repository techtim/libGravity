"""Force U8g2 back to 8-bit coordinate mode.

u8g2.h enables U8G2_16BIT unconditionally (upstream default as of 2.36.x),
which widens every coordinate type from uint8_t to uint16_t. Gravity's panel is
128x64, so 8-bit mode is valid - 16-bit is only needed above 240 px in one
direction - and turning it off saves ~1.1 KB of flash.

It is a plain `#define`, not `#ifndef`-guarded, so -D build flags cannot switch
it off: the header would redefine it regardless. The only lever is the header
itself, and since PlatformIO re-downloads lib_deps into .pio/libdeps on a clean
build, the edit has to be reapplied here. Idempotent - a second run is a no-op.
"""
import os

Import("env")  # noqa: F821  (injected by PlatformIO)

MARKER = "#define U8G2_16BIT"
PATCHED = "/* #define U8G2_16BIT */  /* patched: see scripts/u8g2_8bit.py */"


def patch_u8g2(_target=None, _source=None, _env=None):
    header = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"],  # noqa: F821
                          "U8g2", "src", "clib", "u8g2.h")
    if not os.path.isfile(header):
        return  # library not fetched yet; nothing to do
    with open(header, "r") as f:
        lines = f.readlines()
    changed = False
    for i, line in enumerate(lines):
        # Only the bare top-level definition. The copy inside the "32 bit
        # environments" #ifndef block below it is already unreachable, and
        # leaving it alone keeps this patch minimal.
        if line.strip() == MARKER and not changed:
            lines[i] = PATCHED + "\n"
            changed = True
    if changed:
        with open(header, "w") as f:
            f.writelines(lines)
        print("u8g2_8bit: disabled U8G2_16BIT in %s" % header)


patch_u8g2()
