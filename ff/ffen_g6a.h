/* ffen_g6a.h - Burning Basement, part A: the fire and gunpowder system every Burning monster leans on.
 *   Fire effect 1000.7005 "Gunpowder fire"   (main.lua 9049 mod:AshCreepFireAI, CheckFireCollision 9285)
 *   Gunpowder ash 1000.26 / 1000.45 sub 7001  (_shared.lua mod.SpawnGunpowder 1626, main.lua 12349 mod:gunpowderCreepAI, FindClosestFire _shared.lua 706)
 *   the coal projectiles ("coal", "coal2", "coalRadioactive": ffscripts/callbacks/projectiles.lua 437-540) and their poof (1000.1722)
 * The game has no code for effect variant 7005, and the port's hook only runs from the NPC update, so the effects the Lua animates are kept in small tables and stepped once per
 * game frame (en_piles_tick -> en_burn_tick), exactly like the Stinger poofs. */

/* ------------------------------------------------------------------------------------------ helpers
 * mod:FindRandomValidPathPosition(npc, mode): 2 = a reachable free cell more than 150 away, 3 = one 30..80 away, else any; false (mode 10) when there is none */
static int en_vpos(unsigned long e, int mode, float *ox, float *oy) {
    int w = g_gw ? (int) g_gw : 15, h = g_gh ? (int) g_gh : 9; const float *p = fpos(e);
    float ax[15 * 9 + 16], ay[15 * 9 + 16], mx[15 * 9 + 16], my[15 * 9 + 16]; int na = 0, nm = 0;
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
        float x = en_room_x(c), y = en_room_y(r);
        if (en_xy(x, y, 0) || !en_in_room_walk(x, y, 0.0f)) continue;
        if (!en_line_clear(p[0], p[1], x, y, 0)) continue;                          /* HasPathToPos: a clear line stands in for the path */
        float d = vlen(x - p[0], y - p[1]);
        if (na < (int) (sizeof ax / sizeof ax[0])) { ax[na] = x; ay[na] = y; na++; }
        int ok = (mode == 2 && d > 150.0f) || (mode == 3 && d > 30.0f && d < 80.0f);
        if (ok && nm < (int) (sizeof mx / sizeof mx[0])) { mx[nm] = x; my[nm] = y; nm++; }
    }
    if (nm) { int k = (int) rnd((unsigned) nm); *ox = mx[k]; *oy = my[k]; return 1; }
    if (na) { int k = (int) rnd((unsigned) na); *ox = ax[k]; *oy = ay[k]; return 1; }
    if (mode == 10) return 0;
    *ox = p[0]; *oy = p[1]; return 1;
}
/* grid.CollisionClass > 1 under a point (a rock, a wall, a pit-less obstacle) */
static int en_grid_solid_at(float x, float y) { int v = fb_tile(x, y); return (v >= 2 && v <= 5) || v == 255; }
/* a hit with the DAMAGE_FIRE flag and a source (the Lua's TakeDamage(1, DamageFlag.DAMAGE_FIRE, EntityRef(fire), 0)) */
static void en_hit_fire(unsigned long victim, float dmg, unsigned long src) { hit_enemy_f(victim, dmg, DMG_FIRE, src); }
/* the Lua Hurt callbacks of the fire family: `if flag & DAMAGE_FIRE ~= 0 and source.Type ~= 1 then return false` */
static int en_fire_from_not_player(void) {
    if (!(g_hit_flags & DMG_FIRE)) return 0;
    if (!g_hit_src || !g_readable((const void *) g_hit_src, 0x40)) return 1;
    return *(const int *) (g_hit_src + E_TYPE) != 1;
}
static void hurt_fireproof(en_t *s, unsigned long e, float dmg) { (void) e; (void) dmg; s->hurt_block = en_fire_from_not_player(); }
/* the gunpowder fire (1000.7005) does not hurt the Sun/Earth/Venus-like fire kin either: `source.Variant == 7005` */
static int en_hit_by_gunpowder_fire(void) {
    if (!(g_hit_flags & DMG_FIRE) || !g_hit_src || !g_readable((const void *) g_hit_src, 0x40)) return 0;
    return *(const int *) (g_hit_src + E_TYPE) == 1000 && *(const int *) (g_hit_src + E_VARIANT) == 7005;
}
static void en_alpha(unsigned long e, float a) { ((float *) (e + E_SPRITE + 0xe8))[3] = a; }
static void en_sheet(unsigned long e, int layer, const char *path) { if (g_sheet_l) g_sheet_l((void *) (e + E_SPRITE), layer, path); }   /* mod:ReplaceEnemySpritesheet + LoadGraphics */
static int en_anim_len(unsigned key, const char *name) { const ffa_t *a = ffa_find(key, name); return a ? a->len : 0; }

