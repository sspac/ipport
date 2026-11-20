/* ffen_g7.h - Fiend Folio's grid-like ENTITIES, part A: Super TNT 292.750, Water TNT 292.751 (xalum/grids/tnts.lua) and the six Compost Bins 292.752.0-5
 * (xalum/grids/composters.lua). They are pushable-TNT entities (type 292) the room files place directly; the Lua turns the engine's own damage into its own:
 * every hit is cancelled, a hit that is big enough (or an explosion / TNT hit, or a dead entity) breaks the thing and runs its behaviour once.
 * Included by ffenemy.h after ffen_g6e.h.
 *
 * s->state 0 standing / 1 broken (the Lua's data.dontDoAnything); s->cd2 = frames since it broke (the Lua's Isaac.CreateTimer delays). */

#define DMG_TNT 4096ull
#define G7_TNT_SUPER 750
#define G7_TNT_WATER 751
#define G7_COMPOST 752

/* the TNT / compost debris: six wood particles 1000.27 that wear the bin's own sheet and play GibN */
static const char *g7_skin(int variant, int sub) {
    if (variant == G7_TNT_SUPER) return "gfx/grid/super_tnt.anm2";
    if (variant == G7_TNT_WATER) return "gfx/grid/water_tnt.anm2";
    switch (sub) {
    case 1: return "gfx/grid/composter/composter_poison.anm2";
    case 2: return "gfx/grid/composter/composter_charm.anm2";
    case 3: return "gfx/grid/composter/composter_unstable.anm2";
    case 4: return "gfx/grid/composter/composter_bruise.anm2";
    case 5: return "gfx/grid/composter/composter_excommunicate.anm2";
    default: return "gfx/grid/composter/composter_knockback.anm2";
    }
}
static void g7_common_break(en_t *s, unsigned long e) {
    const float *p = fpos(e); char nm[8];
    for (int i = 0; i < 6; i++) {
        float vx, vy; en_rand_vec(&vx, &vy, (float) rint_(5, 30) / 10.0f);
        unsigned long pt = en_spawn(1000, EV_WOOD_PARTICLE, p[0], p[1], vx, vy, e, 0);
        if (pt && g_load) { snprintf(nm, sizeof nm, "Gib%d", rint_(1, 4)); g_load((void *) (pt + E_SPRITE), g7_skin(s->variant, s->sub)); if (g_play) g_play((void *) (pt + E_SPRITE), nm, 1); }
    }
    s->state = 1; s->cd2 = 0;                                                                    /* data.dontDoAnything */
    en_play(s, "Blown"); en_flag_set(e, EF_FLAG_RENDER_FLOOR);
}

/* ------------------------------------------------------------------------------------------------------ the engine's own status effects (measured in isaac.arm64)
 * Entity::AddPoison 0x82f294 (this, EntityRef*, int duration, float damage), AddCharmed 0x82f5c4 (this, ref, duration), AddConfusion 0x82f914 (this, ref, duration, bool ignoreBoss),
 * Game::Fart 0x894398 (game, Vector*, source, subtype, Color*, s0 radius, s1 scale: it poisons 200 frames and hits for 5 every monster in range), Game::CharmFart 0x89465c (game, Vector*, source, s0 radius: charm 150).
 * They are set by ffminion.c; where the pointer is 0 (the PC tests set mocks) nothing happens and the calls are counted. */
