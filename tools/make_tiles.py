#!/usr/bin/env python3
# Copyright 2026 Justin Cameron
# SPDX-License-Identifier: Apache-2.0
"""Draw the tiles the layout themes build their screens from.

A layout theme (src/layouts.c) redraws the whole menu in the manner of
some other system. Each has up to 32 tiles of its own, which the menu
puts at character codes 96-127 of its font; the font's upper half then
holds every character reversed, so a tile at code c is also there at
c + 128 with ink and paper exchanged.

A tile is 8 x 8 pixels, '#' for ink. On the 80-column screen a pixel is
twice as tall as it is wide: a 2-pixel upright line is as thick as a
1-pixel level one, and a circle is twice as many pixels across as down.
Shapes wider than a cell are drawn on one canvas and cut into tiles
named NAME_0, NAME_1, ... left to right, top row first.

usage: make_tiles.py OUT.h
"""
import sys

FIRST_CODE = 96
MAX_TILES = 32


def art(text):
    rows = [r.strip() for r in text.strip().splitlines()]
    if len(set(len(r) for r in rows)) != 1 or len(rows[0]) % 8 or len(rows) % 8:
        raise SystemExit(f"bad tile art:\n{text}")
    return [[c == "#" for c in r] for r in rows]


def canvas(w, h, fn):
    return [[bool(fn(x, y)) for x in range(w)] for y in range(h)]


def rows_of(*patterns):
    """A tile from 8 row strings."""
    return art("\n".join(patterns))


def lines(levels=(), uprights=(), w=8):
    """Level lines at the given pixel rows, upright ones at the given pixel columns."""
    return canvas(w, 8, lambda x, y: y in levels or x in uprights)


def over(a, b):
    return [[p or q for p, q in zip(ra, rb)] for ra, rb in zip(a, b)]


def hflip(t):
    return [list(reversed(r)) for r in t]


def vflip(t):
    return list(reversed(t))


def disc(w, h, hole=None):
    """An ellipse filling a w x h pixel canvas: a circle on screen when w is 2h."""
    def inside(x, y):
        dx, dy = (x + 0.5 - w / 2) / (w / 2), (y + 0.5 - h / 2) / (h / 2)
        return dx * dx + dy * dy <= 1.0 and not (hole and hole(x, y))
    return canvas(w, h, inside)


DITHER = canvas(8, 8, lambda x, y: (x + y) % 2 == 0)

WINDOW_ICON = art("""
    ................
    ..############..
    ..############..
    ..#..........#..
    ..#..........#..
    ..#..........#..
    ..############..
    ................""")


# ---- Material ----------------------------------------------------------

def material():
    card_tl = rows_of("........", "........", "........", "....####", "..##....", ".##.....", ".##.....", ".##.....")
    play = lambda x, y: 11 <= x <= 22 and abs(y + 0.5 - 8) <= 3.2 * (1 - (x + 0.5 - 11) / 12)
    return [
        ("SIGNAL",  rows_of("........", "......##", "......##", "...##.##", "...##.##", "##.##.##", "##.##.##", "........")),
        ("BATTERY", rows_of("........", ".####...", "######..", "######..", "######..", "######..", "######..", "........")),
        ("MENU",    canvas(16, 8, lambda x, y: y in (1, 3, 5) and 2 <= x <= 13)),
        ("KEBAB",   canvas(8, 8, lambda x, y: y in (1, 3, 5) and x in (3, 4))),
        ("SHADOW",  rows_of("########", "#.#.#.#.", ".#...#..", "........", "........", "........", "........", "........")),
        ("CAP_L",   [r[:8] for r in disc(16, 8)]),
        ("CAP_R",   [r[8:] for r in disc(16, 8)]),
        ("VDIV",    lines(uprights=(3,))),
        ("HDIV",    lines(levels=(3,))),
        ("CARD_TL", card_tl),
        ("CARD_TR", hflip(card_tl)),
        ("CARD_BL", vflip(card_tl)),
        ("CARD_BR", hflip(vflip(card_tl))),
        ("CARD_T",  lines(levels=(3,))),
        ("CARD_B",  lines(levels=(4,))),
        ("CARD_L",  lines(uprights=(1, 2))),
        ("CARD_R",  lines(uprights=(5, 6))),
        ("FAB",     disc(32, 16, hole=play)),
    ]


# ---- Aero --------------------------------------------------------------