/* ------------------------------------------------------------------------------------------ the fire effect 1000.7005 */
typedef struct {
    unsigned long e, parent, spawner; int sub, age, timer, init, no_damage, blue, has_grid, grid_coll, grid_dec, twice, flamethrower, parentdead, off_adj, ghost;
    float fric, scale, radius, ivx, ivy, val1, val2, pcount, lx, ly, pv;
    int col_age, col_dur; float co[3];                              /* EntityEffect:SetColor(color, duration, 1, true): the colour offsets fade out linearly over `col_dur` frames (a blue flame turns into the normal one) */
} fire_t;
#define FIRE_N 192
static fire_t g_gfire[FIRE_N];
static int g_fx_move = -1;                                    /* does the engine itself move an effect by its Velocity? (-1: not measured yet) */
/* Isaac.Spawn(1000, 7005, sub, pos, vel, spawner): the caller sets what the Lua sets on the effect (timer, parent...) and the first step runs at once when the Lua calls fire:Update() */
static fire_t *en_fire_new(unsigned long spawner, float x, float y, float vx, float vy, int sub) {
    unsigned long e = en_spawn(1000, 7005, x, y, vx, vy, spawner, sub); if (!e) return 0;
    for (int k = 0; k < FIRE_N; k++) if (!g_gfire[k].e || !alive(g_gfire[k].e)) {
        fire_t *f = &g_gfire[k]; memset(f, 0, sizeof *f);
        f->e = e; f->spawner = spawner; f->sub = sub; f->timer = 20; f->scale = 1.0f; f->radius = 20.0f; f->val1 = 2.2f; f->val2 = 10.0f; f->pcount = 1.0f;
        return f;
    }
    ENLOG(180, 1, "ffenemy: not done: more than %d fires at once (the extra fire is removed)", FIRE_N);
    en_remove(e); return 0;
}
static int en_fire_dist_ok(unsigned long victim, const float *fp, float radius) {
    const float *vp = fpos(victim); float size = *(const float *) (victim + 0x344);
    return vlen(vp[0] - fp[0], vp[1] - fp[1]) < radius + size;
}
/* CheckFireCollision: every entity inside the radius that is the player or an active enemy takes 1 DAMAGE_FIRE from the fire */
static void en_fire_damage(fire_t *f) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); const float *fp = fpos(f->e);
    { unsigned long pl = g_first_player ? g_first_player() : 0; if (pl && en_fire_dist_ok(pl, fp, f->radius)) en_hit_fire(pl, 1.0f, f->e); }
    for (unsigned k = 0; k < n; k++) {
        unsigned long v = snap[k]; if (!alive(v) || v == f->e) continue;
        int t = *(const int *) (v + E_TYPE);
        if (t == 1) continue;                                                                    /* the player: below, by its own pointer */
        if (t >= 10 && t < 1000 && *(const int *) (v + OFF_ENT_COLLCLASS) > 2 && !en_flag(v, EF_FLAG_FRIENDLY)) { if (en_fire_dist_ok(v, fp, f->radius)) en_hit_fire(v, 1.0f, f->e); }
    }
}
static void en_fire_color_step(fire_t *f);
static void en_fire_step(fire_t *f) {
    unsigned long e = f->e; float *p = fpos(e), *v = fvel(e); unsigned key = (1000u << 20) | (7005u << 4);
    f->age++;
    if (!f->init) {
        if (g_play) g_play((void *) (e + E_SPRITE), "FireStage01", 1);
        if (f->sub == 1) { f->fric = 0.91f; f->timer = 25; }                                     /* spitroast */
        else if (f->sub == 2) { f->blue = 1; f->timer = 15; }                                    /* blazer */
        else if (f->sub == 20) f->fric = 0.95f;                                                  /* mr flare */
        else if (f->sub == 450) { f->fric = 1.0f; f->radius = 10.0f; f->timer = 25; f->grid_dec = 6; f->off_adj = 2; f->twice = 1; }   /* ashtray */
        if (f->blue) en_sheet(e, 0, "gfx/enemies/blazer/effect_005_fire_blue.png");
        f->init = 1; f->lx = p[0]; f->ly = p[1]; f->pv = vlen(v[0], v[1]);
    }
    /* who moves the fire: measured on the first moving one (see g_fx_move) */
    if (f->age > 1 && f->pv > 1.0f) {
        float moved = vlen(p[0] - f->lx, p[1] - f->ly);
        if (g_fx_move < 0 && f->age >= 3) { g_fx_move = moved > 0.3f * f->pv ? 1 : 0; LOG("ffenemy: fire effects are moved by %s", g_fx_move ? "the engine" : "the port (the engine leaves them where they are)"); }
    }
    if (f->sub == 10) {                                                                          /* woodburner neck: it circles its parent */
        en_spr_off(e, 0.0f, -5.0f);
        unsigned long par = f->parent && alive(f->parent) ? f->parent : 0;
        if (par) {
            float ax = f->ivx, ay = f->ivy; vresize(&ax, &ay, (float) f->age * f->val1); float rx, ry; vrot(ax, ay, (float) f->age * f->val2, &rx, &ry);
            const float *pp = fpos(par);
            v[0] = lerpf(v[0], pp[0] + rx - p[0], 0.6f); v[1] = lerpf(v[1], pp[1] + ry - p[1], 0.6f);
        } else f->parentdead = 1;
    }
    if (f->fric != 0.0f) { v[0] *= f->fric; v[1] *= f->fric; }
    if (f->parentdead) { f->pcount -= 0.1f; en_alpha(e, f->pcount); }
    int fc = f->age;
    if (!f->parentdead && fc > 3 && fc < f->timer + 5 && ((unsigned) (e >> 4) + (unsigned) fc) % 3u == 0 && !f->no_damage) en_fire_damage(f);
    if (fc < 5) {
        float k = f->flamethrower ? 0.5f + (float) fc / 10.0f : (float) fc / 5.0f; en_scale(e, f->scale * k, f->scale * k);
    } else if (fc > f->timer) {
        float k = f->flamethrower ? 1.0f + (float) (fc - f->timer) / 50.0f : 1.0f - (float) (fc - f->timer) / 20.0f;
        en_scale(e, f->scale * k, f->scale * k);
        if (!f->pcount || !f->parentdead) en_alpha(e, 1.0f - (float) (fc - f->timer) / 10.0f);
        if (fc > f->timer + 10) { en_remove(e); f->e = 0; return; }
    } else {
        en_scale(e, f->scale, f->scale);
        if (f->has_grid && fc > f->grid_coll) {
            int hit = en_grid_solid_at(p[0], p[1]);
            if (!hit && f->twice) hit = en_grid_solid_at(p[0] - v[0] / 2.0f, p[1] - v[1] / 2.0f);
            if (hit) { f->no_damage = 1; f->timer = fc - (f->grid_dec ? f->grid_dec : 3); }
        }
    }
    if (f->off_adj && fc <= 5) en_spr_off(e, 0.0f, (float) (f->off_adj * fc));
    { int len = en_anim_len(key, "FireStage01"); if (len > 0 && g_setframe) g_setframe((void *) (e + E_SPRITE), "FireStage01", fc % len); }
    en_fire_color_step(f);
    if (g_fx_move == 0) { p[0] += v[0]; p[1] += v[1]; }
    f->lx = p[0]; f->ly = p[1]; f->pv = vlen(v[0], v[1]);
}
/* fire:SetColor(Color(1,1,1,1,ro,go,bo), dur, 1, true, false) */
static void en_fire_set_color(fire_t *f, float ro, float go, float bo, int dur) { if (!f) return; f->co[0] = ro; f->co[1] = go; f->co[2] = bo; f->col_dur = dur; f->col_age = 0; }
static void en_fire_color_step(fire_t *f) {
    if (f->col_dur <= 0) return;
    float *c = (float *) (f->e + E_SPRITE + 0xe8), k = 1.0f - (float) f->col_age / (float) f->col_dur; if (k < 0) k = 0;
    c[8] = f->co[0] * k; c[9] = f->co[1] * k; c[10] = f->co[2] * k;
    if (++f->col_age > f->col_dur) f->col_dur = 0;
}
/* fire:Update() right after the spawn */
static void en_fire_go(fire_t *f) { if (f) en_fire_step(f); }
/* a fire that is flaming at the moment (specialFireTypes: Spark, Fumegeist, Mote, Smokin, Smogger when d.flaming) or burns by nature (fireTypes) */
static int en_is_fire_source(unsigned long v) {
    int t = *(const int *) (v + E_TYPE), var = *(const int *) (v + E_VARIANT), sub = *(const int *) (v + E_SUBTYPE);
    if (t == 33) return fb_hp(v) > 1.0f;                                                         /* fireplaces that are still lit */
    switch (t) {
    case 10: return var == 2;                                                                    /* Flaming Gaper */
    case 15: return var == 3;                                                                    /* Grilled Clotty */
    case 54: return var == 0;                                                                    /* Flaming Hopper */
    case 160: if (var == 442 || var == 443) return 1;                                            /* Flamin, Flamin Chain */
              if (var == 152 || var == 350 || var == 351 || var == 441) return en_slot(v)->flaming;
              return 0;
    case 208: return var == 2;                                                                   /* Flaming Fatty */
    case 226: return var == 2;                                                                   /* Crispy */
    case 824: return var == 1;                                                                   /* Grilled Gyro */
    case 817: return var == 140 && en_slot(v)->flaming;                                          /* Smogger */
    case 451: return var == 151 || var == 150 || (var == 100 && sub == 1);                       /* Litling, Chili, Trailblazer's flame segment */
    case 9: return var == 2;                                                                     /* Flame projectile */
    case 1000: if (var == 1 || var == 147 || var == 148 || var == 7005 || var == 7012 || var == 7015) return 1;
               return var == 51 && sub == 1;                                                     /* Fatty fire */
    }
    return 0;
}
/* mod.FindClosestFire: is any fire (one that has lived more than 2 frames) closer than `dist`? */
static int en_fire_within(float x, float y, float dist) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long v = snap[k]; if (!alive(v) || !en_is_fire_source(v)) continue;
        int t = *(const int *) (v + E_TYPE);
        if (t == 1000 && *(const int *) (v + E_VARIANT) == 7005) { int old = 0; for (int q = 0; q < FIRE_N; q++) if (g_gfire[q].e == v) old = g_gfire[q].age > 2; if (!old) continue; }
        else if (t >= 10 && t < 1000 && t != 33 && en_slot(v)->age <= 2) continue;
        const float *vp = fpos(v);
        if (vlen(vp[0] - x, vp[1] - y) < dist) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------ gunpowder: mod.SpawnGunpowder + mod:gunpowderCreepAI */
