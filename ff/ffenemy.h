/*
 * Fiend Folio's monsters: the behaviour of each, run from the NPC's own Update (Entity_NPC vtable slot 4 = 0x68bd3c, measured: the
 * first thing it does is test the "exists" byte +0x1c2). Included by ffminion.c, which owns the engine helpers used here.
 *
 * What the port has instead of the Lua API: nothing of Sprite's frame is read. Each monster keeps its own frame counter for the
 * animation it asked for (Sprite::Play, 0x89e82c) and tools/ff_anim_table.py wrote down, from the .anm2 files, each animation's
 * length, loop flag and event frames (ffanim_table.h); IsFinished / IsEventTriggered / WasEventTriggered are answered from those.
 * Walls and rocks: ffminion.c's own tile reader and path_step (breadth-first). Projectiles: Game::Spawn type 9 (the game's own
 * NPC::FireProjectiles does exactly that call, 0x685ab0..0x685ac4: w1 = 9, w2 = the params' Variant) with the velocity given.
 *
 * Not done here and so not claimed: fear/charm/confusion/baited states, sound, the second (ccw) Square Fly sheet, the projectile
 * params the Lua sets (Height, FallingSpeed, Scale, flags) - none of those fields is measured yet. Every such gap is also a
 * log line "ffenemy: not done: ..." the first time an enemy needs it.
 */
#include "ffanim_table.h"

static int g_enlog[400], g_ensweep;
#define ENLOG(k, n, ...) do { if (g_enlog[k]++ < (n)) LOG(__VA_ARGS__); } while (0)
#define EN_N 96
typedef struct {
    unsigned long e; unsigned key; int type, variant, sub;
    int init, age, state, cd, cd2, af, dead_done, dead_age, dir;
    char anim[32], ov[32]; int alen, aloop, ovf;
    float f[12]; int i[12]; unsigned long ref, ref2, ref3; int nocarry, hurt_block;
    float po[2]; int la_on, la_hasvel; float la_z, la_acc, la_h, la_vx, la_vy, la_col;   /* Entity.PositionOffset the AI asked for (added to the SpriteOffset) and the launched-enemy state (ferrium/misc/launchedStuff.lua) */
    float so[2]; int so_on;                                               /* the SpriteOffset the AI asked for, re-applied after the engine's update */
    float lastva, lastp[2], dframe, still_dist, lastov; int still, blocked, mv_age, gap_done, st_seen, last_state, dprobe, ovs_n, srun, born; char first_anim[32], mv_anim[32];   /* the log probes (FFC_SPEED/STUCK/AI_TARGET/ANIM_GAP/DEATH/SPRITE_STATIC): the AI's last speed, where the last tick ended, how long it has stood still / been blocked */
    int flaming; float lastreal; int stuck, driven;                                   /* en_sprite_sync: the engine's own frame last tick, how long it has not moved, and whether the port sets the frame itself */
} en_t;
static en_t g_en[EN_N];
static int g_en_on = 1;
static unsigned long long g_hit_flags; static unsigned long g_hit_src;           /* the flag and the source entity of the hit the TakeDamage hook is looking at (the Lua Hurt callbacks' flag, source) */
#define DMG_FIRE 2ull

static const ffa_t *ffa_find(unsigned key, const char *name) {
    for (const ffa_t *a = ffa_tab; a->name; a++) if (a->key == key && !strcmp(a->name, name)) return a;
    unsigned k0 = key & ~15u;                           /* the monster's other subtypes share the sheet of subtype 0 */
    if (k0 != key) for (const ffa_t *a = ffa_tab; a->name; a++) if (a->key == k0 && !strcmp(a->name, name)) return a;
    return 0;
}
/* ---- the log's view of the room (ffsay.h categories). The engine's RoomShape id is NOT measured in this port: what is known is the grid
 * (room+0x14 x room+0x18 tiles, the collision table at room+0xe98), so a room is described by its grid size and where its non-wall tiles are
 * - enough to tell a 1x1 from an L, an I or a 2x2 in the log. */
#if ISAAC_DIAG
static unsigned long ffl_room_key(void) {
    unsigned long r = g_room ? g_room() : 0, k = 0;
    if (!r || !safe_copy(&k, (const void *) (r + 8), 8)) return 0;
    return k;                                                   /* Room+8 = the room's descriptor: one per room of a floor (eid_craft.c) */
}
static void ffl_room_desc(char *b, size_t n) {
    if (!g_gw || !g_gh) { snprintf(b, n, "grid unknown"); return; }
    int open = 0, c0 = 999, c1 = -1, r0 = 999, r1 = -1;
    for (int r = 0; r < g_gh; r++) for (int c = 0; c < g_gw; c++) {
        if (g_cells[c + r * g_gw] >= g_wallv) continue;
        open++; if (c < c0) c0 = c; if (c > c1) c1 = c; if (r < r0) r0 = r; if (r > r1) r1 = r;
    }
    snprintf(b, n, "grid %dx%d, %d non-wall tiles in cols %d..%d rows %d..%d", g_gw, g_gh, open, c0, c1, r0, r1);
}
#endif
static void en_state_watch(en_t *s) {                          /* STATE: every change of a machine's / monster's state number */
    if (!s->st_seen) { s->st_seen = 1; s->last_state = s->state; return; }
    if (s->state != s->last_state) {
        FFLOG(FFC_STATE, FFKEY(s->type, s->variant), "%d.%d.%d state %d -> %d (anim '%s', age %d)", s->type, s->variant, s->sub, s->last_state, s->state, s->anim, s->age);
        s->last_state = s->state;
    }
}
static en_t *en_slot(unsigned long e) {
    int free = -1;
    for (int i = 0; i < EN_N; i++) {
        if (g_en[i].e == e) {
            if (g_en[i].type == *(const int *) (e + E_TYPE) && g_en[i].variant == *(const int *) (e + E_VARIANT) && g_en[i].sub == *(const int *) (e + E_SUBTYPE)) return &g_en[i];
            g_en[i].e = 0;                                   /* the address came back as another entity */
        }
        if (!g_en[i].e && free < 0) free = i;
    }
    if (free < 0) { free = 0; }
    en_t *s = &g_en[free]; memset(s, 0, sizeof *s);
    s->e = e; s->type = *(const int *) (e + E_TYPE); s->variant = *(const int *) (e + E_VARIANT); s->sub = *(const int *) (e + E_SUBTYPE);
    s->key = ((unsigned) s->type << 20) | ((unsigned) s->variant << 4) | ((unsigned) s->sub & 15u);
#if ISAAC_DIAG
    { char rd[110]; ffl_room_desc(rd, sizeof rd);
      FFLOG(FFC_SPAWN, FFKEY(s->type, s->variant), "%d.%d.%d at (%.0f,%.0f) room %lx %s HANDLER=yes", s->type, s->variant, s->sub, fpos(e)[0], fpos(e)[1], ffl_room_key(), rd); }
#endif
    return s;
}

/* ------------------------------------------------------------------------------------------ the Lua's own small helpers */
static float vlen(float x, float y) { return sqrtf(x * x + y * y); }
static float frand(void) { return (float) (xs(&g_seed) & 0xffffff) / 16777216.0f; }
static int rint_(int lo, int hi) { return lo + (int) rnd((unsigned) (hi - lo + 1)); }
static void vrot(float x, float y, float deg, float *ox, float *oy) {
    float r = deg * 3.14159265f / 180.0f, c = cosf(r), s = sinf(r);
    *ox = x * c - y * s; *oy = x * s + y * c;
}
static void vresize(float *x, float *y, float len) { float l = vlen(*x, *y); if (l < 1e-6f) { *x = len; *y = 0; return; } *x *= len / l; *y *= len / l; }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static unsigned long en_game(void) { return g_base ? *(const unsigned long *) (g_base + GAME_PTR) : 0; }

/* sprite: Lua mod:spritePlay(sprite, name) = play unless it already is. The frame counter restarts with the animation. */
static void en_play(en_t *s, const char *name) {
    if (!strcmp(s->anim, name)) return;
    strncpy(s->anim, name, sizeof s->anim - 1); s->anim[sizeof s->anim - 1] = 0;
    s->af = 0;
    const ffa_t *a = ffa_find(s->key, name);
    s->alen = a ? a->len : 0; s->aloop = a ? a->loop : 1;
    if (!a) FFLOG(FFC_ASSET_MISSING, FFKEY(s->type, s->variant), "%d.%d has no animation '%s' in the table (ffanim_table.h)", s->type, s->variant, name);
    g_play((void *) (s->e + E_SPRITE), name, 1);
}
static void en_replay(en_t *s, const char *name) { s->anim[0] = 0; en_play(s, name); }   /* Lua sprite:Play(name, true) */
static int en_playing(const en_t *s, const char *name) { return !strcmp(s->anim, name) && (s->aloop || s->af < s->alen); }
static int en_finished(const en_t *s, const char *name) { return !strcmp(s->anim, name) && !s->aloop && s->alen && s->af >= s->alen; }
static int en_event(const en_t *s, const char *name, const char *ev) {
    const ffa_t *a = ffa_find(s->key, name); if (!a || strcmp(s->anim, name)) return 0;
    for (int i = 0; i < a->evn; i++) if (!strcmp(ffe_tab[a->ev0 + i].name, ev) && ffe_tab[a->ev0 + i].frame == s->af) return 1;
    return 0;
}
static int en_was_event(const en_t *s, const char *name, const char *ev) {
    const ffa_t *a = ffa_find(s->key, name); if (!a || strcmp(s->anim, name)) return 0;
    for (int i = 0; i < a->evn; i++) if (!strcmp(ffe_tab[a->ev0 + i].name, ev) && ffe_tab[a->ev0 + i].frame <= s->af) return 1;
    return 0;
}


#include "ffenums.h"
#include "ffoffsets.h"     /* engine field offsets, each measured and commented with how (agent: RE primitives) */
#include "ffsounds.h"      /* FFSND_<key of mod.Sounds> = the sound id the port registered for it (ffsound.c) */
/* ---------------------------------------------------------------------------------- engine fields (an offset of 0 in ffoffsets.h = not measured: the accessor logs once and does nothing) */
#define EN_FIELD(name, off) static int name##_ok(void) { if (!(off)) { FFLOG(FFC_NOT_DONE, 14000 + __LINE__, "ffenemy: not done: field %s is not measured", #name); return 0; } return 1; }
EN_FIELD(npc_state, OFF_NPC_STATE) EN_FIELD(npc_statef, OFF_NPC_STATEFRAME) EN_FIELD(npc_i1, OFF_NPC_I1) EN_FIELD(npc_i2, OFF_NPC_I2)
EN_FIELD(npc_v1, OFF_NPC_V1) EN_FIELD(npc_v2, OFF_NPC_V2) EN_FIELD(npc_pcd, OFF_NPC_PROJCD) EN_FIELD(npc_pdelay, OFF_NPC_PROJDELAY) EN_FIELD(npc_scale, OFF_NPC_SCALE)
EN_FIELD(ent_coll, OFF_ENT_COLLCLASS) EN_FIELD(ent_gcoll, OFF_ENT_GRIDCOLL) EN_FIELD(ent_friction, OFF_ENT_FRICTION) EN_FIELD(ent_mass, OFF_ENT_MASS) EN_FIELD(ent_target, OFF_ENT_TARGET)
EN_FIELD(ent_child, OFF_ENT_CHILD) EN_FIELD(ent_spriteoff, OFF_ENT_SPRITEOFFSET) EN_FIELD(ent_depthoff, OFF_ENT_DEPTHOFFSET)
EN_FIELD(proj_height, OFF_PROJ_HEIGHT) EN_FIELD(proj_fspeed, OFF_PROJ_FALLINGSPEED) EN_FIELD(proj_faccel, OFF_PROJ_FALLINGACCEL) EN_FIELD(proj_flags, OFF_PROJ_FLAGS)
EN_FIELD(proj_scale, OFF_PROJ_SCALE) EN_FIELD(proj_curving, OFF_PROJ_CURVING) EN_FIELD(proj_homing, OFF_PROJ_HOMING)
EN_FIELD(eff_timeout, OFF_EFF_TIMEOUT) EN_FIELD(eff_lifespan, OFF_EFF_LIFESPAN) EN_FIELD(eff_scale, OFF_EFF_SCALE)
/* npc.SpriteOffset = Vector(x, y): the sprite moves, the shadow and the hitbox stay. Written now and again after the engine's update (see w_npc_update). */
static void en_apply_offsets(en_t *s, unsigned long e) {
    if (!ent_spriteoff_ok()) return;
    *(float *) (e + OFF_ENT_SPRITEOFFSET) = s->so[0] + s->po[0]; *(float *) (e + OFF_ENT_SPRITEOFFSET + 4) = s->so[1] + s->po[1];
}
static void en_sprite_offset(en_t *s, unsigned long e, float x, float y) {
    if (!ent_spriteoff_ok()) return;
    s->so[0] = x; s->so[1] = y; s->so_on = 1; en_apply_offsets(s, e);
}
/* npc.PositionOffset: the engine draws the sprite at Position + the vector at +0x320, which both the vanilla hoppers' jump height and the Lua's SpriteOffset move;
 * the port has one vector, so the two are kept apart here and their sum is written. */
