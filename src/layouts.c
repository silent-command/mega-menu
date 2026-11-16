/* The layout themes: the menu redrawn in the manner of five other
 * systems. ui.h says how a layout gets its colours out of a screen with
 * one paper; tools/make_tiles.py draws the tiles named here.
 *
 * Each layout is four functions and a table at the end of its section:
 * the whole screen, one program's entry, whatever else follows the
 * selection about, and a place for a last word before the menu leaves.
 * None of them animates: these are still pictures.
 */
#include <string.h>
#include "ui.h"
#include "apps.h"
#include "tiles.h"

#define ROM_FONT_PLAIN 0x2d800UL         /* the ROM's usual lowercase set */
#define ROM_FONT_SERIF 0x3d800UL         /* its other one, with slab serifs: nearer a PC's */

static const char TITLE[] = "Mega Net Collection";
static const char SUBTITLE[] = "Internet programs for the MEGA65";

NOINLINE static void one(unsigned char x, unsigned char y, unsigned char colour, unsigned char code)
{
  cells(x, y, 1, colour, code);
}

/* Tiles cut from one shape sit at consecutive codes. */
NOINLINE static void run(unsigned char x, unsigned char y, unsigned char colour, unsigned char first, unsigned char n)
{
  unsigned char i;
  for (i = 0; i < n; i++) cells((unsigned char)(x + i), y, 1, colour, (unsigned char)(first + i));
}

NOINLINE static const char *digit(unsigned char n)
{
  static char s[2];
  s[0] = (char)('0' + n);
  return s;
}

NOINLINE static void put_centred(unsigned char x, unsigned char w, unsigned char y, unsigned char colour, const char *s)
{
  put((unsigned char)(x + (w - strlen(s)) / 2), y, colour, s);
}

/* Two strings a space apart. */
NOINLINE static void say(unsigned char x, unsigned char y, unsigned char ca, unsigned char cb, const char *a, const char *b)
{
  put(x, y, ca, a);
  put((unsigned char)(x + strlen(a) + 1), y, cb, b);
}

/* ==== Material ========================================================
 * An app bar under a status bar, a navigation drawer with a pill on the
 * selected entry, an outlined card for what is selected, and a floating
 * action button. White paper; every other surface is a reversed cell.
 */
#define M_TEXT 1
#define M_GREY 2
#define M_PRIMARY 3
#define M_DARK 4
#define M_ACCENT 5
#define M_LINE 6
#define M_SHADOW 7

static const rgb mat_pal[16] = {
  {0xff,0xff,0xff}, {0x21,0x21,0x21}, {0x75,0x75,0x75}, {0x3f,0x51,0xb5},
  {0x30,0x3f,0x9f}, {0xff,0x40,0x81}, {0xd0,0xd0,0xd0}, {0xb0,0xb0,0xb0},
};

#define MAT_PITCH (N_APPS <= 8 ? 2 : 1)  /* rows from one drawer entry to the next */
#define MAT_Y(i) (6 + (i) * MAT_PITCH)
#define MAT_CARD_X 25
#define MAT_CARD_W 54
#define MAT_TEXT_X 28

static void mat_item(unsigned char i)
{
  unsigned char y = (unsigned char)MAT_Y(i), on = (i == sel);
  cells(1, y, 21, M_TEXT, ' ');
  if (on) {
    one(1, y, M_PRIMARY, MAT_CAP_L);
    cells(2, y, 19, REV | M_PRIMARY, ' ');
    one(21, y, M_PRIMARY, MAT_CAP_R);
  }
  put(3, y, on ? REV | M_PRIMARY : M_GREY, digit((unsigned char)(i + 1)));
  put(6, y, on ? REV | M_PRIMARY : M_TEXT, apps[i].title);
}

static void mat_detail(void)
{
  box(MAT_CARD_X + 1, 10, MAT_CARD_W - 2, 3, M_TEXT, ' ');
  put(MAT_TEXT_X, 10, M_GREY, "PROGRAM");
  put(MAT_TEXT_X + 8, 10, M_GREY, digit((unsigned char)(sel + 1)));
  put(MAT_TEXT_X + 10, 10, M_GREY, "OF");
  put(MAT_TEXT_X + 13, 10, M_GREY, digit(N_APPS));
  put(MAT_TEXT_X, 11, M_TEXT, apps[sel].title);
  put(MAT_TEXT_X, 12, M_GREY, apps[sel].blurb);
}