typedef struct { unsigned long e, spawner; int burntime, timeout, flaming, spawned, sf, friendly, age; float spread; } gp_t;
#define GP_N 256
static gp_t g_gp[GP_N];
static void en_set_timeout(unsigned long e, int t) { *(int *) (e + OFF_EFF_TIMEOUT) = t; *(int *) (e + OFF_EFF_TIMEOUT2) = t; }   /* EntityEffect:SetTimeout */
/* timeout < 0 = the Lua's default (30); burntime < 0 = nil (20 when it burns); spread < 0 = 50 */
static unsigned long en_gunpowder(unsigned long npc, float x, float y, int timeout, int burntime, float spread, int auto_flaming) {
    int friendly = npc && *(const int *) (npc + E_TYPE) == 1;
    unsigned long e = en_spawn(1000, friendly ? 45 : 26, x, y, 0, 0, npc, 7001); if (!e) return 0;
    if (g_load) { char nm[16]; g_load((void *) (e + E_SPRITE), "gfx/effects/1000.092_creep (ash).anm2"); snprintf(nm, sizeof nm, "SmallBlood0%d", rint_(1, 6)); if (g_play) g_play((void *) (e + E_SPRITE), nm, 1); }
    else ENLOG(181, 1, "ffenemy: not done: Sprite::Load is not installed: the gunpowder has the plain creep sprite");
    en_set_timeout(e, timeout < 0 ? 30 : timeout);
    for (int k = 0; k < GP_N; k++) if (!g_gp[k].e || !alive(g_gp[k].e)) {
        gp_t *g = &g_gp[k]; memset(g, 0, sizeof *g);
        g->e = e; g->spawner = npc; g->burntime = burntime < 0 ? 20 : burntime; g->timeout = timeout; g->spread = spread < 0.0f ? 50.0f : spread; g->flaming = auto_flaming; g->friendly = friendly;
        return e;
    }
    ENLOG(182, 1, "ffenemy: not done: more than %d gunpowder creeps at once (the extra one does not burn)", GP_N);
    return e;
}
static void en_gunpowder_step(gp_t *g) {
    unsigned long e = g->e; const float *p = fpos(e); g->age++;
    if (g->flaming && !g->spawned) {
        if (g->friendly) ENLOG(183, 1, "ffenemy: not done: a gunpowder of the player burns with the red-candle fire 1000.51.960 (the monsters' fire is used instead)");
        fire_t *f = en_fire_new(g->spawner && alive(g->spawner) ? g->spawner : 0, p[0], p[1], 0, 0, 0);
        if (f) { f->timer = g->burntime; f->parent = g->spawner && alive(g->spawner) ? g->spawner : 0; en_fire_go(f); }
        g->spawned = 1; g->sf = 0;
    } else if (!g->flaming) {
        if (g->age > 1 && en_fire_within(p[0], p[1], g->spread)) g->flaming = 1;
    }
    if (g->spawned) {
        g->sf++;
        if (g->sf > g->burntime + 10) { en_remove(e); g->e = 0; }
        else if (g->sf > g->burntime) { float k = 1.0f - (float) (g->sf - g->burntime) / 10.0f; en_scale(e, k, k); }
    }
}
/* the closest gunpowder not yet burning (mod.FindClosestUnlitPowder) */
static gp_t *en_unlit_powder(float x, float y, float *dist) {
    gp_t *best = 0; float bd = 99999999.0f;
    for (int k = 0; k < GP_N; k++) { gp_t *g = &g_gp[k]; if (!g->e || !alive(g->e) || g->flaming) continue; float d = vlen(fpos(g->e)[0] - x, fpos(g->e)[1] - y); if (d < bd) { bd = d; best = g; } }
    if (dist) { *dist = bd; }
    return best;
}

