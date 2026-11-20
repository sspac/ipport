/* ffen_g8.h - Fiend Folio's CUSTOM GRIDS, part B: the mod draws them with StageAPI over the game's own poops / rocks / webs and its room files place the
 * placeholder entities 183 (FFID.Grid) and 184 (FFID.Poop) with the grid's number in the variant. The port has no StageAPI, so every one of them is an NPC entity
 * with the grid's own sheet: it never moves, blocks like a grid does, and runs the Lua's callbacks (POST_CUSTOM_GRID_UPDATE / _DESTROY) here.
 * Included by ffenemy.h after ffen_g7.h.
 *
 * kinds:  PG_POOP  four hits (State1..State4, State5 = what is left), explosions / TNT break it at once
 *         PG_ROCK  only explosions / TNT break it (rubble afterwards)
 *         PG_FX    never breaks: fire pot, soot tag
 * s->state 0 standing / 1 destroyed; s->i[0] = hits taken; s->f[10], s->f[11] = where it was placed. */

#define PG_POOP 1
#define PG_ROCK 2
#define PG_FX 3

typedef struct { int type, var, kind; float er, eg, eb; void (*destroy)(en_t *, unsigned long); } pg_def_t;

static unsigned long pg_game_player(void) { return g_first_player ? g_first_player() : 0; }
static const enemy_t *en_find(int type, int variant);
static int pg_have(int type, int var) { return en_find(type, var) != 0; }                 /* an FF monster / pickup the port already runs */
static void pg_pickup(const float *p, int var, int sub, float vx, float vy) {                 /* vanilla pickups always; FF ones once the port has them (en_find(5, variant)) */
    int ff = (var == 20 && sub >= 213) || var >= 1000 || (var == 350 && sub >= 1000) || var == 300;
    if (ff && !pg_have(5, var)) return;
    en_spawn(5, var, p[0], p[1], vx, vy, 0, sub);
}