static void mat_draw(void)
{
  unsigned char i, y;

  cells(0, 0, COLS, REV | M_DARK, ' ');                    /* the status bar */
  one(75, 0, REV | M_DARK, MAT_SIGNAL);
  one(77, 0, REV | M_DARK, MAT_BATTERY);
  box(0, 1, COLS, 3, REV | M_PRIMARY, ' ');                /* the app bar */
  run(2, 2, REV | M_PRIMARY, MAT_MENU_0, 2);
  put(7, 2, REV | M_PRIMARY, TITLE);
  one(76, 2, REV | M_PRIMARY, MAT_KEBAB);
  cells(0, 4, COLS, M_SHADOW, MAT_SHADOW);

  for (i = 0; i < N_APPS; i++) mat_item(i);                /* the drawer */
  y = (unsigned char)(MAT_Y(N_APPS - 1) + 2);
  cells(0, y, 23, M_LINE, MAT_HDIV);
  put(3, (unsigned char)(y + 1), M_GREY, "Q");
  put(6, (unsigned char)(y + 1), M_TEXT, "Exit to BASIC");
  box(23, 5, 1, ROWS - 5, M_LINE, MAT_VDIV);

  put(MAT_CARD_X + 1, 6, M_GREY, SUBTITLE);
  one(MAT_CARD_X, 8, M_SHADOW, MAT_CARD_TL);               /* the card */
  cells(MAT_CARD_X + 1, 8, MAT_CARD_W - 2, M_SHADOW, MAT_CARD_T);
  one(MAT_CARD_X + MAT_CARD_W - 1, 8, M_SHADOW, MAT_CARD_TR);
  box(MAT_CARD_X, 9, 1, 7, M_SHADOW, MAT_CARD_L);
  box(MAT_CARD_X + MAT_CARD_W - 1, 9, 1, 7, M_SHADOW, MAT_CARD_R);
  one(MAT_CARD_X, 16, M_SHADOW, MAT_CARD_BL);
  cells(MAT_CARD_X + 1, 16, MAT_CARD_W - 2, M_SHADOW, MAT_CARD_B);
  one(MAT_CARD_X + MAT_CARD_W - 1, 16, M_SHADOW, MAT_CARD_BR);
  one(MAT_TEXT_X, 14, M_PRIMARY, MAT_CAP_L);               /* a filled button */
  put(MAT_TEXT_X + 1, 14, REV | M_PRIMARY, " OPEN ");
  one(MAT_TEXT_X + 7, 14, M_PRIMARY, MAT_CAP_R);
  mat_detail();

  put(MAT_CARD_X + 1, 18, M_TEXT, "Cursor keys");
  put(MAT_CARD_X + 13, 18, M_GREY, "or");
  say(MAT_CARD_X + 16, 18, M_TEXT, M_GREY, "1-8", "select");
  say(MAT_CARD_X + 34, 18, M_TEXT, M_GREY, "RETURN", "open");
  say(MAT_CARD_X + 1, 19, M_TEXT, M_GREY, "T", "theme: Material");
  say(MAT_CARD_X + 34, 19, M_TEXT, M_GREY, "Q", "exit to BASIC");

  run(73, 21, M_ACCENT, MAT_FAB_0, 4);                     /* the floating button */
  run(73, 22, M_ACCENT, MAT_FAB_4, 4);
}

static void mat_message(const char *a, const char *b)
{
  cells(MAT_CARD_X + 1, 21, 47, M_TEXT, ' ');
  say(MAT_CARD_X + 1, 21, M_TEXT, M_PRIMARY, a, b);
}

const layout lay_material = {
  "Material", ROM_FONT_PLAIN, 1, mat_tiles, MAT_TILES, M_TEXT, mat_pal, mat_draw, mat_item, mat_detail, mat_message
};

