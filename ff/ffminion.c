/*
 * FIEND FOLIO - FIEND MINIONS (ffscripts/players/fiend.lua FiendMinionAI, customhealth.lua POST_HEALTH_DAMAGED), natively.
 *
 * WHAT THE MOD DOES (cited from the Lua; line numbers are ff-test's):
 *   - when a hit lands on an immoral heart in an active room, the run of immoral hearts at the end of the soul chain
 *     becomes minions: 2 halves -> pickup 5.750 subtype 2, a lone half -> subtype 3 (customhealth.lua:643-700). Black
 *     hearts do the same for Fiend with subtype 1 (:756-800) and are "fiendBonus" ones.
 *   - a minion drops (animation "Drop", velocity x0.8 each frame), waits 15 frames, then chases the nearest active enemy
 *     within 900 (the player if there is none): speed max(3, MoveSpeed*5), every 2nd frame vel = lerp(vel, dir*speed, 0.3)
 *     (fiend.lua:967-968, 1111-1145).
 *   - every max(3, MaxFireDelay) frames (a random 0..14 offset each) it hits each active, vulnerable, not friendly enemy it
 *     touches: a CRITICAL with chance 1/max(10, S - floor(Luck*20)) (S = 250, 150 for a fiendBonus one) for Damage*5 and
 *     the "Critical Hit" effect 1000.1734; otherwise Damage*0.2 (0.4 for a fiendBonus one) (fiend.lua:1146-1194).
 *   - when the room is no longer active the minion plays "Transform" and, past frame 20, becomes the heart it stands for
 *     (immoral 5.1024 / half 5.1025 / black 5.10.6) (fiend.lua:1069-1110, fiendMix.lua).
 *
 * WHAT IS MEASURED IN THE IMAGE (sha 217be349..., see FIEND_MINIONS_ANALISIS.md):
 *   Entity_Pickup vtable: functions from 0xa8e760, slot k at 0xa8e760 + 8k. Slot 4 = Pickup::Update 0x7193a4 (the slot is
 *   checked before it is written: if it is not that function nothing is touched), slot 9 = TakeDamage(this, float,
 *   u64 flags, EntityRef *, int) (0x7220dc for pickups, 0x69876c for NPCs: called through the TARGET's own vtable),
 *   slot 18 = IsActiveEnemy (NPC 0x69aae4: byte +0xcc8 set and flag bit 29 (+0x1bb & 0x20, friendly) clear), slot 19 =
 *   IsVulnerableEnemy (0x69ab04: byte +0xcc9). Entity: position +0x310, velocity +0x320, Size +0x344 (the float the game
 *   adds for two entities 14 times), sprite +0x50, type/variant/subtype +0x38/+0x3c/+0x40, exists/dead +0x1c2/+0x1c3.
 *   Sprite: Play 0x89e82c(sprite, name, force), SetFrame 0x89eab8(sprite, name, frame), SetOverlayFrame 0x89ee7c(sprite,
 *   name, frame) (it sets the overlay animation and its frame; the overlay sits at sprite+0x60). Player: MaxFireDelay
 *   +0x1834, Damage +0x1844, MoveSpeed +0x194c, Luck +0x1950 (ffchars.c). Room: [Game+0x21550], active enemy counts +0x1a58
 *   and +0x1a5c, clear delay +0x1918 (spawner.c). Room entity list: array +0x78, count +0x84 (eid_craft.c).
 *
 * WHAT IS NOT MEASURED AND SO IS NOT DONE (each is a line in the report): the EntityRef the game wants with a damage
 * is a zeroed block (type 0, no source entity: the hit is credited to nobody); the walls and rocks are read from the room's tile table when the table can be found (see "the room's grid" below); no sound; no blood splat; the sprite is not flipped; War Horn,
 * Imp Soda, flying players, Birthcake and persistent minions of other mods are not here. The "active room" is the
 * game's own enemy count (+0x1a58/+0x1a5c) or its clear delay: an ambush wave pause counts as clear.
 */
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>
#include "diag.h"

#include "ffsay.h"

#define PICKUP_VTABLE      0xa8e760
#define SLOT_UPDATE        4
#define FN_PICKUP_UPDATE   0x7193a4
#define VT_TAKE_DAMAGE     (9 * 8)
#define VT_ACTIVE_ENEMY    (18 * 8)
#define VT_VULNERABLE      (19 * 8)

#define FN_SPAWN           0x88fcbc
#define FN_SPRITE_PLAY     0x89e82c
#define FN_SPRITE_OVERLAY  0x89ee7c
#define FN_SPRITE_REMOVE_OVERLAY 0x89eef8   /* (sprite): add x0,x0,#0x60; mov x1,#0; b the overlay's own set-animation */

#define GAME_PTR           0xac3b90
#define GAME_ROOM          0x21550
#define GAME_PLAYER_LIST   0x25c50
#define GAME_PLAYERS       0x25c40
#define FN_PLAYER_AT       0x4da574
#define ROOM_ENEMIES_A     0x1a58
#define ROOM_ENEMIES_B     0x1a5c
#define ROOM_CLEAR_DELAY   0x1918

#define E_TYPE 0x38
#define E_VARIANT 0x3c
#define E_SUBTYPE 0x40
#define E_SPRITE 0x50
#define E_FLAGS 0x1b8
#define E_EXISTS 0x1c2
#define E_DEAD 0x1c3
#define E_POS 0x310
#define E_VEL 0x320
#define E_SIZE 0x344
#define SPRITE_OFFSET_AT 0x108
#define LIST_ARRAY 0x78
#define LIST_COUNT 0x84
#define FLAG_FRIENDLY (1ull << 29)

#define P_FIRE_DELAY 0x1834
#define P_DAMAGE 0x1844
#define P_MOVE_SPEED 0x194c
#define P_LUCK 0x1950

#define V_MINION 750
#define SUB_BLACK 1
#define SUB_IMMORAL 2
#define SUB_HALF_IMMORAL 3
#define PICKUP_FRICTION 0.965f
#define SUB_HALF_BLACK 16           /* the Lua's FIEND_MINION_EFFECT (1000.1736.1, 'hollow'): here a pickup like the half immoral one, entities2.xml 5.750.16 */
#define V_POOF 15
#define V_SPLAT 2
#define V_HEART 10
#define V_IMMORAL 1024
#define V_HALF_IMMORAL 1025
#define ST_BLACK 6
#define T_PICKUP 5
#define T_EFFECT 1000
#define V_CRIT 1734

#define DROP_FRAMES 22              /* "Drop" FrameNum */
#define GRACE_FRAMES 15
/* fiend.lua:1086-1106: "Transform" plays; past frame 20 the Lua morphs the entity into the heart (Morph), RELOADS the minion's own anm2
 * (entry.PickupAnim = morsel_minion_*.anm2) and plays ITS "Appear" (22 frames, with "Idle" and "Collect" after it). So the fall the PC
 * shows is the minion anm2's Appear, not the vanilla heart's: the vanilla heart's Appear is a different hop, and that was the second
 * jump in the port's video. Here the minion entity keeps its own sprite (the same anm2): at frame 20 it plays "Appear" itself, and
 * only when that is over, on the ground and still, is the real heart entity made, in its Idle and already collectable. */
#define MORPH_AT 20
#define APPEAR_FRAMES 22
#define TRANSFORM_AT (MORPH_AT + 1 + APPEAR_FRAMES)
#define CHASE_RANGE 900.0f
#define CRIT_MS 1100                /* the "Crit" animation: 31 frames at 30 per second, and a little more */

