/* Warm-start measurement: would a network the menu brought up save time
 * for the programs it launches?
 *
 * Each run is one trial, timed in frames ($D7FA), and ends the way a
 * client launched from the menu ends: the handoff block says LAUNCH
 * WARMTEST, the KERNAL's reset entry, and the disk's AUTOBOOT.C65 runs
 * this again. The modes go round 1, 2, 3:
 *
 *   1 cold   today's path: $D6E0 held in reset, MEGANET loaded, INIT
 *            (controller reset and 200 frames for the PHY), DHCP, DNS
 *   2 adopt  the previous trial left the controller running and saved
 *            its lease below the handoff block: INIT_WARM (no reset,
 *            no PHY wait), SET_IP + SET_DNS from that, DNS
 *   3 warm   as 2, but a full DHCP exchange instead of the saved lease
 *
 * So each exit leaves $D6E0 alone when the next trial is warm, and holds
 * it in reset (as every client does today) when the next is cold.
 *
 * The log is in colour RAM at $FF84000, past anything the clients or the
 * ROM's reset touch (handoff.h), 16 bytes a trial after a 16-byte head.
 * The saved lease is at $FF87FD0, where the real feature would keep it,
 * so each trial also shows whether those bytes survived the reset.
 *
 * First measured with a probe build of the image (INIT with A='W'),
 * which became INIT_WARM, entry 35 (mega-net 5.23); see
 * build_warmtest.py. Run it by typing RUN "WARMTEST" with its disk
 * mounted; spike/warmtest.py does all of it and reads the log back. */
#include <stdint.h>
#include <string.h>
#include "mega65/memory.h"
#include "meganet.h"
#include "meganet_tramp.h"
#include "m65_boot.h"
#include "../src/handoff.h"

#define TRIALS   24
#define LOG      0xff84000UL
#define LEASE    0xff87fd0UL          /* 32 bytes, below the handoff block */
#define DNS_NAME "pool.ntp.org"

/* log head */
#define LH_MAGIC 0                    /* 'W','T' */
#define LH_N     2
#define LH_NEXT  3
#define LH_NTSC  4                    /* $D06F bit 7 at the first trial */
/* log entry (LOG + 16 + 16 * i) */
#define LE_MODE    0
#define LE_RESULT  1                  /* 1 ok, 2 load failed, 3 no lease, 4 no DNS answer */
#define LE_D6E0    2                  /* the controller as the reset left it */
#define LE_FLAGS   3                  /* bit 0 image in bank 4 intact, bit 1 lease block intact */
#define LE_LOAD    4                  /* frames, u16 each */
#define LE_INIT    6
#define LE_DHCP    8
#define LE_DNS     10
#define LE_IRQS    12                 /* events the stack caught during calls */
#define LE_STRAY   14                 /* bit 0: an event reached our own stub */
#define LE_TRIES   15                 /* DHCP messages sent */

/* lease block */
#define LB_MAGIC 0                    /* 'M','N','L' */
#define LB_SEQ   3                    /* the trial that wrote it */
#define LB_CONF  4                    /* ip, mask, gw */
#define LB_DNS   16
#define LB_SUM   20                   /* u16: the image in bank 4, summed after the load */
#define LB_SIZE  22

static meganet_ipconf_t conf;
static uint8_t dns[4];
static meganet_stats_t stats;
static uint8_t lb[LB_SIZE];
static uint8_t chunk[256];

static uint16_t frames;
static uint8_t last_fc;

static void tick(void)
{
  uint8_t f = PEEK(0xd7fa);
  frames += (uint8_t)(f - last_fc);
  last_fc = f;
}

static uint8_t rtc_seconds(void)
{
  uint8_t b = lpeek(0xffd7110UL);
  return (uint8_t)((b >> 4) * 10 + (b & 15));
}

/* A blocking call that may take more than 255 frames: the frame counter
 * gives the remainder, the RTC's seconds the multiple of 256. */
static uint16_t span(uint8_t f0, uint8_t s0, uint8_t ntsc)
{
  uint8_t f1 = PEEK(0xd7fa), s1 = rtc_seconds();
  uint16_t d = (uint8_t)(f1 - f0);
  uint16_t secs = (uint16_t)((s1 + 60 - s0) % 60);
  uint16_t want = secs * (ntsc ? 60 : 50);
  while (d + 128 < want) d += 256;
  last_fc = f1;
  return d;
}

static void wait_frames(uint8_t n)
{
  uint8_t last = PEEK(0xd7fa);
  while (n) if (PEEK(0xd7fa) != last) { last = PEEK(0xd7fa); n--; }
}