/* ==== Aero ============================================================
 * One glass-framed window on a blue desktop, over a taskbar: caption
 * buttons, an address bar and a search box in the glass, a command bar,
 * a details list, and a details pane along the bottom.
 */
#define A_TEXT 1
#define A_DESK 2
#define A_GLASS 3
#define A_BUTTON 4
#define A_RED 5
#define A_TASK 6
#define A_SEL 7
#define A_GREY 8
#define A_COMMAND 9
#define A_SHADOW 10
#define A_LINE 11
#define A_ICON 12

static const rgb aero_pal[16] = {
  {0xff,0xff,0xff}, {0x18,0x18,0x18}, {0x1d,0x5f,0xa8}, {0x6f,0xa4,0xdc},
  {0x4c,0x83,0xc4}, {0xd8,0x45,0x3a}, {0x16,0x33,0x4f}, {0x33,0x90,0xf0},
  {0x6a,0x6a,0x6a}, {0x1e,0x39,0x5b}, {0x14,0x4a,0x86}, {0xc8,0xd2,0xde},
  {0x3f,0x7f,0xcc},
};

#define AERO_X 5                         /* the window, frame included */
#define AERO_W 70
#define AERO_Y 1
#define AERO_H 22
#define AERO_ITEMS 6                     /* the first program's row */

static void aero_item(unsigned char i)
{
  unsigned char y = (unsigned char)(AERO_ITEMS + i), on = (i == sel);
  unsigned char bar = REV | A_SEL;
  cells(AERO_X + 1, y, AERO_W - 2, on ? bar : A_TEXT, ' ');
  run(8, y, on ? bar : A_ICON, AERO_ICON_0, 2);
  put(11, y, on ? bar : A_GREY, digit((unsigned char)(i + 1)));
  put(14, y, on ? bar : A_TEXT, apps[i].title);
  put(25, y, on ? bar : A_GREY, apps[i].blurb);
}

static void aero_hints(void)
{
  cells(AERO_X + 1, 21, AERO_W - 2, A_TEXT, ' ');
  say(11, 21, A_COMMAND, A_GREY, "Cursor keys", "or");
  say(26, 21, A_COMMAND, A_GREY, "1-8", "select");
  say(39, 21, A_COMMAND, A_GREY, "RETURN", "open");
  say(53, 21, A_COMMAND, A_GREY, "T", "theme");
  say(63, 21, A_COMMAND, A_GREY, "Q", "exit");
}

static void aero_detail(void)
{
  cells(AERO_X + 1, 20, AERO_W - 2, A_TEXT, ' ');
  run(8, 20, A_ICON, AERO_ICON_0, 2);
  put(11, 20, A_TEXT, apps[sel].title);
  put(22, 20, A_GREY, apps[sel].blurb);
}