/* ------------------------------------------------------------------------------------------ the smoke and ember effects the Burning monsters throw
 * EffectVariant.DARK_BALL_SMOKE_PARTICLE (the game's own), 1000.66 (ember). They only need spawning: the game animates both. */
static unsigned long en_smoke(unsigned long owner, float x, float y, float vx, float vy, float offy, float rot, int tinted, float r, float g, float b, float a, float ro, float go, float bo) {
    unsigned long sm = en_spawn(1000, EV_DARK_BALL_SMOKE_PARTICLE, x, y, vx, vy, owner, 0); if (!sm) return 0;
    if (offy != 0.0f) en_spr_off(sm, 0.0f, offy);
    if (rot != 0.0f) en_set_rotation(sm, rot);
    if (tinted) en_paint(sm, r, g, b, a, 0, 0, 0, 0, ro, go, bo);
    return sm;
}
/* mod:SetRoomAlight(npc) (_shared.lua 735): every gunpowder of the room and every Mote catches fire (a friend's: only friendly ones) */
static void en_set_room_alight(unsigned long npc) {
    int friend = npc && en_flag(npc, EF_FLAG_FRIENDLY | EF_FLAG_CHARM);
    for (int k = 0; k < 256; k++) if (g_gp[k].e && alive(g_gp[k].e) && (!friend || g_gp[k].friendly)) g_gp[k].flaming = 1;
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) { unsigned long v = snap[k]; if (!alive(v) || *(const int *) (v + E_TYPE) != 160 || *(const int *) (v + E_VARIANT) != 351) continue;
        if (!friend || en_flag(v, EF_FLAG_FRIENDLY | EF_FLAG_CHARM)) en_slot(v)->flaming = 1; }
}