typedef void (*fn_update)(unsigned long);
typedef unsigned long (*fn_spawn)(unsigned long, int, int, const float *, const float *, unsigned long, int, int, int);
typedef void (*fn_play)(void *, const char *, int);
typedef void (*fn_overlay)(void *, const char *, int);
typedef int (*fn_take)(unsigned long, float, unsigned long long, void *, int);
typedef unsigned char (*fn_pred)(unsigned long);       /* a C++ bool comes back in the low byte only */

static unsigned char *g_base;
static fn_update o_update;
static fn_spawn g_spawn;                    /* the host test puts its own here */
static fn_play g_play;
static fn_overlay g_overlay;
static void no_overlay(void *s) { (void) s; }
static void (*g_remove_overlay)(void *) = no_overlay;
static unsigned long (*g_list_now)(void);   /* the room's entity list object (port: shim_eid_list_now) */

unsigned long shim_eid_list_now(void);
int shim_eid_readable(const void *p, unsigned long n);      /* eid.c: the room list may hold a freed entity */
static int (*g_readable)(const void *, unsigned long);
#define ENT_READ 0x350                                       /* up to the Size field */

/* ---------------------------------------------------------------------------------------------- tiny helpers */
static float *fpos(unsigned long e) { return (float *) (e + E_POS); }
static float *fvel(unsigned long e) { return (float *) (e + E_VEL); }
static float fsize(unsigned long e) { return *(const float *) (e + E_SIZE); }

static unsigned xs(unsigned *s) { unsigned x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }
static unsigned g_seed = 0x1badf00d;
static int g_logs[8];                                        /* the first few of each event are logged, not all */
#define LOG_FEW(k, ...) do { if (g_logs[k]++ < 4) LOG(__VA_ARGS__); } while (0)
static unsigned rnd(unsigned n) { return n ? xs(&g_seed) % n : 0; }

static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* ---------------------------------------------------------------------------------------------- the room */
static unsigned long real_room(void) {
    if (!g_base) return 0;
    unsigned long game = *(const unsigned long *) (g_base + GAME_PTR);
    return game ? *(const unsigned long *) (game + GAME_ROOM) : 0;
}
static unsigned long (*g_room)(void) = real_room;

static unsigned snapshot(unsigned long *out);
static void fb_minion_credit(unsigned long player, float dmg, unsigned long e);     /* fffireball.h */
static int target_level(const unsigned long *snap, unsigned n, int *count);

/*
 * Is the room being fought in (Fiend Folio: room:IsAmbushActive() or not room:IsClear())? Measured two ways, either is enough:
 * the room's own counters (spawner.c's arena code uses them) and the entities themselves - any NPC the game calls an active,
 * vulnerable, non-friendly enemy. The second does not depend on a field offset, which a real run showed can read 0 with enemies
 * standing in the room.
 */
void ffminion_room_info(int *a, int *b, int *d, int *targets) {
    *a = *b = *d = *targets = 0;
    unsigned long room = g_room();
    if (room) {
        *a = *(const int *) (room + ROOM_ENEMIES_A); *b = *(const int *) (room + ROOM_ENEMIES_B);
        *d = *(const int *) (room + ROOM_CLEAR_DELAY);
    }
    unsigned long snap[256];
    unsigned n = snapshot(snap);
    target_level(snap, n, targets);
}
int ffminion_room_active(void) {
    int a, b, d, t;
    ffminion_room_info(&a, &b, &d, &t);
    return a + b > 0 || (d >= 1 && d <= 10) || t > 0;      /* t counts only real enemies now (level 2 or 1), never spikes */
}

/* a snapshot of the room's entities (the list may change while we hit things) */
#define SNAP 256
static unsigned snapshot(unsigned long *out) {
    unsigned long list = g_list_now();
    if (!list) return 0;
    const unsigned long *items = *(const unsigned long *const *) (list + LIST_ARRAY);
    unsigned n = *(const unsigned *) (list + LIST_COUNT);
    if (!items || n > 4096) return 0;
    unsigned k = 0;
    for (unsigned i = 0; i < n && k < SNAP; i++)
        if (items[i] && g_readable((const void *) items[i], ENT_READ)) out[k++] = items[i];
    return k;
}

static int alive(unsigned long e) { return *(const unsigned char *) (e + E_EXISTS) && !*(const unsigned char *) (e + E_DEAD); }
static int vcall(unsigned long e, int off) {
    const unsigned long *vt = *(const unsigned long *const *) e;
    return vt ? (((fn_pred) vt[off / 8])(e) & 1) : 0;
}
/* How sure we are that `e` is an enemy a minion should fight. A real run of the first version found 0 targets in rooms the
 * game itself counted enemies in (counters 0 4), so the minions had nothing to chase: the two virtual questions (IsActiveEnemy
 * slot 18, IsVulnerable slot 19) or the flags are not trustworthy for every NPC. Three levels, the first that finds anyone wins:
 *   2 both questions answer yes   1 either does   0 any living, non-friendly NPC that is not poop or a fireplace */
static int target_ok(unsigned long e, int level) {
    if (!alive(e)) return 0;
    int type = *(const int *) (e + E_TYPE);
    if (type < 10 || type >= 1000) return 0;                  /* NPCs only: players, tears, pickups, effects are not targets */
    if (*(const unsigned long long *) (e + E_FLAGS) & FLAG_FRIENDLY) return 0;
    if (level == 0) return type != 33 && type != 245;
    int a = vcall(e, VT_ACTIVE_ENEMY), v = vcall(e, VT_VULNERABLE);
    return level == 2 ? (a && v) : (a || v);
}
static int target_level(const unsigned long *snap, unsigned n, int *count) {
    for (int lvl = 2; lvl >= 1; lvl--) {                      /* the Lua asks IsActiveEnemy AND IsVulnerableEnemy; level 0 (any NPC) chased spikes and blocks */
        int c = 0;
        for (unsigned i = 0; i < n; i++) if (target_ok(snap[i], lvl)) c++;
        if (c) { if (count) *count = c; return lvl; }
    }
    if (count) *count = 0;
    return -1;
}

static int hit_enemy(unsigned long e, float damage) {
    static unsigned char ref[0x100];                          /* an EntityRef with no source: type 0, entity 0 */
    memset(ref, 0, sizeof ref);
    const unsigned long *vt = *(const unsigned long *const *) e;
    if (!vt) return 0;
    return ((fn_take) vt[VT_TAKE_DAMAGE / 8])(e, damage, 0ull, ref, 0);
}

/* ---------------------------------------------------------------------------------------------- per-minion state */
#define MAXM 64
enum { S_DROP = 0, S_ACTIVE = 1, S_TRANSFORM = 2 };
static struct {
    unsigned long e, owner, target;
    int age, state, tick_off, head, tstart, moved_set, carried, horiz, wvalid, fly;
    float wx, wy;
    float lastx, lasty, lvx, lvy, tmp[2];
    unsigned long skip; int skip_until; float chk_x, chk_y;     /* a target it could not get to, left alone for a while */
} g_m[MAXM];

static struct { unsigned long e; long long born; } g_crit[16];

static int slot_of(unsigned long e, int create, unsigned long owner) {
    int free = -1;
    for (int i = 0; i < MAXM; i++) {
        if (g_m[i].e == e) return i;
        if (!g_m[i].e && free < 0) free = i;
    }
    if (!create) return -1;
    if (free < 0) free = 0;                                   /* the oldest rows are long gone; never overrun */
    memset(&g_m[free], 0, sizeof g_m[free]);
    g_m[free].e = e; g_m[free].owner = owner;
    g_m[free].tick_off = (int) rnd(15);
    g_m[free].head = (int) rnd(206);
    return free;
}

