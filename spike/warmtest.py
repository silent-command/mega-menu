#!/usr/bin/env python3
"""Run the warm-start measurement on the MEGA65 and print what it found.

    python3 spike/warmtest.py           put the disk, start it, wait, report
    python3 spike/warmtest.py --report  only read the log back

Build first (spike/build_warmtest.py). The port is MEGA65_PORT, as for
the other tools (tcp#127.0.0.1:4510 through an ssh tunnel to the
keyboard bridge works where macOS blocks the LAN). The machine is reset,
WARMTEST.D81 put on the card's root, mounted, and RUN "WARMTEST" typed;
it then resets itself through the dispatcher once per trial."""
import os, re, statistics, subprocess, sys, time
from pathlib import Path

SPIKE = Path(__file__).resolve().parent
ROOT = SPIKE.parent
TOOLS = ROOT.parent / "mega-net" / "tools"
sys.path.insert(0, str(TOOLS))
import device, mon                      # noqa: E402

D81 = ROOT / "build" / "warmtest" / "WARMTEST.D81"
LOG = 0xFF84000
MODES = {1: "cold (today)", 2: "warm, saved lease", 3: "warm, new DHCP"}
RESULTS = {0: "did not finish", 1: "ok", 2: "load failed", 3: "no lease", 4: "no DNS answer"}


def run(args, timeout=120):
    r = subprocess.run([str(a) for a in args], capture_output=True, text=True, timeout=timeout)
    return r.stdout + r.stderr


PI = os.environ.get("WARMTEST_PI", "pi@192.168.1.225")


def type_on_pi(text):
    """Type through mega-keybridge's own --type: m65 -T through the bridge
    holds each key a network round trip, long enough for the ROM's key
    repeat (seen: a screen scrolled blank by RETURNs)."""
    out = run(["ssh", "-o", "BatchMode=yes", "-o", "HostKeyAlias=pi3", PI,
               "systemctl --user stop mega-keybridge; python3 ~/mega-keybridge/keybridge.py --type '%s';"
               " systemctl --user start mega-keybridge" % text])
    if "typed" not in out:
        sys.exit("typing on the Pi failed:\n" + out)
    time.sleep(2)                                   # the bridge's TCP port back


def close(fd):
    """Let the bridge go at once: it takes one client, and a connection
    left to the garbage collector made every other check "monitor busy"."""
    if hasattr(fd, "s"):
        fd.s.close()


def read_mem(fd, addr, n):
    data = []
    for a in range(addr, addr + n, 16):
        out = mon.command(fd, "\x15m %x" % a, wait=1.0)
        m = re.search(r":0*%X:([0-9A-F]{32})" % a, out.upper())
        if not m:
            raise RuntimeError("no answer reading %x: %r" % (a, out))
        data += bytes.fromhex(m.group(1))
    return bytes(data)


def log(fd):
    head = read_mem(fd, LOG, 16)
    if head[:2] != b"WT":
        return None, head
    n = head[2]
    return read_mem(fd, LOG + 16, 16 * n), head


def report():
    fd = mon.open_port()
    body, head = log(fd)
    if body is None:
        sys.exit("no log on the machine (head %s)" % head.hex())
    n, done, ntsc = head[2], head[3], head[4]
    hz = 60 if ntsc else 50
    print("%d of %d trials, %d Hz frames\n" % (done, n, hz))
    print(" #  mode                 result   $D6E0 image lease   load  init  dhcp   dns  total   irqs stray tries")
    rows = []
    for i in range(done):
        e = body[16 * i:16 * i + 16]
        mode, res, d6e0, flags = e[0], e[1], e[2], e[3]
        load, init, dhcp, dns, irqs = (int.from_bytes(e[k:k + 2], "little") for k in (4, 6, 8, 10, 12))
        total = load + init + dhcp + dns
        rows.append((mode, res, load, init, dhcp, dns, total))
        print("%2d  %-19s  %-7s   $%02X   %-5s %-5s %5.2f %5.2f %5.2f %5.2f %6.2f %6d %5d %5d" % (
            i + 1, MODES.get(mode, mode), RESULTS.get(res, res), d6e0,
            "kept" if flags & 1 else "-", "kept" if flags & 2 else "-",
            load / hz, init / hz, dhcp / hz, dns / hz, total / hz, irqs, e[14], e[15]))
    print("\nmedian seconds, trials that finished:")
    for mode in (1, 2, 3):
        ok = [r for r in rows if r[0] == mode and r[1] == 1]
        bad = [r for r in rows if r[0] == mode and r[1] != 1]
        if not ok:
            print("  %-19s none ok (%d failed)" % (MODES[mode], len(bad)))
            continue
        med = [statistics.median(r[k] for r in ok) / hz for k in range(2, 7)]
        print("  %-19s load %.2f  init %.2f  dhcp %.2f  dns %.2f  total %.2f   (%d ok, %d failed)" % (
            MODES[mode], *med, len(ok), len(bad)))
    return 0


def main():
    if "--report" in sys.argv:
        return report()
    if not D81.exists():
        sys.exit("build first: python3 spike/build_warmtest.py")
    port = device.port()
    m65, ftp = device.find_tool("m65"), device.find_tool("mega65_ftp")
    print("port", port)
    print("reset, and put", D81.name)
    run([m65, "-l", port, "-F"]); time.sleep(2)
    out = run([ftp, "-l", port, "-c", "put %s WARMTEST.D81" % D81], timeout=300)
    if "bytes" not in out and "in " not in out:
        sys.exit("the upload did not report success:\n" + out)
    print("reset, mount, run")
    run([m65, "-l", port, "-F"]); time.sleep(8)
    type_on_pi('mount "warmtest.d81"\\n'); time.sleep(3)
    type_on_pi('run "warmtest"\\n')
    last, since = -1, time.time()
    while True:
        time.sleep(15)
        fd = None
        try:
            fd = mon.open_port()
            head = read_mem(fd, LOG, 16)
        except (RuntimeError, SystemExit, OSError) as e:
            print("  (no reading: %s)" % e)
            continue
        finally:
            close(fd)
        done = head[3] if head[:2] == b"WT" else 0
        print("  %d of %d" % (done, head[2] if head[:2] == b"WT" else 0))
        if head[:2] == b"WT" and done >= head[2]:
            break
        if done != last:
            last, since = done, time.time()
        elif time.time() - since > 120:
            print("no progress for two minutes: stopped there")
            break
    print()
    time.sleep(2)
    return report()


if __name__ == "__main__":
    sys.exit(main())