static void (*g_add_poison)(unsigned long, const void *, int, float);
static void (*g_add_charm)(unsigned long, const void *, int);
static void (*g_add_conf)(unsigned long, const void *, int, int);
static void (*g_add_fear)(unsigned long, const void *, int);                                      /* Entity::AddFear 0x82fc08 (this, ref, duration) */
static void (*g_game_fart)(unsigned long, const float *, unsigned long, int, const float *, float, float);
static void (*g_game_charmfart)(unsigned long, const float *, unsigned long, float);
static int en_stat_enemy(unsigned long v) {                                                    /* Entity:IsEnemy for the loops below: a monster that counts, not a friend */
    if (!alive(v)) return 0;
    int t = *(const int *) (v + E_TYPE); if (t < 10 || t >= 1000 || !*(const int *) (v + OFF_ENT_COLLCLASS)) return 0;
    return !en_flag(v, EF_FLAG_FRIENDLY);
}
static const void *g7_ref(unsigned long src) {                                                  /* an EntityRef whose source is src (the Lua's EntityRef(entity)) */
    static unsigned char ref[0x100]; memset(ref, 0, sizeof ref); if (src) *(unsigned long *) (ref + 0x28) = src; return ref;
}
static void en_add_confusion(unsigned long v, unsigned long src, int dur) { if (g_add_conf) g_add_conf(v, g7_ref(src), dur, 0); }
static void en_add_poison(unsigned long v, unsigned long src, int dur, float dmg) { if (g_add_poison) g_add_poison(v, g7_ref(src), dur, dmg); }

/* ---- Bruise (statuses/bruise.lua): stacks of {duration, damage}; every hit adds the sum of the 5 biggest stack damages; the monster is tinted by its stack count */
#define BR_N 24
#define BR_STACKS 16
static struct { unsigned long e; int n; int dur[BR_STACKS]; float dmg[BR_STACKS]; } g_bruise[BR_N];
static void en_bruise_add(unsigned long v, int dur, int stacks, float dmg) {
    if (!alive(v) || en_flag(v, EF_FLAG_NO_STATUS_EFFECTS)) return;                              /* HasEntityFlags(FLAG_NO_STATUS_EFFECTS) */
    int k, free = -1; for (k = 0; k < BR_N; k++) { if (g_bruise[k].e == v) break; if (free < 0 && (!g_bruise[k].e || !alive(g_bruise[k].e))) free = k; }
    if (k == BR_N) { if (free < 0) return; k = free; memset(&g_bruise[k], 0, sizeof g_bruise[k]); g_bruise[k].e = v; }
    for (int i = 0; i < stacks && g_bruise[k].n < BR_STACKS; i++) { g_bruise[k].dur[g_bruise[k].n] = dur; g_bruise[k].dmg[g_bruise[k].n] = dmg; g_bruise[k].n++; }
}
static float en_bruise_extra(unsigned long v, unsigned long long flags) {                          /* statuses/bruise.lua bruiseOnDamage */
    if (flags & 256ull) return 0.0f;                                                              /* DAMAGE_CLONES */
    for (int k = 0; k < BR_N; k++) if (g_bruise[k].e == v && g_bruise[k].n > 0) {
        float top[5] = { 0, 0, 0, 0, 0 }; int nt = 0;
        for (int i = 0; i < g_bruise[k].n; i++) {
            float d = g_bruise[k].dmg[i]; int j = 0;
            while (j < nt && top[j] > d) j++;
            if (j >= 5) continue;
            for (int q = (nt < 5 ? nt : 4); q > j; q--) top[q] = top[q - 1];
            top[j] = d; if (nt < 5) nt++;
        }
        float sum = 0; for (int j = 0; j < nt; j++) sum += top[j]; return sum;
    }
    return 0.0f;
}
static void en_bruise_step(void) {
    static const float col[5][7] = { { 0.70f, 0.70f, 0.70f }, { 0.65f, 0.60f, 0.65f }, { 0.60f, 0.50f, 0.60f }, { 0.55f, 0.40f, 0.55f }, { 0.50f, 0.30f, 0.50f } };
    for (int k = 0; k < BR_N; k++) {
        unsigned long v = g_bruise[k].e; if (!v) continue;
        if (!alive(v) || en_dead(v)) { g_bruise[k].e = 0; continue; }
        int n = g_bruise[k].n; if (n > 5) n = 5;
        if (n > 0) en_paint(v, col[n - 1][0], col[n - 1][1], col[n - 1][2], 1.0f, 0, 0, 0, 0, 40.0f / 255.0f, 0.0f, 40.0f / 255.0f);
        for (int i = g_bruise[k].n - 1; i >= 0; i--) if (--g_bruise[k].dur[i] <= 0) {
            for (int j = i; j < g_bruise[k].n - 1; j++) { g_bruise[k].dur[j] = g_bruise[k].dur[j + 1]; g_bruise[k].dmg[j] = g_bruise[k].dmg[j + 1]; }
            g_bruise[k].n--;
        }
        if (g_bruise[k].n == 0) g_bruise[k].e = 0;
    }
}

