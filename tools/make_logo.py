#!/usr/bin/env python3
# Copyright 2026 Justin Cameron
# SPDX-License-Identifier: Apache-2.0
"""Draw the "MEGA NET COLLECTION" header and pack it into character tiles.

The letters are an original rounded stroke face, drawn here from line
segments and arcs: no font file is read. Each letter is 4 character
cells wide and 3 tall (I is 2 wide). On the 80-column screen a pixel is
twice as tall as it is wide, so the design grid is 16 x 24 units a
letter with every unit two pixels across.

Output: a C header with the unique 8x8 tiles, the 3-row map of tile
codes, and a handful of extra tiles the menu draws with.

usage: make_logo.py OUT.h [--preview]
"""
import math, sys

TEXT = "MEGA NET COLLECTION"
ROWS = 3
H = ROWS * 8                 # 24 design units tall
STROKE = 3.3                 # design units
L, R, T, B = 2.1, 11.9, 1.9, 22.1
MX = (L + R) / 2
MY = (T + B) / 2
RAD = 4.2                    # corner radius of the round letters
FIRST_CODE = 128             # tiles take the font's reversed half


def arc(cx, cy, r, a0, a1, n=10):
    """Points on a circle; angles in degrees, 0 = east, 90 = south (screen y grows down)."""
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def bowl(open_top=None, open_bottom=None):
    """The round outline C, G and O share, counter-clockwise from the right side's upper end."""
    pts = []
    pts += arc(R - RAD, T + RAD, RAD, 0, -90)          # top right corner, going up and left
    pts += arc(L + RAD, T + RAD, RAD, -90, -180)       # top left
    pts += arc(L + RAD, B - RAD, RAD, 180, 90)         # bottom left
    pts += arc(R - RAD, B - RAD, RAD, 90, 0)           # bottom right
    return pts


def letter_paths(ch):
    if ch == "M":
        return [[(L, B), (L, T), (MX, MY + 2.5), (R, T), (R, B)]]
    if ch == "N":
        return [[(L, B), (L, T), (R, B), (R, T)]]
    if ch == "E":
        return [[(R, T), (L, T), (L, B), (R, B)], [(L, MY), (R - 2.2, MY)]]
    if ch == "L":
        return [[(L, T), (L, B), (R, B)]]
    if ch == "T":
        return [[(L - 0.6, T), (R + 0.6, T)], [(MX, T), (MX, B)]]
    if ch == "I":
        return [[(4.0, T), (4.0, B)]]
    if ch == "A":
        return [[(L, B)] + arc(L + RAD, T + RAD, RAD, 180, 270) + arc(R - RAD, T + RAD, RAD, 270, 360) + [(R, B)],
                [(L, MY + 2.2), (R, MY + 2.2)]]
    if ch == "O":
        b = bowl()
        return [b + [b[0]]]
    if ch == "C":
        b = bowl()
        return [[(R, T + RAD + 1.2)] + b + [(R, B - RAD - 1.2)]]
    if ch == "G":
        b = bowl()
        return [[(R, T + RAD + 1.2)] + b + [(R, MY + 1.2), (MX + 0.4, MY + 1.2)]]
    raise SystemExit(f"no drawing for {ch!r}")


def width_cells(ch):
    return 2 if ch in " I" else 4


def seg_dist(px, py, a, b):
    ax, ay = a; bx, by = b
    dx, dy = bx - ax, by - ay
    d2 = dx * dx + dy * dy
    t = 0 if d2 == 0 else max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / d2))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def render(ch):
    """A letter as rows of pixels: width_cells*8 across, 24 down."""
    w = width_cells(ch) * 8
    rows = [[0] * w for _ in range(H)]
    if ch == " ":
        return rows
    paths = letter_paths(ch)
    for y in range(H):
        for x in range(w):
            # 2x2 supersample; x is in real pixels, two to a design unit
            hits = 0
            for sx in (0.25, 0.75):
                for sy in (0.25, 0.75):
                    ux, uy = (x + sx) / 2.0, y + sy
                    if any(seg_dist(ux, uy, p[i], p[i + 1]) <= STROKE / 2 for p in paths for i in range(len(p) - 1)):
                        hits += 1
            rows[y][x] = 1 if hits >= 2 else 0
    return rows


def main():
    out = sys.argv[1]
    canvas = [[] for _ in range(H)]
    for ch in TEXT:
        g = render(ch)
        for y in range(H):
            canvas[y] += g[y]
    cols = len(canvas[0]) // 8
    tiles, index, tmap = [], {}, []
    for r in range(ROWS):
        line = []
        for c in range(cols):
            t = tuple(sum(canvas[r * 8 + j][c * 8 + i] << (7 - i) for i in range(8)) for j in range(8))
            if not any(t):
                line.append(32)
                continue
            if t not in index:
                index[t] = len(tiles); tiles.append(t)
            line.append(FIRST_CODE + index[t])
        tmap.append(line)

    # Extra tiles the menu draws with.
    extras = {
        "TILE_RULE":    (0, 0, 0, 0xff, 0xff, 0, 0, 0),                  # a rule through the middle of the cell
        # A pointer: every row 2 pixels longer than the last, to 8 and back. A
        # pixel here is twice as tall as wide, so 2 across to 1 down is a true
        # 45-degree edge: the smoothest this grid draws. Its second cell is empty.
        "TILE_ARROW_L": (0x00, 0xc0, 0xf0, 0xfc, 0xff, 0xfc, 0xf0, 0xc0),
        "TILE_ARROW_R": (0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
        "TILE_DOT":     (0, 0, 0x18, 0x3c, 0x3c, 0x18, 0, 0),
        "TILE_BLOCK":   (0xff,) * 8,
    }
    extra_codes = {}
    for name, t in extras.items():
        extra_codes[name] = FIRST_CODE + len(tiles); tiles.append(t)
    if len(tiles) > 128:
        raise SystemExit(f"{len(tiles)} tiles: more than the 128 the font's upper half holds")

    with open(out, "w") as f:
        f.write("/* Generated by tools/make_logo.py: do not edit. */\n")
        f.write(f"#define LOGO_COLS {cols}\n#define LOGO_ROWS {ROWS}\n#define LOGO_TILES {len(tiles)}\n")
        f.write(f"#define LOGO_FIRST_CODE {FIRST_CODE}\n")
        for name, code in extra_codes.items():
            f.write(f"#define {name} {code}\n")
        f.write("static const unsigned char logo_tiles[LOGO_TILES][8] = {\n")
        for t in tiles:
            f.write("  {" + ",".join(f"0x{b:02x}" for b in t) + "},\n")
        f.write("};\nstatic const unsigned char logo_map[LOGO_ROWS][LOGO_COLS] = {\n")
        for line in tmap:
            f.write("  {" + ",".join(str(v) for v in line) + "},\n")
        f.write("};\n")
    print(f"logo: {cols} x {ROWS} cells, {len(tiles)} tiles")
    if "--preview" in sys.argv:
        for y in range(H):
            print("".join("#" if v else "." for v in canvas[y][::1])[:320])


if __name__ == "__main__":
    main()