static void aero_draw(void)
{
  unsigned char i;

  box(0, 0, COLS, ROWS - 1, REV | A_DESK, ' ');            /* the desktop, and the window's shadow on it */
  box(AERO_X + AERO_W, AERO_Y + 1, 1, AERO_H, REV | A_SHADOW, ' ');
  cells(AERO_X + 1, AERO_Y + AERO_H, AERO_W, REV | A_SHADOW, ' ');

  box(AERO_X, AERO_Y, AERO_W, AERO_H, REV | A_GLASS, ' ');             /* the glass */
  box(AERO_X + 1, AERO_Y + 2, AERO_W - 2, AERO_H - 3, A_TEXT, ' ');    /* and what shows through it */
  put(AERO_X + 2, 1, REV | A_GLASS, TITLE);
  one(66, 1, REV | A_GLASS, AERO_MIN);
  one(68, 1, REV | A_GLASS, AERO_MAX);
  cells(70, 1, 3, REV | A_RED, ' ');
  one(71, 1, REV | A_RED, AERO_CLOSE);

  one(7, 2, REV | A_GLASS, AERO_BACK);                     /* the address bar */
  one(9, 2, REV | A_GLASS, AERO_FWD);
  cells(12, 2, 44, A_TEXT, ' ');
  one(13, 2, A_GREY, AERO_CHEVRON);
  put(15, 2, A_TEXT, "Computer");
  one(24, 2, A_GREY, AERO_CHEVRON);
  put(26, 2, A_TEXT, "Net Tools (8:)");
  one(41, 2, A_GREY, AERO_CHEVRON);
  cells(58, 2, 15, A_TEXT, ' ');
  put(59, 2, A_GREY, "Search");
  one(71, 2, A_GREY, AERO_MAG);

  put(8, 3, A_COMMAND, "Organize");                        /* the command bar */
  one(17, 3, A_COMMAND, AERO_DROP);
  put(21, 3, A_COMMAND, "Open");
  put(28, 3, A_COMMAND, "Change theme");
  put(43, 3, A_COMMAND, "Exit to BASIC");

  put(14, 4, A_GREY, "Name");                              /* the list */
  one(23, 4, A_LINE, AERO_VSEP);
  put(25, 4, A_GREY, "Description");
  cells(AERO_X + 1, 5, AERO_W - 2, A_LINE, AERO_LINE);
  for (i = 0; i < N_APPS; i++) aero_item(i);

  cells(AERO_X + 1, 19, AERO_W - 2, A_LINE, AERO_LINE);    /* the details pane */
  aero_detail();
  aero_hints();

  cells(0, 24, COLS, REV | A_TASK, ' ');                   /* the taskbar */
  run(1, 24, REV | A_TASK, AERO_ORB_0, 2);
  cells(5, 24, 23, REV | A_BUTTON, ' ');
  run(6, 24, REV | A_BUTTON, AERO_ICON_0, 2);
  put(9, 24, REV | A_BUTTON, TITLE);
  put(64, 24, REV | A_TASK, "Theme: Aero");
  one(79, 24, REV | A_BUTTON, ' ');
}

static void aero_message(const char *a, const char *b)
{
  cells(AERO_X + 1, 21, AERO_W - 2, A_TEXT, ' ');
  say(11, 21, A_TEXT, A_COMMAND, a, b);
}

const layout lay_aero = {
  "Aero", ROM_FONT_PLAIN, 1, aero_tiles, AERO_TILES, A_TEXT, aero_pal, aero_draw, aero_item, aero_detail, aero_message
};

/* ==== Workbench =======================================================
 * The blue, white, black and orange of Workbench 1.3: a screen title
 * bar, a disk window with a drawer for each program, one under another
 * so that the cursor keys move the way the screen reads, and a Shell
 * window beside it. The paper is the blue; the white bars are reversed
 * cells, so what is drawn on them comes out blue, as it did there.
 */
#define W_WHITE 1
#define W_BLACK 2
#define W_ORANGE 3

static const rgb wb_pal[16] = {
  {0x00,0x55,0xaa}, {0xff,0xff,0xff}, {0x00,0x00,0x22}, {0xff,0x88,0x00},
};

#define WB_Y 2                           /* both windows: the title bar's row and the height */
#define WB_H 22
#define WB_DISK_X 2                      /* the disk window */
#define WB_DISK_W 22
#define WB_SHELL_X 26                    /* the Shell window */
#define WB_SHELL_W 52
#define WB_ITEM_Y(i) (WB_Y + 2 + (i) * 2)                /* a drawer is two rows tall */
#define WB_NAME_X (WB_DISK_X + 7)
#define WB_TEXT_X (WB_SHELL_X + 2)
#define WB_TEXT_W (WB_SHELL_W - 4)
#define WB_PROMPT_Y (WB_Y + 12)

static void wb_window(unsigned char x, unsigned char w, const char *title)
{
  unsigned char x1 = (unsigned char)(x + w - 1), y1 = WB_Y + WB_H - 1;
  unsigned char after = (unsigned char)(x + 2 + strlen(title) + 2);
  run(x, WB_Y, REV | W_WHITE, WB_CLOSE_0, 2);
  cells((unsigned char)(x + 2), WB_Y, (unsigned char)(strlen(title) + 2), REV | W_WHITE, ' ');
  put((unsigned char)(x + 3), WB_Y, REV | W_WHITE, title);
  cells(after, WB_Y, (unsigned char)(x1 - 3 - after), REV | W_WHITE, WB_DRAG);
  run((unsigned char)(x1 - 3), WB_Y, REV | W_WHITE, WB_BACK_0, 2);
  run((unsigned char)(x1 - 1), WB_Y, REV | W_WHITE, WB_FRONT_0, 2);
  box(x, WB_Y + 1, 1, WB_H - 2, W_WHITE, WB_EDGE_L);
  box(x1, WB_Y + 1, 1, WB_H - 2, W_WHITE, WB_EDGE_R);
  one(x, y1, W_WHITE, WB_EDGE_BL);
  cells((unsigned char)(x + 1), y1, (unsigned char)(w - 2), W_WHITE, WB_EDGE_B);
  one(x1, y1, W_WHITE, WB_SIZE);
}