/* ---- loot / effects that run when the grid breaks (the Lua's POST_CUSTOM_GRID_DESTROY) */
static void pg_destroy_beehive(en_t *s, unsigned long e) {                                      /* customgrids.lua: a roll of 6 */
    float pos[2] = { fpos(e)[0], fpos(e)[1] }; (void) s;
    int roll = rint_(0, 5);
    if (roll == 1) pg_pickup(pos, 20, 215, 0, 0);                                                /* Honey Penny */
    else if (roll == 2) en_spawn(256, 0, pos[0], pos[1], 0, 0, 0, 0);                            /* a bee */
    else if (roll == 3) en_spawn(281, 0, pos[0], pos[1], 0, 0, 0, 0);                            /* mod.cheekyspawn(.., 281, 0, 0) */
    else if (roll == 4 && frand() <= 0.4f) { if (pg_have(160, 180)) en_spawn(160, 180, pos[0], pos[1], 0, 0, 0, 0); }      /* Beeter */
    else if (roll == 5 && frand() <= 0.1f) { if (pg_have(160, 401)) en_spawn(160, 401, pos[0], pos[1], 0, 0, 0, 0); }      /* Zingling (it drops) */
    unsigned long c = en_spawn(1000, EV_CREEP_BROWN, pos[0], pos[1], 0, 0, 0, 0);               /* the honey puddle: scale 2, 8000 frames, colorize 5.5 / 3.5 / 1 */
    if (c) { en_scale(c, 2.0f, 2.0f); en_set_timeout(c, 8000); en_paint(c, 1, 1, 1, 1, 5.5f, 3.5f, 1.0f, 1.0f, 0, 0, 0); }
}
static void pg_destroy_spidernest(en_t *s, unsigned long e) {                                   /* guwah/grids/spidernest.lua */
    float pos[2] = { fpos(e)[0], fpos(e)[1] }; (void) s;
    en_sound(SOUND_BOIL_HATCH, 0.6f, 1.5f); en_sound(SOUND_DEATH_BURST_SMALL, 0.6f, 1.0f);
    en_spawn(85, 0, pos[0], pos[1], (float) rint_(-15, 15) / 10.0f, (float) rint_(-15, 15) / 10.0f, 0, 0);          /* EntityNPC.ThrowSpider(pos, nil, pos + (-15..15)) */
    int n = rint_(4, 5 + 1);                                                                      /* the floor number is not read: stage 1 */
    for (int i = 0; i < n; i++) {
        float vx = (float) rint_(-40, 40) / 10.0f, vy = (float) rint_(-40, 40) / 10.0f;
        if (pg_have(85, 962)) en_spawn(85, 962, pos[0], pos[1], vx, vy, 0, 0); else en_spawn(85, 0, pos[0], pos[1], vx, vy, 0, 0);     /* Baby Spider */
    }
    if (frand() < 0.05f) pg_pickup(pos, 300, 0, 0, 0);                                            /* Egg Sack card (once the port has FF's cards) */
}
static void pg_destroy_shampoo(en_t *s, unsigned long e) {                                      /* customgrids.lua: 4 in 10 a Drop, otherwise a roll of 200 */
    float pos[2] = { fpos(e)[0], fpos(e)[1] }; (void) s;
    if (rint_(0, 9) < 4 && pg_have(160, 150)) { en_spawn(160, 150, pos[0], pos[1], 0, 0, 0, 0); return; }
    int r = rint_(0, 199);
    if (r < 1) pg_pickup(pos, 350, 1000, 0, 0);                                                   /* Petrified Gel (FF trinket) */
    else if (r < 4) pg_pickup(pos, 30, 0, 0, 0);                                                  /* key */
    else if (r < 8) pg_pickup(pos, 10, rint_(0, 1) ? 8 : 3, 0, 0);                                /* half soul / soul heart */
    else if (r < 24) pg_pickup(pos, 20, 0, 0, 0);                                                 /* penny */
}
static void pg_destroy_platinum(en_t *s, unsigned long e) {
    float pos[2] = { fpos(e)[0], fpos(e)[1] }; (void) s;
    int n = rint_(2, 3);
    for (int i = 0; i < n; i++) { float vx, vy; en_rand_vec(&vx, &vy, 3.0f); pg_pickup(pos, 20, 4, vx, vy); }          /* dimes */
    ENLOG(330, 1, "ffenemy: not done: the Platinum Penny unlock and the screen shake of the platinum poop");
}
static void pg_destroy_petrified(en_t *s, unsigned long e) {                                    /* its gibs: rock particles and the crumble */
    float pos[2] = { fpos(e)[0], fpos(e)[1] }; (void) s;
    int n = rint_(3, 5);
    for (int i = 0; i < n; i++) { float vx, vy; en_rand_vec(&vx, &vy, 2.0f); en_spawn(1000, EV_ROCK_PARTICLE, pos[0], pos[1], vx, vy, 0, 196609); }
    en_sound(SOUND_ROCK_CRUMBLE, 1.0f, 1.0f);
}
static void pg_destroy_cursed(en_t *s, unsigned long e) {                                       /* dead/grids/cursedpoop.lua */
    float pos[2] = { fpos(e)[0], fpos(e)[1] }; (void) s;
    float chance = frand();
    if (chance < 0.15f) {
        if (chance > 0.075f) pg_pickup(pos, 20, 213, 0, 0);                                       /* Cursed Penny */
        else { int half = frand() > 0.5f; if (half) pg_pickup(pos, 1021, 0, 0, 0); else pg_pickup(pos, 10, 6, 0, 0); }   /* half black heart (FF) / black heart */
    }
}
static void pg_destroy_evil(en_t *s, unsigned long e) {                                         /* guwah/grids/evilpoop.lua */
    float pos[2] = { fpos(e)[0], fpos(e)[1] }; (void) s;
    float roll = frand();
    if (roll <= 0.15f) { if (roll <= 0.075f) pg_pickup(pos, 1021, 0, 0, 0); else pg_pickup(pos, 10, 6, 0, 0); }
    ENLOG(331, 1, "ffenemy: not done: the evil poop's darkness aura (it is drawn by the Lua's render callback) and its confusion when thrown");
}
static void pg_destroy_dogdoo(en_t *s, unsigned long e) {
    (void) s; (void) e;
    ENLOG(332, 1, "ffenemy: not done: the Dog Doo's Glowing Hour Glass rewind (taiga/grids/dogDoo.lua)");
}

