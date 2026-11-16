#!/usr/bin/env python3
"""Build the Mega Net Collection: the menu, the dispatcher, and one disk
with every client on it.

  python3 build.py            the menu, AUTOBOOT.C65 and bin/NET-TOOLS.D81
  python3 build.py menu       the menu and the dispatcher only
  python3 build.py all        rebuild every client first, then the above
  python3 build.py clean      bin/NET-TOOLS-CLEAN.D81: the same disk with no
                              user's settings carried onto it, for giving away

The disk is gathered from the disks the clients' own builds leave in
their bin/ directories, so each client is built by its own build.py and
this script only collects. Files two clients share (the stack image,
ssh's overlays) must be byte-identical or the build stops: one MEGANET
serves every program on the disk.

Settings a user has made live in SEQ files (bookmarks, known hosts, the
BBS list). If a disk named by CARRY exists (default ../net-tools.d81,
then the previous bin/NET-TOOLS.D81), its SEQ files are carried onto the
new disk in place of the clients' defaults.

Overrides: LLVM_MOS_DIR, LIBC_SRC, C1541, PETCAT, CARRY, DEFAULT_THEME.
"""
import hashlib, os, platform, re, shutil, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"
GEN = BUILD / "gen"
BIN = ROOT / "bin"
FAMILY = ROOT.parent
IS_WINDOWS = platform.system() == "Windows"
LIBC_SRC = Path(os.environ.get("LIBC_SRC", FAMILY / "mega65-libc")).resolve()
LIBC_BUILD = BUILD / "libc"
DISK = "NET-TOOLS.D81"

# The menu, in order: the program's name on disk, its title, a line about
# it, its project, the disk its build leaves, and how that project builds.
APPS = [
    ("GOPHER", "Gopher", "Browse Gopherspace",                         "mega-gopher",     "GOPHER.D81", ["sh", "build-native.sh"]),
    ("GEMINI", "Gemini", "Browse Geminispace",                         "mega-gemini",     "GEMINI.D81", None),
    ("IRC",    "IRC",    "Chat on IRC networks",                       "mega-irc",        "IRC.D81",    None),
    ("BBS",    "BBS",    "Call bulletin boards",                       "mega-bbs-client", "BBS.D81",    None),
    ("SSH",    "SSH",    "Log in to another computer, securely",       "mega-ssh",        "SSH.D81",    None),
    ("SFTP",   "SFTP",   "Move files over SSH",                        "mega-sftp",       "SFTP.D81",   None),
    ("FTPC",   "FTP",    "Move files to and from FTP servers",         "mega-ftp",        "FTPC.D81",   None),
    ("NTP",    "NTP",    "Set the clock from an internet time server", "mega-ntp",        "NTP.D81",    None),
]
NOT_SETTINGS = {"zones"}                         # SEQ files that are a client's data, not a user's settings: never carried
# In the order of the themes table in src/menu.c: the colourings, then the layouts.
THEMES = ["neon", "amber", "phosphor", "ice", "grayscale", "sunset", "ocean", "material", "aero", "workbench", "dos", "mac"]


def die(msg):
    print(f"error: {msg}", file=sys.stderr); sys.exit(1)


def run(cmd, **kw):
    print("  " + " ".join(Path(c).name if os.sep in str(c) else str(c) for c in cmd))
    if subprocess.run([str(c) for c in cmd], **kw).returncode != 0:
        die("command failed")


def find_tool(exe, roots=(), env=None):
    if env and os.environ.get(env):
        return os.environ[env]
    name = exe + (".exe" if IS_WINDOWS else "")
    for r in roots:
        cand = Path(r) / "bin" / name
        if cand.is_file():
            return str(cand)
    found = shutil.which(name)
    if found:
        return found
    for cand in ("/opt/homebrew/bin/" + name, "/usr/local/bin/" + name):
        if Path(cand).is_file():
            return cand
    die(f"{exe} not found")