static void wb_item(unsigned char i)
{
  unsigned char y = (unsigned char)WB_ITEM_Y(i), on = (i == sel);
  run(WB_DISK_X + 2, y, on ? REV | W_ORANGE : W_WHITE, WB_DRAWER_0, 4);
  run(WB_DISK_X + 2, (unsigned char)(y + 1), on ? REV | W_ORANGE : W_WHITE, WB_DRAWER_4, 4);
  cells(WB_NAME_X, y, WB_DISK_W - 8, W_WHITE, ' ');
  if (on) cells(WB_NAME_X, y, (unsigned char)(strlen(apps[i].title) + 2), REV | W_WHITE, ' ');
  put(WB_NAME_X, y, on ? REV | W_WHITE : W_ORANGE, digit((unsigned char)(i + 1)));
  put(WB_NAME_X + 2, y, on ? REV | W_WHITE : W_WHITE, apps[i].title);
}

static void wb_detail(void)
{
  box(WB_TEXT_X, WB_Y + 3, WB_TEXT_W, 2, W_WHITE, ' ');
  put(WB_TEXT_X, WB_Y + 3, W_ORANGE, apps[sel].title);
  put(WB_TEXT_X, WB_Y + 4, W_WHITE, apps[sel].blurb);
}

/* The Shell's prompt, with the cursor after it: or a last word, the cursor on the line below. */
static void wb_prompt(const char *a, const char *b)
{
  box(WB_TEXT_X, WB_PROMPT_Y, WB_TEXT_W, 3, W_WHITE, ' ');
  put(WB_TEXT_X, WB_PROMPT_Y, W_WHITE, "1>");
  if (!a) { one(WB_TEXT_X + 3, WB_PROMPT_Y, REV | W_ORANGE, ' '); return; }
  put(WB_TEXT_X + 3, WB_PROMPT_Y, W_WHITE, a);
  put(WB_TEXT_X + 3, WB_PROMPT_Y + 1, W_ORANGE, b);
  one(WB_TEXT_X, WB_PROMPT_Y + 2, REV | W_ORANGE, ' ');
}

static void wb_draw(void)
{
  unsigned char i;

  cells(0, 0, COLS, REV | W_WHITE, ' ');                   /* the screen's title bar */
  put(1, 0, REV | W_WHITE, TITLE);
  run(76, 0, REV | W_WHITE, WB_BACK_0, 2);
  run(78, 0, REV | W_WHITE, WB_FRONT_0, 2);

  wb_window(WB_DISK_X, WB_DISK_W, "Net Tools");
  for (i = 0; i < N_APPS; i++) wb_item(i);

  wb_window(WB_SHELL_X, WB_SHELL_W, "Shell");
  put(WB_TEXT_X, WB_Y + 1, W_WHITE, SUBTITLE);
  wb_detail();
  say(WB_TEXT_X, WB_Y + 6, W_ORANGE, W_WHITE, "Cursor keys", "or");
  say(WB_TEXT_X + 15, WB_Y + 6, W_ORANGE, W_WHITE, "1-8", "select");
  say(WB_TEXT_X, WB_Y + 7, W_ORANGE, W_WHITE, "RETURN", "start");
  say(WB_TEXT_X, WB_Y + 8, W_ORANGE, W_WHITE, "T", "theme");
  say(WB_TEXT_X, WB_Y + 9, W_ORANGE, W_WHITE, "Q", "exit to BASIC");
  wb_prompt(0, 0);
}