/* ---- Excommunicated (statuses/excommunicated.lua): the monster is frozen where it stands under a target mark; in the last 8 frames it squashes up, is hit at the 3rd frame of
 * the way down (plus its neighbours for 2.5x), and goes back. A monster that cannot take it (boss with resistance...) is not handled: every monster here takes it whole. */
#define EX_N 16
typedef struct { unsigned long e, target; int dur, maxdur, up, down, started, shorted, anim_age; float dmg, ox, oy, osx, osy, px, py; unsigned long src; int coll; } ex_t;
static ex_t g_ex[EX_N];
static const float EX_UP[8][5] = { { 0, 8, 0.90f, 1.10f, 0 }, { 0, 8, 0.90f, 1.10f, 1 }, { 0, -24, 1.40f, 0.60f, 0 }, { 0, -24, 1.40f, 0.60f, 1 }, { 0, -24, 1.80f, 0.50f, 0 }, { 0, 26, 0.50f, 2.20f, 1 }, { 0, 26, 0.30f, 3.00f, 0 }, { 0, 30, 0.10f, 8.00f, 1 } };
static const float EX_DOWN[8][5] = { { 0, 30, 0.10f, 8.00f, 1 }, { 0, 26, 0.30f, 3.00f, 0 }, { 0, 26, 0.50f, 2.20f, 1 }, { 0, -24, 1.80f, 0.50f, 0 }, { 0, -24, 1.40f, 0.60f, 1 }, { 0, -24, 1.40f, 0.60f, 0 }, { 0, 8, 0.90f, 1.10f, 1 }, { 0, 8, 0.90f, 1.10f, 0 } };
static int en_ex_remaining(int *exc) {                                                             /* the Lua's remainingEntities / excommunicatedEntities: active, door-shutting, non-friendly monsters that are not excommunicated */
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); int rem = 0, ex = 0;
    for (int k = 0; k < EX_N; k++) if (g_ex[k].e && alive(g_ex[k].e)) ex++;
    for (unsigned k = 0; k < n; k++) { unsigned long v = snap[k]; if (!en_stat_enemy(v)) continue; if (*(const int *) (v + OFF_ENT_COLLCLASS) < 2 || *(const unsigned char *) (v + OFF_NPC_CANSHUTDOORS)) continue; rem++; }
    if (exc) *exc = ex; return rem;
}
static int en_ex_busy(unsigned long v) { for (int k = 0; k < EX_N; k++) if (g_ex[k].e == v && alive(v)) return 1; return 0; }
static void en_excommunicate(unsigned long v, unsigned long src, int dur, float dmg) {                 /* FiendFolio.AddExcommunicated(entity, source, duration, damage, nil, true, true) */
    if (!en_stat_enemy(v) || en_flag(v, EF_FLAG_NO_STATUS_EFFECTS)) return;
    { int exc = 0, rem = en_ex_remaining(&exc); if (!(rem > 1 || exc > 0)) return; }                /* isPartialExcommunicationOnAdd with ignorePartial: the last monster standing is left alone */
    for (int k = 0; k < EX_N; k++) if (g_ex[k].e == v && alive(v)) { if (g_ex[k].dur > 8) { if (dur > g_ex[k].dur) g_ex[k].dur = dur; if (dur > g_ex[k].maxdur) g_ex[k].maxdur = dur; if (dmg > g_ex[k].dmg) g_ex[k].dmg = dmg; } return; }
    for (int k = 0; k < EX_N; k++) if (!g_ex[k].e || !alive(g_ex[k].e)) {
        memset(&g_ex[k], 0, sizeof g_ex[k]); g_ex[k].e = v; g_ex[k].dur = g_ex[k].maxdur = dur; g_ex[k].dmg = dmg; g_ex[k].src = src;
        const float *so = (const float *) (v + OFF_ENT_SPRITEOFFSET); g_ex[k].ox = so[0]; g_ex[k].oy = so[1];
        const float *sc = (const float *) (v + E_SPRITE + 0xdc); g_ex[k].osx = sc[0]; g_ex[k].osy = sc[1]; g_ex[k].coll = *(const int *) (v + OFF_ENT_COLLCLASS);
        return;
    }
}
static void en_ex_damage(ex_t *x) {                                                              /* mod:doExcommunicatedDamage (not partial) */
    unsigned long v = x->e; const float *p = fpos(v); float R = *(const float *) (v + 0x344) + 20.0f; if (R < 80.0f) R = 80.0f;
    unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) { unsigned long o = snap[k]; if (o == v || !alive(o)) continue; { int ot = *(const int *) (o + E_TYPE); if (ot < 10 || ot >= 1000) continue; } const float *op = fpos(o); if (vlen(op[0] - p[0], op[1] - p[1]) < R) hit_enemy_f(o, x->dmg * 2.5f, 0, x->src); }
    hit_enemy_f(v, x->dmg, 0, x->src);
    en_sound(SOUND_BLACK_POOF, 0.9f, 2.0f + frand() * 0.1f);
    unsigned long poof = en_spawn(1000, 16, p[0], p[1], 0, 0, 0, 1);
    if (poof) { float sc = *(const float *) (v + 0x344) / 40.0f + 0.2f; en_scale(poof, sc, sc); en_paint(poof, 1, 1, 1, 0.5f, 0, 0, 0, 0, 0, 0, 0); }
}
static void en_ex_finish(ex_t *x) {
    unsigned long v = x->e;
    if (alive(v)) {
        float *so = (float *) (v + OFF_ENT_SPRITEOFFSET); so[0] = x->ox; so[1] = x->oy; en_scale(v, x->osx, x->osy);
        *(int *) (v + OFF_ENT_COLLCLASS) = x->coll; *(unsigned char *) (v + E_VISIBLE) = 1;
        en_flag_clear(v, EF_FLAG_FREEZE | EF_FLAG_NO_TARGET);
    }
    if (x->target && alive(x->target)) en_remove(x->target);
    x->e = 0;
}
static void en_ex_step(void) {
    for (int k = 0; k < EX_N; k++) {
        ex_t *x = &g_ex[k]; unsigned long v = x->e; if (!v) continue;
        if (!alive(v)) { en_ex_finish(x); continue; }
        x->dur--;
        if (x->dur >= 0 && (x->started || !en_dead(v))) {
            x->started = 1; float *p = fpos(v);
            if (!x->px && !x->py) { x->px = p[0]; x->py = p[1]; }
            p[0] = x->px; p[1] = x->py; float *vv = fvel(v); vv[0] = vv[1] = 0;
            if (!x->target || !alive(x->target)) { x->target = en_spawn(1000, 1756, p[0], p[1], 0, 0, v, 0); if (x->target) en_paint(x->target, 1, 1, 1, 0.75f, 0, 0, 0, 0, 0, 0, 0); }
            if (x->dur > 30 && en_ex_remaining(0) == 0) { x->dur = 30; x->shorted = 1; }              /* FiendFolio.CheckIfShouldExcommunicate(false): nothing else is left to wait for */
            if (x->up == 0) en_sound(SOUND_HELL_PORTAL1, 1.0f, 1.0f); else if (x->dur <= 8 && x->down == 0) en_sound(SOUND_HELL_PORTAL2, 1.0f, 1.0f);
            *(int *) (v + OFF_ENT_COLLCLASS) = 0;
            if (x->dur == 0 || x->down >= 8) { en_ex_finish(x); continue; }
            const float *fr = 0; int tel = 0;
            if (x->dur <= 8 && x->down < 8) { fr = EX_DOWN[x->down]; tel = 1; if (x->down == 2) en_ex_damage(x); x->down++; }
            else if (x->up < 8) { fr = EX_UP[x->up]; x->up++; }
            if (fr) {
                float *so = (float *) (v + OFF_ENT_SPRITEOFFSET); so[0] = x->ox + fr[0]; so[1] = x->oy + fr[1]; en_scale(v, x->osx * fr[2], x->osy * fr[3]);
                if (fr[4] > 0.5f) en_paint(v, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1); else en_paint(v, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);
                *(unsigned char *) (v + E_VISIBLE) = 1; (void) tel;
            } else *(unsigned char *) (v + E_VISIBLE) = 0;
            if (x->dur == 10) { unsigned long b = en_spawn(1000, 1757, p[0], p[1], 0, 0, 0, 0); if (b && g_play) g_play((void *) (b + E_SPRITE), "Spotlight", 1); }
            en_flag_set(v, EF_FLAG_FREEZE | EF_FLAG_NO_TARGET);
            if (x->target && alive(x->target) && g_setframe) {                                   /* the countdown ring: 08..78 Blink by the share elapsed, then Warning Blink for the last 15 frames */
                const char *an = "78 Blink"; float pf = 1.0f - (float) (x->dur - 15) / (float) (x->maxdur - 15 > 0 ? x->maxdur - 15 : 1);
                if (pf < 0.125f) an = "08 Blink"; else if (pf < 0.25f) an = "18 Blink"; else if (pf < 0.375f) an = "28 Blink"; else if (pf < 0.5f) an = "38 Blink";
                else if (pf < 0.625f) an = "48 Blink"; else if (pf < 0.75f) an = "58 Blink"; else if (pf < 0.875f) an = "68 Blink";
                if (x->dur <= 15 || x->shorted) an = "Warning Blink";
                g_setframe((void *) (x->target + E_SPRITE), an, x->anim_age++);
                if (x->down >= 3) *(unsigned char *) (x->target + E_VISIBLE) = 0;
            }
        } else en_ex_finish(x);
    }
}
static void en_g7_tick(void) { en_bruise_step(); en_ex_step(); }