def mos_clang():
    roots = [os.environ["LLVM_MOS_DIR"]] if os.environ.get("LLVM_MOS_DIR") else []
    roots += [Path.home() / "llvm-mos", "/opt/llvm-mos", "/usr/local/llvm-mos"]
    return find_tool("mos-mega65-clang", roots)


def ensure_libc():
    lib = LIBC_BUILD / "src" / "libmega65libc.a"
    if lib.is_file():
        return lib
    if not LIBC_SRC.is_dir():
        die(f"mega65-libc not found at {LIBC_SRC}; set LIBC_SRC")
    print("building mega65-libc for llvm-mos:")
    prefix = Path(mos_clang()).parent.parent
    run(["cmake", f"-DCMAKE_PREFIX_PATH={prefix}", "-B", LIBC_BUILD, "-S", LIBC_SRC], stdout=subprocess.DEVNULL)
    run(["cmake", "--build", LIBC_BUILD], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return lib


def gen_headers():
    GEN.mkdir(parents=True, exist_ok=True)
    run([sys.executable, ROOT / "tools" / "make_logo.py", GEN / "logo.h"])
    run([sys.executable, ROOT / "tools" / "make_tiles.py", GEN / "tiles.h"])
    theme = os.environ.get("DEFAULT_THEME", "neon").lower()
    if theme not in THEMES:
        die(f"DEFAULT_THEME must be one of {', '.join(THEMES)}")
    for file, title, blurb, *_ in APPS:
        if len(file) > 10 or len(title) > 9 or len(blurb) > 48:
            die(f"{file}: the menu's columns hold a 10-letter name, a 9-letter title and a 48-letter line")
    with open(GEN / "apps.h", "w") as f:
        f.write("/* Generated by build.py from its APPS table: do not edit. */\n")
        f.write(f"#define N_APPS {len(APPS)}\n#define DEFAULT_THEME {THEMES.index(theme)}\n")
        f.write("static const struct { const char *file, *title, *blurb; } apps[N_APPS] = {\n")
        for file, title, blurb, *_ in APPS:
            f.write(f'  {{ "{file}", "{title}", "{blurb}" }},\n')
        f.write("};\n")


def build_menu(extra=()):
    clang = mos_clang(); lib = ensure_libc()
    gen_headers()
    BIN.mkdir(exist_ok=True)
    prg = BIN / "menu.prg"
    print("menu:")
    run([clang, "-Os", "-I", LIBC_SRC / "include", "-I", GEN, "-I", ROOT / "src",
         "-Wall", "-Wextra", "-Werror", *extra,
         ROOT / "src" / "menu.c", ROOT / "src" / "layouts.c", ROOT / "src" / "m65_reset.c", lib, f"-Wl,-Map={BIN / 'menu.map'}", "-o", prg])
    print(f"  {prg.name}: {prg.stat().st_size} bytes")
    print("dispatcher:")
    petcat = find_tool("petcat", env="PETCAT")
    run([petcat, "-w65", "-o", BIN / "autoboot.prg", "--", ROOT / "src" / "autoboot.bas"])
    return prg, BIN / "autoboot.prg"


def disk_dir(c1541, d81):
    """[(name, type)] of a disk's files, in directory order."""
    out = subprocess.run([c1541, "-attach", str(d81), "-dir"], capture_output=True, text=True).stdout
    return [(m.group(1), m.group(2)) for m in re.finditer(r'^\d+\s+"([^"]+)"\s+(prg|seq|usr|rel)', out, re.M)]


def disk_read(c1541, d81, name, kind, dest):
    spec = name if kind == "prg" else f"{name},{kind[0]}"
    r = subprocess.run([c1541, "-attach", str(d81), "-read", spec, str(dest)], capture_output=True, text=True)
    if r.returncode != 0 or not dest.is_file():
        die(f"cannot read {name} from {d81.name}")


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def carry_source():
    if os.environ.get("CARRY"):
        return Path(os.environ["CARRY"])
    for cand in (BIN / DISK, FAMILY / "net-tools.d81"):
        if cand.is_file():
            return cand
    return None


def build_disk(menu, autoboot, out_name=DISK, carry_settings=True):
    c1541 = find_tool("c1541", env="C1541")
    work = Path(tempfile.mkdtemp(prefix="net-tools-"))
    files = []                                     # (name, kind, path), in disk order
    seen = {}
    print("collecting:")
    for file, title, blurb, proj, d81name, _ in APPS:
        d81 = FAMILY / proj / "bin" / d81name
        if not d81.is_file():
            die(f"{d81} is missing: build {proj} first (or run: python3 build.py all)")
        names = disk_dir(c1541, d81)
        if (file.lower(), "prg") not in names:
            die(f"{d81.name} has no program named {file.lower()}")
        for name, kind in names:
            dest = work / f"{proj}--{name}.{kind}"
            disk_read(c1541, d81, name, kind, dest)
            if name in seen:
                if kind == "prg" and sha(seen[name][2]) != sha(dest):
                    die(f'"{name}" differs between {seen[name][3]} and {proj}: rebuild both against the same sources')
                continue
            seen[name] = (name, kind, dest, proj)
            files.append(seen[name])
        print(f"  {proj}: {', '.join(n for n, _ in names)}")

    carry = carry_source() if carry_settings else None
    carried = []
    if carry and carry.is_file():
        kept = work / "carry.d81"
        shutil.copy(carry, kept)                   # the old disk may be the one about to be replaced
        for name, kind in disk_dir(c1541, kept):
            if kind != "seq" or name in NOT_SETTINGS:
                continue
            dest = work / f"carry--{name}.seq"
            disk_read(c1541, kept, name, kind, dest)
            if name in seen:
                files[files.index(seen[name])] = (name, kind, dest, "carried")
            else:
                files.append((name, kind, dest, "carried"))
            seen[name] = (name, kind, dest, "carried")
            carried.append(name)
        print(f"  settings carried from {carry.name}: {', '.join(carried) if carried else 'none'}")

    out = BIN / out_name
    tmp = work / out_name
    cmd = [c1541, "-format", "net-tools,nt", "d81", tmp,
           "-write", menu, "menu", "-write", autoboot, "autoboot.c65"]   # the menu first, so LOAD"*" finds it; the ROM boots AUTOBOOT.C65 by name
    for name, kind, path, _ in files:
        cmd += ["-write", path, name if kind == "prg" else f"{name},{kind[0]}"]
    print("disk:")
    run(cmd, stdout=subprocess.DEVNULL)
    shutil.copy(tmp, out)
    run([c1541, "-attach", out, "-dir"])
    shutil.rmtree(work, ignore_errors=True)
    print(f"built {out}")


def build_clients():
    net = FAMILY / "mega-net"
    print("== mega-net")
    run([sys.executable, "build.py", "abi"], cwd=net, stdout=subprocess.DEVNULL)
    order = sorted(APPS, key=lambda a: a[3] == "mega-sftp")     # sftp builds on ssh's output
    for file, title, blurb, proj, d81, how in order:
        print(f"== {proj}")
        run(how or [sys.executable, "build.py"], cwd=FAMILY / proj, stdout=subprocess.DEVNULL)


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "disk"
    if target not in ("disk", "menu", "all", "clean"):
        die("usage: build.py [disk|menu|all|clean]")
    if target == "all":
        build_clients()
    extra = [f"-D{d}" for d in os.environ.get("MENU_DEFINES", "").split()]
    menu, autoboot = build_menu(extra)
    if target == "clean":
        build_disk(menu, autoboot, "NET-TOOLS-CLEAN.D81", carry_settings=False)
    elif target != "menu":
        build_disk(menu, autoboot)
    return 0


if __name__ == "__main__":
    sys.exit(main())