static void en_position_offset(en_t *s, unsigned long e, float x, float y) { s->po[0] = x; s->po[1] = y; s->so_on = 1; en_apply_offsets(s, e); }
/* Sprite.Offset (sprite + 0xd4, measured): the sprite's own offset, kept by the sprite itself */
static void en_spr_off(unsigned long e, float x, float y) { float *o = (float *) (e + E_SPRITE + 0xd4); o[0] = x; o[1] = y; }
static int  en_get_state(unsigned long e) { return npc_state_ok() ? *(int *) (e + OFF_NPC_STATE) : 0; }
static void en_set_state(unsigned long e, int v) { if (npc_state_ok()) *(int *) (e + OFF_NPC_STATE) = v; }
static int  en_get_statef(unsigned long e) { return npc_statef_ok() ? *(int *) (e + OFF_NPC_STATEFRAME) : 0; }
static void en_set_statef(unsigned long e, int v) { if (npc_statef_ok()) *(int *) (e + OFF_NPC_STATEFRAME) = v; }
static void en_set_collision(unsigned long e, int c) { if (ent_coll_ok()) *(int *) (e + OFF_ENT_COLLCLASS) = c; }
static void en_set_gridcoll(unsigned long e, int c) { if (ent_gcoll_ok()) *(int *) (e + OFF_ENT_GRIDCOLL) = c; }
static void en_set_friction(unsigned long e, float f) { if (ent_friction_ok()) *(float *) (e + OFF_ENT_FRICTION) = f; }
static void en_set_mass(unsigned long e, float f) { if (ent_mass_ok()) *(float *) (e + OFF_ENT_MASS) = f; }
static void en_set_can_shut_doors(unsigned long e, int can) { *(unsigned char *) (e + OFF_NPC_CANSHUTDOORS) = can ? 0 : 1; }   /* stored inverted, see ffoffsets.h */
static int  en_flag(unsigned long e, unsigned long long f) { return (*(const unsigned long long *) (e + E_FLAGS) & f) != 0; }
static void en_flag_set(unsigned long e, unsigned long long f) { *(unsigned long long *) (e + E_FLAGS) |= f; }
static void en_flag_clear(unsigned long e, unsigned long long f) { *(unsigned long long *) (e + E_FLAGS) &= ~f; }
/* Entity status checks the Lua makes (mod:isScare / isConfuse / isCharm / isBaited ...) */
static int en_scared(unsigned long e) { return en_flag(e, EF_FLAG_FEAR); }
static int en_confused(unsigned long e) { return en_flag(e, EF_FLAG_CONFUSION); }
static int en_charmed(unsigned long e) { return en_flag(e, EF_FLAG_CHARM); }
/* the port's sound player (ffsound.c): sound id (vanilla enum or FFSND_*), volume, pitch */
void ffsound_play(int id, float vol, float pitch);
static void en_sound(int id, float vol, float pitch) { ffsound_play(id, vol, pitch); }
void ffsound_loop(int id, float vol, float pitch);
void ffsound_stop(int id);
/* sfx:Play(id, vol, 0, true, pitch) guarded by `not sfx:IsPlaying(id)`: the port keeps the on/off itself (a bit per sound id slot, 8 loops are enough) */
static int g_loop_on[8], g_loop_id[8]; static unsigned long g_loop_owner[8];
static int en_loop_playing(int id) { for (int k = 0; k < 8; k++) if (g_loop_on[k] && g_loop_id[k] == id) return 1; return 0; }
static void en_loop_start(int id, float vol, float pitch, unsigned long owner) {
    if (en_loop_playing(id)) return;
    for (int k = 0; k < 8; k++) if (!g_loop_on[k]) { g_loop_on[k] = 1; g_loop_id[k] = id; g_loop_owner[k] = owner; ffsound_loop(id, vol, pitch); return; }
}
static void en_loop_stop(int id) { for (int k = 0; k < 8; k++) if (g_loop_on[k] && g_loop_id[k] == id) { g_loop_on[k] = 0; ffsound_stop(id); } }
/* projectiles: what FireProjectiles sets from a ProjectileParams after Game::Spawn(9, Variant ...) */
typedef struct { int variant; unsigned long long flags; float height, fspeed, faccel, faccel_mod, scale, vel_multi, curving, homing; int has_height, has_fspeed, has_faccel, has_scale, has_faccel_mod, has_curving; float fspeed_mod, height_mod; int has_fspeed_mod, has_height_mod; } en_pp_t;
static unsigned long en_shoot_p(unsigned long e, float x, float y, float vx, float vy, const en_pp_t *pp);


/* more sprite pieces: SetFrame 0x89eab8 / SetOverlayFrame 0x89ee7c (measured: both call the same inner setter on the main (+0x30) / overlay (+0x60) animation state),
 * the overlay's own frame counter, and FlipX (below). */
static void en_set_frame(en_t *s, const char *name, int frame) { s->af = frame; strncpy(s->anim, name, sizeof s->anim - 1); if (g_setframe) g_setframe((void *) (s->e + E_SPRITE), name, frame); }
static fn_setframe g_setoframe;
static void en_set_overlay_frame(unsigned long e, const char *name, int frame) { if (g_setoframe) g_setoframe((void *) (e + E_SPRITE), name, frame); }   /* null in the host tests */
static void en_overlay(en_t *s, const char *name) { if (strcmp(s->ov, name)) { strncpy(s->ov, name, sizeof s->ov - 1); s->ovf = 0; g_overlay((void *) (s->e + E_SPRITE), name, 1); } }
static void en_overlay_off(en_t *s) { if (s->ov[0]) { s->ov[0] = 0; g_remove_overlay((void *) (s->e + E_SPRITE)); } }
static int en_overlay_finished(const en_t *s, const char *name) { const ffa_t *a = ffa_find(s->key, name); return a && !strcmp(s->ov, name) && !a->loop && s->ovf >= a->len; }
/* FlipX: measured. Vanilla NPC code that plays WalkHori does `sprite.Play(name); *(u8 *)(npc + 0x190) = (velocity.x < 0)` (7 sites: 0x6025f4, 0x6a9c84,
 * 0x7b9640, 0x803bd8, 0x804090, 0x80d360, 0x8849bc), so Sprite::FlipX is the byte at sprite + 0x140 (= entity + 0x190); 1 = mirrored, i.e. facing left. */
#define E_FLIPX 0x190
#define E_SPR_FRAME 0xa0                                          /* the main animation state's current frame (float), measured */
#define E_SPR_PLAYING 0xa4                                        /* its Playing byte: Play sets it, SetFrame clears it */
static void en_flip(unsigned long e, int flip) { *(unsigned char *) (e + E_FLIPX) = flip ? 1 : 0; }
static int en_flipped(unsigned long e) { return *(unsigned char *) (e + E_FLIPX) != 0; }
#define E_VISIBLE 0x1c1                                                        /* Entity::Visible: the ctor 0x82bd5c stores 0x101 at +0x1c1 (Visible, Exists); NPC::Render skips the draw (and the shadow) when it is 0 */
static void en_visible(unsigned long e, int v) { *(unsigned char *) (e + E_VISIBLE) = v ? 1 : 0; }

static float *en_target(unsigned long e) { unsigned long p = g_first_player(); (void) e; return p ? fpos(p) : 0; }
static unsigned long en_spawn(int type, int variant, float x, float y, float vx, float vy, unsigned long spawner, int sub) {
    float p[2] = { x, y }, v[2] = { vx, vy };
    return g_spawn(en_game(), type, variant, p, v, spawner, sub, 0, 0);
}
/* Isaac.Spawn(1000, EffectVariant.SHOCKWAVE (61), ...) with Parent/MinRadius/MaxRadius/Timeout. Measured at the game's own spawn sites (0x50b758, 0x52127c,
 * 0x5ef94c...): after Game::Spawn it calls Entity::SetParent(effect, owner) 0x82bafc and stores MinRadius +0x44c (float), MaxRadius +0x450 (float) and the Timeout
 * twice, +0x454 and +0x458 (int, the countdown and its reload). The game's Effect::Update then runs the ring: it breaks rocks and hurts whatever is inside. */
#define OFF_EFF_MINRADIUS 0x44c
#define OFF_EFF_MAXRADIUS 0x450
#define OFF_EFF_TIMEOUT2  0x458
static unsigned long en_shockwave(unsigned long owner, float x, float y, float minr, float maxr, int timeout) {
    unsigned long w = en_spawn(1000, 61, x, y, 0, 0, owner, 0);
    if (!w) return 0;
    if (g_set_parent) g_set_parent(w, owner);
    *(float *) (w + OFF_EFF_MINRADIUS) = minr; *(float *) (w + OFF_EFF_MAXRADIUS) = maxr;
    *(int *) (w + OFF_EFF_TIMEOUT) = timeout; *(int *) (w + OFF_EFF_TIMEOUT2) = timeout;
    return w;
}
/* NPC:FireProjectiles for one shot: Game::Spawn(9, Variant, ...) with the velocity, then the params' fields written on the new projectile */
static unsigned long en_shoot_p(unsigned long e, float x, float y, float vx, float vy, const en_pp_t *pp) {
    float m = pp && pp->vel_multi > 0 ? pp->vel_multi : 1.0f;
    unsigned long pr = en_spawn(9, pp ? pp->variant : 0, x, y, vx * m, vy * m, e, 0);
    if (!pr || !pp) return pr;
    if (pp->has_faccel_mod && proj_faccel_ok()) *(float *) (pr + OFF_PROJ_FALLINGACCEL) += pp->faccel_mod;
    if (pp->has_fspeed_mod && proj_fspeed_ok()) *(float *) (pr + OFF_PROJ_FALLINGSPEED) += pp->fspeed_mod;
    if (pp->has_height_mod && proj_height_ok()) *(float *) (pr + OFF_PROJ_HEIGHT) += pp->height_mod;
    if (pp->has_curving) FFLOG(FFC_NOT_DONE, 15, "ffenemy: not done: ProjectileParams.CurvingStrength (field not measured)");
    if (pp->has_height && proj_height_ok()) *(float *) (pr + OFF_PROJ_HEIGHT) = pp->height;
    if (pp->has_fspeed && proj_fspeed_ok()) *(float *) (pr + OFF_PROJ_FALLINGSPEED) = pp->fspeed;
    if (pp->has_faccel && proj_faccel_ok()) *(float *) (pr + OFF_PROJ_FALLINGACCEL) = pp->faccel;
    if (pp->flags && proj_flags_ok()) *(unsigned long long *) (pr + OFF_PROJ_FLAGS) |= pp->flags;
    if (pp->has_scale) { if (proj_scale_ok()) *(float *) (pr + OFF_PROJ_SCALE) = pp->scale; }
    return pr;
}
static void en_shoot(unsigned long e, int variant, float x, float y, float vx, float vy) { en_pp_t pp; memset(&pp, 0, sizeof pp); pp.variant = variant; en_shoot_p(e, x, y, vx, vy, &pp); }
static void en_kill(unsigned long e) { hit_enemy(e, 1000000.0f); }
/* an entity whose XML baseHP is 0 (Stinger projectiles, creeps...) has MaxHP 0 and is never dead from its HP: the engine kills only by Kill() */
static int en_dead(unsigned long e) { return !alive(e) || (g_hp_off && fb_hp(e) <= 0.0f && *(const float *) (e + 0x358) > 0.0f); }
static int en_room_clear(void) { int a, b, d, t; ffminion_room_info(&a, &b, &d, &t); return t == 0; }
/* Walls and rocks, from the ENGINE's own answer when it is known (Room::GetGridCollision: 1 pit, 2 object, 3 solid, 4 wall, 5 wall-but-player):
 * a flyer meets only walls, a walker pits, rocks and walls. Without any grid nothing counts as solid (tile_at() says "wall" for every tile then,
 * and the monsters would never move). */
static int en_xy(float x, float y, int fly) {
    if (!g_gw && !g_grid_class) return 0;
    int v = fb_tile(x, y);
    return fly ? (v == 4 || v == 5 || v == 255) : (v >= 1 && v <= 5) || v == 255;
}
static int en_box(float x, float y, int fly) { const float r = 6.0f; return en_xy(x - r, y - r, fly) || en_xy(x + r, y - r, fly) || en_xy(x - r, y + r, fly) || en_xy(x + r, y + r, fly); }
static int en_line_clear(float x0, float y0, float x1, float y1, int fly) {
    float dx = x1 - x0, dy = y1 - y0, d = vlen(dx, dy); int steps = (int) (d / 10.0f) + 1;
    for (int k = 1; k < steps; k++) if (en_box(x0 + dx * k / steps, y0 + dy * k / steps, fly)) return 0;
    return 1;
}
/* Room::CheckLine(a, b, mode 0, ...): nothing solid between the two */
static int en_line(const float *a, const float *b, int fly) { return en_line_clear(a[0], a[1], b[0], b[1], fly); }

/* ------------------------------------------------------------------------------------------ Red Horf (erfly/enemies/redhorf.lua) */
static void ai_redhorf(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    v[0] *= 0.8f; v[1] *= 0.8f;
    float dist = vlen(t[0] - p[0], t[1] - p[1]);
    if (s->dead_done) return;
    if (s->state) {                                                            /* d.dying */
        en_play(s, "Death");
        if (en_event(s, "Death", "Explode")) { s->state = 0; en_kill(e); }
    } else {
        if (dist < 160.0f && !en_confused(e)) s->f[0] += 30.0f * (1.0f / (dist / 3.0f));
        else if ((dist > 200.0f || en_confused(e)) && s->f[0] > 0) s->f[0] -= 1.0f;
        if (s->f[0] > 100.0f || en_scared(e)) s->state = 1;
        else if (s->f[0] > 66.0f) en_play(s, "Shake03");
        else if (s->f[0] > 33.0f) en_play(s, "Shake02");
        else en_play(s, "Shake01");
    }
}
static void die_redhorf(en_t *s, unsigned long e) {
    float *p = fpos(e), ex = s->f[0]; en_pp_t pp; memset(&pp, 0, sizeof pp);
    en_sound(SOUND_SHAKEY_KID_ROAR, 1.0f, 1.0f);
    if (ex > 66.0f) { pp.has_faccel_mod = 1; pp.faccel_mod = -0.05f; for (int a = 0; a <= 360; a += 45) { float x, y; vrot(0, 9, (float) a, &x, &y); en_shoot_p(e, p[0], p[1], x, y, &pp); } }
    else if (ex > 33.0f) for (int a = 0; a <= 360; a += 45) { float x, y; vrot(0, 6, (float) a, &x, &y); en_shoot_p(e, p[0], p[1], x, y, &pp); }
    else for (int a = 45; a <= 315; a += 90) { float x, y; vrot(0, 6, (float) a, &x, &y); en_shoot_p(e, p[0], p[1], x, y, &pp); }
}

