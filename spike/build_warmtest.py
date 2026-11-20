#!/usr/bin/env python3
# Copyright 2026 Justin Cameron
# SPDX-License-Identifier: Apache-2.0
"""Build the warm-start measurement disk, build/warmtest/WARMTEST.D81:
WARMTEST (spike/warmtest.c), mega-net's MEGANET, and the collection's
AUTOBOOT.C65, which runs WARMTEST again after each of its resets.

    python3 spike/build_warmtest.py

Borrows the menu's toolchain helpers (build.py) and mega-ntp's disk
loader (src/platform), and builds mega-net's image (with INIT_WARM)."""
import subprocess, sys
from pathlib import Path

SPIKE = Path(__file__).resolve().parent
ROOT = SPIKE.parent
sys.path.insert(0, str(ROOT))
import build as mb                     # noqa: E402  (mega-menu/build.py)

MEGANET = mb.FAMILY / "mega-net"
NTP_PLATFORM = mb.FAMILY / "mega-ntp" / "src" / "platform"
OUT = mb.BUILD / "warmtest"


def main():
    gen, image = MEGANET / "build" / "gen", MEGANET / "build" / "m65" / "meganet.bin"
    print("mega-net image:")
    mb.run([sys.executable, "build.py", "abi"], cwd=MEGANET, stdout=subprocess.DEVNULL)
    clang, lib = mb.mos_clang(), mb.ensure_libc()
    OUT.mkdir(parents=True, exist_ok=True)
    autoboot = OUT / "autoboot.prg"                  # the collection's dispatcher, as build.py makes it
    mb.run([mb.find_tool("petcat", env="PETCAT"), "-w65", "-o", autoboot, "--", ROOT / "src" / "autoboot.bas"])
    prg = OUT / "warmtest.prg"
    print("warmtest:")
    mb.run([clang, "-Oz", "-I", mb.LIBC_SRC / "include", "-I", gen,
            "-I", MEGANET / "src" / "abi", "-I", NTP_PLATFORM,
            "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
            SPIKE / "warmtest.c",
            NTP_PLATFORM / "m65_boot.c", NTP_PLATFORM / "m65_cbmdos.c",
            NTP_PLATFORM / "m65_f011.c", NTP_PLATFORM / "m65_scratch.c",
            gen / "meganet_tramp.c", MEGANET / "src" / "abi" / "meganet_vectors.c",
            lib, f"-Wl,-Map={OUT / 'warmtest.map'}", "-o", prg])
    print(f"  {prg.name}: {prg.stat().st_size} bytes")
    # ssh 5.6's loop miscompile, which this spike hit once (warmtest.c, ent[])
    objdump = mb.find_tool("llvm-objdump", [Path(clang).parent.parent])
    if subprocess.run([sys.executable, str(mb.FAMILY / "mega-ntp" / "tools" / "orphan_rmw.py"),
                       str(prg.with_suffix(".prg.elf")), objdump]).returncode:
        mb.die("warmtest: the miscompile of ssh 5.6; rewrite what it names")
    c1541 = mb.find_tool("c1541", env="C1541")
    d81 = OUT / "WARMTEST.D81"
    if d81.exists():
        d81.unlink()
    mb.run([c1541, "-format", "warmtest,wt", "d81", d81,
            "-write", prg, "warmtest", "-write", image, "meganet",
            "-write", autoboot, "autoboot.c65"], stdout=subprocess.DEVNULL)
    mb.run([c1541, "-attach", d81, "-dir"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