def aero():
    back = rows_of("........", ".....##.", "...####.", ".######.", "...####.", ".....##.", "........", "........")
    return [
        ("MIN",     rows_of("........", "........", "........", "........", "........", ".######.", "........", "........")),
        ("MAX",     rows_of("........", "........", ".######.", ".#....#.", ".#....#.", ".######.", "........", "........")),
        ("CLOSE",   rows_of("........", "........", ".##..##.", "..####..", "..####..", ".##..##.", "........", "........")),
        ("BACK",    back),
        ("FWD",     hflip(back)),
        ("CHEVRON", rows_of("........", "........", "..##....", "..####..", "..##....", "........", "........", "........")),
        ("DROP",    rows_of("........", "........", "........", ".######.", "..####..", "...##...", "........", "........")),
        ("MAG",     rows_of("........", "..###...", ".#...#..", ".#...#..", "..###...", ".....##.", "......##", "........")),
        ("LINE",    lines(levels=(0,))),
        ("VSEP",    lines(uprights=(3,))),
        ("ICON",    WINDOW_ICON),
        ("ORB",     disc(16, 8, hole=lambda x, y: 6 <= x <= 9 and 3 <= y <= 4)),
    ]


# ---- Workbench ---------------------------------------------------------

def workbench():
    def depth(front):
        a = lambda x, y: 1 <= x <= 9 and 1 <= y <= 4          # the upper-left sheet
        b = lambda x, y: 5 <= x <= 14 and 3 <= y <= 6         # the lower-right one
        edge = lambda x, y, x0, x1, y0, y1: x in (x0, x1) or y in (y0, y1)
        if front:
            return canvas(16, 8, lambda x, y: a(x, y) or (b(x, y) and edge(x, y, 5, 14, 3, 6)))
        return canvas(16, 8, lambda x, y: b(x, y) or (a(x, y) and edge(x, y, 1, 9, 1, 4)))
    drawer = art("""
        ................................
        ..############################..
        ..##........................##..
        ..##..####################..##..
        ..##..##................##..##..
        ..##..##.....######.....##..##..
        ..##..##.....##..##.....##..##..
        ..##..##.....######.....##..##..
        ..##..##................##..##..
        ..##..####################..##..
        ..##........................##..
        ..############################..
        ....########################....
        ................................
        ................................
        ................................""")
    return [
        ("CLOSE",   art("""
            ................
            .##############.
            .##..........##.
            .##...####...##.
            .##...####...##.
            .##..........##.
            .##############.
            ................""")),
        ("DRAG",    lines(levels=(2, 5))),
        ("BACK",    depth(False)),
        ("FRONT",   depth(True)),
        ("EDGE_L",  lines(uprights=(0, 1))),
        ("EDGE_R",  lines(uprights=(6, 7))),
        ("EDGE_B",  lines(levels=(7,))),
        ("EDGE_BL", lines(levels=(7,), uprights=(0, 1))),
        ("SIZE",    rows_of("......##", "......##", "......##", "..######", "..#...##", "..#...##", "..#...##", "########")),
        ("DRAWER",  drawer),
    ]


# ---- DOS ---------------------------------------------------------------

def dos():
    v, h = lines(uprights=(3, 4)), lines(levels=(3,))
    right = canvas(8, 8, lambda x, y: y == 3 and x >= 3)       # half a level line, from the upright rightwards
    left = canvas(8, 8, lambda x, y: y == 3 and x <= 4)
    down = canvas(8, 8, lambda x, y: x in (3, 4) and y >= 3)
    up = canvas(8, 8, lambda x, y: x in (3, 4) and y <= 3)
    arrow_u = rows_of("........", "...##...", "..####..", ".######.", "...##...", "...##...", "...##...", "........")
    arrow_l = rows_of("........", "....##..", "..####..", ".#######", "..####..", "....##..", "........", "........")
    return [
        ("H",       h),
        ("V",       v),
        ("TL",      over(right, down)),
        ("TR",      over(left, down)),
        ("BL",      over(right, up)),
        ("BR",      over(left, up)),
        ("TEE_L",   over(v, right)),
        ("TEE_R",   over(v, left)),
        ("ARROW_U", arrow_u),
        ("ARROW_D", vflip(arrow_u)),
        ("ARROW_L", arrow_l),
        ("ARROW_R", hflip(arrow_l)),
        ("SHADE",   canvas(8, 8, lambda x, y: (x + 2 * y) % 4 == 0)),
    ]


# ---- Mac ---------------------------------------------------------------