/* ------------------------------------------------------------------------------------------ Square Fly (erfly/enemies/squarefly.lua) */
static void ai_squarefly(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    const float speed = 4.5f;
    en_play(s, "Fly");
    float mv1 = t[0] - p[0] > 0 ? 2 : 1, mv3 = t[0] - p[0] > 0 ? 1 : 2, mv2 = t[1] - p[1] > 0 ? 2 : 1, mv4 = t[1] - p[1] > 0 ? 1 : 2;
    if (!s->init) { s->init = 1; if (s->sub & 1) FFLOG(FFC_NOT_DONE, 7, "ffenemy: not done: Square Fly's counter-clockwise sheet (Sprite::Load)"); }
    if (!s->i[0]) { s->cd = 1; s->dir = ((s->sub >> 1) & 3) + 1; s->i[0] = 1; }
    else {
        float tx = 0, ty = 0;
        if (s->dir == 1) tx = speed * mv1; else if (s->dir == 2) ty = speed * mv2; else if (s->dir == 3) tx = -speed * mv3; else ty = -speed * mv4;
        s->cd++;
        v[0] = tx * 0.3f + v[0] * 0.6f; v[1] = ty * 0.3f + v[1] * 0.6f;
    }
    if (s->cd == 25) {
        if (!(s->sub & 1)) { if (++s->dir == 5) s->dir = 1; } else { if (--s->dir == 0) s->dir = 4; }
        s->cd = 1;
    }
}
static void die_burst(en_t *s, unsigned long e) { (void) s; float *p = fpos(e); en_spawn(1000, 3, p[0], p[1], 0, 0, e, 0); }

/* ------------------------------------------------------------------------------------------ Slobber / Bleedy (erfly/enemies/slobber.lua) */
static void ai_slobber(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    float dist = vlen(t[0] - p[0], t[1] - p[1]), ang = atan2f(t[1] - p[1], t[0] - p[0]) * 57.29578f;
    v[0] *= 0.7f; v[1] *= 0.7f;
    if (!s->init) { s->init = 1; s->state = 0; s->cd = 20 + (int) rnd(11); s->cd2 = 80 + (int) rnd(16); }
    if (s->age % 8 == 0 && s->age > 1) {
        unsigned long c = s->variant == 2100 ? en_spawn(1000, 94, p[0], p[1], 0, 0, e, 160) : en_spawn(1000, 22, p[0], p[1], 0, 0, e, 0);
        if (c) {
            float *sc = (float *) (c + E_SPRITE + 0xdc); sc[0] *= 0.75f; sc[1] *= 0.75f;
            /* FF's row 1000.94.160 is "Slippery Blue Creep" (effects/Blue Creep.anm2 + effect_waterpool.png). The game showed it brown (ff-codigo46/47 videos): the sheet is also set from here, as
             * slobber.lua does for the friendly one (ReplaceSpritesheet(0, "gfx/effects/effect_waterpool.png") + LoadGraphics) - the vanilla water creep sheet, blue. */
            if (s->variant == 2100 && g_sheet) g_sheet((void *) (c + E_SPRITE), "gfx/effects/effect_waterpool.png");
        }
    }
    if (s->state == 0) {
        g_play((void *) (e + E_SPRITE), v[0] * v[0] > v[1] * v[1] ? "WalkHori" : "WalkVert", 0);       /* AnimWalkFrame */
        g_overlay((void *) (e + E_SPRITE), "HeadWalk", 0);
        float l = vlen(v[0], v[1]); if (l > 0.01f) { v[0] += v[0] / l * 1.05f; v[1] += v[1] / l * 1.05f; }
        if (s->age % 12 == 0 || l < 0.2f) {                                    /* path:MoveRandomly */
            float a = frand() * 360.0f, x, y; vrot(2.0f, 0, a, &x, &y);
            v[0] += x * 0.5f; v[1] += y * 0.5f;
        }
        if (s->cd < s->cd2) s->cd++;
        if (en_line(p, t, 0) && dist <= 180.0f && s->cd >= s->cd2 && s->age > 4) { s->state = 1; s->cd = 0; s->cd2 = 85 + (int) rnd(16); }
    } else {
        g_remove_overlay((void *) (e + E_SPRITE));
        if (en_finished(s, "Attack")) { s->state = 0; s->anim[0] = 0; }
        else if (en_event(s, "Attack", "Shoot")) {
            float rad = ang / 57.29578f, x = cosf(rad), y = sinf(rad);
            en_sound(SOUND_WHEEZY_COUGH, 1.0f, 1.1f);
            en_pp_t pp; memset(&pp, 0, sizeof pp); pp.vel_multi = 7.0f;       /* 9 on Hard/Greedier (Difficulty % 2 == 1): the difficulty is not read here, normal assumed */
            unsigned long ef = en_spawn(1000, 2, p[0], p[1], 0, 0, e, 5);
            (void) ef;
            FFLOG(FFC_NOT_DONE, 16, "ffenemy: not done: Slobber's mouth effect SpriteOffset/DepthOffset/Color/FollowParent");
            if (s->variant == 2100) pp.variant = 4;
            if (s->variant == 2101 && s->af >= 36) { pp.has_scale = 1; pp.scale = 1.4f; }
            en_shoot_p(e, p[0], p[1], x, y, &pp);
        } else en_play(s, "Attack");
    }
}

/* ------------------------------------------------------------------------------------------ Benign (ferrium/enemies/benign.lua) */
static void diag_move(en_t *s, unsigned long e, float speed, float *ox, float *oy) {
    float *v = fvel(e); (void) s;
    *ox = v[0] < 0 ? -speed : speed; *oy = v[1] < 0 ? -speed : speed;
}
static void ai_benign(en_t *s, unsigned long e) {
    float *v = fvel(e), tx, ty;
    if (!s->anim[0]) en_play(s, "Appear");
    if (en_finished(s, "Appear") || en_finished(s, "Retaliate")) en_replay(s, "Idle");
    if (!strcmp(s->anim, "Idle")) { diag_move(s, e, 2.0f, &tx, &ty); v[0] = lerpf(v[0], tx, 0.1f); v[1] = lerpf(v[1], ty, 0.1f); }
    else if (!strcmp(s->anim, "Retaliate")) {
        diag_move(s, e, 1.5f, &tx, &ty); v[0] = lerpf(v[0], tx, 0.1f); v[1] = lerpf(v[1], ty, 0.1f);
        if (en_event(s, "Retaliate", "shoot")) {
            en_sound(SOUND_LITTLE_SPIT, 1.0f, (float) rint_(180, 220) / 100.0f);
            float a = frand() * 360.0f, *p = fpos(e);
            for (int i = 0; i < 3; i++) { float x, y; vrot(0, 8, a + 120.0f * i, &x, &y); en_shoot(e, 0, p[0], p[1], x, y); }
        }
    }
}
/* Hurt: any hit while it Idles makes it retaliate (the Lua's Hurt callback) */
static void hurt_benign(en_t *s, unsigned long e, float dmg) { (void) e; if (dmg > 0 && !strcmp(s->anim, "Idle")) en_replay(s, "Retaliate"); }


/* ------------------------------------------------------------------------------------------ shared pieces of the Lua */
static float en_angle(float x, float y) { return atan2f(y, x) * 57.29578f; }
static float en_angle_diff(float ax, float ay, float bx, float by) {         /* mod:GetAbsoluteAngleDifference: 0..180 */
    float d = fabsf(en_angle(ax, ay) - en_angle(bx, by)); while (d > 360.0f) d -= 360.0f; return d > 180.0f ? 360.0f - d : d;
}
static void en_scale(unsigned long e, float sx, float sy) { float *sc = (float *) (e + E_SPRITE + 0xdc); sc[0] = sx; sc[1] = sy; }   /* SpriteScale (sprite +0xdc, measured) */
/* a walker's step toward (tx,ty): in sight -> straight, else along the breadth-first path (Pathfinder:FindGridPath adds speed along it) */
static void en_walk_to(unsigned long e, const float *t, float speed_direct, float speed_path, int set) {
    float *p = fpos(e), *v = fvel(e); float wx, wy;
    if (en_line_clear(p[0], p[1], t[0], t[1], 0)) {
        float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, speed_direct);
        if (set) { v[0] = x; v[1] = y; } else { v[0] += x; v[1] += y; }
    } else if (path_step(p[0], p[1], t[0], t[1], 0, &wx, &wy)) {
        float x = wx - p[0], y = wy - p[1]; vresize(&x, &y, speed_path); v[0] += x; v[1] += y;
    }
}
static en_t *en_adopt(unsigned long e) { return e ? en_slot(e) : 0; }
static unsigned long en_parent_alive(unsigned long ref) {                      /* a monster we hold a pointer to must be in the room's list and alive */
    if (!ref) return 0;
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned i = 0; i < n; i++) if (snap[i] == ref) return alive(ref) && !en_dead(ref) ? ref : 0;
    return 0;
}
static void en_remove(unsigned long e) {                                       /* npc:Remove(): vtable slot 11 = 0x6801a0 */
    const unsigned long *vt = *(const unsigned long *const *) e;
    if (vt) ((void (*)(unsigned long)) vt[11])(e); else { *(unsigned char *) (e + E_EXISTS) = 0; *(unsigned char *) (e + E_DEAD) = 1; }
}

/* ------------------------------------------------------------------------------------------ Butt Fly (guwah/enemies/buttfly.lua) */
static void ai_buttfly(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) {
        static const float dx[4] = { -1, 1, -1, 1 }, dy[4] = { 1, 1, -1, -1 };
        int d = (s->sub == 0 || s->sub > 4) ? rint_(1, 4) : s->sub;
        s->f[0] = dx[d - 1]; s->f[1] = dy[d - 1]; s->cd = rint_(15, 30); s->state = 0; s->init = 1;
        en_play(s, "Appear");
        en_flag_set(e, EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_PHYSICS_KNOCKBACK);                  /* the Lua's d.init */
    }
    if (s->state == 0) {
        if (en_finished(s, "Appear")) { v[0] = s->f[0]; v[1] = s->f[1]; en_play(s, "Fly"); }
        else if (!en_playing(s, "Appear")) {
            float a = en_angle(v[0], v[1]); float snap = 90.0f * floorf((a + 45.0f) / 90.0f + 0.5f) - (a + 45.0f);
            if (vlen(v[0], v[1]) <= 0.01f) { v[0] = s->f[0]; v[1] = s->f[1]; }
            else { float x, y; vrot(v[0], v[1], snap, &x, &y); vresize(&x, &y, 4.0f); v[0] = lerpf(v[0], x, 0.1f); v[1] = lerpf(v[1], y, 0.1f); }
            en_play(s, "Fly");
            if (--s->cd <= 0 && vlen(t[0] - p[0], t[1] - p[1]) < 130.0f) {
                for (int i = -90; i <= 90; i += 90) {
                    float rx, ry; vrot(v[0], v[1], (float) i, &rx, &ry);
                    if (en_angle_diff(rx, ry, t[0] - p[0], t[1] - p[1]) < 15.0f) { s->f[2] = snap + (float) i; s->state = 1; break; }
                }
            }
        }
    } else {
        if (en_finished(s, "Shoot")) { s->state = 0; s->cd = rint_(30, 45); s->anim[0] = 0; en_play(s, "Fly"); }
        else if (en_event(s, "Shoot", "Shoot")) {
            en_spawn(1000, 34, p[0], p[1], 0, 0, e, 0);                       /* game:ButterBeanFart(pos, 60, ...): the cloud; its push of the others is not done */
            FFLOG(FFC_NOT_DONE, 9, "ffenemy: not done: ButterBeanFart's knock-back and poison of the others around");
            float x, y; vrot(v[0], v[1], s->f[2], &x, &y); vresize(&x, &y, 8.0f); v[0] = x; v[1] = y;
        } else en_play(s, "Shoot");
        if (!en_was_event(s, "Shoot", "Shoot")) { v[0] *= 0.85f; v[1] *= 0.85f; }
    }
}
static void die_buttfly(en_t *s, unsigned long e) { (void) s; float *p = fpos(e); en_spawn(1000, 34, p[0], p[1], 0, 0, e, 0); }