static unsigned long first_player(void) {
    if (!g_base) return 0;
    unsigned long game = *(const unsigned long *) (g_base + GAME_PTR);
    if (!game) return 0;
    const unsigned long *l = (const unsigned long *) (game + GAME_PLAYER_LIST);
    if (!l[0] || l[1] < l[0]) return 0;
    return ((unsigned long (*)(unsigned long, int)) (g_base + FN_PLAYER_AT))(game + GAME_PLAYERS, 0);
}
static unsigned long (*g_first_player)(void) = first_player;

static void sweep_crits(void) {
    long long t = now_ms();
    for (int i = 0; i < 16; i++) {
        if (!g_crit[i].e) continue;
        if (t - g_crit[i].born >= CRIT_MS) {                  /* the animation is over: the effect goes (Lua CritHit) */
            unsigned long e = g_crit[i].e;
            if (*(const int *) (e + E_TYPE) == T_EFFECT && *(const int *) (e + E_VARIANT) == V_CRIT) {
                *(unsigned char *) (e + E_DEAD) = 1;
                *(unsigned char *) (e + E_EXISTS) = 0;
            }
            g_crit[i].e = 0;
        } else {                                              /* it rises one pixel a frame (Lua CritHit) */
            unsigned long e = g_crit[i].e;
            if (*(const int *) (e + E_TYPE) == T_EFFECT && *(const int *) (e + E_VARIANT) == V_CRIT)
                fpos(e)[1] -= 1.0f;                                   /* the Lua lifts SpriteOffset; +0x108 of the sprite is the COLOUR offset, so the effect itself moves */
            else g_crit[i].e = 0;
        }
    }
}

static void show_crit(unsigned long at_minion) {
    float vel[2] = { 0, 0 };
    unsigned long c = g_spawn(*(const unsigned long *) (g_base + GAME_PTR), T_EFFECT, V_CRIT, fpos(at_minion), vel, at_minion, 0, 0, 0);
    if (!c) return;
    fpos(c)[1] -= 15.0f;       /* Lua: SpriteOffset (0, -15) - done by moving the effect: the sprite's +0x108 is the colour offset */
    for (int i = 0; i < 16; i++) if (!g_crit[i].e) { g_crit[i].e = c; g_crit[i].born = now_ms(); return; }
}

/* ---------------------------------------------------------------------------------------------- the rules, testable */
/* chance 1 in n of a critical; S is 250, or 150 for a fiendBonus (black) minion */
int ffminion_crit_n(int bonus, float luck) {
    int n = (bonus ? 150 : 250) - (int) (luck * 20.0f);       /* floor for a positive luck; the Lua floors a negative one down */
    if (luck < 0 && (luck * 20.0f) != (float) (int) (luck * 20.0f)) n = (bonus ? 150 : 250) - (int) (luck * 20.0f) + 1;
    return n < 10 ? 10 : n;
}
float ffminion_hit_damage(float player_damage, int bonus, int crit) {
    return crit ? player_damage * 5.0f : player_damage * (bonus ? 0.4f : 0.2f);
}
int ffminion_period(float max_fire_delay) { int p = (int) max_fire_delay; return p < 3 ? 3 : p; }
float ffminion_speed(float move_speed) { float s = move_speed * 5.0f; return s < 3.0f ? 3.0f : s; }
void ffminion_heart_for(int sub, int *variant, int *subtype) {
    if (sub == SUB_IMMORAL) { *variant = V_IMMORAL; *subtype = 0; }
    else if (sub == SUB_HALF_IMMORAL) { *variant = V_HALF_IMMORAL; *subtype = 0; }
    else { *variant = V_HEART; *subtype = ST_BLACK; }
}


/* ---------------------------------------------------------------------------------------------- the room's grid
 * The Lua leaves the walking to the engine: it sets pickup.GridCollisionClass to GRIDCOLL_GROUND (rocks, pits, walls stop a
 * minion) or GRIDCOLL_WALLS when the player CanFly (only walls), and a minion that cannot see its target takes
 * CatheryPathFinding (fiend.lua:1019, 1131-1145). Here the minion moves itself, so it reads the room's tile classes itself.
 * NOT MEASURED: where the engine keeps them. The room is searched, once per room, for a W x H table of small numbers that looks like
 * a room (a wall class all round the border, mostly 0 inside; 15x9, 28x9, 15x16 or 28x16 tiles, either as bytes or ints), either
 * inside the Room object or behind a pointer in it. Tile (c, r) has its centre at (20 + 40c, 100 + 40r). If nothing is found
 * the log says so and the minions walk without the grid, as before. Memory is copied through a pipe so a bad pointer is an
 * error, not a crash.
 */
static int g_pipe[2] = { -1, -1 };
static int safe_copy(void *dst, const void *src, unsigned long n) {
    if ((unsigned long) src < 0x10000ul || n > 60000) return 0;
    if (g_pipe[0] < 0) {
        if (pipe(g_pipe) != 0) { g_pipe[0] = g_pipe[1] = -1; return 0; }
        fcntl(g_pipe[0], F_SETFL, O_NONBLOCK); fcntl(g_pipe[1], F_SETFL, O_NONBLOCK);
    }
    long w = write(g_pipe[1], src, n);
    if (w <= 0) return 0;
    unsigned long got = 0; char tmp[256];
    while (got < (unsigned long) w) {
        unsigned long want = (unsigned long) w - got;
        long r = read(g_pipe[0], w == (long) n ? (char *) dst + got : tmp, w == (long) n ? want : (want > sizeof tmp ? sizeof tmp : want));
        if (r <= 0) break;
        got += (unsigned long) r;
    }
    return w == (long) n && got == n;
}

static unsigned char g_cells[448];
static int g_gw, g_gh, g_wallv;
static unsigned long g_grid_at; static int g_grid_stride, g_grid_logs;
static long long g_grid_checked, g_grid_searched;
static const int g_dims[4][2] = { { 15, 9 }, { 28, 9 }, { 15, 16 }, { 28, 16 } };

/* is cells[0 .. W*H) a room? returns the wall value or 0 */
static int looks_like_room(const unsigned char *c, int W, int H) {
    int wall = 0, walls = 0, border = 0, zeros = 0, inner = 0;
    for (int x = 0; x < W; x++) for (int y = 0; y < H; y++) {
        int v = c[x + y * W];
        if (v > 6) return 0;
        if (x == 0 || y == 0 || x == W - 1 || y == H - 1) {
            border++;
            if (v) { if (!wall) wall = v; if (v == wall) walls++; }
        } else { inner++; if (!v) zeros++; }
    }
    if (!wall || walls * 100 < border * 80 || zeros * 100 < inner * 40) return 0;
    return wall;
}
static int try_block(const void *mem, unsigned long avail, int stride, unsigned long at) {
    unsigned char cells[448];
    for (int d = 3; d >= 0; d--) {                               /* the biggest first */
        int W = g_dims[d][0], H = g_dims[d][1], n = W * H;
        if ((unsigned long) n * stride > avail) continue;
        for (int i = 0; i < n; i++) { unsigned v = stride == 4 ? ((const unsigned *) mem)[i] : ((const unsigned char *) mem)[i]; cells[i] = v > 250 ? 250 : (unsigned char) v; }
        int w = looks_like_room(cells, W, H);
        if (!w) continue;
        memcpy(g_cells, cells, (size_t) n); g_gw = W; g_gh = H; g_wallv = w; g_grid_at = at; g_grid_stride = stride;
        return 1;
    }
    return 0;
}
/* re-read the table where it was found; 1 if it still looks like a room */
static int grid_reread(void) {
    if (!g_grid_at) return 0;
    unsigned char raw[448 * 4];
    unsigned long n = (unsigned long) 448 * g_grid_stride;
    if (!safe_copy(raw, (const void *) g_grid_at, n)) return 0;
    return try_block(raw, n, g_grid_stride, g_grid_at);
}
/*
 * The collision table the engine itself reads: one int per tile at room+0xe98 (0x5a1eec: ldr w8,[room + idx*4 + 3736] compared
 * with the entity's own class), the tile count W*H from room+0x14 / room+0x18 (the room's size plus the wall ring; the same
 * fields eid.c uses for room+0x30, the GridEntity pointers). Values are GridCollisionClass: 0 none, 1 pit, 2 object, 3 solid,
 * 4 wall, 5 wall-except-player. Tried first; the search below is only the fallback if the room does not look like that.
 */
