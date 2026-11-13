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

static int g_enlog[64], g_ensweep;
#define ENLOG(k, n, ...) do { if (g_enlog[k]++ < (n)) LOG(__VA_ARGS__); } while (0)
#define EN_N 96
typedef struct {
    unsigned long e; unsigned key; int type, variant, sub;
    int init, age, state, cd, cd2, af, dead_done, dir;
    char anim[32], ov[32]; int alen, aloop, ovf;
    float f[6]; int i[6]; unsigned long ref, ref2;
} en_t;
static en_t g_en[EN_N];
static int g_en_on = 1;

static const ffa_t *ffa_find(unsigned key, const char *name) {
    for (const ffa_t *a = ffa_tab; a->name; a++) if (a->key == key && !strcmp(a->name, name)) return a;
    unsigned k0 = key & ~15u;                           /* the monster's other subtypes share the sheet of subtype 0 */
    if (k0 != key) for (const ffa_t *a = ffa_tab; a->name; a++) if (a->key == k0 && !strcmp(a->name, name)) return a;
    return 0;
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
    if (!a) ENLOG(5, 12, "ffenemy: %d.%d has no animation '%s' in the table", s->type, s->variant, name);
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
#define EN_FIELD(name, off) static int name##_ok(void) { if (!(off)) { ENLOG(14, 40, "ffenemy: not done: field %s is not measured", #name); return 0; } return 1; }
EN_FIELD(npc_state, OFF_NPC_STATE) EN_FIELD(npc_statef, OFF_NPC_STATEFRAME) EN_FIELD(npc_i1, OFF_NPC_I1) EN_FIELD(npc_i2, OFF_NPC_I2)
EN_FIELD(npc_v1, OFF_NPC_V1) EN_FIELD(npc_v2, OFF_NPC_V2) EN_FIELD(npc_pcd, OFF_NPC_PROJCD) EN_FIELD(npc_pdelay, OFF_NPC_PROJDELAY) EN_FIELD(npc_scale, OFF_NPC_SCALE)
EN_FIELD(ent_coll, OFF_ENT_COLLCLASS) EN_FIELD(ent_gcoll, OFF_ENT_GRIDCOLL) EN_FIELD(ent_friction, OFF_ENT_FRICTION) EN_FIELD(ent_mass, OFF_ENT_MASS) EN_FIELD(ent_target, OFF_ENT_TARGET)
EN_FIELD(ent_child, OFF_ENT_CHILD) EN_FIELD(ent_spriteoff, OFF_ENT_SPRITEOFFSET) EN_FIELD(ent_depthoff, OFF_ENT_DEPTHOFFSET)
EN_FIELD(proj_height, OFF_PROJ_HEIGHT) EN_FIELD(proj_fspeed, OFF_PROJ_FALLINGSPEED) EN_FIELD(proj_faccel, OFF_PROJ_FALLINGACCEL) EN_FIELD(proj_flags, OFF_PROJ_FLAGS)
EN_FIELD(proj_scale, OFF_PROJ_SCALE) EN_FIELD(proj_curving, OFF_PROJ_CURVING) EN_FIELD(proj_homing, OFF_PROJ_HOMING)
EN_FIELD(eff_timeout, OFF_EFF_TIMEOUT) EN_FIELD(eff_lifespan, OFF_EFF_LIFESPAN) EN_FIELD(eff_scale, OFF_EFF_SCALE)
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
/* projectiles: what FireProjectiles sets from a ProjectileParams after Game::Spawn(9, Variant ...) */
typedef struct { int variant; unsigned long long flags; float height, fspeed, faccel, faccel_mod, scale, vel_multi, curving, homing; int has_height, has_fspeed, has_faccel, has_scale, has_faccel_mod, has_curving; float fspeed_mod, height_mod; int has_fspeed_mod, has_height_mod; } en_pp_t;
static unsigned long en_shoot_p(unsigned long e, float x, float y, float vx, float vy, const en_pp_t *pp);


/* more sprite pieces: SetFrame 0x89eab8 / SetOverlayFrame 0x89ee7c (measured: both call the same inner setter on the main (+0x30) / overlay (+0x60) animation state),
 * the overlay's own frame counter, and FlipX. FlipX is NOT measured: it is emulated by the sign of SpriteScale.x (sprite +0xdc), logged once. */
static void en_set_frame(en_t *s, const char *name, int frame) { s->af = frame; strncpy(s->anim, name, sizeof s->anim - 1); if (g_setframe) g_setframe((void *) (s->e + E_SPRITE), name, frame); }
static void en_set_overlay_frame(unsigned long e, const char *name, int frame) { ((fn_setframe) (g_base + 0x89ee7c))((void *) (e + E_SPRITE), name, frame); }
static void en_overlay(en_t *s, const char *name) { if (strcmp(s->ov, name)) { strncpy(s->ov, name, sizeof s->ov - 1); s->ovf = 0; g_overlay((void *) (s->e + E_SPRITE), name, 1); } }
static void en_overlay_off(en_t *s) { if (s->ov[0]) { s->ov[0] = 0; g_remove_overlay((void *) (s->e + E_SPRITE)); } }
static int en_overlay_finished(const en_t *s, const char *name) { const ffa_t *a = ffa_find(s->key, name); return a && !strcmp(s->ov, name) && !a->loop && s->ovf >= a->len; }
static void en_flip(unsigned long e, int flip) {
    float *sc = (float *) (e + E_SPRITE + 0xdc); float ax = fabsf(sc[0]); sc[0] = flip ? -ax : ax;
    ENLOG(17, 1, "ffenemy: FlipX is emulated by the sign of the sprite's scale X (the real field is not measured)");
}

static float *en_target(unsigned long e) { unsigned long p = g_first_player(); (void) e; return p ? fpos(p) : 0; }
static unsigned long en_spawn(int type, int variant, float x, float y, float vx, float vy, unsigned long spawner, int sub) {
    float p[2] = { x, y }, v[2] = { vx, vy };
    return g_spawn(en_game(), type, variant, p, v, spawner, sub, 0, 0);
}
/* NPC:FireProjectiles for one shot: Game::Spawn(9, Variant, ...) with the velocity, then the params' fields written on the new projectile */
static unsigned long en_shoot_p(unsigned long e, float x, float y, float vx, float vy, const en_pp_t *pp) {
    float m = pp && pp->vel_multi > 0 ? pp->vel_multi : 1.0f;
    unsigned long pr = en_spawn(9, pp ? pp->variant : 0, x, y, vx * m, vy * m, e, 0);
    if (!pr || !pp) return pr;
    if (pp->has_faccel_mod && proj_faccel_ok()) *(float *) (pr + OFF_PROJ_FALLINGACCEL) += pp->faccel_mod;
    if (pp->has_fspeed_mod && proj_fspeed_ok()) *(float *) (pr + OFF_PROJ_FALLINGSPEED) += pp->fspeed_mod;
    if (pp->has_height_mod && proj_height_ok()) *(float *) (pr + OFF_PROJ_HEIGHT) += pp->height_mod;
    if (pp->has_curving) ENLOG(15, 3, "ffenemy: not done: ProjectileParams.CurvingStrength (field not measured)");
    if (pp->has_height && proj_height_ok()) *(float *) (pr + OFF_PROJ_HEIGHT) = pp->height;
    if (pp->has_fspeed && proj_fspeed_ok()) *(float *) (pr + OFF_PROJ_FALLINGSPEED) = pp->fspeed;
    if (pp->has_faccel && proj_faccel_ok()) *(float *) (pr + OFF_PROJ_FALLINGACCEL) = pp->faccel;
    if (pp->flags && proj_flags_ok()) *(unsigned long long *) (pr + OFF_PROJ_FLAGS) |= pp->flags;
    if (pp->has_scale) { if (proj_scale_ok()) *(float *) (pr + OFF_PROJ_SCALE) = pp->scale; }
    return pr;
}
static void en_shoot(unsigned long e, int variant, float x, float y, float vx, float vy) { en_pp_t pp; memset(&pp, 0, sizeof pp); pp.variant = variant; en_shoot_p(e, x, y, vx, vy, &pp); }
static void en_kill(unsigned long e) { hit_enemy(e, 1000000.0f); }
static int en_dead(unsigned long e) { return !alive(e) || (g_hp_off && fb_hp(e) <= 0.0f); }
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
    if (!s->init) { s->init = 1; if (s->sub & 1) ENLOG(7, 2, "ffenemy: not done: Square Fly's counter-clockwise sheet (Sprite::Load)"); }
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
        if (c) { float *sc = (float *) (c + E_SPRITE + 0xdc); sc[0] *= 0.75f; sc[1] *= 0.75f; }
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
            ENLOG(16, 3, "ffenemy: not done: Slobber's mouth effect SpriteOffset/DepthOffset/Color/FollowParent, sprite FlipX");
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
        ENLOG(8, 2, "ffenemy: not done: Butt Fly's no-knockback flags");
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
            ENLOG(9, 2, "ffenemy: not done: ButterBeanFart's knock-back and poison of the others around");
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
    *(unsigned long long *) (c + E_FLAGS) &= ~0x20000000ull;
}
static void ai_shotfly(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    int shoter = s->variant == 24;
    if (!s->init) {
        s->init = 1; en_play(s, "Fly");
        ENLOG(10, 2, "ffenemy: not done: Shot Fly's collision class / no-knockback flags and the wait-then-descend subtype");
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
    ENLOG(11, 2, "ffenemy: not done: creep Timeout (the creep keeps the game's default lifetime)");
}
static void ai_casualty(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e), *t = en_target(e); if (!t) return;
    if (!s->init) { s->init = 1; splat_at(e, 1.5f, 2); s->state = 0; }
    if (s->state == 0) {
        if (en_event(s, "Walk", "Move")) { en_walk_to(e, t, 4.0f, 1.8f, 1); }
        else en_play(s, "Walk");
        v[0] *= 0.9f; v[1] *= 0.9f;
        if (s->age % 10 == 5) { creep_at(e, 0.1f); splat_at(e, 0.5f, 1); }
        if (fb_hp(e) <= *(const float *) (e + 0x358) * 0.5f) s->state = 1;
    } else if (s->state == 1) {
        if (en_was_event(s, "Trip", "Shoot")) { v[0] *= 0.6f; v[1] *= 0.6f; } else { v[0] *= 0.9f; v[1] *= 0.9f; }
        if (en_finished(s, "Trip")) { s->state = 2; ENLOG(12, 2, "ffenemy: not done: Casualty's CanShutDoors = false (the corpse still counts for the room)"); }
        else if (en_event(s, "Trip", "Move")) { en_sound(SOUND_FETUS_JUMP, 1.0f, 1.0f); float x = t[0] - p[0], y = t[1] - p[1]; vresize(&x, &y, 10.0f); v[0] = x; v[1] = y; }
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
    if (!s->init) { s->init = 1; s->cd = 0; s->i[1] = 0; s->f[1] = s->f[0] ? s->f[0] : (float) rint_(1, 75); ENLOG(13, 2, "ffenemy: not done: Eternal Fly's collision class / flags"); }
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
        unsigned long f = en_spawn(18, 0, p[0], p[1], v[0], v[1], 0, 0);          /* npc:Morph(18, 0, 0, -1) */
        if (f) en_remove(e);
    }
}