/* ------------------------------------------------------------------------------------------ Shot Fly / Shoter (guwah/enemies/shotfly.lua) */
/* state: 0 flying, 1 starting a charge, 2 charging.  i[0] hurt once (CanAttack), i[1] forced charge (a Shoter's child), f[2..3] the charge velocity, f[4..5] ForceVec */
static void shotfly_child(unsigned long parent, float x, float y, float vx, float vy, const char *anim, int force) {
    unsigned long c = en_spawn(450, 23, x, y, vx, vy, parent, 0);
    if (!c) return;
    en_t *cs = en_adopt(c); if (!cs) return;
    cs->init = 1; cs->state = force ? 1 : 2; cs->i[0] = 1; cs->i[1] = force;
    if (force) { cs->f[4] = vx; cs->f[5] = vy; cs->f[0] = vx; }
    else { cs->f[2] = vx; cs->f[3] = vy; }
    en_replay(cs, anim);
    en_flag_clear(c, EF_FLAG_APPEAR); en_flag_set(c, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK); en_set_collision(c, ENTCOLL_PLAYEROBJECTS);
}
static void ai_shotfly(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    int shoter = s->variant == 24;
    if (!s->init) {
        s->init = 1; en_play(s, "Fly");
        if (s->state == 2 || s->i[1]) { en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK); en_set_collision(e, ENTCOLL_PLAYEROBJECTS); }      /* data.Charging / ForceVec */
        FFLOG(FFC_NOT_DONE, 10, "ffenemy: not done: Shot Fly's wait-then-descend subtype (data.waited)");
    }
    if (s->state == 2) {                                                       /* charging */
        if (!s->i[3]) { s->i[3] = 1; en_sound(FFSND_Ricochet, 0.7f, 0.8f); }
        v[0] = s->f[2]; v[1] = s->f[3];
        if (en_box(p[0] + v[0], p[1] + v[1], 1)) en_kill(e);
        return;
    }
    if (s->state == 0) {
        float tx, ty, dist = vlen(t[0] - p[0], t[1] - p[1]);
        if (dist > 100.0f) {
            float bx = t[0], by = t[1], bd = 1e9f; int have = 0;
            for (int i = 90; i <= 360; i += 90) { float ox, oy; vrot(0, 100, (float) i, &ox, &oy); float qx = t[0] + ox, qy = t[1] + oy;
                if (en_xy(qx, qy, 1)) continue; float d = vlen(qx - p[0], qy - p[1]); if (d < bd) { bd = d; bx = qx; by = qy; have = 1; } }
            (void) have; tx = bx - p[0]; ty = by - p[1];
        } else { tx = t[0] - p[0]; ty = t[1] - p[1]; }
        vresize(&tx, &ty, 3.0f);
        if (dist < 200.0f && s->i[0] && s->age > 15) {
            float margin = shoter ? 40.0f : 20.0f;
            if (fabsf(p[0] - t[0]) < margin) { en_replay(s, "ChargeStartVerti"); s->state = 1; }
            else if (fabsf(p[1] - t[1]) < margin) { en_replay(s, "ChargeStartHori"); s->state = 1; }
            if (s->state == 1) { en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK); en_set_collision(e, ENTCOLL_PLAYEROBJECTS); }      /* shotfly.lua 112 */
        }
        v[0] = lerpf(v[0], tx, 0.3f); v[1] = lerpf(v[1], ty, 0.3f);
        return;
    }
    /* state 1: the charge start animation, standing still */
    int hori = !strcmp(s->anim, "ChargeStartHori"), vert = !strcmp(s->anim, "ChargeStartVerti");
    if (hori && en_finished(s, "ChargeStartHori")) {
        float dirx = s->i[1] ? s->f[4] : (p[0] < t[0] ? 14.0f : -14.0f);
        if (s->i[1]) { s->f[2] = s->f[4]; s->f[3] = s->f[5]; en_replay(s, s->f[4] > 0 ? "ChargeRight" : "ChargeLeft"); s->state = 2; }
        else if (shoter) { for (int a = -15; a <= 15; a += 30) { float x, y; vrot(dirx, 0, (float) a, &x, &y); shotfly_child(e, p[0], p[1], x, y, dirx > 0 ? "ChargeRight" : "ChargeLeft", 0); } s->i[2] = 1; en_kill(e); s->state = 3; }
        else { s->f[2] = dirx; s->f[3] = 0; en_replay(s, dirx > 0 ? "ChargeRight" : "ChargeLeft"); s->state = 2; }
    } else if (vert && en_finished(s, "ChargeStartVerti")) {
        float diry = p[1] < t[1] ? 14.0f : -14.0f;
        if (shoter) { for (int a = -15; a <= 15; a += 30) { float x, y; vrot(0, diry, (float) a, &x, &y); shotfly_child(e, p[0], p[1], x, y, diry > 0 ? "ChargeDown" : "ChargeUp", 0); } s->i[2] = 1; en_kill(e); s->state = 3; }
        else { s->f[2] = 0; s->f[3] = diry; en_replay(s, diry > 0 ? "ChargeDown" : "ChargeUp"); s->state = 2; }
    } else if (s->state == 1) { v[0] = 0; v[1] = 0; }
}
static void die_shotfly(en_t *s, unsigned long e) {
    float *p = fpos(e);
    if (s->variant == 24) {
        if (!s->i[2]) for (int a = 0; a <= 180; a += 180) {
            float ox, oy, fx, fy; vrot(15, 0, (float) a, &ox, &oy); vrot(14, 0, (float) a, &fx, &fy);
            unsigned long c = en_spawn(450, 23, p[0] + ox, p[1] + oy, 0, 0, e, 0);
            if (!c) continue;
            en_t *cs = en_adopt(c); if (!cs) continue;
            cs->init = 1; cs->state = 1; cs->i[0] = 1; cs->i[1] = 1; cs->f[4] = fx; cs->f[5] = fy;
            en_replay(cs, "ChargeStartHori");
            en_flag_clear(c, EF_FLAG_APPEAR); en_flag_set(c, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK); en_set_collision(c, ENTCOLL_PLAYEROBJECTS);
        }
    } else en_spawn(1000, 3, p[0], p[1], 0, 0, e, 0);
}
static void hurt_shotfly(en_t *s, unsigned long e, float dmg) { (void) e; if (dmg > 0) s->i[0] = 1; }

/* ------------------------------------------------------------------------------------------ Casualty (guwah/enemies/casualty.lua) */
static void splat_at(unsigned long e, float sc, int n) {
    for (int i = 0; i < n; i++) { float *p = fpos(e); unsigned long f = en_spawn(1000, 7, p[0], p[1], 0, 0, e, 0); if (f) en_scale(f, sc, sc); }
}
static void creep_at(unsigned long e, float sc) {
    float *p = fpos(e); unsigned long c = en_spawn(1000, 22, p[0], p[1], 0, 0, e, 0); if (c) en_scale(c, sc, sc);
    FFLOG(FFC_NOT_DONE, 11, "ffenemy: not done: creep Timeout (the creep keeps the game's default lifetime)");
}
static int en_has_path(unsigned long e, const float *t) {                       /* npc.Pathfinder:HasPathToPos(target, false) */
    float *p = fpos(e), wx, wy;
    return en_line_clear(p[0], p[1], t[0], t[1], 0) || path_step(p[0], p[1], t[0], t[1], 0, &wx, &wy);
}
static void ai_casualty(en_t *s, unsigned long e) {                                 /* casualty.lua CasualtyAI; FlipSprite(a, b) = FlipX when a.x > b.x */
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; splat_at(e, 1.5f, 2); s->state = 0; }
    if (s->state == 0) {
        if (en_has_path(e, t)) {
            if (en_event(s, "Walk", "Move")) {
                if (en_line_clear(p[0], p[1], t[0], t[1], 0) || en_scared(e)) { float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 4.0f); if (en_scared(e)) { x = -x; y = -y; } v[0] = x; v[1] = y; }
                else en_walk_to(e, t, 4.0f, 1.8f, 1);
                en_flip(e, v[0] > 0.0f);                                              /* FlipSprite(sprite, Position + Velocity, Position) */
            } else en_play(s, "Walk");
        } else { en_play(s, "Idle"); en_flip(e, t[0] > p[0]); }
        v[0] *= 0.9f; v[1] *= 0.9f;
        if (s->age % 10 == 5) { creep_at(e, 0.1f); splat_at(e, 0.5f, 1); }
        if (fb_hp(e) <= *(const float *) (e + 0x358) * 0.5f) s->state = 1;
    } else if (s->state == 1) {
        if (en_was_event(s, "Trip", "Shoot")) { v[0] *= 0.6f; v[1] *= 0.6f; } else { v[0] *= 0.9f; v[1] *= 0.9f; }
        if (en_finished(s, "Trip")) { s->state = 2; FFLOG(FFC_NOT_DONE, 12, "ffenemy: not done: Casualty's CanShutDoors = false (the corpse still counts for the room)"); }
        else if (en_event(s, "Trip", "Move")) { en_sound(SOUND_FETUS_JUMP, 1.0f, 1.0f); float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 10.0f); if (en_scared(e)) { x = -x; y = -y; } v[0] = x; v[1] = y; en_flip(e, v[0] > 0.0f); }
        else if (en_event(s, "Trip", "Shoot")) {
            en_sound(SOUND_MEAT_IMPACTS, 1.0f, 1.0f); v[0] *= 0.5f; v[1] *= 0.5f; creep_at(e, 3.0f); splat_at(e, 1.5f, 1);
            unsigned long f = en_spawn(1000, 16, p[0], p[1], 0, 0, e, 3); if (f) en_scale(f, 0.8f, 0.8f);
        } else en_play(s, "Trip");
    } else {
        v[0] *= 0.6f; v[1] *= 0.6f;
        if (s->age % 60 == 30) { if (!en_room_clear()) creep_at(e, 3.0f); splat_at(e, 1.5f, 1); }
    }
}

/* ------------------------------------------------------------------------------------------ Pester + its Eternal Fly (erfly/enemies/pester.lua, deadfly.lua) */
static void ai_pester(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) {
        s->init = 1;
        unsigned long o = en_spawn(160, 461, p[0], p[1], 0, 0, e, 0);
        if (o) { en_t *os = en_adopt(o); if (os) { os->ref = e; os->f[0] = (float) rint_(1, 100); } }
    }
    if (vlen(v[0], v[1]) > 0.1f) {
        if (fabsf(v[0]) > fabsf(v[1])) en_play(s, v[0] > 0 ? "WalkRight" : "WalkLeft"); else en_play(s, "WalkVert");
    } else g_play((void *) (e + E_SPRITE), "WalkVert", 1);
    if (en_line_clear(p[0], p[1], t[0], t[1], 0)) {
        float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 4.0f); v[0] = lerpf(v[0], x, 0.25f); v[1] = lerpf(v[1], y, 0.25f);
    } else en_walk_to(e, t, 0.6f, 0.6f, 0);
}
static void die_pester(en_t *s, unsigned long e) {
    float *p = fpos(e);
    if (rnd(3) == 1) {
        int nv = 1; if (rnd(3) == 1) nv = 0;
        unsigned long f = en_spawn(11, nv, p[0], p[1], fvel(e)[0], fvel(e)[1], e, s->variant);
        if (f) {
            for (int i = 0; i < EN_N; i++) if (g_en[i].e && g_en[i].ref == e) g_en[i].ref = f;       /* the orbiting fly follows the new body (npc.Child.Parent = spawned) */
            en_remove(e);
        }
    }
}
static void ai_eternalfly(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    if (!s->init) {
        s->init = 1; s->cd = 0; s->i[1] = 0; s->f[1] = s->f[0] ? s->f[0] : (float) rint_(1, 75);
        en_set_collision(e, ENTCOLL_PLAYEROBJECTS); en_set_gridcoll(e, GRIDCOLL_NONE);                      /* the Lua's d.init */
        en_flag_clear(e, EF_FLAG_APPEAR); en_flag_set(e, EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET);
        FFLOG(FFC_NOT_DONE, 13, "ffenemy: not done: Eternal Fly's TeleportIn/TeleportOut when the parent has an OverlayEffect null, the subtype 1 chase, charm of a friendly parent");
    }
    en_play(s, "Fly");
    unsigned long par = en_parent_alive(s->ref);
    if (par) {
        s->cd++; s->i[1]++;
        float *pp = fpos(par), dist = s->cd / 3.0f; if (dist > 50.0f) dist = 50.0f;
        float frame = (float) s->i[1] + s->f[1];
        float xv = cosf(frame / 16.0f + 3.14159265f) * dist, yv = sinf(frame / 16.0f + 3.14159265f) * dist;
        float dx = pp[0] - xv - p[0], dy = pp[1] - yv - p[1];
        if (vlen(dx, dy) > 50.0f) vresize(&dx, &dy, 50.0f);
        v[0] = lerpf(v[0], dx, 0.5f); v[1] = lerpf(v[1], dy, 0.5f);
    } else {
        en_flag_clear(e, EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET);
        unsigned long f = en_spawn(18, 0, p[0], p[1], v[0], v[1], 0, 0);          /* npc:Morph(18, 0, 0, -1) */
        if (f) en_remove(e);
    }
}

/* ------------------------------------------------------------------------------------------ dispatch */
typedef struct { int type, variant; void (*ai)(en_t *, unsigned long); void (*die)(en_t *, unsigned long); void (*hurt)(en_t *, unsigned long, float); int immune; int free_move; int corpse; } enemy_t;   /* free_move: it may leave the room on purpose (flies away, flees) */
static const enemy_t g_enemies[] = {
    { 160, 630, ai_redhorf, die_redhorf, 0 },
    { 152, 0, ai_squarefly, die_burst, 0 },
    { 160, 2100, ai_slobber, 0, 0 },
    { 160, 2101, ai_slobber, 0, 0 },
    { 114, 0, ai_benign, 0, hurt_benign },
    { 450, 44, ai_buttfly, die_buttfly, 0 },
    { 450, 23, ai_shotfly, die_shotfly, hurt_shotfly },
    { 450, 24, ai_shotfly, die_shotfly, hurt_shotfly },
    { 451, 70, ai_casualty, 0, 0 },
    { 160, 830, ai_pester, die_pester, 0 },
    { 160, 461, ai_eternalfly, 0, 0, 1 },
};
#define N_ENEMIES ((int) (sizeof g_enemies / sizeof g_enemies[0]))

