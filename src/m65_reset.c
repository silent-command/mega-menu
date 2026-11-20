/* Copyright 2026 Justin Cameron
 * SPDX-License-Identifier: Apache-2.0 */
/* Leaving through the KERNAL's reset entry.
 *
 * This is the family's exit (mega-ftp src/platform/m65_exit.c, where the
 * reasons are written out) without the Ethernet half, since the menu
 * never brings the controller up: drain the keyboard, give the font
 * pointer and the hot registers back to the ROM, and jump through
 * ($FFFC) from a stub in low RAM, outside the window the BASIC ROM maps
 * over. Hyppo does not run again, so the disk stays mounted; the ROM
 * then boots AUTOBOOT.C65 from it. */
#include <string.h>
#include "mega65/memory.h"
#include "m65_reset.h"

#define STUB_AT 0x1fb0

static const unsigned char stub[] = {
  0x78,                               /* sei */
  0xa9, 0x00, 0xaa, 0xa8, 0x4b,       /* lda #0 ; tax ; tay ; taz */
  0x5c, 0xea,                         /* map ; eom  -- no MAP anywhere */
  0xa9, 0x3f, 0x85, 0x01,             /* $01 = $3f */
  0xa9, 0x64, 0x8d, 0x30, 0xd0,       /* $D030 = $64: ROM at $C000 and $E000 */
  0xa9, 0x00, 0x8d, 0x6a, 0xd0,       /* $D06A = 0: the font's bank, after every other write */
  0x6c, 0xfc, 0xff                    /* jmp ($fffc) */
};

/* Black between programs (Mega Net Collection, mega-menu/README.md).
 *
 * The ROM's reset paints the screen and border colour 6 and reloads the
 * palette, so every exit flashed blue until the collection's dispatcher
 * ran and set black. The reset loads the palette into the bank $D070
 * selects for writing and leaves $D070 alone (ROM 920413, read from the
 * image and seen on the machine). So: colour 6 is made black here in
 * bank 3, the one BASIC draws from ($D070 = $FF as the machine starts),
 * and the ROM's writes are sent to bank 0. The dispatcher sets real
 * black, then puts $D070 and the palette back. It has to be bank 3:
 * shown from bank 0 the reset screen stayed blue (the BBS client, which
 * left on bank 0), the ROM's palette setting drawing from 3 regardless.
 *
 * Only when the collection's handoff block is there, since without the
 * dispatcher nothing would put them back. */
static void black_until_dispatch(void)
{
  if (lpeek(0xff87ff0UL) != 77 || lpeek(0xff87ff1UL) != 78 || lpeek(0xff87ff2UL) != 67) return;
  POKE(0xd070, 0xff);                                      /* show and write bank 3 */
  POKE(0xd106, 0); POKE(0xd206, 0); POKE(0xd306, 0);
  POKE(0xd070, 0x3f);                                      /* the ROM's writes: bank 0 */
}

void m65_reset(void)
{
  unsigned char frames, last;
  /* A quarter of a second for the key that asked to be released: the ROM
   * enters its monitor when RUN/STOP is down at reset. */
  last = PEEK(0xd7fa);
  for (frames = 0; frames < 12; ) {
    if (PEEK(0xd7fa) != last) { last = PEEK(0xd7fa); frames++; }
  }
  while (PEEK(0xd610)) POKE(0xd610, 0);
  POKE(0xd05d, (unsigned char)(PEEK(0xd05d) | 0x80));      /* hot registers on first, then the font */
  POKE(0xd068, 0x00); POKE(0xd069, 0xd8);
  POKE(0xd06a, 0x00);
  black_until_dispatch();
  memcpy((void *)STUB_AT, stub, sizeof stub);
  __asm__ volatile("jmp $1fb0");
  for (;;) ;
}