static uint16_t image_sum(void)
{
  uint16_t s = 0, i, n;
  uint32_t a = 0x42000UL, left = MEGANET_BIN_SIZE;
  while (left) {
    n = left > 256 ? 256 : (uint16_t)left;
    lcopy(a, (uint32_t)(uintptr_t)chunk, n);
    for (i = 0; i < n; i++) s = (uint16_t)(s * 31 + chunk[i]);
    a += n; left -= n;
  }
  return s;
}

/* The trial's log entry, built here and copied out in one piece: a
 * helper that lpoke'd the two bytes of a u16 was compiled to increment a
 * word it never used and write both bytes to the first address (ssh 5.6's
 * miscompile; orphan_rmw.py, run by the build, now checks this file). */
static uint8_t ent[16];

static void set16(uint8_t off, uint16_t v)
{
  ent[off] = (uint8_t)v;
  ent[off + 1] = (uint8_t)(v >> 8);
}

/* "WARMTEST 7/24 MODE 2" on the top line, so a watcher sees it move. */
static void show(uint8_t idx, uint8_t mode, uint8_t done)
{
  static const char text[] = "warmtest   /24 mode   ";
  volatile uint8_t *scr = (volatile uint8_t *)0x0800;
  uint8_t i;
  for (i = 0; text[i]; i++) {
    char c = text[i];
    scr[i] = (uint8_t)(c >= 'a' && c <= 'z' ? c - 'a' + 1 : c);
  }
  scr[9] = (uint8_t)('0' + (idx + 1) / 10); scr[10] = (uint8_t)('0' + (idx + 1) % 10);
  scr[20] = (uint8_t)('0' + mode);
  scr[22] = done ? 4 : 32;            /* 'D' when finished */
}

static const unsigned char stub[] = {
  0x78,                               /* sei */
  0xa9, 0x00, 0xaa, 0xa8, 0x4b,       /* lda #0 ; tax ; tay ; taz */
  0x5c, 0xea,                         /* map ; eom */
  0xa9, 0x3f, 0x85, 0x01,             /* $01 = $3f */
  0xa9, 0x64, 0x8d, 0x30, 0xd0,       /* $D030 = $64 */
  0xa9, 0x00, 0x8d, 0x6a, 0xd0,       /* $D06A = 0 */
  0x6c, 0xfc, 0xff                    /* jmp ($fffc) */
};

/* The clients' exit (m65_exit.c), with the controller's reset optional. */
static void leave(uint8_t hold_controller)
{
  if (hold_controller) POKE(0xd6e0, 0x00);
  wait_frames(25);
  while (PEEK(0xd610)) POKE(0xd610, 0);
  POKE(0xd05d, (uint8_t)(PEEK(0xd05d) | 0x80));
  memcpy((void *)0x1fb0, stub, sizeof stub);
  __asm__ volatile("jmp $1fb0");
  for (;;) ;
}

static void handoff(uint8_t state, const char *name)
{
  uint8_t i;
  lpoke(HANDOFF, HO_MAGIC0); lpoke(HANDOFF + 1, HO_MAGIC1); lpoke(HANDOFF + 2, HO_MAGIC2);
  lpoke(HANDOFF + HO_STATE, state);
  for (i = 0; i < HO_NAME_LEN; i++)
    lpoke(HANDOFF + HO_NAME + i, (uint8_t)(name && *name ? *name++ : 0));
}

static uint8_t mode_of(uint8_t idx) { return (uint8_t)(1 + idx % 3); }