static const pg_def_t g_pg[] = {
    { 184, 1031, PG_POOP, 255.0f / 255.0f, 165.0f / 255.0f, 0.0f, pg_destroy_beehive },
    { 184, 1032, PG_POOP, 1.0f, 1.0f, 1.0f, pg_destroy_spidernest },
    { 184, 1020, PG_POOP, 57.0f / 255.0f, 133.0f / 255.0f, 197.0f / 255.0f, pg_destroy_shampoo },
    { 184, 1050, PG_POOP, 134.0f / 255.0f, 153.0f / 255.0f, 160.0f / 255.0f, 0 },
    { 184, 1051, PG_POOP, 0, 0, 0, 0 },
    { 184, 1052, PG_POOP, 153.0f / 255.0f, 102.0f / 255.0f, 0.0f, 0 },
    { 184, 1037, PG_POOP, 0.8f, 0.1f, 0.1f, pg_destroy_evil },
    { 183, 1026, PG_POOP, 79.0f / 255.0f, 47.0f / 255.0f, 80.0f / 255.0f, pg_destroy_cursed },
    { 183, 1027, PG_POOP, 131.0f / 255.0f, 131.0f / 255.0f, 131.0f / 255.0f, pg_destroy_platinum },
    { 183, 1028, PG_POOP, 0.4f, 0.4f, 0.4f, pg_destroy_petrified },
    { 183, 1036, PG_POOP, 0, 0, 0, pg_destroy_dogdoo },
    { 183, 1025, PG_ROCK, 0, 0, 0, 0 },
    { 183, 1038, PG_ROCK, 0, 0, 0, 0 },
    { 183, 1016, PG_FX, 0, 0, 0, 0 },
    { 183, 1331, PG_FX, 0, 0, 0, 0 },
};
#define N_PG ((int) (sizeof g_pg / sizeof g_pg[0]))
static const pg_def_t *pg_def(int type, int var) { for (int i = 0; i < N_PG; i++) if (g_pg[i].type == type && g_pg[i].var == var) return &g_pg[i]; return 0; }

/* the poop's picture for the hits it took: State1 .. State4, State5 once broken (the Lua's SetPoopSpriteState: SetFrame("State<n>", 4)) */
static void pg_poop_frame(en_t *s, unsigned long e, int hits) {
    char nm[8]; int st = hits + 1; if (st > 5) st = 5;
    snprintf(nm, sizeof nm, "State%d", st);
    strncpy(s->anim, nm, sizeof s->anim - 1);
    if (g_setframe) g_setframe((void *) (e + E_SPRITE), nm, st == 1 ? 0 : 4);
}
static void pg_destroy(en_t *s, unsigned long e) {
    const pg_def_t *d = pg_def(s->type, s->variant); if (!d) return;
    const float *p = fpos(e); float pos[2] = { p[0], p[1] };
    s->state = 1; s->cd2 = 0;
    en_set_collision(e, ENTCOLL_NONE); en_flag_set(e, EF_FLAG_RENDER_FLOOR);
    if (d->kind == PG_POOP) {
        pg_poop_frame(s, e, 4);
        unsigned long x = en_spawn(1000, EV_POOP_EXPLOSION, pos[0], pos[1], 0, 0, 0, 0);                         /* PoopExplosionColor */
        if (x) en_paint(x, 0, 0, 0, 0.7f, 0, 0, 0, 0, d->er, d->eg, d->eb);
        int n = rint_(3, 4);
        for (int i = 0; i < n; i++) { float vx, vy; en_rand_vec(&vx, &vy, (float) rint_(20, 50) / 10.0f); en_spawn(1000, EV_POOP_PARTICLE, pos[0], pos[1], vx, vy, 0, 0); }
        en_sound(SOUND_PLOP, 1.0f, 1.0f);
    } else if (d->kind == PG_ROCK) {
        en_play(s, "rubble_alt");
        for (int i = 0; i < 4; i++) { float vx, vy; en_rand_vec(&vx, &vy, 3.0f); en_spawn(1000, EV_ROCK_PARTICLE, pos[0], pos[1], vx, vy, 0, 0); }
        en_sound(SOUND_ROCK_CRUMBLE, 1.0f, 1.0f);
    }
    if (d->destroy) d->destroy(s, e);
}