/* ---- the farts: Game:Fart / CharmFart / ButterBeanFart as the compost bins call them */
static float g7_white[11] = { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static void g7_fart_push(unsigned long e, float radius, float push, int confuse) {                              /* the pushing half of the knockback bin: Velocity + (enemy - bin):Resized(15) for what is not NO_PHYSICS_KNOCKBACK / NO_KNOCKBACK */
    const float *p = fpos(e); unsigned long snap[SNAP]; unsigned n = snapshot(snap);
    for (unsigned k = 0; k < n; k++) {
        unsigned long v = snap[k]; if (v == e || !en_stat_enemy(v)) continue;
        const float *vp = fpos(v); float dx = vp[0] - p[0], dy = vp[1] - p[1], len = vlen(dx, dy);
        if (len >= radius) continue;
        float dist = 80.0f - len;
        if (confuse && !en_flag(v, EF_FLAG_CONFUSION)) en_add_confusion(v, e, (int) ceilf(30.0f + dist));       /* enemy:AddConfusion(EntityRef(entity), ceil(30 + distance), false) */
        if (en_flag(v, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK)) continue;
        float *vv = fvel(v); vresize(&dx, &dy, push); vv[0] += dx; vv[1] += dy;
    }
}
static unsigned long g7_cloud(unsigned long e, float tr, float tg, float tb, float orr, float og, float ob, int tint) {
    const float *p = fpos(e); unsigned long c = en_spawn(1000, EV_FART, p[0], p[1], 0, 0, e, 0);
    if (c && tint) en_paint(c, tr, tg, tb, 1.0f, 0, 0, 0, 0, orr, og, ob);
    return c;
}
static void g7_butter_fart(unsigned long e) {                                                        /* Game:ButterBeanFart(pos, r, entity, showEffect, false): its cloud and sound; its own push of everything is the one above */
    g7_cloud(e, 0, 0, 0, 0, 0, 0, 0); en_sound(SOUND_FART, 1.0f, 1.0f);
    ENLOG(320, 1, "ffenemy: not done: Game:ButterBeanFart's own knockback (its function is not found): approximated by a push of 15 within 120");
}
static void g7_compost_behaviour(en_t *s, unsigned long e) {
    const float *p = fpos(e); unsigned long game = en_game(); float pos[2] = { p[0], p[1] };
    switch (s->sub) {
    default: case 0: case 3:                                                                         /* knockback (a bin that is not a thrown unstable one) */
        g7_butter_fart(e); g7_fart_push(e, 120.0f, 15.0f, 1); break;
    case 1:                                                                                          /* Game:Fart(pos, 80, entity, 1, 0) + mod:FakeFart */
        if (g_game_fart) g_game_fart(game, pos, e, 0, g7_white, 80.0f, 1.0f);
        else { g7_cloud(e, 0, 0, 0, 0, 0, 0, 0); en_sound(SOUND_FART, 1.0f, 1.0f); }
        { unsigned long pl = g_first_player ? g_first_player() : 0; if (pl && !en_flag(e, EF_FLAG_FRIENDLY) && vlen(fpos(pl)[0] - p[0], fpos(pl)[1] - p[1]) < 40.0f + *(const float *) (pl + 0x344) * 0.0f) hit_enemy_f(pl, 1.0f, 0, e); }
        break;
    case 2:                                                                                          /* Game:CharmFart(pos, 120, entity) */
        if (g_game_charmfart) g_game_charmfart(game, pos, e, 120.0f); else { g7_cloud(e, 0, 0, 0, 0, 0, 0, 0); en_sound(SOUND_FART, 1.0f, 1.0f); }
        break;
    case 4: {                                                                                        /* bruise: every monster within its size + 120 gets a 120-frame stack of 1 damage */
        unsigned long snap[SNAP]; unsigned n = snapshot(snap);
        for (unsigned k = 0; k < n; k++) { unsigned long v = snap[k]; if (!en_stat_enemy(v)) continue; const float *vp = fpos(v);
            if (vlen(vp[0] - p[0], vp[1] - p[1]) < *(const float *) (v + 0x344) + 120.0f) en_bruise_add(v, 120, 1, 1.0f); }
        g7_fart_push(e, 120.0f, 15.0f, 0); en_sound(SOUND_FART, 1.0f, 1.0f); g7_cloud(e, 0.4f, 0.4f, 0.4f, 40.0f / 255.0f, 0, 40.0f / 255.0f, 1);
        break; }
    case 5: {                                                                                        /* holy: if the room is not clear every monster within its size + 120 is excommunicated: 90 frames, 40 damage */
        if (!en_room_clear()) {
            unsigned long snap[SNAP]; unsigned n = snapshot(snap);
            for (unsigned k = 0; k < n; k++) { unsigned long v = snap[k]; if (!en_stat_enemy(v)) continue; const float *vp = fpos(v);
                if (vlen(vp[0] - p[0], vp[1] - p[1]) < *(const float *) (v + 0x344) + 120.0f) en_excommunicate(v, e, 90, 40.0f); }
        }
        g7_fart_push(e, 120.0f, 15.0f, 0); en_sound(SOUND_FART, 1.0f, 1.0f); g7_cloud(e, 1.3f, 1.3f, 1.3f, 0, 30.0f / 255.0f, 45.0f / 255.0f, 1); break; }
    }
}

/* the Water TNT's 7019 effect: it puts out what burns within 80 (the Lua does it on the effect's first update) */
static int g7_extinguish_one(unsigned long v, unsigned long src) {
    int t = *(const int *) (v + E_TYPE), var = *(const int *) (v + E_VARIANT); float mh = *(const float *) (v + 0x358) / 5.0f; unsigned long pl = g_first_player ? g_first_player() : 0; (void) src;
    if (t == 10 && var == 2) { unsigned long n = en_morph(v, 10, 1, 0); if (n) hit_enemy_f(n, mh, 0, pl); return 1; }          /* Flaming Gaper */
    if (t == 54 && var == 0) { unsigned long n = en_morph(v, 29, 0, 0); if (n) hit_enemy_f(n, mh, 0, pl); return 1; }          /* Flaming Hopper */
    if (t == 208 && var == 2) { unsigned long n = en_morph(v, 208, 0, 0); if (n) hit_enemy_f(n, mh, 0, pl); return 1; }       /* Flaming Fatty */
    if (t == 226 && var == 2) { unsigned long n = en_morph(v, 226, 0, 0); if (n) hit_enemy_f(n, mh, 0, pl); return 1; }       /* Crispy */
    if (t == 33 && var <= 1) { hit_enemy_f(v, 99999.0f, 0, 0); return 0; }                                                    /* (Red) Fireplace */
    if (t == 1000 && (var == 51 || var == 52)) { en_remove(v); return 1; }                                                    /* Fire / Red Flame */
    if (t == 1000 && var == 26 && (*(const int *) (v + E_SUBTYPE) == 7000 || *(const int *) (v + E_SUBTYPE) == 7001)) { en_remove(v); return 0; }   /* gunpowder */
    if (t == 1000 && var == 7005) { en_remove(v); return 1; }                                                                 /* gunpowder fire */
    if (t == 1000 && var == 7015) { en_remove(v); return 0; }                                                                 /* Fiend Fire */
    if ((t == 61 && (var == 960 || var == 961)) || (t == 208 && var == 963) || (t == 240 && var == 700) || (t == 151 && var == 5) || (t == 160 && (var == 280 || var == 310 || var == 350 || var == 351 || var == 430 || var == 431))
        || (t == 750 && var == 110) || (t == 114 && var == 8)) {                                                                 /* Spitroast, Spitfire, Big Smoke, Fried, S'More, Flare, Woodburner, Fumegeist, Mote, Charlie, Sooty, Wick, Crucible */
        en_t *os = en_slot(v); if (os) { os->flaming = 0; }
        hit_enemy_f(v, mh, 0, pl); return 1;
    }
    return 0;
}
static void g7_water_break(en_t *s, unsigned long e) {
    const float *p = fpos(e);
    { unsigned long ef = en_spawn(1000, 7019, p[0], p[1], 0, 0, 0, 0); if (ef) en_set_timeout(ef, 19); }                  /* the Lua removes it once "Explosion" (19 frames) is finished */
    en_sound(FFSND_Waterboom, 2.0f, 1.0f);
    unsigned long cr = en_spawn(1000, 32, p[0], p[1] + 10.0f, 0, 0, e, 0);
    if (cr) { en_paint(cr, 1, 1, 1, 1, 160.0f / 255.0f, 190.0f / 255.0f, 230.0f / 255.0f, 1.0f, 0, 0, 0); en_set_timeout(cr, 90); }
    for (float a = 22.5f; a <= 360.0f; a += 22.5f) { float vx, vy; vrot(9.0f, 0, a, &vx, &vy); en_spawn(2, 0, p[0], p[1], vx, vy, e, 0); }
    unsigned long snap[SNAP]; unsigned n = snapshot(snap); int hit = 0;
    for (unsigned k = 0; k < n; k++) {
        unsigned long v = snap[k]; if (!alive(v) || v == e) continue;
        const float *vp = fpos(v); if (vlen(vp[0] - p[0], vp[1] - p[1]) >= 80.0f + *(const float *) (v + 0x344)) continue;
        hit |= g7_extinguish_one(v, e);
    }
    if (hit) en_sound(SOUND_FIREDEATH_HISS, 1.0f, 1.5f);
    ENLOG(321, 1, "ffenemy: not done: Water TNT's QuickSetEntityGridPath(0), the Crispy's 'haha' sheet, the Flaming Gaper's Head overlay and the 'unignited' flag of the monsters it puts out");
}
static void g7_super_break(en_t *s, unsigned long e) {
    const float *p = fpos(e); (void) s;
    en_explode(e, p[0], p[1], 100.0f, 0.5f, 1);
}
static void g7_break(en_t *s, unsigned long e) {
    s->state = 1; s->cd2 = 0;                                  /* dontDoAnything first: the blast below hurts the bin itself and must not break it again */
    if (s->variant == G7_COMPOST) en_sound(SOUND_MUSHROOM_POOF, 2.0f, 1.5f);
    g7_common_break(s, e);
    if (s->variant == G7_COMPOST) g7_compost_behaviour(s, e);
    else if (s->variant == G7_TNT_WATER) g7_water_break(s, e);
    else g7_super_break(s, e);
}

static void ai_tnt(en_t *s, unsigned long e) {
    float *p = fpos(e), *v = fvel(e);
    if (!s->init) {
        s->init = 1; s->state = 0;
        if (s->variant == G7_TNT_SUPER || s->variant == G7_COMPOST) {
            en_set_gridcoll(e, GRIDCOLL_WALLS); en_flag_set(e, EF_FLAG_NO_PHYSICS_KNOCKBACK | EF_FLAG_NO_KNOCKBACK); v[0] = v[1] = 0;
        }
        if (en_playing(s, "Blown")) s->state = 1;                                                 /* it came back already blown */
        ENLOG(322, 1, "ffenemy: not done: the TNT / compost bin's pathfinding cost (QuickSetEntityGridPath 3999 / 900): monsters do not walk around it on purpose");
    }
    if (s->variant != G7_TNT_WATER) { v[0] = v[1] = 0; }
    if (s->state == 1) {
        s->cd2++;
        if (s->variant == G7_TNT_SUPER) {                                                          /* the Lua's two timers: 5 and 10 frames after the blast */
            if (s->cd2 == 5) for (int i = 1; i <= 4; i++) { float ox, oy; vrot(40.0f, 0, 90.0f * (float) i, &ox, &oy); en_explode(e, p[0] + ox, p[1] + oy, 100.0f, 0.5f, 1); }
            if (s->cd2 == 10) for (int i = 1; i <= 2; i++) { float ox, oy; vrot(80.0f, 0, 180.0f * (float) i, &ox, &oy); en_explode(e, p[0] + ox, p[1] + oy, 50.0f, 0.5f, 1); }
        }
    }
}
/* MC_ENTITY_TAKE_DMG: the damage never lands; the Lua keeps its own count (a hit counts for amount/15, between 1 and 3) */
static void hurt_tnt(en_t *s, unsigned long e, float dmg) {
    s->hurt_block = 1;
    if (s->state == 1) return;
    int real = (int) floorf(dmg / 15.0f); if (real < 1) real = 1; if (real > 3) real = 3;
    float hp = g_hp_off ? *(const float *) (e + g_hp_off) : 0.0f;
    if (hp - (float) real <= 1.0f || (g_hit_flags & (DMG_TNT | DMG_EXPLOSION)) || en_dead(e)) g7_break(s, e);
    else if (g_hp_off) *(float *) (e + g_hp_off) = hp - (float) real;
}

static const enemy_t g_tab_g7[] = {
    { 292, 750, ai_tnt, 0, hurt_tnt, 0, 0, 0 },
    { 292, 751, ai_tnt, 0, hurt_tnt, 0, 0, 0 },
    { 292, 752, ai_tnt, 0, hurt_tnt, 0, 0, 0 },
};
#define N_TAB_G7 ((int) (sizeof g_tab_g7 / sizeof g_tab_g7[0]))