/* ------------------------------------------------------------------------------------------ pieces shared by the monster groups */
/* npc:AnimWalkFrame(hori, vert, threshold): horizontal when |vx| > |vy| (and mirrored when going left), still (frame 0) at or under the threshold */
static void en_walk_anim(en_t *s, unsigned long e, const char *hori, const char *vert, float thr) {
    float *v = fvel(e);
    if (vlen(v[0], v[1]) <= thr) { en_set_frame(s, s->anim[0] ? s->anim : vert, 0); return; }
    if (fabsf(v[0]) > fabsf(v[1])) { en_play(s, hori); en_flip(e, v[0] < 0); } else { en_play(s, vert); en_flip(e, 0); }
}
static float en_room_x(int c) { return 40.0f + 40.0f * (float) c; }
static float en_room_y(int r) { return 120.0f + 40.0f * (float) r; }
/* Room:GetClampedPosition(pos, margin): inside the room's walls, margin px of the wall tiles' inner edge (negative margin = further in) */
static void en_clamp_room(float *x, float *y, float margin) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
    float x0 = en_room_x(1) - 20.0f + margin, x1 = en_room_x(w - 2) + 20.0f - margin, y0 = en_room_y(1) - 20.0f + margin, y1 = en_room_y(h - 2) + 20.0f - margin;
    if (*x < x0) *x = x0; if (*x > x1) *x = x1; if (*y < y0) *y = y0; if (*y > y1) *y = y1;
}
/* Room:FindFreeTilePosition(pos, radius): the centre of the nearest free tile (or the position itself when its tile is free) */
static void en_free_tile(float *x, float *y, float radius) {
    if (!en_xy(*x, *y, 0)) return;
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9; float best = 1e9f, bx = *x, by = *y;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
        float tx = en_room_x(c), ty = en_room_y(r), d = vlen(tx - *x, ty - *y);
        if (d <= radius && d < best && !en_xy(tx, ty, 0)) { best = d; bx = tx; by = ty; }
    }
    *x = bx; *y = by;
}
static int en_in_room(float x, float y, float margin) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9;
    return x > en_room_x(0) - 20.0f + margin && x < en_room_x(w - 1) + 20.0f - margin && y > en_room_y(0) - 20.0f + margin && y < en_room_y(h - 1) + 20.0f - margin;
}
/* Room:IsPositionInRoom(pos, margin): inside the walls' inner edge, margin px in (the same rectangle Room:GetClampedPosition uses) */
static int en_in_room_walk(float x, float y, float margin) { float cx = x, cy = y; en_clamp_room(&cx, &cy, margin); return cx == x && cy == y; }
static void en_rand_vec(float *x, float *y, float len) { float a = frand() * 6.2831853f; *x = cosf(a) * len; *y = sinf(a) * len; }
/* npc:Morph(type, variant, subtype, -1): the engine's in-place type change is not available, so the new monster is spawned where the old one is and the old one removed */
static unsigned long en_morph(unsigned long e, int type, int variant, int sub) {
    float *p = fpos(e), *v = fvel(e);
    unsigned long n = en_spawn(type, variant, p[0], p[1], v[0], v[1], *(const unsigned long *) (e + 0x400), sub);
    if (n) { en_flag_clear(n, EF_FLAG_APPEAR); en_remove(e); }
    return n;
}
/* ------------------------------------------------------------------------------------------ projectiles that a monster keeps (Entity_Projectile vtable 0xa8dc68, slot 4 = Projectile::Update 0x677fd0) */
#define PJ_N 48
typedef struct { unsigned long p, parent; int kind; float a, b; } pj_t;
enum { PJ_MUTANT_ORBITAL = 1, PJ_HONEY = 2, PJ_BROOD = 3, PJ_COAL = 4, PJ_FLAME = 5 };
static pj_t g_pj[PJ_N];
static fn_update o_proj_update;
static void pj_add(unsigned long p, unsigned long parent, int kind, float a) {
    int f = 0; for (int i = 0; i < PJ_N; i++) if (!g_pj[i].p || !*(const unsigned char *) (g_pj[i].p + E_EXISTS)) { f = i; break; }
    g_pj[f].p = p; g_pj[f].parent = parent; g_pj[f].kind = kind; g_pj[f].a = a; g_pj[f].b = 0;
}
static pj_t *pj_find(unsigned long p) { for (int i = 0; i < PJ_N; i++) if (g_pj[i].p == p) return &g_pj[i]; return 0; }
static void en_paint(unsigned long e, float tr, float tg, float tb, float ta, float cr, float cg, float cb, float ca, float orr, float og, float ob);
static void w_proj_update(unsigned long e) {
    pj_t *j = g_en_on ? pj_find(e) : 0;
    if (j && *(const unsigned char *) (e + E_EXISTS) && j->kind == PJ_MUTANT_ORBITAL) {
        float *p = fpos(e), *v = fvel(e);
        en_set_gridcoll(e, 0);
        unsigned long par = en_parent_alive(j->parent);
        if (par) {
            if (proj_height_ok()) *(float *) (e + OFF_PROJ_HEIGHT) = -35.0f;
            float radius = 75.0f, *pp = fpos(par);                                   /* pd.radius of the Mutant Horf */
            float cx = (radius + sinf(j->b) * 9.0f) * sinf(j->a), cy = (radius + sinf(j->b) * 9.0f) * cosf(j->a);
            float dx = pp[0] - cx - p[0], dy = pp[1] - cy - p[1];
            if (vlen(dx, dy) > 50.0f) vresize(&dx, &dy, 50.0f);
            v[0] = dx; v[1] = dy;
            if (j->a >= 360.0f) j->a = 0; else j->a += 0.1f;
            j->b += 3.14159265f / (float) rint_(3, 12);
            en_t *ps = en_slot(par);
            if (!strcmp(ps->anim, "Attack") || !strcmp(ps->anim, "Attack2") || !strcmp(ps->anim, "DoubleAttack")) { v[0] /= 2; v[1] /= 2; }
        } else {
            v[0] *= 0.9f; v[1] *= 0.9f;
            if (proj_faccel_ok()) *(float *) (e + OFF_PROJ_FALLINGACCEL) = 0.1f;
        }
    }
    o_proj_update(e);
    if (j && j->kind == PJ_BROOD && *(const unsigned char *) (e + E_EXISTS)) { if (++j->a > 2.0f && proj_flags_ok()) *(unsigned long long *) (e + OFF_PROJ_FLAGS) &= ~PF_NO_WALL_COLLIDE; }   /* broodProj: v.FrameCount > 2 -> ClearProjectileFlags(NO_WALL_COLLIDE) */
    if (j && j->kind == PJ_COAL && *(const unsigned char *) (e + E_EXISTS)) {                        /* projectiles.lua "coal"/"coal2"/"coalRadioactive" Update: no entity collision, walls only, spinning */
        en_set_collision(e, ENTCOLL_NONE); en_set_gridcoll(e, GRIDCOLL_WALLS); *(float *) (e + E_SPRITE + 0xe4) += 30.0f;
    }
    if (j && j->kind == PJ_FLAME && *(const unsigned char *) (e + E_EXISTS)) en_paint(e, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0);   /* GuwahProjectileUpdate: FlamethrowerFire -> sprite.Color = Color.Default every frame */
    if (j && j->kind == PJ_HONEY && *(const unsigned char *) (e + E_EXISTS)) en_paint(e, 1, 1, 1, 1, 1, 1, 1, 1, 0.7f, 0.4f, 0.0f);   /* FiendFolio.ColorHoneyYellow = Color(1,1,1,1, 0.7,0.4,0) + SetColorize(1,1,1,1): the projectile's colour, kept after every engine update */
}

/* The Weaver's opening pile (weaver.lua: Spawn(1000, 146), dirt:Load("gfx/enemies/weaver/weaver.anm2", true), weaverDirtPileOpeningAI = play "Ground", Remove when finished).
 * Sprite::Load is not in the port, so the pile is spawned with subtype 150: FF's entities2.xml has a row "Weaver Dirtpile" (1000.146.150, enemies/weaver/weaver.anm2) and the
 * engine loads that anm2 itself. The port plays 'Ground' (38 frames), sets its frame every tick (the engine's own Dirt Pile code would otherwise play its animation) and removes it. */
#define PILE_N 12
#define PILE_LEN 38
static struct { unsigned long e; int age; } g_pile[PILE_N];
static void en_pile_open(float x, float y, unsigned long spawner) {
    unsigned long c = en_spawn(1000, 146, x, y, 0, 0, spawner, 150); if (!c) return;
    if (g_play) g_play((void *) (c + E_SPRITE), "Ground", 1);
    for (int k = 0; k < PILE_N; k++) if (!g_pile[k].e || !alive(g_pile[k].e)) { g_pile[k].e = c; g_pile[k].age = 0; return; }
    en_remove(c);                                                                          /* no free slot: do not leave a pile nobody will remove */
}

/* ------------------------------------------------------------------------------------------ colours and rotation of a sprite (measured: ffchars.c, fffireball.h)
 * The sprite's Color is 11 floats at sprite+0xe8: tint rgba, colorize rgba (+0xf8), offset rgb (+0x108). SpriteRotation is the float at sprite+0xe4 (degrees). */
static void en_paint(unsigned long e, float tr, float tg, float tb, float ta, float cr, float cg, float cb, float ca, float orr, float og, float ob) {
    float *c = (float *) (e + E_SPRITE + 0xe8);
    c[0] = tr; c[1] = tg; c[2] = tb; c[3] = ta; c[4] = cr; c[5] = cg; c[6] = cb; c[7] = ca; c[8] = orr; c[9] = og; c[10] = ob;
}
static void en_colorize(unsigned long e, float cr, float cg, float cb, float ca) {      /* Color:SetColorize only: the engine's own fade of the tint alpha stays */
    float *c = (float *) (e + E_SPRITE + 0xf8); c[0] = cr; c[1] = cg; c[2] = cb; c[3] = ca;
}
static void en_set_rotation(unsigned long e, float deg) { *(float *) (e + E_SPRITE + 0xe4) = deg; }
static float en_angle_deg(float x, float y) { return atan2f(y, x) * 180.0f / 3.14159265f; }     /* Vector:GetAngleDegrees() */

/* ------------------------------------------------------------------------------------------ effects that have a Lua update of their own (MC_POST_EFFECT_UPDATE)
 * The port's monster hook runs from the NPC update only, so an effect whose variant the engine has no code for would sit on its anm2's first animation for ever. The ones the
 * Lua animates are remembered here and stepped once per game frame (en_piles_tick): the frame is set directly (SetFrame clears the Playing byte, so the engine never advances it twice). */