/* mod:ShootFlamethrowerFlame(npc, pos, vel, params, height, sheet): PROJECTILE_FIRE (variant 2), FallingAccel -0.1, Height -3, colour kept at default; `sheet` replaces layer 0.
 * `pp` may carry the caller's other params (flags...), or be 0. */
static unsigned long en_flame_shot(unsigned long e, float x, float y, float vx, float vy, const en_pp_t *pp0, const char *sheet) {
    en_pp_t pp; if (pp0) pp = *pp0; else memset(&pp, 0, sizeof pp);
    pp.variant = 2; pp.has_faccel = 1; pp.faccel = -0.1f; pp.has_height = 1; pp.height = -3.0f;
    unsigned long pr = en_shoot_p(e, x, y, vx, vy, &pp); if (!pr) return 0;
    if (sheet) en_sheet(pr, 0, sheet);
    pj_add(pr, e, PJ_FLAME, 0);
    return pr;
}

/* ------------------------------------------------------------------------------------------ explosions
 * game:BombExplosionEffects / Isaac.Explode: the address of the engine's own is not known, so the explosion is built from its parts: the effect 1000.1, the boom, and the damage
 * (bomb radius ~ 75 x radius multiplier; 2 half hearts to the player, `dmg` to every monster). It does NOT break rocks (logged). */