/* ------------------------------------------------------------------------------------------ dispatch */
typedef struct { int type, variant; void (*ai)(en_t *, unsigned long); void (*die)(en_t *, unsigned long); void (*hurt)(en_t *, unsigned long, float); int immune; } enemy_t;
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
enum { PJ_MUTANT_ORBITAL = 1 };
static pj_t g_pj[PJ_N];
static fn_update o_proj_update;
static void pj_add(unsigned long p, unsigned long parent, int kind, float a) {
    int f = 0; for (int i = 0; i < PJ_N; i++) if (!g_pj[i].p || !*(const unsigned char *) (g_pj[i].p + E_EXISTS)) { f = i; break; }
    g_pj[f].p = p; g_pj[f].parent = parent; g_pj[f].kind = kind; g_pj[f].a = a; g_pj[f].b = 0;
}
static pj_t *pj_find(unsigned long p) { for (int i = 0; i < PJ_N; i++) if (g_pj[i].p == p) return &g_pj[i]; return 0; }
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
}

#include "ffen_g1.h"
#include "ffen_g2.h"
#include "ffen_g3.h"
#include "ffen_g4.h"
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
    return en_find_in(g_tab_g4, N_TAB_G4, type, variant);
}


static fn_update o_npc_update;
static void en_sweep(void) {
    for (int i = 0; i < EN_N; i++) if (g_en[i].e && (!*(const unsigned char *) (g_en[i].e + E_EXISTS) || *(const int *) (g_en[i].e + E_TYPE) != g_en[i].type)) g_en[i].e = 0;
}
static void w_npc_update(unsigned long e) {
    int type = *(const int *) (e + E_TYPE);
    const enemy_t *d = g_en_on ? en_find(type, *(const int *) (e + E_VARIANT)) : 0;      /* e is the engine's own pointer */
    if (d && *(const unsigned char *) (e + E_EXISTS)) {
        en_t *s = en_slot(e);
        s->af++; if (s->ov[0]) s->ovf++;
        if (s->aloop && s->alen > 0 && s->af >= s->alen) s->af = 0;
        float x0 = fpos(e)[0], y0 = fpos(e)[1];
        if (en_dead(e)) {
            if (!s->dead_done) { s->dead_done = 1; ENLOG(0, 24, "ffenemy: %d.%d dies (frame %d)", type, s->variant, s->age); if (d->die) d->die(s, e); }
        } else {
            if (d->ai) d->ai(s, e);
        }
        s->age++;
        ENLOG(1, 60, "ffenemy: %d.%d frame %d at (%.0f,%.0f) vel (%.2f,%.2f) anim %s/%d", type, s->variant, s->age, x0, y0, fvel(e)[0], fvel(e)[1], s->anim, s->af);
    }
    o_npc_update(e);
    if ((g_ensweep++ & 255) == 0) en_sweep();
}
/* the NPC TakeDamage hook asks this: a Hurt callback of the monster */
static int en_hurt(unsigned long e, float dmg) {               /* 1: the hit does not land (the Lua's Hurt callback returning false) */
    if (!g_en_on) return 0;
    const enemy_t *d = en_find(*(const int *) (e + E_TYPE), *(const int *) (e + E_VARIANT));
    if (!d) return 0;
    if (d->hurt) d->hurt(en_slot(e), e, dmg);
    return d->immune;
}