#define FX_N 64
enum { FX_POOF = 1, FX_WEBCORPSE, FX_CREEPTINT, FX_SOOTYPOOF };
static struct { unsigned long e; int kind, age, len; float a; } g_fx[FX_N];
static unsigned long en_fx_add(unsigned long e, int kind, int len, float a) {
    if (!e) return 0;
    for (int k = 0; k < FX_N; k++) if (!g_fx[k].e || !alive(g_fx[k].e)) { g_fx[k].e = e; g_fx[k].kind = kind; g_fx[k].age = 0; g_fx[k].len = len; g_fx[k].a = a; return e; }
    FFLOG(FFC_NOT_DONE, 73, "ffenemy: not done: more than %d tracked effects at once (the extra one is dropped: %d)", FX_N, kind);
    if (kind == FX_POOF) en_remove(e);
    return 0;
}
/* colonel.lua 658 / homer.lua 137 / technical_entities.lua 213: Isaac.Spawn(1000, 7014, sub, pos), SpriteRotation = the shot's, then checkStingerPoof (erfly/main.lua 1460): SpriteOffset (0,-15), play "Poof", Remove when finished */
static void en_stinger_poof(float x, float y, int sub, float rot) {
    unsigned long f = en_spawn(1000, 7014, x, y, 0, 0, 0, sub); if (!f) return;
    en_set_rotation(f, rot);
    if (g_play) g_play((void *) (f + E_SPRITE), "Poof", 1);
    en_fx_add(f, FX_POOF, sub ? 11 : 9, rot);
}
static void en_fx_tick(void) {
    for (int k = 0; k < 8; k++) if (g_loop_on[k] && g_loop_owner[k] && !alive(g_loop_owner[k])) { g_loop_on[k] = 0; ffsound_stop(g_loop_id[k]); }   /* a loop whose monster is gone (room left, removed) stops */
    for (int k = 0; k < FX_N; k++) {
        unsigned long f = g_fx[k].e; if (!f) continue;
        if (!alive(f)) { g_fx[k].e = 0; continue; }
        if (g_fx[k].kind == FX_POOF) {
            if (ent_spriteoff_ok()) { *(float *) (f + OFF_ENT_SPRITEOFFSET) = 0.0f; *(float *) (f + OFF_ENT_SPRITEOFFSET + 4) = -15.0f; }
            if (++g_fx[k].age >= g_fx[k].len) { en_remove(f); g_fx[k].e = 0; continue; }
            en_set_rotation(f, g_fx[k].a);
            if (g_setframe) g_setframe((void *) (f + E_SPRITE), "Poof", g_fx[k].age);
        } else if (g_fx[k].kind == FX_WEBCORPSE) {                                   /* ransackedwebai: play "webcorpse", DepthOffset 1, Offset 0 */
            if (g_setframe) g_setframe((void *) (f + E_SPRITE), "webcorpse", 0);
            en_spr_off(f, 0.0f, 0.0f);
        } else if (g_fx[k].kind == FX_SOOTYPOOF) {                                   /* checkSootyTearPoof: play "Destroy", Remove when finished */
            if (++g_fx[k].age >= g_fx[k].len) { en_remove(f); g_fx[k].e = 0; continue; }
            if (g_setframe) g_setframe((void *) (f + E_SPRITE), "Destroy", g_fx[k].age);
        } else if (g_fx[k].kind == FX_CREEPTINT) {                                   /* HoneyCreep: colorize 5.5 3.5 1 1 (the creep's own tint alpha fades by itself) */
            en_colorize(f, 5.5f, 3.5f, 1.0f, 1.0f);
        }
    }
}
static void en_proj_tick(void);
static void en_burn_tick(void);
static void en_piles_tick(void) {
    static int last = -2; int fr = g_frame_fn ? g_frame_fn() : -1;
    if (fr >= 0) { if (fr == last) return; last = fr; }                                   /* once per game frame, whatever the number of NPC updates */
    for (int k = 0; k < PILE_N; k++) {
        unsigned long c = g_pile[k].e; if (!c) continue;
        if (!alive(c)) { g_pile[k].e = 0; continue; }
        if (++g_pile[k].age >= PILE_LEN) { en_remove(c); g_pile[k].e = 0; }
        else if (g_setframe) g_setframe((void *) (c + E_SPRITE), "Ground", g_pile[k].age);
    }
    en_fx_tick();
    en_burn_tick();
    en_proj_tick();
}
#define PW_HONEY 1
#define PW_BROOD 2
static void en_proj_watch(unsigned long p, int kind);
#include "ffen_g1.h"
#include "ffen_g2.h"
#include "ffen_g3.h"
#include "ffen_g4.h"
#include "ffen_g5.h"
#include "ffen_g5b.h"
#include "ffen_g5c.h"
#include "ffen_g5d.h"
#include "ffen_g6a.h"
#include "ffen_g6.h"
#include "ffen_g6b.h"
#include "ffen_g6c.h"
#include "ffen_g6d.h"
#include "ffen_g6e.h"
#include "ffen_g7.h"
#include "ffen_g8.h"
#include "ffen_g9.h"
#include "ffen_g10.h"
#include "ffen_g11.h"
#include "ffen_g12.h"
#include "ffen_g13.h"
static const enemy_t *en_find_in(const enemy_t *t, int n, int type, int variant) {
    for (int i = 0; i < n; i++) if (t[i].type == type && t[i].variant == variant) return &t[i];
    return 0;
}
static const enemy_t *en_find(int type, int variant) {
    const enemy_t *r;
    if ((r = en_find_in(g_enemies, N_ENEMIES, type, variant))) return r;
    if ((r = en_find_in(g_tab_g1, N_TAB_G1, type, variant))) return r;
    if ((r = en_find_in(g_tab_g2, N_TAB_G2, type, variant))) return r;
    if ((r = en_find_in(g_tab_g3, N_TAB_G3, type, variant))) return r;
    if ((r = en_find_in(g_tab_g4, N_TAB_G4, type, variant))) return r;
    if ((r = en_find_in(g_tab_g5, N_TAB_G5, type, variant))) return r;
    if ((r = en_find_in(g_tab_g5d, N_TAB_G5D, type, variant))) return r;
    if ((r = en_find_in(g_tab_g6, N_TAB_G6, type, variant))) return r;
    if ((r = en_find_in(g_tab_g6b, N_TAB_G6B, type, variant))) return r;
    if ((r = en_find_in(g_tab_g6c, N_TAB_G6C, type, variant))) return r;
    if ((r = en_find_in(g_tab_g6d, N_TAB_G6D, type, variant))) return r;
    if ((r = en_find_in(g_tab_g6e, N_TAB_G6E, type, variant))) return r;
    if ((r = en_find_in(g_tab_g7, N_TAB_G7, type, variant))) return r;
    if ((r = en_find_in(g_tab_g8, N_TAB_G8, type, variant))) return r;
    if ((r = en_find_in(g_tab_g9, N_TAB_G9, type, variant))) return r;
    if ((r = en_find_in(g_tab_g10, N_TAB_G10, type, variant))) return r;
    if ((r = en_find_in(g_tab_g11, N_TAB_G11, type, variant))) return r;
    return en_find_in(g_tab_g11b, N_TAB_G11B, type, variant);
}


/* ---- the log's census of a room (ROOM, SPAWN HANDLER=no / UNHANDLED, GRID_COLLISION, GRID_SAVE / GRID_RESTORE / GRID_LOST).
 * "Looks like Fiend Folio" = a row of ffenemies_table.h (the monsters the console lists), a slot variant >= 1000 or a 183/184 variant >= 1000
 * (the NPC pseudo-grids of ffen_g8.h / g9 / g10). It is a census of what the port can recognise, not of every entity of the mod's XML. */
#if ISAAC_DIAG
typedef struct { int type, variant, sub, grp, id, a, b, c, d, e; const char *nm[7]; } ffl_row_t;
#define SP_GROUP_FF 0
#define FF_ENEMY_BODY
static const ffl_row_t g_ffl_rows[] = {
#include "ffenemies_table.h"
};
#undef FF_ENEMY_BODY
#undef SP_GROUP_FF
#endif
static int ffl_ff_like(int type, int var) {
#if ISAAC_DIAG
    for (unsigned i = 0; i < sizeof g_ffl_rows / sizeof g_ffl_rows[0]; i++) if (g_ffl_rows[i].type == type && g_ffl_rows[i].variant == var) return 1;
    if (type == 6 || type == 183 || type == 184) return var >= 1000;
#else
    (void) type; (void) var;
#endif
    return 0;
}
#if ISAAC_DIAG
#define FFG_ROOMS 48
#define FFG_ITEMS 40
typedef struct { unsigned long key; int n; struct { int type, var, sub; float x, y; } it[FFG_ITEMS]; } ffg_t;
static ffg_t g_ffg[FFG_ROOMS]; static int g_ffg_next, g_ffg_floor = -1;
static unsigned long g_rt_key; static int g_rt_ticks, g_rt_frames;
static int ffl_is_grid_npc(int type, int var) { return (type == 183 || type == 184) && var >= 1000; }
static ffg_t *ffg_of(unsigned long key, int create) {
    for (int i = 0; i < FFG_ROOMS; i++) if (g_ffg[i].key == key) return &g_ffg[i];
    if (!create) return 0;
    ffg_t *g = &g_ffg[g_ffg_next++ % FFG_ROOMS]; memset(g, 0, sizeof *g); g->key = key; return g;
}
/* the grid-like entities alive now, remembered for this room (what GRID_SAVE keeps and the next visit is compared with) */
static int ffg_remember(unsigned long key, const unsigned long *snap, unsigned n) {
    ffg_t *g = ffg_of(key, 1); g->n = 0;
    for (unsigned k = 0; k < n && g->n < FFG_ITEMS; k++) {
        unsigned long o = snap[k]; if (!alive(o)) continue;
        int ty = *(const int *) (o + E_TYPE), va = *(const int *) (o + E_VARIANT);
        if (!ffl_is_grid_npc(ty, va)) continue;
        int i = g->n++; g->it[i].type = ty; g->it[i].var = va; g->it[i].sub = *(const int *) (o + E_SUBTYPE); g->it[i].x = fpos(o)[0]; g->it[i].y = fpos(o)[1];
    }
    return g->n;
}
static void ffroom_tick(void) {
    unsigned long key = ffl_room_key();
    if (!key) return;
    unsigned long game = en_game();
    int floor = game ? *(const int *) game * 16 + *(const int *) (game + 4) : 0;
    if (floor != g_ffg_floor) { memset(g_ffg, 0, sizeof g_ffg); g_ffg_floor = floor; g_ffg_next = 0; }
    g_rt_frames++;
    if (key != g_rt_key) {
        if (g_rt_key) {
            ffg_t *g = ffg_of(g_rt_key, 0); char rd[110]; ffl_room_desc(rd, sizeof rd);
            FFLOG(FFC_ROOM, (unsigned) g_rt_key, "left room %lx after %d ticks; %d FF monsters/machines tracked; %d grid-like entities kept for it", g_rt_key, g_rt_ticks, ({ int c = 0; for (int i = 0; i < EN_N; i++) if (g_en[i].e) c++; c; }), g ? g->n : 0);
            if (g) FFLOG(FFC_GRID_SAVE, (unsigned) g_rt_key, "room %lx: %d grid-like entities saved on leaving", g_rt_key, g->n);
            ff_log_summary("room exit");
        }
        g_rt_key = key; g_rt_ticks = 0;
    }
    g_rt_ticks++;
    if (g_rt_ticks == 12) {                              /* a few ticks after entering, when the room's entities exist */
        unsigned long snap[SNAP]; unsigned n = snapshot(snap); char rd[110]; ffl_room_desc(rd, sizeof rd);
        int total = 0, ff = 0, noh = 0, grids = 0;
        for (unsigned k = 0; k < n; k++) {
            unsigned long o = snap[k]; if (!alive(o)) continue;
            int ty = *(const int *) (o + E_TYPE), va = *(const int *) (o + E_VARIANT); total++;
            if (!ffl_ff_like(ty, va)) continue;
            ff++;
            int handled = en_find(ty, va) != 0 || (ty == 6 && sl_find(va) != 0);
            if (!handled) {
                noh++;
                FFLOG(FFC_SPAWN, FFKEY(ty, va), "%d.%d.%d at (%.0f,%.0f) room %lx %s HANDLER=NO", ty, va, *(const int *) (o + E_SUBTYPE), fpos(o)[0], fpos(o)[1], key, rd);
                FFLOG(FFC_UNHANDLED, FFKEY(ty, va), "%d.%d.%d is in the room and no handler of the port runs it", ty, va, *(const int *) (o + E_SUBTYPE));
            }
            if (ffl_is_grid_npc(ty, va)) {
                grids++;
                unsigned raw = g_gw ? grid_raw_at(fpos(o)[0], fpos(o)[1]) : 0xfffeu;
                float size = *(const float *) (o + E_SIZE);
                FFLOG(FFC_GRID_COLLISION, FFKEY(ty, va), "%d.%d.%d at (%.0f,%.0f): size %.1f%s, grid class %d, collision class %d, engine tile under it %s%u",
                      ty, va, *(const int *) (o + E_SUBTYPE), fpos(o)[0], fpos(o)[1], (double) size, size < 38.0f ? " (LESS than a 40 px tile: its corners can be crossed)" : "",
                      ent_gcoll_ok() ? *(const int *) (o + OFF_ENT_GRIDCOLL) : -1, ent_coll_ok() ? *(const int *) (o + OFF_ENT_COLLCLASS) : -1, raw >= 0xfffeu ? "n/a " : "", raw >= 0xfffeu ? 0u : raw);
            }
        }
        /* the persistence check: what this room kept when it was left, against what is here now */
        ffg_t *g = ffg_of(key, 0);
        if (g && g->n) {
            int back = 0;
            for (int i = 0; i < g->n; i++) {
                int found = 0;
                for (unsigned k = 0; k < n && !found; k++) {
                    unsigned long o = snap[k]; if (!alive(o)) continue;
                    if (*(const int *) (o + E_TYPE) == g->it[i].type && *(const int *) (o + E_VARIANT) == g->it[i].var && vlen(fpos(o)[0] - g->it[i].x, fpos(o)[1] - g->it[i].y) < 12.0f) found = 1;
                }
                if (found) back++;
                else FFLOG(FFC_GRID_LOST, FFKEY(g->it[i].type, g->it[i].var), "room %lx: %d.%d.%d at (%.0f,%.0f) was there when the room was left and is not now (destroyed by the player, or not restored)", key, g->it[i].type, g->it[i].var, g->it[i].sub, g->it[i].x, g->it[i].y);
            }
            FFLOG(FFC_GRID_RESTORE, (unsigned) key, "room %lx: %d of %d grid-like entities saved on leaving are back", key, back, g->n);
        }
        ffg_remember(key, snap, n);
        FFLOG(FFC_ROOM, (unsigned) key, "entered room %lx floor %d: %s; %d entities, %d look like Fiend Folio (%d with no handler), %d grid-like; revisit: %s", key, floor, rd, total, ff, noh, grids, g ? "yes" : "first visit");
    } else if (g_rt_ticks > 12 && g_rt_ticks % 30 == 0) {   /* keep the room's grid-like list current while we are in it */
        unsigned long snap[SNAP]; unsigned n = snapshot(snap); ffg_remember(key, snap, n);
    }
    if (g_rt_frames % 3600 == 0) ff_log_summary("every minute");      /* the app has no exit hook: the totals are repeated every minute of play */
}
#else
static void ffroom_tick(void) { }
#endif


static fn_update o_npc_update;
static void en_sweep(void) {
    for (int i = 0; i < EN_N; i++) if (g_en[i].e && (!*(const unsigned char *) (g_en[i].e + E_EXISTS) || *(const int *) (g_en[i].e + E_TYPE) != g_en[i].type)) g_en[i].e = 0;
}
/* ---- what the engine does with the velocity the AI set. A real run (ff-codigo40) showed monsters that never went to the player, a jumper that
 * jumped on the spot and flyers pushed out of the map: the AI's formulas match the Lua, so the suspect is the engine's own Update leaving
 * the position alone for these monsters. Every monster is therefore measured: the position before the engine's update, the velocity the AI
 * left, and where it ended. If the engine moved it less than 5%% of what the velocity asks for, the move is made here (per axis, against
 * the walls the engine's own grid answers), and the first times are logged ("telemetry" lines say what each kind did). */