#define DMG_EXPLOSION 4ull
static void en_explode(unsigned long src, float x, float y, float dmg, float radmult, int hurt_src) {
    float R = 75.0f * radmult;
    en_spawn(1000, 1, x, y, 0, 0, src, 0);
    en_sound(SOUND_BOSS1_EXPLOSIONS, 1.0f, 1.0f);
    ENLOG(184, 1, "ffenemy: not done: an explosion breaks no rocks / grid and shakes no camera (the engine's BombExplosionEffects address is not known): effect, boom and damage only");
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    { unsigned long pl = g_first_player ? g_first_player() : 0;
      if (pl && !(src && en_flag(src, EF_FLAG_FRIENDLY)) && vlen(fpos(pl)[0] - x, fpos(pl)[1] - y) < R + *(const float *) (pl + 0x344)) hit_enemy_f(pl, 2.0f, DMG_EXPLOSION, src); }
    for (unsigned k = 0; k < n; k++) {
        unsigned long v = snap[k]; if (!alive(v) || (v == src && !hurt_src)) continue;
        int t = *(const int *) (v + E_TYPE); const float *vp = fpos(v); float size = *(const float *) (v + 0x344);
        if (vlen(vp[0] - x, vp[1] - y) >= R + size) continue;
        if (t == 1) continue;
        if (t >= 10 && t < 1000 && *(const int *) (v + OFF_ENT_COLLCLASS) > 0) hit_enemy_f(v, dmg, DMG_EXPLOSION, src);
    }
}

/* ------------------------------------------------------------------------------------------ the coal projectiles and their poofs */
enum { PW_COAL = 3, PW_COAL2 = 4, PW_COALRAD = 5, PW_REDCREEP = 6 };
static void en_g6_tick(void);
static void en_g7_tick(void);
static void en_g9_tick(void);
static void en_g11_tick(void);
static void en_g12_tick(void);
static void en_g13_tick(void);
static void en_g15_tick(void);
static void en_burn_tick(void) {
    en_g6_tick();
    en_g7_tick();
    en_g9_tick();
    en_g11_tick();
    en_g12_tick();
    en_g13_tick();
    en_g15_tick();
    for (int k = 0; k < FIRE_N; k++) { fire_t *f = &g_gfire[k]; if (!f->e) continue; if (!alive(f->e)) { f->e = 0; continue; } en_fire_step(f); }
    for (int k = 0; k < GP_N; k++) { gp_t *g = &g_gp[k]; if (!g->e) continue; if (!alive(g->e)) { g->e = 0; continue; } en_gunpowder_step(g); }
}