const layout lay_workbench = {
  "Workbench", ROM_FONT_PLAIN, 0, wb_tiles, WB_TILES, 0, wb_pal, wb_draw, wb_item, wb_detail, wb_prompt
};

/* ==== DOS =============================================================
 * A full-screen text-mode editor with its File menu pulled down: the
 * programs are the menu's entries, and the status line tells of the one
 * under the bar. The paper is the light grey of the menus, so the blue
 * editing window is reversed cells and its text comes out grey on blue.
 */
#define D_BLACK 1
#define D_BLUE 2
#define D_WHITE 3

static const rgb dos_pal[16] = {
  {0xaa,0xaa,0xaa}, {0x00,0x00,0x00}, {0x00,0x00,0xaa}, {0xff,0xff,0xff},
};

#define DOS_MENU_X 1                     /* the pulled-down menu, frame included */
#define DOS_MENU_W 22
#define DOS_DOC_X 28                     /* the text in the editing window */

static void dos_item(unsigned char i)
{
  unsigned char y = (unsigned char)(2 + i), on = (i == sel);
  cells(DOS_MENU_X + 1, y, DOS_MENU_W - 2, on ? REV | D_BLACK : D_BLACK, ' ');
  put(DOS_MENU_X + 2, y, on ? REV | D_BLACK : D_WHITE, digit((unsigned char)(i + 1)));
  put(DOS_MENU_X + 5, y, on ? REV | D_BLACK : D_BLACK, apps[i].title);
}

static void dos_status(const char *a, const char *b)
{
  cells(0, 24, 58, D_BLACK, ' ');
  if (b) say(1, 24, D_BLACK, D_BLACK, a, b);
  else put(1, 24, D_BLACK, a);
}

static void dos_detail(void)
{
  dos_status(apps[sel].blurb, 0);
}

/* A menu's name, its first letter picked out in white. */
static void dos_menu_name(unsigned char x, const char *s)
{
  static char first[2];
  first[0] = s[0];
  put(x, 0, D_WHITE, first);
  put((unsigned char)(x + 1), 0, D_BLACK, s + 1);
}

