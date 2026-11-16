# mega-menu

Net-Tools: one disk with every mega-net client on it, and
a menu to start them from.

```
python3 build.py        the menu, the dispatcher and bin/NET-TOOLS.D81
python3 build.py menu   the menu and the dispatcher only
python3 build.py all    rebuild mega-net and every client first
python3 build.py clean  bin/NET-TOOLS-CLEAN.D81: no user's settings on it, for giving away
```

`DEFAULT_THEME=amber python3 build.py` sets the theme a cold start shows
(`neon` unless set; pictures of all of them in `docs/themes/`). On the
machine, `T` steps through them and the choice is kept until power-off.

## Themes

Seven are colourings of the menu's own screen: `neon`, `amber`,
`phosphor`, `ice`, `grayscale`, `sunset`, `ocean`.

Five are layouts: the whole screen redrawn in the manner of another
system, with its colours, its shapes and the nearest font the machine
has.

| Theme | After | The programs are |
|---|---|---|
| `material` | Material Design | entries in a navigation drawer, with a card for the selected one |
| `aero` | Windows Aero | a details list in a glass-framed window over a taskbar |
| `workbench` | Amiga Workbench 1.3 | drawers one under another in a disk window, beside a Shell window |
| `dos` | a DOS text-mode editor | the entries of its pulled-down File menu |
| `mac` | the classic Mac OS Finder | a list in a striped-title window on a grey desktop |

The keys are the same in every theme. The menu bars, buttons and
gadgets in the layouts are pictures: nothing but the keys does anything.

How a layout is made (`src/ui.h`, `src/layouts.c`): the text screen
gives each cell one ink and all cells one paper. A layout picks as paper
the colour most of its text sits on, and the font's upper half holds
every character reversed, so a reversed cell is a block of its ink with
paper-coloured marks: every bar, panel and background of another colour
is made of those. Each layout also has up to 32 tiles of its own at
codes 96-127, drawn in `tools/make_tiles.py`. No raster tricks and no
VIC-III attributes are used.

Fonts: `dos` uses the ROM's slab-serif set at `$3D800`; `material` and
`aero` use the usual set with its 2-pixel strokes thinned to one on the
machine (three letters that spoils are redrawn in `make_tiles.py`); the
others use the usual set as it is. The font addresses are those of
ROM 920413.

To add a layout: its tiles in `make_tiles.py`, its section and `layout`
table in `layouts.c`, a line in `themes[]` in `menu.c`, and its name in
`THEMES` in `build.py`, in the same order.

## How it works

Two files on the disk do it, and the clients know nothing of either.

- `MENU` is the menu (`src/menu.c`). It never loads a program itself.
  It writes what was chosen into the handoff block and resets the
  machine through the KERNAL's reset entry, the way every client leaves.
- `AUTOBOOT.C65` is the dispatcher (`src/autoboot.bas`), ten lines of
  BASIC 65. The ROM boots it from the mounted disk after every such
  reset. It reads the handoff block and decides what the reset meant.

The handoff block is the last 16 bytes of colour RAM, `$FF87FF0`
(`src/handoff.h`). It survives a reset and nothing else uses it.

| State | Meaning | The dispatcher |
|---|---|---|
| none | cold start | runs `MENU` |
| IDLE | BASIC is in charge | does nothing: `READY.` |
| LAUNCH | the menu asked for a program | sets RETURN, runs it |
| RETURN | a program the menu started has left | runs `MENU` |
| MENU | the menu was on screen when the machine was reset | runs `MENU` |

So a program started from the menu comes back to the menu, and one
started with `RUN "NAME"` from BASIC comes back to BASIC. `Q` or
RUN/STOP in the menu leaves for BASIC; `RUN "MENU"` brings it back.

## Black between programs

The ROM's reset paints the screen and border blue and reloads the
palette, so each start and exit used to flash blue until the dispatcher
ran. Two things stop that:

- Each program's exit (`black_until_dispatch` in its `m65_exit.c`,
  `gopher_exit.S` for Gopher, `src/m65_reset.c` here) makes colour 6
  black in palette bank 3, the one BASIC draws from, and points the
  ROM's palette writes at bank 0 (`$D070 = $3F`). The reset leaves
  `$D070` alone (ROM 920413), so its blue comes out black. It does this
  only when the handoff block is present.
- The dispatcher sets border and background to black first, then puts
  `$D070` back to `$FF` and runs `PALETTE RESTORE`. It does so in every
  state, so BASIC is black too after a program or the menu has left.

BASIC is still blue before the disk has been booted once, and a program
started from that blue BASIC shows it while loading.

If a program leaves with the handoff block present but the collection
disk no longer mounted, nothing puts `$D070` back: BASIC then shows blue
as black and `PALETTE` changes do not show until the machine is reset.

## The disk

`build.py` gathers the files from each client's own `bin/*.D81`. A file
two clients share must be byte-identical or the build stops, since one
`MEGANET` serves them all. SEQ files holding a user's settings are
carried over from the previous `bin/NET-TOOLS.D81` (or `../net-tools.d81`
the first time, or the disk named by `CARRY`).

`MENU` is the first file on the disk, so `RUN "*"` starts it, and
`AUTOBOOT.C65` is found by name wherever it sits. The disk boots by
itself when it is the one mounted at power-on (named `MEGA65.D81` on the
SD card, or set as the default disk); otherwise mount it and type `BOOT`.

To add a program, add a line to `APPS` in `build.py`.

## Tested

On a MEGA65 with ROM 920413, over JTAG, and in Xemu with the same ROM:
the menu from `BOOT`, cursor keys, number keys and `T`; all eight
clients started from the menu, each on a black screen, and each quit
back to the menu; `Q` to BASIC; a client started from BASIC quitting
back to BASIC. No client was taken as far as a network connection.
The reset screen was seen black on the machine after the menu, Gopher,
BBS, FTP and NTP; the other four share NTP's exit file.

The layout themes were added later. On the machine: the disk booted,
`T` typed over JTAG through all twelve themes and back to the first,
and each layout read back with `m65 -S` (which redraws the screen from
the machine's memory: it is not a picture of the display). In Xemu
(ROM 920413): each drawn from a cold start with the
first and another row selected, stepping from a layout back into the
colourings, NTP started from `workbench`, and `Q` to BASIC from
`material`. No key can be pressed in those tests, so they use builds
that act by themselves: `MENU_DEFINES="AUTOSEL=2 AUTOTHEME=3"`,
`AUTOPICK=n`, `AUTOQUIT`.

One thing seen: a reset forced from outside (`m65 -F`) while IRC was
running left no disk mounted, so nothing booted and the machine stopped
at `READY.`; `MOUNT "NET-TOOLS.D81"` then `BOOT` brought the menu back.
The same reset during Gemini or NTP kept the disk and returned to the
menu.
