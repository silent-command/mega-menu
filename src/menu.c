/* The Mega Net Collection menu.
 *
 * One screen: an animated header, the list of programs, a key line. It
 * never touches the network. Choosing a program does not load it from
 * here: the menu leaves a request in the handoff block (handoff.h) and
 * resets the machine the way every client in the family leaves
 * (m65_reset.c). The ROM then boots AUTOBOOT.C65, the dispatcher, which
 * runs the program from a clean BASIC, exactly as RUN "NAME" typed by
 * hand would, and brings the menu back when the program resets in turn.
 *
 * The screen is black on black-bordered by decision, whatever BASIC or
 * a client had before. Everything else is palette: the 16 colours are
 * redefined here and put back before leaving.
 *
 *   0       black            border, background
 *   1       text
 *   2       dim text
 *   3       accent           key names, numbers
 *   4       highlight        the selected row; it breathes
 *   6..15   the header's gradient, rotated a step a frame
 *
 * That is the menu's own look, in several colourings. The layout themes
 * (layouts.c, ui.h) replace all of it: font, palette and every cell.
 */
#include <string.h>
#include "mega65/memory.h"
#include "ui.h"
#include "handoff.h"
#include "m65_reset.h"
#include "logo.h"
#include "apps.h"
#include "tiles.h"

#define SCREEN      0x0800U
#define COLOUR_RAM  0xff80000UL
#define ROM_FONT    0x2d800UL            /* the ROM's lowercase set */

#define C_TEXT 1
#define C_DIM 2
#define C_ACCENT 3
#define C_HILITE 4
#define C_GRAD0 6
#define N_GRAD 10

#define ROW_LOGO 2
#define ROW_RULE 6
#define ROW_SUB 7
#define ROW_ITEMS 10
#define ROW_QUIT (ROW_ITEMS + N_APPS + 1)
#define ROW_RULE2 22
#define ROW_KEYS 23
#define COL_LOGO ((COLS - LOGO_COLS) / 2)
#define COL_ITEMS 16

/* ---- themes --------------------------------------------------------- */

typedef struct {
  const char *name;
  rgb text, dim, accent, hilite;
  unsigned char nkeys;
  rgb keys[6];                           /* the gradient, a closed loop through these */
  const layout *lay;                     /* or a layout of its own, and none of the above */
} theme;