int main(void)
{
  const char *err;
  uint8_t idx, mode, d6e0, flags = 0, result = 1, st, f0, s0, ntsc;
  uint16_t i;
  uint16_t t_load = 0, t_init = 0, t_dhcp = 0, t_dns = 0, sum;
  uint32_t e;

  d6e0 = PEEK(0xd6e0);                /* before anything touches it */
  m65_own_vectors();

  /* A run typed from BASIC starts a new log; one the dispatcher started
   * (state RETURN) continues it. */
  if (lpeek(HANDOFF + HO_STATE) != HO_RETURN || lpeek(LOG) != 'W' || lpeek(LOG + 1) != 'T'
      || lpeek(LOG + LH_NEXT) >= TRIALS) {
    for (i = 0; i < 16 + 16 * TRIALS; i++) lpoke(LOG + i, 0);
    lpoke(LOG, 'W'); lpoke(LOG + 1, 'T'); lpoke(LOG + LH_N, TRIALS);
    lpoke(LOG + LH_NTSC, (uint8_t)(PEEK(0xd06f) >> 7));
    lpoke(LEASE, 0);                  /* no lease saved yet */
  }
  ntsc = lpeek(LOG + LH_NTSC);
  idx = lpeek(LOG + LH_NEXT);
  mode = mode_of(idx);
  show(idx, mode, 0);
  e = LOG + 16 + 16UL * idx;
  lpoke(e + LE_MODE, mode); lpoke(e + LE_D6E0, d6e0);

  /* What survived the reset: the lease block, and the image in bank 4. */
  lcopy(LEASE, (uint32_t)(uintptr_t)lb, LB_SIZE);
  if (lb[LB_MAGIC] == 'M' && lb[1] == 'N' && lb[2] == 'L' && idx && lb[LB_SEQ] == idx - 1) {
    flags |= 2;
    if (image_sum() == (uint16_t)(lb[LB_SUM] | (lb[LB_SUM + 1] << 8))) flags |= 1;
  }
  if (mode == 2 && !(flags & 2)) mode = 3;   /* nothing to adopt: fall back to DHCP */
  lpoke(e + LE_MODE, mode);
  ent[LE_MODE] = mode; ent[LE_D6E0] = d6e0;

  if (mode == 1) {                    /* as every client does before loading */
    POKE(0xd6e0, 0x00);
    wait_frames(3);
  }

  last_fc = f0 = PEEK(0xd7fa); s0 = rtc_seconds();
  if (!m65_boot_load(&err)) { result = 2; goto out; }
  t_load = span(f0, s0, ntsc);
  meganet_set_restore_map(0x00, 0xE0, 0x00, 0x00);
  sum = image_sum();

  f0 = PEEK(0xd7fa); s0 = rtc_seconds();
  meganet_call(mode == 1 ? MEGANET_INIT : MEGANET_INIT_WARM, 0, 0, 0, 0);
  t_init = span(f0, s0, ntsc);

  frames = 0; tick();
  if (mode == 2) {
    memcpy(&conf, lb + LB_CONF, sizeof conf);
    meganet_set_ip(&conf);
    meganet_call(MEGANET_SET_DNS, lb[LB_DNS], lb[LB_DNS + 1], lb[LB_DNS + 2], lb[LB_DNS + 3]);
  } else {
    meganet_dhcp_start();
    for (;;) {
      meganet_poll(); tick();
      st = meganet_dhcp_state();
      if (st == MEGANET_DHCP_BOUND) break;
      if (st == MEGANET_DHCP_FAILED || frames > 1500) { result = 3; break; }
    }
    t_dhcp = frames;
  }

  if (result == 1) {
    frames = 0; tick();
    meganet_dns_start(DNS_NAME);
    for (;;) {
      meganet_poll(); tick();
      st = meganet_dns_state();
      if (st == 2) break;
      if (st == 3 || frames > 500) { result = 4; break; }
    }
    t_dns = frames;
  }

  meganet_get_stats(&stats);
  ent[LE_IRQS] = stats.window_irqs[0]; ent[LE_IRQS + 1] = stats.window_irqs[1];
  ent[LE_TRIES] = stats.dhcp_tries[0];
  ent[LE_STRAY] = (uint8_t)(lpeek(0xff81UL) != 0);    /* the kit's stub records here */

  /* Save the lease for the next trial, as the menu would. */
  if (result == 1) {
    meganet_get_ip(&conf);
    meganet_get_dns(dns);
    lb[0] = 'M'; lb[1] = 'N'; lb[2] = 'L'; lb[LB_SEQ] = idx;
    memcpy(lb + LB_CONF, &conf, sizeof conf);
    memcpy(lb + LB_DNS, dns, 4);
    lb[LB_SUM] = (uint8_t)sum; lb[LB_SUM + 1] = (uint8_t)(sum >> 8);
    lcopy((uint32_t)(uintptr_t)lb, LEASE, LB_SIZE);
  } else {
    lpoke(LEASE, 0);
  }
  set16(LE_LOAD, t_load); set16(LE_INIT, t_init);
  set16(LE_DHCP, t_dhcp); set16(LE_DNS, t_dns);

out:
  ent[LE_RESULT] = result; ent[LE_FLAGS] = flags;
  lcopy((uint32_t)(uintptr_t)ent, e, sizeof ent);
  idx++;
  lpoke(LOG + LH_NEXT, idx);
  if (idx < TRIALS) {
    handoff(HO_LAUNCH, "WARMTEST");
    leave(mode_of(idx) == 1);
  }
  show(idx - 1, mode, 1);
  handoff(HO_IDLE, 0);
  leave(1);
  return 0;
}