#define ROOM_COLL   0xe98
#define ROOM_W      0x14
#define ROOM_H      0x18
static int grid_direct(unsigned long room) {
    int wh[2];
    if (!safe_copy(wh, (const void *) (room + ROOM_W), 4) || !safe_copy(wh + 1, (const void *) (room + ROOM_H), 4)) return 0;
    if (wh[0] < 3 || wh[1] < 3 || wh[0] * wh[1] > 448) return 0;
    unsigned raw[448];
    unsigned long n = (unsigned long) wh[0] * wh[1] * 4;
    if (!safe_copy(raw, (const void *) (room + ROOM_COLL), n)) return 0;
    unsigned char cells[448];
    int border = 0, walls = 0;
    for (int i = 0; i < wh[0] * wh[1]; i++) {
        cells[i] = raw[i] > 250 ? 250 : (unsigned char) raw[i];
        int c = i % wh[0], r = i / wh[0];
        if (c == 0 || r == 0 || c == wh[0] - 1 || r == wh[1] - 1) { border++; if (raw[i] >= 4) walls++; }
    }
    if (walls * 100 < border * 70) return 0;                       /* the ring of walls must be there */
    memcpy(g_cells, cells, (size_t) (wh[0] * wh[1])); g_gw = wh[0]; g_gh = wh[1]; g_wallv = 4; g_grid_at = room + ROOM_COLL; g_grid_stride = 4;
    return 1;
}
static void grid_search(unsigned long room) {
    if (grid_direct(room)) return;
    static unsigned char buf[0x30000];
    unsigned long have = 0;
    for (unsigned long off = 0; off < sizeof buf; off += 0x1000) {
        if (!safe_copy(buf + off, (const void *) (room + off), 0x1000)) break;
        have = off + 0x1000;
    }
    for (unsigned long o = 0; o + 64 < have; o += 4)               /* ints, inside the object */
        if (((const unsigned *) (buf + o))[0] != 0 && try_block(buf + o, have - o, 4, room + o)) return;
    for (unsigned long o = 0; o + 64 < have; o++)                  /* bytes, inside the object */
        if (buf[o] && buf[o] == buf[o + 1] && try_block(buf + o, have - o, 1, room + o)) return;
    for (unsigned long o = 0; o + 8 <= have; o += 8) {             /* behind a pointer in the object */
        unsigned long p = *(const unsigned long *) (buf + o);
        if (p < 0x100000ul || (p & 3)) continue;
        unsigned char raw[448 * 4];
        if (!safe_copy(raw, (const void *) p, sizeof raw)) continue;
        if (try_block(raw, sizeof raw, 4, p)) return;
        if (try_block(raw, sizeof raw, 1, p)) return;
    }
    g_gw = g_gh = 0; g_grid_at = 0;
}
static void grid_update(void) {
    long long t = now_ms();
    if (t - g_grid_checked < 100) return;
    g_grid_checked = t;
    unsigned long room = g_room ? g_room() : 0;
    if (!room) return;
    if (g_gw && grid_direct(room)) return;
    if (g_gw && grid_reread()) return;
    if (t - g_grid_searched < 700) return;                         /* not more than a search every 0.7 s */
    g_grid_searched = t;
    int had = g_gw;
    grid_search(room);
    if (g_gw && g_grid_logs++ < 12) LOG("ffminion: the room grid is at room%+ld (%s), %d x %d tiles, wall value %d, %d tiles not free", (long) (g_grid_at - room), g_grid_stride == 4 ? "ints" : "bytes", g_gw, g_gh, g_wallv, ({ int k = 0; for (int i = 0; i < g_gw * g_gh; i++) k += g_cells[i] != 0; k; }));
    else if (!g_gw && !had && g_grid_logs++ < 12) LOG("ffminion: no room grid found in the room object - minions walk without it");
}
static int tile_at(int c, int r) { if (c < 0 || r < 0 || c >= g_gw || r >= g_gh) return 255; return g_cells[c + r * g_gw]; }
/* the room's grid: tile (c,r) is centred on (40+40c, 120+40r) - the playable area of a 1x1 room starts at (60,140) (GetTopLeftPos) and its centre (320,280) is tile (7,4) */
static int tile_blocked(int c, int r, int fly) { int v = tile_at(c, r); return v == 255 || (fly ? v >= 4 : v != 0); }   /* a flyer only meets walls; the rest meets pits, objects, rocks too */
static int xy_blocked(float x, float y, int fly) { return tile_blocked((int) __builtin_floorf((x - 20.0f) / 40.0f), (int) __builtin_floorf((y - 100.0f) / 40.0f), fly); }
static int box_blocked(float x, float y, int fly) {
    const float r = 6.0f;
    return xy_blocked(x - r, y - r, fly) || xy_blocked(x + r, y - r, fly) || xy_blocked(x - r, y + r, fly) || xy_blocked(x + r, y + r, fly);
}
static int line_clear(float x0, float y0, float x1, float y1, int fly) {
    float dx = x1 - x0, dy = y1 - y0, d = __builtin_sqrtf(dx * dx + dy * dy);
    int steps = (int) (d / 10.0f) + 1;
    for (int k = 1; k < steps; k++) if (box_blocked(x0 + dx * k / steps, y0 + dy * k / steps, fly)) return 0;
    return 1;
}
/* the centre of the next tile on the way to (tx, ty) around what blocks: breadth-first from the target; 0 if there is no way */
static int path_step(float fx, float fy, float tx, float ty, int fly, float *wx, float *wy) {
    if (!g_gw) return 0;
    int W = g_gw, H = g_gh, n = W * H;
    int sc = (int) __builtin_floorf((fx - 20.0f) / 40.0f), sr = (int) __builtin_floorf((fy - 100.0f) / 40.0f);
    int tc = (int) __builtin_floorf((tx - 20.0f) / 40.0f), tr = (int) __builtin_floorf((ty - 100.0f) / 40.0f);
    if (sc < 0 || sr < 0 || sc >= W || sr >= H || tc < 0 || tr < 0 || tc >= W || tr >= H) return 0;
    short dist[448], q[448]; int qh = 0, qt = 0;
    for (int i = 0; i < n; i++) dist[i] = -1;
    dist[tc + tr * W] = 0; q[qt++] = (short) (tc + tr * W);
    static const int dc[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, dr[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    while (qh < qt) {
        int cur = q[qh++], c = cur % W, r = cur / W;
        for (int k = 0; k < 8; k++) {
            int nc = c + dc[k], nr = r + dr[k];
            if (nc < 0 || nr < 0 || nc >= W || nr >= H || dist[nc + nr * W] >= 0 || tile_blocked(nc, nr, fly)) continue;
            if (k >= 4 && (tile_blocked(c + dc[k], r, fly) || tile_blocked(c, r + dr[k], fly))) continue;        /* no cutting a corner */
            dist[nc + nr * W] = (short) (dist[cur] + 1); q[qt++] = (short) (nc + nr * W);
        }
    }
    /* Follow the way down the distance field and aim at the FARTHEST tile on it that can be seen from here in a straight line (the
     * path pulled tight): aiming at the next tile's centre made a minion rub along a rock's corner and stop in a room of rock columns. */
    int cc = sc, cr = sr, here = dist[sc + sr * W], first = 1;
    if (here < 0) return 0;                                         /* the target cannot be reached from this tile */
    float bx = 0, by = 0; int have = 0;
    for (int step = 0; step < 24 && here > 0; step++) {
        int best = -1, bd = 32000;
        for (int k = 0; k < 8; k++) {
            int nc = cc + dc[k], nr = cr + dr[k];
            if (nc < 0 || nr < 0 || nc >= W || nr >= H) continue;
            int d = dist[nc + nr * W];
            if (d < 0 || d >= here || d >= bd) continue;
            if (k >= 4 && (tile_blocked(cc + dc[k], cr, fly) || tile_blocked(cc, cr + dr[k], fly))) continue;
            bd = d; best = k;
        }
        if (best < 0) break;
        cc += dc[best]; cr += dr[best]; here = dist[cc + cr * W];
        float cx = 40.0f + 40.0f * cc, cy = 120.0f + 40.0f * cr;
        if (first || line_clear(fx, fy, cx, cy, fly)) { bx = cx; by = cy; have = 1; first = 0; } else break;
    }
    if (!have) return 0;
    *wx = bx; *wy = by;
    return 1;
}
static void move_on_grid(float *p, float vx, float vy, int fly, float *lvx, float *lvy) {
    if (!g_gw) { p[0] += vx; p[1] += vy; return; }
    int inside = box_blocked(p[0], p[1], fly);                      /* already in something: let it walk out */
    if (inside || !box_blocked(p[0] + vx, p[1], fly)) p[0] += vx; else *lvx = 0;
    if (inside || !box_blocked(p[0], p[1] + vy, fly)) p[1] += vy; else *lvy = 0;
}

/* Lua CanFly, for the items that give it for as long as they are held (the wiki's flight page): Transcendence 20, Lord of the
 * Pit 82, Spirit of the Night 159, Fate 179, Holy Grail 184, Dead Dove 185, Revelation 643, Spirit Shackles 674, Dogma 633.
 * The player keeps a count per collectible id at *(player+0x1ab8). Temporary and conditional flight is not seen. */
static int owner_flies(unsigned long owner) {
    static const int ids[] = { 20, 82, 159, 179, 184, 185, 633, 643, 674 };
    const int *counts = (const int *) *(const unsigned long *) (owner + 0x1ab8);
    if (!counts) return 0;
    int tmp[700];
    if (!safe_copy(tmp, counts, sizeof tmp)) return 0;
    for (unsigned k = 0; k < sizeof ids / sizeof ids[0]; k++) if (tmp[ids[k]] > 0) return 1;
    return 0;
}

/* ---------------------------------------------------------------------------------------------- one Update */
void ffsound_heart_drop(void) __attribute__((weak));
void ffsound_minion_hit(void) __attribute__((weak));
void ffsound_minion_crit(void) __attribute__((weak));
typedef void (*fn_setframe)(void *, const char *, int);
#define FN_SPRITE_SETFRAME 0x89eab8
static void to_heart(unsigned long e, int sub, float vx, float vy) {
    int v, st;
    ffminion_heart_for(sub, &v, &st);
    float zero[2] = { vx, vy };                              /* it carries on with the minion's slide (Lua morphs the same entity, which keeps its velocity) */
    unsigned long h = g_spawn(*(const unsigned long *) (g_base + GAME_PTR), T_PICKUP, v, fpos(e), zero, 0, st, 0, 0);
    if (!h) return;
    /* The minion's Transform animation ends on the ground, and a fresh heart pickup starts its "Appear" in the air (it jumps up and
     * lands): that was the pop between the two. The minion's own Appear has finished (TRANSFORM_AT); the heart takes over on the floor, in its Idle. */
    if (g_play) g_play((void *) (h + E_SPRITE), "Idle", 1);
    /* Playing "Idle" alone left the heart un-collectable until the room was reloaded (user's video). Pickup::Update (0x7193a4) keeps a
     * state at +0x1e4: 0 while it appears; when "Appear" is seen finishing it plays "Idle" and stores 4 (0x71a908-0x71a920), and only
     * from state != 0 does it count frames (+0x608) and set the collectable flag +0x1c1 (0x719918-0x719938). Since the Idle is set here, the
     * state the engine would have reached is set with it. */
    *(int *) (h + 0x1e4) = 4;
    if (ffsound_heart_drop) ffsound_heart_drop();
    *(unsigned char *) (e + E_DEAD) = 1;
    *(unsigned char *) (e + E_EXISTS) = 0;
    LOG_FEW(4, "ffminion: a minion went back to being a heart (5.%d.%d)", v, st);
}

static unsigned long pick_target(unsigned long e, unsigned long owner, unsigned long skip) {
    unsigned long snap[SNAP];
    unsigned n = snapshot(snap);
    unsigned long best = 0;
    float bd = CHASE_RANGE * CHASE_RANGE;
    int lvl = target_level(snap, n, 0);
    for (unsigned i = 0; lvl >= 0 && i < n; i++) {
        unsigned long c = snap[i];
        if (c == e || c == skip || !target_ok(c, lvl)) continue;
        float dx = fpos(c)[0] - fpos(e)[0], dy = fpos(c)[1] - fpos(e)[1], d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = c; }
    }
    return best ? best : owner;
}

/* An effect's colour and scale as the Lua sets them (entity.Color = Color(...), entity.SpriteScale *= x). Measured elsewhere in the port, not
 * on this effect: the entity's SpriteScale is two floats at entity+0x12c (rangefix.c: the knife's, the render grows the sprite by it) and
 * the sprite's Color sits in the sprite object at +0xe8 (tint rgba) and +0x108 (offset rgb) (spawner.c SPRITE_TINT_AT/OFFSET_AT, written
 * there for a card's sprite). Read back and logged, so a screen that shows it unpainted says exactly what the engine kept. */
static void paint_effect(unsigned long ef, float r, float g, float b, float a, float ro, float go, float bo, float scale) {
    if (!ef) return;
    float *tint = (float *) (ef + E_SPRITE + 0xe8), *off = (float *) (ef + E_SPRITE + 0x108), *sc = (float *) (ef + 0x12c);
    tint[0] = r; tint[1] = g; tint[2] = b; tint[3] = a; off[0] = ro; off[1] = go; off[2] = bo;
    if (scale != 1.0f) { sc[0] *= scale; sc[1] *= scale; }
    static int logs;
    if (logs++ < 4) LOG("ffminion: effect %p painted: tint %.2f %.2f %.2f %.2f, offset %.2f %.2f %.2f, entity scale %.2f x %.2f (read back)", (void *) ef, tint[0], tint[1], tint[2], tint[3], off[0], off[1], off[2], sc[0], sc[1]);
}
static void contact_tick(unsigned long e, unsigned long owner, int bonus) {
    float damage = *(const float *) (owner + P_DAMAGE), luck = *(const float *) (owner + P_LUCK);
    unsigned long snap[SNAP];
    unsigned n = snapshot(snap);
    int lvl = target_level(snap, n, 0);
    for (unsigned i = 0; lvl >= 0 && i < n; i++) {
        unsigned long c = snap[i];
        if (c == e || !target_ok(c, lvl)) continue;           /* re-checked here: an earlier hit may have killed it */
        float dx = fpos(c)[0] - fpos(e)[0], dy = fpos(c)[1] - fpos(e)[1];
        float reach = fsize(c) + fsize(e);
        if (dx * dx + dy * dy > reach * reach) continue;
        int crit = rnd((unsigned) ffminion_crit_n(bonus, luck)) == 0;
        hit_enemy(c, ffminion_hit_damage(damage, bonus, crit));
        fb_minion_credit(owner, ffminion_hit_damage(damage, bonus, crit), c);
        if (crit) { if (ffsound_minion_crit) ffsound_minion_crit(); } else if (ffsound_minion_hit) ffsound_minion_hit();
        LOG_FEW(3, "ffminion: a minion hit an enemy (type %d) for %.2f%s", *(const int *) (c + E_TYPE), ffminion_hit_damage(damage, bonus, crit), crit ? " - CRITICAL" : "");
        if (crit) { static int cl; if (cl++ < 20) LOG("ffminion: CRITICAL hit (x5 = %.2f) on an enemy of type %d (luck %.1f, 1 in %d)", ffminion_hit_damage(damage, bonus, 1), *(const int *) (c + E_TYPE), (double) luck, ffminion_crit_n(bonus, luck)); show_crit(e); }
    }
}

static void minion_ai(unsigned long e) {
    int i = slot_of(e, 1, 0);
    if (!g_m[i].owner) g_m[i].owner = g_first_player();
    unsigned long owner = g_m[i].owner;
    if (!owner) return;
    int sub = *(const int *) (e + E_SUBTYPE);
    int bonus = sub == SUB_BLACK || sub == SUB_HALF_BLACK;    /* fiendBonus (Lua flags it on the hollow one too) */
    void *sprite = (void *) (e + E_SPRITE);
    sweep_crits();
    if (g_m[i].age == 0 && sub == SUB_HALF_BLACK) g_m[i].head = (int) rnd(10);   /* Lua: hollow -> data.headframe = random(10) - 1 (ball-like heads) */
    if (g_m[i].age == 0) { g_play(sprite, "Drop", 1); LOG_FEW(2, "ffminion: first Update of a minion (sub %d): the AI runs", sub); }
    g_m[i].age++;
    float *v = fvel(e);

    if (g_m[i].state == S_TRANSFORM) {                        /* Lua: "Transform", past frame 20 it becomes the heart */
        /* keeps sliding with the last velocity (friction), instead of stopping dead */
        /* The slide decays by the pickup's own friction: Pickup init multiplies Entity::Friction (+0x340, 1.0 from entities2.xml) by the
         * float at 0x947bf8 = 0.965 for every pickup that is not a collectible (0x71a598-0x71a5b0, whose 0x9460a0 = 0.8 is the item's).
         * The Lua's minion is such a pickup, so over its 43 frames of Transform and Appear it keeps 0.965^43 = 22% of its speed, and the
         * heart that comes out carries it on. The 0.9 used before kept 1% - the heart seemed to stop dead. */
        g_m[i].lvx *= PICKUP_FRICTION; g_m[i].lvy *= PICKUP_FRICTION;
        move_on_grid(fpos(e), g_m[i].lvx, g_m[i].lvy, g_m[i].fly, &g_m[i].lvx, &g_m[i].lvy);
        v[0] = g_m[i].lvx; v[1] = g_m[i].lvy;
        if (g_m[i].tstart == MORPH_AT + 1) g_play(sprite, "Appear", 1);              /* Lua: sprite:Play("Appear", true) after the Morph */
        if (++g_m[i].tstart > TRANSFORM_AT) { to_heart(e, sub, g_m[i].lvx, g_m[i].lvy); g_m[i].e = 0; }
        return;
    }
    if (g_m[i].age < DROP_FRAMES) { v[0] *= 0.8f; v[1] *= 0.8f; return; }          /* "Drop" */

    g_overlay(sprite, "Head", g_m[i].head);                   /* the head is an overlay animation of its own */

    if (!ffminion_room_active() && sub == SUB_HALF_BLACK) {   /* fiend.lua:1075-1083: a hollow minion never goes back to being a heart, it vanishes in a puff of smoke */
        float zero[2] = { 0.0f, 0.0f };
        unsigned long game = *(const unsigned long *) (g_base + GAME_PTR);
        unsigned long ded = g_spawn(game, T_EFFECT, V_SPLAT, fpos(e), zero, 0, 1, 0, 0);
        paint_effect(ded, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 1.0f);                   /* dedbaby.Color = Color(0,0,0,0.7,0,0,0) */
        unsigned long poof = g_spawn(game, T_EFFECT, V_POOF, fpos(e), zero, 0, 0, 0, 0);
        paint_effect(poof, 0.3f, 0.3f, 0.3f, 1.0f, 10.0f / 255.0f, 0.0f, 10.0f / 255.0f, 0.5f);   /* poof.SpriteScale *= 0.5; poof.Color = Color(0.3,0.3,0.3,1,10/255,0,10/255) */
        *(unsigned char *) (e + E_DEAD) = 1;
        *(unsigned char *) (e + E_EXISTS) = 0;
        g_m[i].e = 0;
        LOG_FEW(4, "ffminion: a half black (hollow) minion vanished in a puff of smoke");
        return;
    }
    if (!ffminion_room_active()) {                            /* the fight is over: back to being a heart */
        g_m[i].state = S_TRANSFORM; g_m[i].tstart = 0;
        g_remove_overlay(sprite);                              /* Lua: sprite:RemoveOverlay() - else the whole minion's head stays on top of the Transform animation */
        g_play(sprite, "Transform", 1);
        return;
    }
    if (g_m[i].age < DROP_FRAMES + GRACE_FRAMES) return;      /* 15 frames of grace after the drop */
    unsigned long t = g_m[i].target;
    if (t && (!alive(t) || g_m[i].age % 60 == 0 || (t == owner && g_m[i].age % 15 == 0))) t = 0;
    if (!t) t = pick_target(e, owner, g_m[i].age < g_m[i].skip_until ? g_m[i].skip : 0);
    g_m[i].target = t;

    float speed = ffminion_speed(*(const float *) (owner + P_MOVE_SPEED));
    /* THE MINION MOVES ITSELF: the engine did not move a pickup along its velocity in real runs (the minions stood still with
     * the AI running), so the steering keeps its own velocity (lvx, lvy), steers it every frame toward the target (Lua:
     * Lerp(vel, dir*speed, 0.3)) and adds it to the position here; the engine's own velocity is kept at 0 so nothing moves it twice */
    grid_update();
    if (g_m[i].age % 15 == 0) g_m[i].fly = owner_flies(owner);
    int fly = g_m[i].fly;
    if (t) {
        float ex = fpos(e)[0], ey = fpos(e)[1], gx = fpos(t)[0], gy = fpos(t)[1];
        float aimx = gx, aimy = gy; int final = 1;
        if (g_gw && !line_clear(ex, ey, gx, gy, fly)) {         /* Lua: no line of sight -> CatheryPathFinding around what is in the way */
            if (!g_m[i].wvalid || g_m[i].age % 6 == 0) g_m[i].wvalid = path_step(ex, ey, gx, gy, fly, &g_m[i].wx, &g_m[i].wy);
            if (g_m[i].wvalid) { aimx = g_m[i].wx; aimy = g_m[i].wy; final = 0; }
        } else g_m[i].wvalid = 0;
        float dx = aimx - ex, dy = aimy - ey;
        float len = __builtin_sqrtf(dx * dx + dy * dy);
        float sp = (final && len < 24.0f) ? speed * (len / 24.0f) : speed;   /* arrive softly: at full speed over the target's centre the direction flipped every frame (the blinking) */
        if (len > 0.001f) { dx = dx / len * sp; dy = dy / len * sp; }
        if (t == owner && len < 40.0f) { dx = 0; dy = 0; }      /* nothing to fight: wait beside the owner */
        g_m[i].lvx += (dx - g_m[i].lvx) * 0.3f;
        g_m[i].lvy += (dy - g_m[i].lvy) * 0.3f;
    } else { g_m[i].lvx *= 0.8f; g_m[i].lvy *= 0.8f; }
    move_on_grid(fpos(e), g_m[i].lvx, g_m[i].lvy, fly, &g_m[i].lvx, &g_m[i].lvy); g_m[i].carried++;
    if (g_m[i].age % 30 == 0) {                                /* stuck against rocks: it gave no ground in half a second while chasing an enemy */
        float mx = fpos(e)[0] - g_m[i].chk_x, my = fpos(e)[1] - g_m[i].chk_y;
        if (t && t != owner && g_m[i].chk_x != 0.0f && mx * mx + my * my < 64.0f && g_m[i].age >= DROP_FRAMES + GRACE_FRAMES + 30) {
            g_m[i].skip = t; g_m[i].skip_until = g_m[i].age + 240; g_m[i].target = 0; g_m[i].wvalid = 0;
            LOG_FEW(6, "ffminion: a minion made no progress toward an enemy (type %d) for half a second: it picks another target for a while", *(const int *) (t + E_TYPE));
        }
        g_m[i].chk_x = fpos(e)[0]; g_m[i].chk_y = fpos(e)[1];
    }
    {   /* minions do not stack on one pixel: each is pushed off any other minion closer than 14 units */
        unsigned long sn[SNAP]; unsigned sc = snapshot(sn);
        for (unsigned k = 0; k < sc; k++) {
            unsigned long o = sn[k];
            if (o == e || *(const int *) (o + E_TYPE) != T_PICKUP || *(const int *) (o + E_VARIANT) != V_MINION || !alive(o)) continue;
            float ox = fpos(e)[0] - fpos(o)[0], oy = fpos(e)[1] - fpos(o)[1], od = __builtin_sqrtf(ox * ox + oy * oy);
            if (od >= 14.0f) continue;
            if (od < 0.01f) { float an = (float) rnd(628) / 100.0f; ox = __builtin_cosf(an); oy = __builtin_sinf(an); od = 1.0f; }
            float push = (14.0f - od) * 0.25f;
            float px = fpos(e)[0] + ox / od * push, py = fpos(e)[1] + oy / od * push;
            if (!g_gw || !box_blocked(px, py, fly)) { fpos(e)[0] = px; fpos(e)[1] = py; }
        }
    }
    v[0] = 0; v[1] = 0;
    v = g_m[i].tmp;                                           /* the animation choice below reads our velocity */
    v[0] = g_m[i].lvx; v[1] = g_m[i].lvy;
    {
        float ax = v[0] < 0 ? -v[0] : v[0], ay = v[1] < 0 ? -v[1] : v[1];
        if (ax > ay * 1.3f) g_m[i].horiz = 1; else if (ay > ax * 1.3f) g_m[i].horiz = 0;     /* hysteresis: no flipping on the diagonal */
        g_play(sprite, fly ? (g_m[i].horiz ? "FlyHori" : "FlyVert") : (g_m[i].horiz ? "WalkHori" : "WalkVert"), 0);   /* Lua: FlyHori/FlyVert (the wings) while the player can fly */
    }
    g_m[i].moved_set = 1;
    if (g_m[i].age == 90 || g_m[i].age == 240)
        LOG("ffminion: minion %d at age %d: pos %.0f,%.0f vel %.2f,%.2f speed %.2f target %s (type %d) carried-by-us %d times, room active %d",
            i, g_m[i].age, fpos(e)[0], fpos(e)[1], v[0], v[1], speed, t == owner ? "the owner" : (t ? "an enemy" : "none"),
            t ? *(const int *) (t + E_TYPE) : -1, g_m[i].carried, ffminion_room_active());
    int period = ffminion_period(*(const float *) (owner + P_FIRE_DELAY));
    if ((g_m[i].age + g_m[i].tick_off) % period == 0) contact_tick(e, owner, bonus);
}

void ffheart_poll(void) __attribute__((weak));       /* ffheart.c: sees a hit the game did not report */

/*
 * BONE ORBITALS (fiend.lua:262-275, 309-325): the Lua spawns familiar 3.128 (BONE_ORBITAL) and sets fam.Player = player. The
 * owner pointer sits at familiar+0x3a8 (measured: variants 1, 31, 109, 112 and 239 all hold the player there; +0x400 holds it too,
 * which is the spawner the engine records when the player is passed to Spawn).
 */
#define FAMILIAR_PLAYER 0x3a8
#define T_FAMILIAR 3
#define V_BONE_ORBITAL 128
int ffminion_bone_orbitals(unsigned long player, int n) {
    if (!g_spawn || !g_base || !player || n <= 0) return 0;
    int made = 0;
    for (int i = 0; i < n && i < 64; i++) {
        float vel[2] = { 0, 0 };
        float at[2] = { fpos(player)[0] + 20.0f * cosf(0.785398f * (float) i), fpos(player)[1] + 20.0f * sinf(0.785398f * (float) i) };   /* player.Position + RandomVector() * 20 */
        unsigned long f = g_spawn(*(const unsigned long *) (g_base + GAME_PTR), T_FAMILIAR, V_BONE_ORBITAL, at, vel, player, 0, 0, 0);
        if (!f) continue;
        *(unsigned long *) (f + FAMILIAR_PLAYER) = player;
        made++;
    }
    { static int logs; if (logs++ < 8) LOG("ffminion: %d of %d bone orbitals made for the player", made, n); }
    return made;
}

/*
 * MEASUREMENT for the bone orbitals (Lua fiend.lua:262-275, 309-325 spawns familiar 3.128 and sets fam.Player = player): the
 * familiar's owner field is not known in this image, and writing a guessed offset could crash. So, for any familiar (type 3)
 * that exists, this looks in its fields for the owning player's pointer and logs the offset - one line per familiar variant.
 * Pick up any familiar item (Brother Bobby...) with Fiend, then read "ffminion: familiar variant".
 */
static void probe_familiars(void) {
    static long long next;
    long long t = now_ms();
    if (t < next) return;
    next = t + 1000;
    unsigned long p = g_first_player();
    if (!p) return;
    unsigned long snap[SNAP];
    unsigned n = snapshot(snap);
    static int told[512];
    for (unsigned i = 0; i < n; i++) {
        unsigned long e = snap[i];
        if (*(const int *) (e + E_TYPE) != 3) continue;
        int v = *(const int *) (e + E_VARIANT);
        if (v < 0 || v >= 512 || told[v]) continue;
        told[v] = 1;
        char line[200]; int ll = snprintf(line, sizeof line, "ffminion: familiar variant %d holds the player pointer at +", v);
        int found = 0;
        for (unsigned off = 0x300; off < 0x700 && ll < 170; off += 8)
            if (g_readable((const void *) (e + off), 8) && *(const unsigned long *) (e + off) == p) { ll += snprintf(line + ll, sizeof line - ll, "%#x ", off); found++; }
        if (!found) snprintf(line + ll, sizeof line - ll, "(none in 0x300-0x6ff)");
        LOG("%s", line);
    }
}

#include "fffireball.h"

static void w_update(unsigned long e) {
    ff_flush();
    probe_familiars();
    LOG_FEW(5, "ffminion: the Pickup::Update hook is running (first pickup: 5.%d.%d)", *(const int *) (e + E_VARIANT), *(const int *) (e + E_SUBTYPE));
    o_update(e);
    if (ffheart_poll) ffheart_poll();
    if (*(const int *) (e + E_VARIANT) == V_MINION && *(const int *) (e + E_TYPE) == T_PICKUP && alive(e)) minion_ai(e);
}

/* ---------------------------------------------------------------------------------------------- the hit makes them */
/* a minion for `halves` (2 -> a whole one, 1 -> a half), thrown out of the player */
int ffminion_spawn(unsigned long player, int sub) {
    if (!g_base || !player) return 0;
    float vel[2] = { (float) ((int) rnd(11) - 5), (float) ((int) rnd(11) - 5) };       /* Lua: random(-5, 5) */
    unsigned long e = g_spawn(*(const unsigned long *) (g_base + GAME_PTR), T_PICKUP, V_MINION, fpos(player), vel, player, sub, 0, 0);
    if (!e) { LOG_FEW(0, "ffminion: Spawn(5.750.%d) returned nothing - is the entity in entities2.xml?", sub); return 0; }
    LOG_FEW(1, "ffminion: made a minion (5.750.%d) at %.0f,%.0f", sub, fpos(player)[0], fpos(player)[1]);
    for (int i = 0; i < MAXM; i++) if (g_m[i].e == e) g_m[i].e = 0;      /* a row left by a minion that stood at this address */
    slot_of(e, 1, player);
    return 1;
}

/* The player's frame: the double tap before the game's update, the fireballs after it, then ffheart's hit poll. ffheart's poll used to hang on this
 * same slot; with ours installed first it can no longer wrap it (a real run: "slot 4 holds ..., not code"), so it was left to the pickups and
 * hits were missed or seen late. It runs from here now. */
static void fb_player_frame(unsigned long p, void (*orig)(unsigned long)) {
    fb_before_update(p);
    orig(p);
    fb_after_update(p);
    if (ffheart_poll) ffheart_poll();
}
#ifndef FF_HOST_TEST
static unsigned long real_spawn(unsigned long game, int type, int variant, const float *pos, const float *vel,
                                unsigned long sp, int sub, int seed, int flag) {
    return ((fn_spawn) (g_base + FN_SPAWN))(game, type, variant, pos, vel, sp, sub, seed, flag);
}
static void real_play(void *s, const char *n, int f) { ((fn_play) (g_base + FN_SPRITE_PLAY))(s, n, f); }
static void real_remove_overlay(void *s) { ((void (*)(void *)) (g_base + FN_SPRITE_REMOVE_OVERLAY))(s); }
static void real_overlay(void *s, const char *n, int f) { ((fn_overlay) (g_base + FN_SPRITE_OVERLAY))(s, n, f); }

/* the fireballs' three hooks (fffireball.h): Player::Update (before: the double tap; after: charge, orbit, the shots), Familiar::Update
 * (after: the orbiter put on its ring) and NPC::TakeDamage (what a Fiend's hits credit). Vtables found by their RTTI: Entity_Player
 * 0xa8c0b8 (slot 4 = 0x51e828), Entity_Familiar 0xa904b0 (slot 4 = 0x808574), Entity_NPC 0xa8dd60 (slot 9 = 0x69876c). */
#define PLAYER_VTABLE 0xa8c0b8
#define FAMILIAR_VTABLE 0xa904b0
#define NPC_VTABLE 0xa8dd60
#define FN_PLAYER_UPDATE 0x51e828
#define FN_FAMILIAR_UPDATE 0x808574
#define FN_NPC_TAKE 0x69876c
static fn_update o_player_update, o_familiar_update;
static fn_take o_npc_take;
static void w_player_update(unsigned long p) { fb_player_frame(p, o_player_update); }
static void w_familiar_update(unsigned long f) {
    o_familiar_update(f);
    fb_familiar_update(f);
}
static int w_npc_take(unsigned long e, float dmg, unsigned long long flags, void *ref, int cd) {
    float before[HP_N];
    int have = fb_pre_hit(e, before);
    int r = o_npc_take(e, dmg, flags, ref, cd);
    if (have) fb_post_hit(e, dmg, ref, before);
    return r;
}
#define FN_HAS_COLLECTIBLE_FB 0x4eeb74
#define FN_SPRITE_REPLACE_FB 0x8a0228      /* Sprite::ReplaceSpritesheet(this, int layer, std::string const *)  - marked.c */
#define FN_SPRITE_GRAPHICS_FB 0x8a0284     /* Sprite::LoadGraphics(this, 0) */
void *shim_str_assign(void *self, const char *s);
void shim_str_dtor(void *self);
static void fb_real_sheet(void *sprite, const char *path) {
    long long raw[3]; unsigned char *str = (unsigned char *) raw;
    str[23] = 0;
    shim_str_assign(str, path);
    ((void (*)(void *, int, const void *)) (g_base + FN_SPRITE_REPLACE_FB))(sprite, 0, str);
    shim_str_dtor(str);
    ((void (*)(void *, int)) (g_base + FN_SPRITE_GRAPHICS_FB))(sprite, 0);
}
static int fb_real_trigger(int action, int controller) { return ((int (*)(unsigned long, int, int)) (g_base + FN_TRIGGERED))(0, action, controller) & 1; }
static void take_slot(const char *name, unsigned long vt, int slot, unsigned long expect, void *mine, void **orig) {
    unsigned long *at = (unsigned long *) (g_base + vt + 8 * slot);
    if (*at != (unsigned long) g_base + expect) { LOG("fffireball: %s slot %d holds %lx, not %lx - not hooked", name, slot, *at - (unsigned long) g_base, expect); return; }
    *orig = (void *) *at; *at = (unsigned long) mine;
    LOG("fffireball: %s hooked (vtable slot %d)", name, slot);
}

/* at load, before the image is closed: the Pickup vtable's Update slot is ours (the way rangefix.c takes the knife's) */
void shim_ffminion_install(unsigned char *base) {
    unsigned long *slot = (unsigned long *) (base + PICKUP_VTABLE + 8 * SLOT_UPDATE);
    if (*slot == (unsigned long) w_update) return;
    if (*slot != (unsigned long) base + FN_PICKUP_UPDATE) {
        LOG("ffminion: pickup vtable slot 4 holds %lx, not Pickup::Update - nothing touched", *slot - (unsigned long) base);
        return;
    }
    g_base = base;
    g_spawn = real_spawn; g_play = real_play; g_overlay = real_overlay; g_remove_overlay = real_remove_overlay; g_list_now = shim_eid_list_now; g_readable = shim_eid_readable;
    g_seed ^= (unsigned) now_ms();
    o_update = (fn_update) *slot;
    *slot = (unsigned long) w_update;
    LOG("ffminion: Pickup::Update wrapped (vtable slot 4)");
    g_trigger = fb_real_trigger; g_frame_fn = fb_real_frame;
    g_has_item = (int (*)(unsigned long, int, int)) (g_base + FN_HAS_COLLECTIBLE_FB);
    g_sheet = fb_real_sheet;
    take_slot("Player::Update", PLAYER_VTABLE, 4, FN_PLAYER_UPDATE, (void *) w_player_update, (void **) &o_player_update);
    take_slot("Familiar::Update", FAMILIAR_VTABLE, 4, FN_FAMILIAR_UPDATE, (void *) w_familiar_update, (void **) &o_familiar_update);
    take_slot("NPC::TakeDamage", NPC_VTABLE, 9, FN_NPC_TAKE, (void *) w_npc_take, (void **) &o_npc_take);
}
#endif