static const theme themes[] = {
  { "Neon",   {0xd8,0xe8,0xff}, {0x60,0x70,0xa8}, {0x00,0xe5,0xff}, {0xff,0x60,0xd0}, 4,
    { {0x00,0xe5,0xff}, {0x30,0x60,0xff}, {0xa0,0x40,0xff}, {0xff,0x40,0xc0} }, 0 },
  { "Amber",   {0xff,0xd8,0x90}, {0xa0,0x68,0x20}, {0xff,0xb0,0x00}, {0xff,0xff,0xff}, 4,
    { {0xff,0x38,0x00}, {0xff,0x90,0x00}, {0xff,0xe0,0x40}, {0xff,0x90,0x00} }, 0 },
  { "Phosphor",{0x70,0xff,0x98}, {0x20,0x88,0x48}, {0xb8,0xff,0x40}, {0xff,0xff,0xff}, 3,
    { {0x00,0xff,0x60}, {0xa0,0xff,0x00}, {0x00,0xc8,0xa8} }, 0 },
  { "Ice",     {0xff,0xff,0xff}, {0x78,0x88,0x98}, {0x80,0xd0,0xff}, {0xff,0xff,0xff}, 3,
    { {0xff,0xff,0xff}, {0x60,0xb0,0xff}, {0x20,0x50,0xc0} }, 0 },
  { "Grayscale",{0xe0,0xe0,0xe0}, {0x70,0x70,0x70}, {0xb0,0xb0,0xb0}, {0xff,0xff,0xff}, 4,
    { {0xff,0xff,0xff}, {0x98,0x98,0x98}, {0x38,0x38,0x38}, {0x98,0x98,0x98} }, 0 },
  { "Sunset",  {0xff,0xe0,0xc8}, {0x98,0x60,0x80}, {0xff,0x90,0x50}, {0xff,0xf0,0x80}, 5,
    { {0xff,0xd0,0x40}, {0xff,0x70,0x30}, {0xf0,0x30,0x70}, {0x90,0x30,0xb0}, {0x40,0x30,0x90} }, 0 },
  { "Ocean",   {0xc8,0xf0,0xf0}, {0x40,0x80,0x90}, {0x30,0xe0,0xc0}, {0xff,0xff,0xff}, 4,
    { {0x00,0xf0,0xc0}, {0x00,0xa0,0xe0}, {0x10,0x40,0xb0}, {0x00,0xa0,0xe0} }, 0 },
  { "Material",  {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, 0, { {0,0,0} }, &lay_material },
  { "Aero",      {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, 0, { {0,0,0} }, &lay_aero },
  { "Workbench", {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, 0, { {0,0,0} }, &lay_workbench },
  { "DOS",       {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, 0, { {0,0,0} }, &lay_dos },
  { "Mac",       {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, 0, { {0,0,0} }, &lay_mac },
};
#define N_THEMES (sizeof themes / sizeof themes[0])

/* ---- state ---------------------------------------------------------- */

static unsigned char font[2048];         /* the ROM font, its upper half the header's tiles or the lower half reversed */
static unsigned char saved_pal[3][16];
static unsigned char cur_theme, phase, pulse;
unsigned char sel;
#define LAY (themes[cur_theme].lay)

/* ---- hardware ------------------------------------------------------- */

static unsigned char swap(unsigned char v) { return (unsigned char)((v << 4) | (v >> 4)); }

static void pal_set(unsigned char i, unsigned char r, unsigned char g, unsigned char b)
{
  POKE(0xd100 + i, swap(r & 0xfe));      /* red keeps its register's bit 4 clear */
  POKE(0xd200 + i, swap(g));
  POKE(0xd300 + i, swap(b));
}

static void pal_rgb(unsigned char i, const rgb *c) { pal_set(i, c->r, c->g, c->b); }

static void wait_frame(void)
{
  unsigned char f = PEEK(0xd7fa);
  while (PEEK(0xd7fa) == f) ;
}

/* The font for the theme: the menu's own has the header's tiles in its
 * upper half; a layout has its tiles at 96-127 and everything reversed
 * above (ui.h). */
static void build_font(void)
{
  const layout *l = LAY;
  unsigned int i;
  unsigned char j;
  lcopy(l ? l->font : ROM_FONT, (unsigned long)(unsigned int)font, 2048);
  if (!l) {
    for (i = 0; i < LOGO_TILES; i++)
      for (j = 0; j < 8; j++)
        font[(LOGO_FIRST_CODE + i) * 8 + j] = logo_tiles[i][j];
    return;
  }
  if (l->thin) {
    for (i = 0; i < 1024; i++) font[i] &= (unsigned char)(font[i] << 1);   /* a 2-pixel stroke loses its right half */
    for (i = 0; i < THIN_FIXES; i++)
      for (j = 0; j < 8; j++)
        font[thin_fixes[i].code * 8 + j] = thin_fixes[i].rows[j];
  }
  for (i = 0; i < l->ntiles; i++)
    for (j = 0; j < 8; j++)
      font[(TILE_FIRST_CODE + i) * 8 + j] = l->tiles[i][j];
  for (i = 0; i < 1024; i++) font[1024 + i] = (unsigned char)~font[i];
}

static void screen_init(void)
{
  unsigned char i;
  POKE(0xd02f, 0x47); POKE(0xd02f, 0x53);                  /* the VIC-IV personality */
  POKE(0xd05d, PEEK(0xd05d) & 0x7f);                       /* hot registers off: what is set below stays */
  while (PEEK(0xd610)) POKE(0xd610, 0);
  POKE(0xd031, (PEEK(0xd031) | 0x80) & 0xf7); POKE(0xd04c, 0x50);   /* 80 columns, 25 rows */
  POKE(0xd07b, 24);
  POKE(0xd060, 0x00); POKE(0xd061, 0x08); POKE(0xd062, 0x00);       /* the screen at $0800 */
  POKE(0xd063, PEEK(0xd063) & 0xf0);
  POKE(0xd058, 80); POKE(0xd059, 0); POKE(0xd05e, 80);

  POKE(0xd068, (unsigned int)font & 0xff); POKE(0xd069, (unsigned int)font >> 8); POKE(0xd06a, 0);

  for (i = 0; i < 16; i++) {
    saved_pal[0][i] = PEEK(0xd100 + i); saved_pal[1][i] = PEEK(0xd200 + i); saved_pal[2][i] = PEEK(0xd300 + i);
  }
}

static void screen_restore(void)
{
  unsigned char i;
  lfill(SCREEN, 0x20, COLS * ROWS);
  for (i = 0; i < 16; i++) {
    POKE(0xd100 + i, saved_pal[0][i]); POKE(0xd200 + i, saved_pal[1][i]); POKE(0xd300 + i, saved_pal[2][i]);
  }
}

/* ---- drawing -------------------------------------------------------- */

static unsigned char screencode(char c)
{
  if (c >= 'a' && c <= 'z') return (unsigned char)(c - 'a' + 1);
  if (c == '@') return 0;
  return (unsigned char)c;               /* capitals, digits and punctuation sit at their ASCII codes */
}

void put(unsigned char x, unsigned char y, unsigned char colour, const char *s)
{
  unsigned int at = (unsigned int)y * COLS + x;
  unsigned char n = (unsigned char)strlen(s), i;
  if (!n) return;
  for (i = 0; i < n; i++) POKE(SCREEN + at + i, screencode(s[i]) | (colour & REV));
  lfill(COLOUR_RAM + at, colour & 0x0f, n);
}

/* A run of one screen code: a space, or a tile. */
void cells(unsigned char x, unsigned char y, unsigned char n, unsigned char colour, unsigned char code)
{
  unsigned int at = (unsigned int)y * COLS + x;
  lfill(SCREEN + at, code | (colour & REV), n);
  lfill(COLOUR_RAM + at, colour & 0x0f, n);
}

void box(unsigned char x, unsigned char y, unsigned char w, unsigned char h, unsigned char colour, unsigned char code)
{
  while (h--) cells(x, y++, w, colour, code);
}

static void put_centred(unsigned char y, unsigned char colour, const char *s)
{
  put((unsigned char)((COLS - strlen(s)) / 2), y, colour, s);
}

static void clear_row(unsigned char y)
{
  lfill(SCREEN + (unsigned int)y * COLS, 0x20, COLS);
}

/* A gradient colour for a cell: bands lean with the rows, three cells wide. */
static unsigned char band(unsigned char x, unsigned char y)
{
  return (unsigned char)(C_GRAD0 + ((unsigned char)(x + y * 2) / 3) % N_GRAD);
}

static void draw_rule(unsigned char y)
{
  unsigned char x;
  unsigned int at = (unsigned int)y * COLS;
  for (x = COL_LOGO; x < COL_LOGO + LOGO_COLS; x++) {
    POKE(SCREEN + at + x, TILE_RULE);
    lpoke(COLOUR_RAM + at + x, band(x, y));
  }
}

static void draw_logo(void)
{
  unsigned char x, y;
  for (y = 0; y < LOGO_ROWS; y++) {
    unsigned int at = (unsigned int)(ROW_LOGO + y) * COLS + COL_LOGO;
    for (x = 0; x < LOGO_COLS; x++) {
      POKE(SCREEN + at + x, logo_map[y][x]);
      lpoke(COLOUR_RAM + at + x, band(x, y));
    }
  }
}

static void classic_item(unsigned char i)
{
  unsigned char y = (unsigned char)(ROW_ITEMS + i);
  unsigned char on = (i == sel);
  char num[2];
  num[0] = (char)('1' + i); num[1] = 0;
  clear_row(y);
  if (on) {
    POKE(SCREEN + (unsigned int)y * COLS + COL_ITEMS - 4, TILE_ARROW_L);
    POKE(SCREEN + (unsigned int)y * COLS + COL_ITEMS - 3, TILE_ARROW_R);
    lfill(COLOUR_RAM + (unsigned int)y * COLS + COL_ITEMS - 4, C_HILITE, 2);
  }
  put(COL_ITEMS, y, on ? C_HILITE : C_ACCENT, num);
  put(COL_ITEMS + 4, y, on ? C_HILITE : C_TEXT, apps[i].title);
  put(COL_ITEMS + 14, y, on ? C_HILITE : C_DIM, apps[i].blurb);
}

static void draw_keys(void)
{
  clear_row(ROW_KEYS);
  put(8, ROW_KEYS, C_ACCENT, "Cursor keys");
  put(20, ROW_KEYS, C_DIM, "or");
  put(23, ROW_KEYS, C_ACCENT, "1-8");
  put(27, ROW_KEYS, C_DIM, "select");
  put(37, ROW_KEYS, C_ACCENT, "RETURN");
  put(44, ROW_KEYS, C_DIM, "start");
  put(53, ROW_KEYS, C_ACCENT, "T");
  put(55, ROW_KEYS, C_DIM, "theme:");
  put(62, ROW_KEYS, C_TEXT, themes[cur_theme].name);
}

static void draw_item(unsigned char i)
{
  if (LAY) LAY->item(i); else classic_item(i);
}

static void draw_all(void)
{
  unsigned char i;
  draw_logo();
  draw_rule(ROW_RULE);
  put_centred(ROW_SUB, C_DIM, "Internet programs for the MEGA65");
  for (i = 0; i < N_APPS; i++) draw_item(i);
  put(COL_ITEMS, ROW_QUIT, C_ACCENT, "Q");
  put(COL_ITEMS + 4, ROW_QUIT, C_DIM, "Exit to BASIC");
  draw_rule(ROW_RULE2);
  draw_keys();
}

/* ---- colour --------------------------------------------------------- */

/* A point on the theme's gradient loop, p in 0..255. */
static void gradient(unsigned char p, rgb *out)
{
  const theme *t = &themes[cur_theme];
  unsigned int pos = (unsigned int)p * t->nkeys;          /* key index in the high byte, fraction in the low */
  unsigned char k = (unsigned char)(pos >> 8), f = (unsigned char)pos;
  const rgb *a = &t->keys[k], *b = &t->keys[(unsigned char)(k + 1) % t->nkeys];
  out->r = (unsigned char)(a->r + (((int)b->r - a->r) * f) / 256);
  out->g = (unsigned char)(a->g + (((int)b->g - a->g) * f) / 256);
  out->b = (unsigned char)(a->b + (((int)b->b - a->b) * f) / 256);
}

static void apply_theme(void)
{
  const theme *t = &themes[cur_theme];
  pal_set(0, 0, 0, 0);
  pal_rgb(C_TEXT, &t->text);
  pal_rgb(C_DIM, &t->dim);
  pal_rgb(C_ACCENT, &t->accent);
  pal_rgb(C_HILITE, &t->hilite);
}

/* One frame of movement: the gradient a step round, the selected row's breath. */
static void animate(void)
{
  unsigned char i, lvl;
  rgb c;
  const rgb *h = &themes[cur_theme].hilite;
  if (LAY) return;
  phase = (unsigned char)(phase - 2);
  for (i = 0; i < N_GRAD; i++) {
    gradient((unsigned char)(phase + i * (256 / N_GRAD)), &c);
    pal_rgb((unsigned char)(C_GRAD0 + i), &c);
  }
  pulse = (unsigned char)(pulse + 4);
  lvl = (pulse & 0x80) ? (unsigned char)(255 - pulse) : pulse;       /* a triangle, 0..127 */
  lvl = (unsigned char)(160 + (lvl * 3) / 4);                        /* 63% to 100% */
  pal_set(C_HILITE, (unsigned char)(((unsigned int)h->r * lvl) >> 8), (unsigned char)(((unsigned int)h->g * lvl) >> 8),
          (unsigned char)(((unsigned int)h->b * lvl) >> 8));
}

/* The whole screen in the current theme, from nothing. */
static void show_theme(void)
{
  const layout *l = LAY;
  unsigned char i;
  build_font();
  lfill(SCREEN, 0x20, COLS * ROWS);
  lfill(COLOUR_RAM, C_TEXT, COLS * ROWS);
  POKE(0xd021, 0);
  if (l) {
    for (i = 0; i < 16; i++) pal_rgb(i, &l->pal[i]);
    POKE(0xd020, l->border);
    l->draw();
  } else {
    POKE(0xd020, 0);
    apply_theme();
    animate();
    draw_all();
  }
}

static void next_theme(void)
{
  const layout *was = LAY;
  cur_theme = (unsigned char)((cur_theme + 1) % N_THEMES);
  if (was || LAY) show_theme();
  else { apply_theme(); draw_keys(); }   /* one colouring to the next: nothing moves */
}

/* A last word on the way out: what is happening, and to what. */
static void message(const char *a, const char *b)
{
  if (LAY) { LAY->message(a, b); return; }
  clear_row(ROW_KEYS);
  put((unsigned char)((COLS - strlen(a) - strlen(b) - 1) / 2), ROW_KEYS, C_DIM, a);
  put((unsigned char)((COLS - strlen(a) - strlen(b) - 1) / 2 + strlen(a) + 1), ROW_KEYS, C_TEXT, b);
}

/* ---- leaving -------------------------------------------------------- */

static void remember(void)
{
  lpoke(HANDOFF + HO_THEME, (unsigned char)(cur_theme + 1));
  lpoke(HANDOFF + HO_SEL, sel);
}

static void leave(unsigned char state, const char *name)
{
  unsigned char i;
  lpoke(HANDOFF + 0, HO_MAGIC0); lpoke(HANDOFF + 1, HO_MAGIC1); lpoke(HANDOFF + 2, HO_MAGIC2);
  for (i = 0; i < HO_NAME_LEN; i++) {
    lpoke(HANDOFF + HO_NAME + i, (unsigned char)(name && *name ? *name : 0));
    if (name && *name) name++;
  }
  lpoke(HANDOFF + HO_STATE, state);
  remember();
  screen_restore();
  m65_reset();
}

/* ---- main ----------------------------------------------------------- */

static void select(unsigned char n)
{
  unsigned char old = sel;
  sel = n;
  draw_item(old);
  draw_item(sel);
  if (LAY) LAY->detail();
}

int main(void)
{
  unsigned char k, known = 0;

  if (lpeek(HANDOFF + 0) == HO_MAGIC0 && lpeek(HANDOFF + 1) == HO_MAGIC1 && lpeek(HANDOFF + 2) == HO_MAGIC2) {
    known = 1;
    cur_theme = (unsigned char)(lpeek(HANDOFF + HO_THEME) - 1);     /* kept as theme + 1: zero is "never chosen" */
    sel = lpeek(HANDOFF + HO_SEL);
  } else {
    cur_theme = 0xff;
  }
  if (cur_theme >= N_THEMES) cur_theme = DEFAULT_THEME;
  if (sel >= N_APPS) sel = 0;
#ifdef AUTOSEL
  sel = AUTOSEL;                         /* for the emulator tests: a row other than the first */
#endif

  /* The network. The programs started from here leave the ethernet
   * controller running when they exit, so the next one starts it with
   * mega-net's INIT_WARM and skips the 3.5 s a controller and its PHY
   * need after a reset (mega-net REQUIREMENTS.md 5.23). At a cold start,
   * or when something held it in reset ($D6E0 bit 0 clear), it is reset
   * and released here instead, so the link comes up while a program is
   * being chosen. Nothing else of the network is needed for that. */
  mega65_io_enable();
  if (!known || !(PEEK(0xd6e0) & 0x01)) {
    POKE(0xd6e0, 0x00);
    for (k = 0; k < 5; k++) wait_frame();
    POKE(0xd6e0, 0x03);
  }

  lpoke(HANDOFF + HO_STATE, HO_MENU);
  screen_init();
  show_theme();
  /* For the emulator tests, which cannot press keys: act after three seconds. */
#ifdef AUTOPICK
  { unsigned char f; for (f = 0; f < 150; f++) { wait_frame(); animate(); } leave(HO_LAUNCH, apps[AUTOPICK].file); }
#endif
#ifdef AUTOTHEME                         /* step through that many themes first, a second apart */
  { unsigned char f, n; for (n = 0; n < AUTOTHEME; n++) { for (f = 0; f < 50; f++) { wait_frame(); animate(); }
      next_theme(); } }
#endif
#ifdef AUTOQUIT
  { unsigned char f; for (f = 0; f < 150; f++) { wait_frame(); animate(); } leave(HO_IDLE, 0); }
#endif

  for (;;) {
    wait_frame();
    animate();
    k = PEEK(0xd610);
    if (!k) continue;
    POKE(0xd610, 0);
    if (k == 0x11) select((unsigned char)((sel + 1) % N_APPS));                 /* cursor down */
    else if (k == 0x91) select((unsigned char)((sel + N_APPS - 1) % N_APPS));   /* cursor up */
    else if (k >= '1' && k < '1' + N_APPS) { select((unsigned char)(k - '1')); k = 0x0d; }
    else if (k == 't' || k == 'T') next_theme();
    else if (k == 'q' || k == 'Q' || k == 0x03) {                              /* 0x03: RUN/STOP, as in the clients */
      message("Exiting to BASIC.", "RUN \"MENU\" brings this back.");
      leave(HO_IDLE, 0);
    }
    if (k == 0x0d) {
      message("Starting", apps[sel].title);
      leave(HO_LAUNCH, apps[sel].file);
    }
  }
}
