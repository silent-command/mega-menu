/* The handoff block: how the menu, the dispatcher (AUTOBOOT.C65) and a
 * reset talk to each other.
 *
 * It sits in the last 16 bytes of the 32 KB colour RAM, $FF87FF0-$FF87FFF.
 * Nothing else goes there: the ROM's reset clears only the cells of the
 * screen, the clients fill at most 80x50 = 4000 cells from $FF80000, and
 * mega-net never touches colour RAM. Measured on ROM 920413: a byte
 * written at $FF87FF0 is still there after three resets through the
 * clients' own exit path.
 *
 * The clients know nothing of it. They leave as they always have, by the
 * KERNAL's reset entry; the ROM then boots AUTOBOOT.C65 from the mounted
 * disk, and the dispatcher reads the state here to decide what the reset
 * meant:
 *
 *   no magic   a cold start                     -> the menu
 *   IDLE       BASIC is in charge               -> nothing: READY.
 *   LAUNCH     the menu asked for NAME          -> RETURN, then RUN NAME
 *   RETURN     a program the menu started left  -> the menu
 *   MENU       the menu itself was reset        -> the menu
 *
 * The same numbers are written out again in src/autoboot.bas. */
#ifndef HANDOFF_H
#define HANDOFF_H

#define HANDOFF      0xff87ff0UL
#define HO_MAGIC0    77                  /* 'M' */
#define HO_MAGIC1    78                  /* 'N' */
#define HO_MAGIC2    67                  /* 'C' */
#define HO_STATE     3
#define HO_NAME      4                   /* the program's name on disk, capitals, zero padded */
#define HO_NAME_LEN  10
#define HO_THEME     14                  /* the menu's own: what it looked like, */
#define HO_SEL       15                  /* and where its pointer was */

#define HO_IDLE      1
#define HO_LAUNCH    2
#define HO_RETURN    3
#define HO_MENU      4                   /* the menu is on screen: a reset now comes back to it */

#endif