typedef struct { int type, variant, frames, moving, ignored, samples, sums, good, kept, zeroed; } tel_t;
static tel_t g_tel[48];
static tel_t *tel_of(int type, int variant) {
    for (int i = 0; i < 48; i++) if (g_tel[i].type == type && g_tel[i].variant == variant && g_tel[i].frames >= 0) return &g_tel[i];
    for (int i = 0; i < 48; i++) if (!g_tel[i].type) { g_tel[i].type = type; g_tel[i].variant = variant; return &g_tel[i]; }
    return 0;
}
/* ---- log only, no behaviour: where the physics numbers live and how a hit moves a monster. XML says friction="1" and a collisionMass per monster
 * (Slim 45); a dump of the entity's dwords for a few FF monsters and a few vanilla ones shows which offset holds 1.0 / the mass, and the hit trace shows
 * the velocity a hit adds and how fast it dies away (or not). */
typedef struct { int type, variant, hits, dumped; } dk_t;
static dk_t g_dk[32]; static int g_dumps_ff, g_dumps_van;
static dk_t *dk_of(int type, int variant) {
    for (int i = 0; i < 32; i++) if (g_dk[i].type == type && g_dk[i].variant == variant) return &g_dk[i];
    for (int i = 0; i < 32; i++) if (!g_dk[i].type) { g_dk[i].type = type; g_dk[i].variant = variant; return &g_dk[i]; }
    return 0;
}
static void en_dump(unsigned long e, int vanilla) {
    int *cnt = vanilla ? &g_dumps_van : &g_dumps_ff;
    int type = *(const int *) (e + E_TYPE), var = *(const int *) (e + E_VARIANT), sub = *(const int *) (e + E_SUBTYPE);
    if (*cnt >= (vanilla ? 5 : 4)) return;
    dk_t *k = dk_of(type, var); if (!k || k->dumped) return;
    k->dumped = 1; (*cnt)++;
    LOG("ffenemy: dump %s %d.%d.%d at %lx (nonzero dwords from +0x200 to +0x400, as +off=float/hex)", vanilla ? "VANILLA" : "FF", type, var, sub, e);
    char buf[300]; int n = 0; buf[0] = 0;
    for (int off = 0x200; off < 0x400; off += 4) {
        unsigned w = *(const unsigned *) (e + off); if (!w) continue;
        float f; memcpy(&f, &w, 4);
        int ok = (f > -100000.0f && f < 100000.0f && (f > 0.0001f || f < -0.0001f));
        n += snprintf(buf + n, sizeof buf - (size_t) n, " +%x=%s%g/%x", off, ok ? "" : "~", ok ? (double) f : 0.0, w);
        if (n > 230) { LOG("ffenemy: dump%s", buf); n = 0; buf[0] = 0; }
    }
    if (n) LOG("ffenemy: dump%s", buf);
}
typedef struct { unsigned long e; int left, type, variant, age; } ht_t;
static ht_t g_ht[6];
static void ht_before(unsigned long e, float dmg, unsigned long long flags, float *vel) {      /* in NPC::TakeDamage, before the engine's own */
    vel[0] = fvel(e)[0]; vel[1] = fvel(e)[1]; (void) dmg; (void) flags;
}
static void ht_after(unsigned long e, float dmg, unsigned long long flags, const float *vel, int result) {
    int type = *(const int *) (e + E_TYPE), var = *(const int *) (e + E_VARIANT);
    dk_t *k = dk_of(type, var);
    if (!k || k->hits >= 3) return;
    k->hits++;
    int slot = -1; for (int i = 0; i < 6; i++) if (!g_ht[i].left) { slot = i; break; }
    float *p = fpos(e), *v = fvel(e);
    LOG("ffenemy: hit %d.%d dmg %.1f flags %llx result %d: velocity (%.2f,%.2f) -> (%.2f,%.2f) = pushed %.2f px/frame, pos (%.0f,%.0f)", type, var, dmg, flags, result, vel[0], vel[1], v[0], v[1], vlen(v[0] - vel[0], v[1] - vel[1]), p[0], p[1]);
    if (slot >= 0) { g_ht[slot].e = e; g_ht[slot].left = 10; g_ht[slot].type = type; g_ht[slot].variant = var; g_ht[slot].age = 0; }
}
static void ht_trace(unsigned long e) {       /* at the start of every NPC update: the 10 frames after a traced hit */
    for (int i = 0; i < 6; i++) if (g_ht[i].left && g_ht[i].e == e) {
        g_ht[i].left--; g_ht[i].age++;
        LOG("ffenemy: hit trace %d.%d +%d: pos (%.0f,%.0f) velocity (%.2f,%.2f) |v| %.2f", g_ht[i].type, g_ht[i].variant, g_ht[i].age, fpos(e)[0], fpos(e)[1], fvel(e)[0], fvel(e)[1], vlen(fvel(e)[0], fvel(e)[1]));
    }
}
#if ISAAC_DIAG
/* What the log asks of every FF monster, each tick after the engine's update: pa = where the AI left it, va = the velocity the AI left,
 * p = where the engine ended it. SPEED (real move vs what the AI asked), SPEED_SPIKE (more than 1.5x), TELEPORT (a jump the AI made),
 * STUCK (wants to move, does not), AI_TARGET (stands still with a target / how far the player was when it woke), ANIM_GAP. */
static void en_probe(en_t *s, unsigned long e, const enemy_t *d, const float *pa, const float *va) {
    float *p = fpos(e), *v = fvel(e);
    float mx = p[0] - pa[0], my = p[1] - pa[1], real = vlen(mx, my), want = vlen(va[0], va[1]), eng = vlen(v[0], v[1]);
    float *pl = en_target(e); float dist = pl ? vlen(pl[0] - p[0], pl[1] - p[1]) : -1.0f;
    unsigned key = FFKEY(s->type, s->variant);
    if (!s->born) { s->born = 1; snprintf(s->first_anim, sizeof s->first_anim, "%s", s->anim); }
    if (s->age > 1 && vlen(pa[0] - s->lastp[0], pa[1] - s->lastp[1]) > 100.0f) {          /* TELEPORT: the AI set a position far from where the last tick ended */
        char rd[110]; ffl_room_desc(rd, sizeof rd);
        FFLOG(FFC_TELEPORT, key, "%d.%d.%d asked to go from (%.0f,%.0f) to (%.0f,%.0f), the engine left it at (%.0f,%.0f); state %d anim '%s'; %s",
              s->type, s->variant, s->sub, s->lastp[0], s->lastp[1], pa[0], pa[1], p[0], p[1], s->state, s->anim, rd);
    }
    s->lastp[0] = p[0]; s->lastp[1] = p[1];
    if (want > 0.3f && (s->age == 3 || s->age % 90 == 0))
        FFLOG(FFC_SPEED, key, "%d.%d.%d age %d: AI asks %.2f px/tick, it moved %.2f, engine velocity %.2f, player %.0f px away, state %d anim '%s'", s->type, s->variant, s->sub, s->age, want, real, eng, dist, s->state, s->anim);
    if (real > 3.0f && real > 1.5f * (want > 0.5f ? want : 0.5f)) {
        int ai_grew = s->lastva > 0.3f && want > 1.5f * s->lastva;
        FFLOG(FFC_SPEED_SPIKE, key, "%d.%d.%d moved %.2f px in one tick, the AI asked %.2f (last tick %.2f, %s), engine velocity %.2f, player %.0f px away, state %d anim '%s' age %d",
              s->type, s->variant, s->sub, real, want, s->lastva, ai_grew ? "the AI's own velocity grew" : "the AI did not ask for more: the engine moved it", eng, dist, s->state, s->anim, s->age);
    }
    s->lastva = want;
    /* STUCK: it wants to move and does not (20 ticks) - what is in the way: the cell in that direction, and the entity standing there */
    if (want > 0.3f && real < 0.1f * want) s->blocked++; else s->blocked = 0;
    if (s->blocked == 20) {
        float dx = va[0] / want, dy = va[1] / want, qx = p[0] + dx * 24.0f, qy = p[1] + dy * 24.0f;
        unsigned raw = g_gw ? grid_raw_at(qx, qy) : 0xfffeu;
        unsigned long snap[SNAP]; unsigned n = snapshot(snap); char who[80]; who[0] = 0; float best = 30.0f;
        for (unsigned k = 0; k < n; k++) {
            unsigned long o = snap[k]; if (o == e || !alive(o)) continue;
            float dd = vlen(fpos(o)[0] - qx, fpos(o)[1] - qy);
            if (dd < best) { best = dd; snprintf(who, sizeof who, "%d.%d.%d size %.0f", *(const int *) (o + E_TYPE), *(const int *) (o + E_VARIANT), *(const int *) (o + E_SUBTYPE), (double) *(const float *) (o + E_SIZE)); }
        }
        char rd[110]; ffl_room_desc(rd, sizeof rd);
        FFLOG(FFC_STUCK, key, "%d.%d.%d wants (%.2f,%.2f) and has not moved for 20 ticks at (%.0f,%.0f); cell ahead (%.0f,%.0f) collision %s%u; entity there: %s; own grid class %d, collision class %d; state %d anim '%s'; %s",
              s->type, s->variant, s->sub, va[0], va[1], p[0], p[1], qx, qy, raw >= 0xfffeu ? "outside the table/no grid " : "", raw >= 0xfffeu ? 0u : raw, who[0] ? who : "none within 30 px",
              ent_gcoll_ok() ? *(const int *) (e + OFF_ENT_GRIDCOLL) : -1, ent_coll_ok() ? *(const int *) (e + OFF_ENT_COLLCLASS) : -1, s->state, s->anim, rd);
    }
    /* AI_TARGET: with a player in the room it neither asks for nor makes any move for 90 ticks; and when it moves again, how far the player is */
    if (pl && want < 0.05f && real < 0.05f) {
        s->still++;
        if (s->still == 90) {
            s->still_dist = dist;
            FFLOG(FFC_AI_TARGET, key, "%d.%d.%d has stood still 90 ticks with the player %.0f px away; line of sight ground %d / flying %d; state %d anim '%s' cd %d age %d%s",
                  s->type, s->variant, s->sub, dist, g_gw ? line_clear(p[0], p[1], pl[0], pl[1], 0) : -1, g_gw ? line_clear(p[0], p[1], pl[0], pl[1], 1) : -1, s->state, s->anim, s->cd, s->age, d->free_move ? " (free mover)" : "");
        }
    } else {
        if (s->still >= 90) FFLOG(FFC_AI_TARGET, key, "%d.%d.%d moved again after %d still ticks: player now %.0f px away (it was %.0f when it went still); state %d anim '%s'", s->type, s->variant, s->sub, s->still, dist, s->still_dist, s->state, s->anim);
        s->still = 0;
    }
    /* ANIM_GAP: ticks from the spawn to its first move, the animation it had then, and how long it slid before the animation changed */
    if (!s->mv_age && real > 0.5f && want > 0.3f) {
        s->mv_age = s->age ? s->age : 1; snprintf(s->mv_anim, sizeof s->mv_anim, "%s", s->anim);
        FFLOG(FFC_ANIM_GAP, key, "%d.%d.%d first moved at tick %d (animation at spawn '%s', now '%s')", s->type, s->variant, s->sub, s->age, s->first_anim, s->anim);
    } else if (s->mv_age && !s->gap_done) {
        int gap = s->age - s->mv_age;
        if (strcmp(s->anim, s->mv_anim)) { s->gap_done = 1; if (gap >= 4) FFLOG(FFC_ANIM_GAP, key, "%d.%d.%d slid %d ticks on '%s' before the animation became '%s'", s->type, s->variant, s->sub, gap, s->mv_anim, s->anim); }
        else if (gap == 90) { s->gap_done = 1; FFLOG(FFC_ANIM_GAP, key, "%d.%d.%d is still on '%s' 90 ticks after it began to move (no walking animation was ever asked for)", s->type, s->variant, s->sub, s->mv_anim); }
    }
}
#endif
static void en_post(en_t *s, unsigned long e, const enemy_t *d, const float *pa, const float *va, const float *vb) {
    float *p = fpos(e), *v = fvel(e), mx = p[0] - pa[0], my = p[1] - pa[1];
    int gc = ent_gcoll_ok() ? *(const int *) (e + OFF_ENT_GRIDCOLL) : -1, cc = ent_coll_ok() ? *(const int *) (e + OFF_ENT_COLLCLASS) : -1;
    /* a safety net only, the Lua has nothing like it: a monster that ends up well outside the room's walls (a bug, never a normal move) is put back */
    if (!d->free_move && g_gw) {
        float x = p[0], y = p[1]; en_clamp_room(&x, &y, -60.0f);
        if (x != p[0] || y != p[1]) {
#if ISAAC_DIAG
            char rd[110]; ffl_room_desc(rd, sizeof rd);
            FFLOG(FFC_OUT_OF_ROOM, FFKEY(s->type, s->variant), "%d.%d.%d at (%.0f,%.0f) was far outside the room: put back at (%.0f,%.0f); it was at (%.0f,%.0f) after the AI, last tick ended at (%.0f,%.0f); state %d anim '%s' age %d; %s",
                  s->type, s->variant, s->sub, p[0], p[1], x, y, pa[0], pa[1], s->lastp[0], s->lastp[1], s->state, s->anim, s->age, rd);
#endif
            p[0] = x; p[1] = y; v[0] = 0; v[1] = 0; }
    }
#if ISAAC_DIAG
    en_probe(s, e, d, pa, va);
#endif
    tel_t *t = tel_of(s->type, s->variant);
    if (t) {
        float want = vlen(va[0], va[1]); int moving = want > 0.3f;
        t->frames++; if (moving) t->moving++;
        if ((s->age == 3 || (moving && s->age % 12 == 0)) && t->samples < 7) {
            float *pl = en_target(e); t->samples++;
            LOG("ffenemy: telemetry %d.%d.%d age %d pos (%.0f,%.0f) v0 (%.2f,%.2f) vAI (%.2f,%.2f) moved (%.2f,%.2f) vEng (%.2f,%.2f) player (%.0f,%.0f) st %d anim %s/%d spr %.1f/%d gc %d cc %d velo +0x%x",
                s->type, s->variant, s->sub, s->age, pa[0], pa[1], vb[0], vb[1], va[0], va[1], mx, my, v[0], v[1], pl ? pl[0] : -1.0f, pl ? pl[1] : -1.0f, s->state, s->anim, s->af, *(const float *) (e + E_SPR_FRAME), *(const unsigned char *) (e + E_SPR_PLAYING), gc, cc, g_vel_off);
        }
    }
}
/* ---- velocity probe. The offset of the engine's Velocity was taken as +0x320 and was wrong (ff-codigo42's log: a hit moved a monster 8 px/frame with
 * "velocity 0.00"; vanilla monsters carry 1e-20 leftovers of friction at +0x334/+0x338, and the engine's movement-direction and wall code read that pair).
 * The default is now +0x334. This checks it against what vanilla monsters really do: on each update the distance a vanilla NPC just travelled must
 * be about the speed stored at the offset. Only if the other offset wins clearly (never the case in the log) is it switched, and the log says so. */