/* ---- the rubber rock (dead/grids/rubberrock.lua): the player is thrown off at 15, enemies at 20, tears and projectiles turn around */
static struct { unsigned long e; int age; } g_pg_bounced[48];
static int pg_bounced(unsigned long v) {
    for (int i = 0; i < 48; i++) if (g_pg_bounced[i].e == v && g_pg_bounced[i].age > 0) return 1;
    for (int i = 0; i < 48; i++) if (g_pg_bounced[i].age <= 0) { g_pg_bounced[i].e = v; g_pg_bounced[i].age = 8; return 0; }
    return 0;
}
static void pg_rubber(en_t *s, unsigned long e) {
    const float *p = fpos(e); unsigned long snap[SNAP]; unsigned n = snapshot(snap); int bounced = 0;
    if (!s->cd2) for (int i = 0; i < 48; i++) if (g_pg_bounced[i].age > 0) g_pg_bounced[i].age--;
    for (unsigned k = 0; k < n; k++) {
        unsigned long v = snap[k]; if (v == e || !alive(v)) continue;
        int t = *(const int *) (v + E_TYPE); const float *vp = fpos(v); float dx = vp[0] - p[0], dy = vp[1] - p[1];
        if (t == 1) {
            float cx = fminf(fmaxf(vp[0], p[0] - 20.0f), p[0] + 20.0f), cy = fminf(fmaxf(vp[1], p[1] - 20.0f), p[1] + 20.0f);
            float sz = *(const float *) (v + 0x344) + 2.0f;
            if (vlen(vp[0] - cx, vp[1] - cy) < sz && vlen(dx, dy) < 28.0f) { float *vv = fvel(v); vresize(&dx, &dy, 15.0f); vv[0] = dx; vv[1] = dy; bounced = 1; }
        } else if (t == 2 || t == 9) {
            if (vlen(dx, dy) >= 28.0f || pg_bounced(v)) continue;
            float *vv = fvel(v); if (fabsf(dx) > fabsf(dy)) vv[0] = -vv[0]; else vv[1] = -vv[1];
        } else if (t >= 10 && t < 1000 && en_stat_enemy(v) && !en_flag(v, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK)) {
            if (vlen(dx, dy) < 28.0f + *(const float *) (v + 0x344) * 0.5f) { float *vv = fvel(v); vresize(&dx, &dy, 20.0f); vv[0] = dx; vv[1] = dy; bounced = 1; }
        }
    }
    if (bounced) { en_play(s, "Bounce"); en_sound(FFSND_RubberRockBounce, 0.65f, 0.9f + frand() * 0.2f); }
    else if (!strcmp(s->anim, "Bounce") && s->af >= 15) en_play(s, "Idle");
}

/* ---- the fire pot (customgrids.lua firepotAI): lights when a player is within 100, burns the ones flying into it */
static void pg_firepot(en_t *s, unsigned long e) {
    const float *p = fpos(e); unsigned long pl = pg_game_player();
    int near = 0, touch = 0;
    if (pl) {
        const float *pp = fpos(pl); float d = vlen(pp[0] - p[0], pp[1] - p[1]);
        near = d <= 100.0f; touch = d <= *(const float *) (pl + 0x344) + 24.0f;
        if (near && s->age >= 30 && touch && owner_flies(pl) && !(s->cd2 > 0)) { hit_enemy_f(pl, 1.0f, 0, e); s->cd2 = 30; }
    }
    if (s->cd2 > 0) s->cd2--;
    if (!strcmp(s->anim, "Extinguish") && s->af >= 6) en_play(s, "Idle");
    else if (!strcmp(s->anim, "Ignite") && s->af >= 6) en_play(s, "Burning");
    if (near && s->age >= 30) { if (!strcmp(s->anim, "Idle") || !strcmp(s->anim, "Extinguish")) { en_sound(FFSND_Lighter, 1.3f, 1.0f); en_play(s, "Ignite"); } }
    else if (!strcmp(s->anim, "Burning") || !strcmp(s->anim, "Ignite")) { en_sound(FFSND_WingFlap, 0.5f, 1.0f); en_play(s, "Extinguish"); }
}

/* ---- the soot tag (dead/grids/soottag.lua): it catches fire from any fire within 50 and sets the barrels next to it off */
static int pg_fire_near(const float *p, float r) {
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long v = snap[k]; if (!alive(v)) continue; int t = *(const int *) (v + E_TYPE), var = *(const int *) (v + E_VARIANT);
        int fire = (t == 33) || (t == 1000 && (var == EV_HOT_BOMB_FIRE || var == 52 || var == 7005));
        if (!fire) continue; const float *vp = fpos(v); if (vlen(vp[0] - p[0], vp[1] - p[1]) < r) return 1;
    }
    return 0;
}
static void pg_soottag(en_t *s, unsigned long e) {
    const float *p = fpos(e); float pos[2] = { p[0], p[1] };
    if (s->state == 0 && s->age > 1 && pg_fire_near(pos, 50.0f)) {
        s->state = 1; s->cd2 = 0; en_play(s, "Bombed");
        unsigned long f = en_spawn(1000, 7005, pos[0], pos[1], 0, 0, 0, 0); if (f) en_set_timeout(f, 100);
        unsigned long c = en_spawn(1000, 18, pos[0], pos[1], 0, 0, 0, 0); if (c) en_scale(c, 0.5f, 0.5f);
    }
    if (s->state == 1) {
        s->cd2++;
        en_paint(e, 1, 1, 1, 0.1f, 0, 0, 0, 0, 0, 0, 0);
        if (s->cd2 == 5) {                                                                        /* blow up the barrels around it (all but the Water TNT) */
            unsigned long snap[SNAP]; unsigned n = snapshot(snap);
            for (int a = 90; a <= 360; a += 90) {
                float ox, oy; vrot(40.0f, 0, (float) a, &ox, &oy);
                for (unsigned k = 0; k < n; k++) {
                    unsigned long b = snap[k]; if (!alive(b) || *(const int *) (b + E_TYPE) != 292 || *(const int *) (b + E_VARIANT) == G7_TNT_WATER) continue;
                    const float *bp = fpos(b); if (vlen(bp[0] - (pos[0] + ox), bp[1] - (pos[1] + oy)) >= 10.0f) continue;
                    en_t *bs = en_slot(b); if (bs && bs->state == 0) g7_break(bs, b);
                }
            }
        }
    }
}