def mac():
    corner = rows_of("####....", "##......", "#.......", "#.......", "........", "........", "........", "........")
    stripes = lines(levels=(1, 3, 5))
    bar_l = canvas(8, 8, lambda x, y: x < 2 or (y in (1, 3, 5) and x >= 3))
    shade_r = canvas(8, 8, lambda x, y: x < 2 or DITHER[y][x])
    under = canvas(8, 8, lambda x, y: y < 2 or DITHER[y][x])
    return [
        ("DESK",      DITHER),
        ("DESK_TOP",  over(DITHER, lines(levels=(0,)))),
        ("DESK_BOT",  over(DITHER, lines(levels=(7,)))),
        ("CORNER_TL", corner),
        ("CORNER_TR", hflip(corner)),
        ("CORNER_BL", over(DITHER, vflip(corner))),
        ("CORNER_BR", over(DITHER, hflip(vflip(corner)))),
        ("DIAMOND",   rows_of("........", "...##...", "..####..", ".######.", "..####..", "...##...", "........", "........")),
        ("STRIPES",   stripes),
        ("BAR_L",     bar_l),
        ("BAR_R",     hflip(bar_l)),
        ("CLOSE",     art("""
            ................
            #.############.#
            ..#..........#..
            #.#..........#.#
            ..#..........#..
            #.############.#
            ................
            ................""")),
        ("EDGE_L",    lines(uprights=(0, 1))),
        ("EDGE_R",    lines(uprights=(6, 7))),
        ("RULE",      lines(levels=(0,))),
        ("RULE_L",    lines(levels=(0,), uprights=(0, 1))),
        ("RULE_R",    lines(levels=(0,), uprights=(6, 7))),
        ("DRULE",     lines(levels=(2, 4))),
        ("ICON",      WINDOW_ICON),
        ("SHADE_R",   shade_r),                                         # the window's shadow down its right side
        ("UNDER",     under),                                           # its bottom edge and the shadow beneath
        ("UNDER_L",   canvas(8, 8, lambda x, y: y == 0 or (y == 1 and x >= 2) or (y > 1 and DITHER[y][x]))),
        ("UNDER_R",   canvas(8, 8, lambda x, y: (y < 2 and x < 2) or DITHER[y][x])),
    ]


# ---- the thin font -----------------------------------------------------

# Material and Aero take the ROM font with every 2-pixel stroke halved.
# That is done on the machine and suits most letters; these it spoils,
# so they are drawn here instead. Keyed by screen code.
THIN = {
    13: rows_of("........", "........", ".##.#...", ".#.#.#..", ".#.#.#..", ".#.#.#..", ".#.#.#..", "........"),   # m
    77: rows_of(".#....#.", ".##..##.", ".#.##.#.", ".#.##.#.", ".#....#.", ".#....#.", ".#....#.", "........"),   # M
    78: rows_of(".#...#..", ".##..#..", ".##..#..", ".#.#.#..", ".#..##..", ".#..##..", ".#...#..", "........"),   # N
}


def pack(shape, cx=0, cy=0):
    return tuple(sum(shape[cy * 8 + j][cx * 8 + i] << (7 - i) for i in range(8)) for j in range(8))


def main():
    out = sys.argv[1]
    with open(out, "w") as f:
        f.write("/* Generated by tools/make_tiles.py: do not edit. */\n")
        f.write(f"#define TILE_FIRST_CODE {FIRST_CODE}\n")
        f.write(f"#define THIN_FIXES {len(THIN)}\n")
        f.write("static const struct { unsigned char code, rows[8]; } thin_fixes[THIN_FIXES] = {\n")
        for code, shape in THIN.items():
            f.write(f"  {{ {code}, {{" + ",".join(f"0x{b:02x}" for b in pack(shape)) + "} },\n")
        f.write("};\n")
        for prefix, shapes in (("MAT", material()), ("AERO", aero()), ("WB", workbench()), ("DOS", dos()), ("MAC", mac())):
            tiles = []
            for name, shape in shapes:
                cells_w, cells_h = len(shape[0]) // 8, len(shape) // 8
                many = cells_w * cells_h > 1
                for cy in range(cells_h):
                    for cx in range(cells_w):
                        n = cy * cells_w + cx
                        f.write(f"#define {prefix}_{name}{'_' + str(n) if many else ''} {FIRST_CODE + len(tiles)}\n")
                        tiles.append(pack(shape, cx, cy))
            if len(tiles) > MAX_TILES:
                raise SystemExit(f"{prefix}: {len(tiles)} tiles, more than the {MAX_TILES} a layout has room for")
            f.write(f"#define {prefix}_TILES {len(tiles)}\n")
            f.write(f"static const unsigned char {prefix.lower()}_tiles[{prefix}_TILES][8] = {{\n")
            for t in tiles:
                f.write("  {" + ",".join(f"0x{b:02x}" for b in t) + "},\n")
            f.write("};\n")
            print(f"tiles: {prefix} {len(tiles)}")


if __name__ == "__main__":
    main()
