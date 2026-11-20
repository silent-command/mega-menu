/* Copyright 2026 Justin Cameron
 * SPDX-License-Identifier: Apache-2.0 */
/* What the menu (menu.c) and its layout themes (layouts.c) share.
 *
 * A layout theme redraws the whole screen in the manner of another
 * system. The text screen gives every cell one ink and all cells one
 * paper, so a layout works like this:
 *
 *   - palette entry 0 is the paper, and the layout picks it to be the
 *     colour most of its text sits on;
 *   - the font's upper half holds every character reversed, so a cell
 *     drawn with REV shows paper-coloured marks on a block of its ink:
 *     that is how bars, panels and backgrounds of other colours are made;
 *   - codes 96-127 are the layout's own tiles (tools/make_tiles.py).
 */
#ifndef UI_H
#define UI_H

#define COLS 80
#define ROWS 25
#define REV 0x80                         /* with a colour: the cell reversed */

typedef struct { unsigned char r, g, b; } rgb;

typedef struct {
  const char *name;
  unsigned long font;                    /* where in the ROM its font is */
  unsigned char thin;                    /* draw the font with strokes one pixel wide */
  const unsigned char (*tiles)[8];
  unsigned char ntiles;
  unsigned char border;                  /* a palette entry */
  const rgb *pal;                        /* 16 colours, entry 0 the paper */
  void (*draw)(void);                    /* the whole screen */
  void (*item)(unsigned char i);         /* one program, selected or not */
  void (*detail)(void);                  /* whatever else tells of the selected program */
  void (*message)(const char *a, const char *b);   /* a last word before leaving */
} layout;

extern const layout lay_material, lay_aero, lay_workbench, lay_dos, lay_mac;

extern unsigned char sel;                /* the selected program */

/* Not inlined: the layouts call these hundreds of times, and each holds a DMA job or two. */
#define NOINLINE __attribute__((noinline))

NOINLINE void put(unsigned char x, unsigned char y, unsigned char colour, const char *s);
NOINLINE void cells(unsigned char x, unsigned char y, unsigned char n, unsigned char colour, unsigned char code);
NOINLINE void box(unsigned char x, unsigned char y, unsigned char w, unsigned char h, unsigned char colour, unsigned char code);

#endif