typedef struct { unsigned long e; float x, y; } vp_t;
static vp_t g_vp[16]; static int g_vp_a, g_vp_b, g_vp_n, g_vp_done;
static void vel_probe(unsigned long e) {
    if (g_vp_done) return;
    vp_t *slot = 0;
    for (int i = 0; i < 16; i++) if (g_vp[i].e == e) { slot = &g_vp[i]; break; }
    if (!slot) { for (int i = 0; i < 16; i++) if (!g_vp[i].e) { slot = &g_vp[i]; break; } if (!slot) slot = &g_vp[(unsigned) (e >> 4) & 15]; slot->e = e; slot->x = fpos(e)[0]; slot->y = fpos(e)[1]; return; }
    float dx = fpos(e)[0] - slot->x, dy = fpos(e)[1] - slot->y, dl = vlen(dx, dy); slot->x = fpos(e)[0]; slot->y = fpos(e)[1];
    if (dl < 1.5f || dl > 40.0f) return;
    const float *a = (const float *) (e + 0x334), *b = (const float *) (e + 0x320);
    float la = vlen(a[0], a[1]), lb = vlen(b[0], b[1]);
    g_vp_n++;
    if (la > 0.6f * dl && la < 1.6f * dl) g_vp_a++;
    if (lb > 0.6f * dl && lb < 1.6f * dl) g_vp_b++;
    if (g_vp_n >= 60) {
        g_vp_done = 1;
        LOG("ffenemy: velocity probe over %d vanilla steps: +0x334 matched %d, +0x320 matched %d (the code uses +0x%x)", g_vp_n, g_vp_a, g_vp_b, g_vel_off);
        if (g_vp_b >= 15 && g_vp_a * 4 < g_vp_b) { g_vel_off = 0x320; LOG("ffenemy: velocity probe: +0x320 wins clearly, switched"); }
    }
}
/* Sprite animation of the port's monsters. Measured in the binary: Sprite::Update = 0x89d598(sprite) runs the main (+0x30) and overlay (+0x60) animation states
 * through 0x89d488, which adds the playback speed to the float frame at state+0x20 (= entity +0xa0) while the byte state+0x24 (= entity +0xa4, set by Sprite::Play,
 * cleared by SetFrame and when a non-looping animation ends) is set. NPC::Update (0x68bd3c) never calls it for the main sprite: every vanilla monster's own AI
 * does (100+ call sites). A port monster of a type/variant with no vanilla AI therefore stood on frame 0 (Square Fly, Shot Fly, Butt Fly, Poobottle ... in the
 * 23rd log's videos) because the Lua's sprite:Play only starts the animation, the engine's update of the Lua-driven monsters is what advances it.
 * So after the engine's update, a port monster whose animation is playing and whose frame did not move gets exactly one Sprite::Update. Frame-pinned monsters
 * (SetFrame every tick, byte cleared) and monsters whose vanilla AI already updated the sprite are left alone, so nothing runs twice. */
typedef void (*fn_sprupd)(void *);
static fn_sprupd g_sprupd;                                       /* null in the host tests unless a test installs a mock */
static unsigned char g_spr_logged[256];
static void en_sprite_tick(unsigned long e, float frame_before) {
    if (!g_sprupd) return;
    if (!*(const unsigned char *) (e + E_SPR_PLAYING)) return;
    if (*(const float *) (e + E_SPR_FRAME) != frame_before) return;
    g_sprupd((void *) (e + E_SPRITE));
    int k = (*(const int *) (e + E_TYPE) * 7 + *(const int *) (e + E_VARIANT)) & 255;
    if (!g_spr_logged[k]) { g_spr_logged[k] = 1; LOG("ffenemy: %d.%d: the engine did not advance the sprite, Sprite::Update is called by the port", *(const int *) (e + E_TYPE), *(const int *) (e + E_VARIANT)); }
}
/* The engine's own animation can be pinned: ff-codigo44's video shows a Square Fly (152.0) and a Casualty (451.70) frozen on one frame of 'Fly' / 'Walk' while the
 * log never printed "engine did not advance the sprite" for them, so the engine touches their sprite itself (a SetFrame each tick clears the Playing byte, which is
 * also what stops en_sprite_tick). The port keeps its own frame counter per animation (s->af, the Lua's sprite:GetFrame()), so when the engine's frame has not moved
 * for 3 ticks while the animation has more than one frame, the port sets the frame itself every tick from that counter (and from then on). */
static void en_sprite_sync(en_t *s, unsigned long e) {
    if (!g_setframe || !s->anim[0] || s->alen < 2) { s->stuck = 0; return; }
    float real = *(const float *) (e + E_SPR_FRAME);
    if (real == s->lastreal) s->stuck++; else s->stuck = 0;
    s->lastreal = real;
    int exp = s->aloop ? s->af % s->alen : (s->af < s->alen ? s->af : s->alen - 1);
    if (!s->driven && s->stuck >= 3) {
        s->driven = 1;
        FFLOG(FFC_SPRITE_STATIC, FFKEY(s->type, s->variant), "%d.%d.%d: the engine's sprite frame sat at %.1f for %d ticks (playing %d, speed %.2f) while '%s' should be at %d/%d: the port sets the frame itself",
              s->type, s->variant, s->sub, real, s->stuck, *(const unsigned char *) (e + E_SPR_PLAYING), *(const float *) (e + E_SPRITE + 0x144), s->anim, exp, s->alen);
    }
    if (s->driven) {
        g_setframe((void *) (e + E_SPRITE), s->anim, exp); s->lastreal = *(const float *) (e + E_SPR_FRAME);
#if ISAAC_DIAG
        /* the frame the port asked for must be the one the sprite now holds, or SetFrame did not take (the frames would still look frozen) */
        if ((int) s->lastreal != exp) { if (++s->srun == 10) FFLOG(FFC_SPRITE_STATIC, FFKEY(s->type, s->variant), "%d.%d.%d: SetFrame('%s', %d) did not take: the sprite reads frame %.1f 10 ticks in a row", s->type, s->variant, s->sub, s->anim, exp, s->lastreal); }
        else s->srun = 0;
#endif
    }
}
#if ISAAC_DIAG
/* the overlay animation (Woodburner's wood spin and the like): the engine's overlay frame must move while the port's own counter does. The overlay
 * state sits at sprite+0x60 and its frame at state+0x20 (the main one is sprite+0x30 / +0x20 = entity+0xa0, measured) = entity+0xd0: DERIVED, not
 * measured on its own. */
static void en_overlay_watch(en_t *s, unsigned long e) {
    if (!s->ov[0]) { s->ovs_n = 0; return; }
    const ffa_t *a = ffa_find(s->key, s->ov);
    if (!a || a->len < 2) return;
    float ofr = *(const float *) (e + E_SPRITE + 0x80);
    if (ofr == s->lastov) s->ovs_n++; else s->ovs_n = 0;
    s->lastov = ofr;
    if (s->ovs_n == 30) FFLOG(FFC_SPRITE_STATIC, FFKEY(s->type, s->variant), "%d.%d.%d: overlay '%s' (%d frames): the engine's overlay frame (entity+0xd0, derived offset) sat at %.1f for 30 ticks while the port's counter is at %d",
                              s->type, s->variant, s->sub, s->ov, a->len, ofr, s->ovf);
}
#endif
static void w_npc_update(unsigned long e) {
    en_piles_tick();
    int type = *(const int *) (e + E_TYPE);
    const enemy_t *d = g_en_on ? en_find(type, *(const int *) (e + E_VARIANT)) : 0;      /* e is the engine's own pointer */
    en_t *s = 0; float pa[2] = { 0, 0 }, va[2] = { 0, 0 }, vb[2] = { 0, 0 };
    ht_trace(e);
    if (*(const unsigned char *) (e + E_EXISTS)) { en_dump(e, d == 0); if (!d) vel_probe(e); }
    if (d && *(const unsigned char *) (e + E_EXISTS)) {
        s = en_slot(e);
        s->af++; if (s->ov[0]) s->ovf++;
        if (s->aloop && s->alen > 0 && s->af >= s->alen) s->af = 0;
        float x0 = fpos(e)[0], y0 = fpos(e)[1];
        vb[0] = fvel(e)[0]; vb[1] = fvel(e)[1];
        if (en_dead(e)) {
            if (!s->dead_done) {
                s->dead_done = 1; ENLOG(0, 24, "ffenemy: %d.%d dies (frame %d)", type, s->variant, s->age); if (d->die) d->die(s, e);
#if ISAAC_DIAG
                { const ffa_t *da = ffa_find(s->key, "Death"); unsigned k = FFKEY(type, s->variant);
                  s->dframe = *(const float *) (e + E_SPR_FRAME); s->dprobe = da ? 1 : 0;
                  FFLOG(FFC_DEATH, k, "%d.%d.%d dies at age %d, pos (%.0f,%.0f), animation '%s' at frame %.1f; 'Death' in the table: %s", type, s->variant, s->sub, s->age, fpos(e)[0], fpos(e)[1], s->anim, (double) s->dframe, da ? "yes" : "NO");
                  if (!da) FFLOG(FFC_DEATH_NO_ANIM, k, "%d.%d.%d has no 'Death' animation in the table: nothing to play when it dies", type, s->variant, s->sub); }
#endif
            }
#if ISAAC_DIAG
            if (s->dprobe && s->dead_age + 1 == 8) {                /* 8 ticks after dying the Death animation must have moved */
                float fr = *(const float *) (e + E_SPR_FRAME);
                if (fr - s->dframe < 1.0f && s->dframe - fr < 1.0f) FFLOG(FFC_DEATH_NO_ANIM, FFKEY(type, s->variant), "%d.%d.%d: 8 ticks after dying the engine's sprite frame is still %.1f (was %.1f at death; animation '%s', playing %d, speed %.2f)", type, s->variant, s->sub, (double) fr, (double) s->dframe, s->anim, *(const unsigned char *) (e + E_SPR_PLAYING), *(const float *) (e + E_SPRITE + 0x144));
            }
#endif
            /* the corpse is left to the engine. A monster the engine has no death code for (the Lua kills it with npc:Kill() and the engine waits for a "Death" animation these
             * anm2 files do not have) would stay for ever: alive, unhittable and counted by the room's clear check (ff-codigo47's bee). After 75 frames it is removed. */
            if (++s->dead_age == (d->corpse ? d->corpse : 75)) { if (!d->corpse) ENLOG(74, 24, "ffenemy: %d.%d was still in the room 75 frames after dying (HP <= 0): removed", type, s->variant); en_remove(e); }   /* corpse: a monster whose anm2 has no Death animation leaves after that many frames */
            s = 0;
        } else {
            if (d->ai) d->ai(s, e);
            en_state_watch(s);
        }
        if (s) { s->age++; pa[0] = fpos(e)[0]; pa[1] = fpos(e)[1]; va[0] = fvel(e)[0]; va[1] = fvel(e)[1]; }
        (void) x0; (void) y0;
    }
    float fb = *(const float *) (e + E_SPR_FRAME);
    o_npc_update(e);
    if (d && *(const unsigned char *) (e + E_EXISTS)) en_sprite_tick(e, fb);
    if (s && d && *(const unsigned char *) (e + E_EXISTS) && s->e == e) en_sprite_sync(s, e);
#if ISAAC_DIAG
    if (s && d && *(const unsigned char *) (e + E_EXISTS) && s->e == e) en_overlay_watch(s, e);
#endif
    if (s && s->so_on && *(const unsigned char *) (e + E_EXISTS) && s->e == e) en_apply_offsets(s, e);
    if (s && d && *(const unsigned char *) (e + E_EXISTS) && s->e == e) en_post(s, e, d, pa, va, vb);
    if ((g_ensweep++ & 255) == 0) en_sweep();
}
/* the NPC TakeDamage hook asks this: a Hurt callback of the monster */
static int en_hurt(unsigned long e, float dmg) {               /* 1: the hit does not land (the Lua's Hurt callback returning false) */
    if (!g_en_on) return 0;
    const enemy_t *d = en_find(*(const int *) (e + E_TYPE), *(const int *) (e + E_VARIANT));
    if (!d) {
        int ty = *(const int *) (e + E_TYPE), va = *(const int *) (e + E_VARIANT);
        if (ffl_ff_like(ty, va)) FFLOG(FFC_UNHANDLED, FFKEY(ty, va), "hit of %.1f on %d.%d.%d, which looks like a Fiend Folio entity and has no handler", (double) dmg, ty, va, *(const int *) (e + E_SUBTYPE));
        return 0;
    }
    en_t *hs = en_slot(e);
    if (d->hurt) d->hurt(hs, e, dmg);
    return d->immune || hs->hurt_block;
}