static void dos_draw(void)
{
  unsigned char i, y;
  unsigned char bottom = DOS_MENU_X + DOS_MENU_W - 1;

  put(2, 0, REV | D_BLACK, " File ");                      /* the menu bar */
  dos_menu_name(9, "Edit");
  dos_menu_name(15, "Search");
  dos_menu_name(23, "View");
  dos_menu_name(29, "Options");
  dos_menu_name(38, "Help");

  box(0, 1, COLS, 23, REV | D_BLUE, ' ');                  /* the editing window */
  one(0, 1, REV | D_BLUE, DOS_TL);
  cells(1, 1, 78, REV | D_BLUE, DOS_H);
  one(79, 1, REV | D_BLUE, DOS_TR);
  put(40, 1, D_BLUE, " ");
  put(41, 1, D_BLUE, TITLE);
  put((unsigned char)(41 + sizeof TITLE - 1), 1, D_BLUE, " ");
  box(0, 2, 1, 21, REV | D_BLUE, DOS_V);
  one(79, 2, D_BLACK, DOS_ARROW_U);                        /* its scroll bars */
  box(79, 3, 1, 19, D_BLACK, DOS_SHADE);
  one(79, 3, REV | D_BLACK, ' ');
  one(79, 22, D_BLACK, DOS_ARROW_D);
  one(0, 23, REV | D_BLUE, DOS_BL);
  one(1, 23, D_BLACK, DOS_ARROW_L);
  cells(2, 23, 76, D_BLACK, DOS_SHADE);
  one(2, 23, REV | D_BLACK, ' ');
  one(78, 23, D_BLACK, DOS_ARROW_R);
  one(79, 23, REV | D_BLUE, DOS_BR);

  put(DOS_DOC_X, 3, REV | D_BLUE, SUBTITLE);               /* the text being edited */
  put(DOS_DOC_X, 5, REV | D_BLUE, "Cursor keys or 1-8    select a program");
  put(DOS_DOC_X, 6, REV | D_BLUE, "RETURN                start it");
  put(DOS_DOC_X, 7, REV | D_BLUE, "T                     next theme");
  put(DOS_DOC_X, 8, REV | D_BLUE, "Q                     exit to BASIC");

  one(DOS_MENU_X, 1, D_BLACK, DOS_TL);                     /* the File menu */
  cells(DOS_MENU_X + 1, 1, DOS_MENU_W - 2, D_BLACK, DOS_H);
  one(bottom, 1, D_BLACK, DOS_TR);
  for (i = 0; i < N_APPS + 2; i++) {
    one(DOS_MENU_X, (unsigned char)(2 + i), D_BLACK, DOS_V);
    one(bottom, (unsigned char)(2 + i), D_BLACK, DOS_V);
  }
  for (i = 0; i < N_APPS; i++) dos_item(i);
  y = 2 + N_APPS;
  one(DOS_MENU_X, y, D_BLACK, DOS_TEE_L);
  cells(DOS_MENU_X + 1, y, DOS_MENU_W - 2, D_BLACK, DOS_H);
  one(bottom, y, D_BLACK, DOS_TEE_R);
  y++;
  cells(DOS_MENU_X + 1, y, DOS_MENU_W - 2, D_BLACK, ' ');
  put(DOS_MENU_X + 2, y, D_WHITE, "Q");
  put(DOS_MENU_X + 5, y, D_BLACK, "Exit to BASIC");
  y++;
  one(DOS_MENU_X, y, D_BLACK, DOS_BL);
  cells(DOS_MENU_X + 1, y, DOS_MENU_W - 2, D_BLACK, DOS_H);
  one(bottom, y, D_BLACK, DOS_BR);
  box(bottom + 1, 2, 2, (unsigned char)(y - 1), REV | D_BLACK, ' ');   /* and its shadow */
  cells(DOS_MENU_X + 2, (unsigned char)(y + 1), DOS_MENU_W, REV | D_BLACK, ' ');

  cells(0, 24, COLS, D_BLACK, ' ');                        /* the status line */
  one(58, 24, D_BLACK, DOS_V);
  put(60, 24, D_BLACK, "T=Theme  Q=Exit");
  dos_detail();
}

const layout lay_dos = {
  "DOS", ROM_FONT_SERIF, 0, dos_tiles, DOS_TILES, D_BLACK, dos_pal, dos_draw, dos_item, dos_detail, dos_status
};

/* ==== Mac =============================================================
 * Black on white and nothing else: a menu bar with rounded screen
 * corners, a grey desktop made of alternate pixels, and one window with
 * a striped title bar, a close box and a shadow. The selected program's
 * icon and name are reversed, as a Finder list had them.
 */
#define C_BLACK 1

static const rgb mac_pal[16] = {
  {0xff,0xff,0xff}, {0x00,0x00,0x00},
};

#define MAC_X 4                          /* the window, frame included */
#define MAC_W 72
#define MAC_Y 3                          /* its title bar */
#define MAC_ITEMS (MAC_Y + 4)
#define MAC_FOOT (MAC_ITEMS + N_APPS + 2)                /* the row of key names */
#define MAC_END (MAC_X + MAC_W - 1)

static void mac_item(unsigned char i)
{
  unsigned char y = (unsigned char)(MAC_ITEMS + i), on = (i == sel);
  cells(MAC_X + 1, y, MAC_W - 2, C_BLACK, ' ');
  run(MAC_X + 3, y, on ? REV | C_BLACK : C_BLACK, MAC_ICON_0, 2);
  put(MAC_X + 6, y, C_BLACK, digit((unsigned char)(i + 1)));
  if (on) cells(MAC_X + 8, y, (unsigned char)(strlen(apps[i].title) + 2), REV | C_BLACK, ' ');
  put(MAC_X + 9, y, on ? REV | C_BLACK : C_BLACK, apps[i].title);
  put(MAC_X + 20, y, C_BLACK, apps[i].blurb);
}

static void mac_detail(void)
{
}