/* ---- the AI and the hurt hook */
static void ai_pgrid(en_t *s, unsigned long e) {
    const pg_def_t *d = pg_def(s->type, s->variant); if (!d) return;
    float *p = fpos(e), *v = fvel(e);
    if (!s->init) {
        s->init = 1; s->state = 0; s->f[10] = p[0]; s->f[11] = p[1];
        en_flag_clear(e, EF_FLAG_APPEAR);
        en_set_gridcoll(e, GRIDCOLL_NONE);
        en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK | EF_FLAG_NO_STATUS_EFFECTS | EF_FLAG_NO_TARGET);
        if (d->kind == PG_POOP) pg_poop_frame(s, e, 0);
        else if (d->kind == PG_ROCK) en_play(s, "Idle");
        else if (s->variant == 1016) en_play(s, "Idle");
        else if (s->variant == 1331) en_play(s, "Idle");
        if (d->kind == PG_FX) en_set_collision(e, s->variant == 1016 ? ENTCOLL_ALL : ENTCOLL_NONE);
        ENLOG(334, 1, "ffenemy: not done: the custom grids' pathfinding cost / room-clear behaviour as grids, the Dirty Mind dips and the thrown (lifted) versions of the poops");
    }
    s->age++;
    p[0] = s->f[10]; p[1] = s->f[11]; v[0] = v[1] = 0;
    if (d->kind == PG_POOP) {
        if (s->state == 0) { if (g_setframe && s->i[0] > 0) pg_poop_frame(s, e, s->i[0]); if (s->variant == 1027 && s->age % 60 == 0) { /* Shimmer */ } }
        else if (g_setframe) g_setframe((void *) (e + E_SPRITE), "State5", 4);
        if (s->state == 0 && s->type == 183 && s->variant == 1026) {                              /* the cursed poop frightens the players within 80 */
            unsigned long pl = pg_game_player();
            if (pl) { const float *pp = fpos(pl); if (vlen(pp[0] - p[0], pp[1] - p[1]) < 80.0f && g_add_fear) g_add_fear(pl, g7_ref(e), 3); }
        }
    } else if (d->kind == PG_ROCK) {
        if (s->state == 0 && s->variant == 1025) pg_rubber(s, e);
        if (s->state == 1 && g_setframe) g_setframe((void *) (e + E_SPRITE), "rubble_alt", 3);
    } else if (s->variant == 1016) pg_firepot(s, e);
    else if (s->variant == 1331) pg_soottag(s, e);
}
static void hurt_pgrid(en_t *s, unsigned long e, float dmg) {
    const pg_def_t *d = pg_def(s->type, s->variant); s->hurt_block = 1; (void) dmg;
    if (!d || s->state == 1 || d->kind == PG_FX) return;
    int boom = (g_hit_flags & (DMG_EXPLOSION | DMG_TNT)) != 0;
    if (d->kind == PG_ROCK) { if (boom) pg_destroy(s, e); return; }
    if (g_hit_flags & DMG_FIRE) return;
    if (boom) { s->i[0] = 4; pg_destroy(s, e); return; }
    s->i[0]++;
    if (s->i[0] >= 4) pg_destroy(s, e); else pg_poop_frame(s, e, s->i[0]);
}

static const enemy_t g_tab_g8[] = {
    { 184, 1031, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 184, 1032, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 184, 1020, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 },
    { 184, 1050, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 184, 1051, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 184, 1052, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 },
    { 184, 1037, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 },
    { 183, 1026, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 183, 1027, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 183, 1028, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 },
    { 183, 1036, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 183, 1025, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 183, 1038, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 },
    { 183, 1016, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 }, { 183, 1331, ai_pgrid, 0, hurt_pgrid, 0, 0, 0 },
};
#define N_TAB_G8 ((int) (sizeof g_tab_g8 / sizeof g_tab_g8[0]))