/* A row of the window that is a rule from edge to edge. */
static void mac_rule(unsigned char y)
{
  one(MAC_X, y, C_BLACK, MAC_RULE_L);
  cells(MAC_X + 1, y, MAC_W - 2, C_BLACK, MAC_RULE);
  one(MAC_END, y, C_BLACK, MAC_RULE_R);
}

static void mac_foot(const char *s)
{
  cells(MAC_X + 1, MAC_FOOT, MAC_W - 2, C_BLACK, ' ');
  put_centred(MAC_X, MAC_W, MAC_FOOT, C_BLACK, s);
}

static void mac_draw(void)
{
  unsigned char i;

  one(0, 0, C_BLACK, MAC_CORNER_TL);                       /* the menu bar */
  one(2, 0, C_BLACK, MAC_DIAMOND);
  put(5, 0, C_BLACK, "File  Edit  View  Special");
  one(79, 0, C_BLACK, MAC_CORNER_TR);
  cells(0, 1, COLS, C_BLACK, MAC_DESK_TOP);                /* the desktop */
  box(0, 2, COLS, ROWS - 2, C_BLACK, MAC_DESK);
  one(0, 24, C_BLACK, MAC_CORNER_BL);
  one(79, 24, C_BLACK, MAC_CORNER_BR);

  cells(MAC_X, MAC_Y - 1, MAC_W, C_BLACK, MAC_DESK_BOT);   /* the window: its top edge is the foot of the row above */
  box(MAC_X, MAC_Y, MAC_W, (unsigned char)(MAC_FOOT - MAC_Y + 1), C_BLACK, ' ');
  box(MAC_X, MAC_Y, 1, (unsigned char)(MAC_FOOT - MAC_Y + 1), C_BLACK, MAC_EDGE_L);
  box(MAC_END, MAC_Y, 1, (unsigned char)(MAC_FOOT - MAC_Y + 1), C_BLACK, MAC_EDGE_R);
  box(MAC_END + 1, MAC_Y, 1, (unsigned char)(MAC_FOOT - MAC_Y + 1), C_BLACK, MAC_SHADE_R);
  one(MAC_X, MAC_FOOT + 1, C_BLACK, MAC_UNDER_L);
  cells(MAC_X + 1, MAC_FOOT + 1, MAC_W - 1, C_BLACK, MAC_UNDER);
  one(MAC_END + 1, MAC_FOOT + 1, C_BLACK, MAC_UNDER_R);

  one(MAC_X, MAC_Y, C_BLACK, MAC_BAR_L);                   /* the title bar */
  cells(MAC_X + 1, MAC_Y, MAC_W - 2, C_BLACK, MAC_STRIPES);
  one(MAC_END, MAC_Y, C_BLACK, MAC_BAR_R);
  run(MAC_X + 2, MAC_Y, C_BLACK, MAC_CLOSE_0, 2);
  cells((unsigned char)(MAC_X + (MAC_W - sizeof TITLE - 1) / 2), MAC_Y, sizeof TITLE + 1, C_BLACK, ' ');
  put_centred(MAC_X, MAC_W, MAC_Y, C_BLACK, TITLE);
  mac_rule(MAC_Y + 1);

  put(MAC_X + 3, MAC_Y + 2, C_BLACK, digit(N_APPS));       /* the line under it */
  put(MAC_X + 5, MAC_Y + 2, C_BLACK, "items");
  put((unsigned char)(MAC_END - 2 - (sizeof SUBTITLE - 1)), MAC_Y + 2, C_BLACK, SUBTITLE);
  cells(MAC_X + 1, MAC_Y + 3, MAC_W - 2, C_BLACK, MAC_DRULE);

  for (i = 0; i < N_APPS; i++) mac_item(i);
  mac_rule(MAC_FOOT - 1);
  mac_foot("Cursor keys or 1-8 select    RETURN open    T theme    Q exit");
}

static void mac_message(const char *a, const char *b)
{
  static char line[64];
  strcpy(line, a);
  strcat(line, " ");
  strcat(line, b);
  mac_foot(line);
}

const layout lay_mac = {
  "Mac", ROM_FONT_PLAIN, 0, mac_tiles, MAC_TILES, C_BLACK, mac_pal, mac_draw, mac_item, mac_detail, mac_message
};
