/*
 * Fiend Folio's Vending Machine, 6.1100 (ffscripts/erfly/slots/vendingmachine.lua) and the pickups / machines that go with it.
 *
 * Run the same way as ffen_g12.h (polled from en_g12_tick; the animation by the port's own frame counter, touching by the Lua's own test).
 * Subtype = (index in the Lua's item list << 8) | (price << 1) | one-use. The machine takes `price` coins, plays one coin animation per 99 / 50 / 10 / 5 / 1 cents
 * (the speed grows up to 3x with their number), then "Payout": at its Prize event the player USES the active item of its index (Player::UseActiveItem,
 * 0x500588 - the engine's own function, called with the arguments the engine's own card / item code passes: item, flags 0, show animation 1, slot 0, var 0). With
 * probability 1 - 0.8^uses (or at once, if the subtype says so) the next payout is "Payout_alt" and the machine blows up when it ends.
 * The 31 items the Lua flags Broken are not machines in the rooms (the Lua replaces them by nothing: tools/ff_rooms.py drops them).
 * The Fiend Folio machine 6.1101 holds only the mod's own items, which the Lua-less port does not have: its rooms get a vanilla machine of the same price and
 * one-use flag (tools/ff_rooms.py).
 *
 * Not done (each logs "ffslot: not done" once): the Curse of the Blind question mark on the item, the 1% pedestal of the mod's own Voyager Vendor item when it
 * dies, the Electrum / Beah hooks of the mod's own items, Stitches' innate while a Stitches machine is alive, the item's forced raise animation (UseActiveItem's
 * own animation is what shows).
 */
#include "ffvending_table.h"
static void (*g_use_active)(unsigned long pl, int item, int flags, int show, int slot, int var);      /* Player::UseActiveItem 0x500588 */
static int g_use_log[8], g_use_n;                                                                          /* the host tests read what a machine used */

#define SLV_VEND 1100
static float sd_float(sd_t *d) { return (float) (sd_next(d) >> 8) / 16777216.0f; }
static int vend_pay_anim(int rem) { return rem >= 99 ? 99 : rem >= 50 ? 50 : rem >= 10 ? 10 : rem >= 5 ? 5 : rem >= 1 ? 1 : 0; }
static int vend_pay_count(int price) { int n = 0; if (price <= 0) return 1; while (price > 0) { price -= vend_pay_anim(price); n++; } return n; }
static const char *vend_anim_name(int c) { return c == 99 ? "Initiate_99c" : c == 50 ? "Initiate_50c" : c == 10 ? "Initiate_10c" : c == 5 ? "Initiate_5c" : c == 1 ? "Initiate_1c" : "Initiate_none"; }
/* state: 0 idle, 1 paid (coin animations), 2 paidPost, 3 activate, 4 dead.  i[0] price, i[1] item index, i[2] coins still to animate, i[3] speed, i[4] player */
static int vend_touch(unsigned long e, unsigned long pl) {
    const float *p = fpos(e), *q = fpos(pl); float pz = *(const float *) (pl + E_SIZE);
    return fabsf(q[0] - p[0]) <= 9.0f * 2.0f + pz + 2.0f && fabsf(q[1] - p[1]) <= 9.0f + pz + 2.0f;
}
static unsigned long vend_toucher(unsigned long e) {
    int n = sl_nplayers(); for (int i = 0; i < n; i++) { unsigned long pl = sl_player(i); if (pl && vend_touch(e, pl)) return pl; } return 0;
}
static void sl_vend(en_t *s, unsigned long e) {
    sd_t *d = sd_get(e); float *p = fpos(e); int sub = *(const int *) (e + E_SUBTYPE);
    if (!s->init) {
        s->init = 1; s->age = 0; s->state = 0; s->i[0] = (sub % 256) / 2; s->i[1] = sub / 256;
        if (s->i[1] >= VEND_VAN_N) s->i[1] = 0;
        if (sub % 2 == 1) d->a[0] = 1;                                   /* data.oneUse = data.oneUse or (SubType % 2 == 1) */
        if (g_sheet_l) {
            void *sp = (void *) (e + E_SPRITE); char b[160];
            if (sub % 2 == 1) { static const int L[5] = { 0, 3, 4, 5, 7 }; for (int k = 0; k < 5; k++) g_sheet_l(sp, L[k], "gfx/items/slots/vending machine/slot_vending_machine_singleuse.png"); }
            snprintf(b, sizeof b, "gfx/items/slots/vending machine/slot_vending_numbers_%d.png", s->i[0] / 10); g_sheet_l(sp, 1, b);
            snprintf(b, sizeof b, "gfx/items/slots/vending machine/slot_vending_numbers_%d.png", s->i[0] % 10); g_sheet_l(sp, 2, b);
            g_sheet_l(sp, 6, g_vend_van[s->i[1]].gfx);
        }
        SL_NOTDONE("Vending Machine: Curse of the Blind's question mark on the item, the 1% Voyager Vendor pedestal (a mod item), Stitches' innate while one is alive, the item's forced raise animation");
    }
    if (s->state != 4 && sl_exploded(e)) { s->state = 4; en_replay(s, "Death"); }
    if (s->state == 0) {
        en_play(s, "Idle");
        unsigned long pl = vend_toucher(e);
        if (pl && pl_coins(pl) >= s->i[0]) {
            pl_add_coins(pl, -s->i[0]); s->state = 1; s->i[2] = s->i[0]; s->i[3] = vend_pay_count(s->i[0]) < 3 ? vend_pay_count(s->i[0]) : 3;
            s->ref = pl; en_sound(SOUND_COIN_SLOT, 1.0f, 1.0f); en_replay(s, vend_anim_name(vend_pay_anim(s->i[2])));
        }
    } else if (s->state == 1) {
        const char *cur = vend_anim_name(vend_pay_anim(s->i[2]));
        s->af += s->i[3] - 1;                                            /* sprite.PlaybackSpeed = min(3, #paymentAnims) */
        if (en_finished(s, cur)) {
            s->i[2] -= vend_pay_anim(s->i[2]);
            if (s->i[2] > 0) en_replay(s, vend_anim_name(vend_pay_anim(s->i[2]))); else { s->state = 2; en_replay(s, "Initiate_post"); }
        } else en_play(s, cur);
    } else if (s->state == 2) {
        if (en_finished(s, "Initiate_post")) {
            if (!d->a[0]) {
                d->a[1]++;                                               /* OverallUses (starts at -1: the first use is 0) */
                if (sd_float(d) > powf(0.8f, (float) (d->a[1] - 1))) d->a[0] = 1;
            }
            s->state = 3; en_replay(s, d->a[0] ? "Payout_alt" : "Payout");
        } else en_play(s, "Initiate_post");
    } else if (s->state == 3) {
        const char *an = d->a[0] ? "Payout_alt" : "Payout";
        if (en_finished(s, "Payout")) { s->state = 0; en_replay(s, "Idle"); }
        else if (en_finished(s, "Payout_alt")) {
            s->state = 4; en_replay(s, "Death"); en_spawn(1000, 1, p[0], p[1], 0, 0, e, 0);          /* TakeDamage(explosion) + the explosion effect: the machine dies */
        } else if (en_event(s, an, "Pulse")) en_sound(FFSND_FlashZap, 1.0f, sd_range(d, 50, 150) / 100.0f);
        else if (en_event(s, an, "Prize")) {
            unsigned long pl = s->ref; int n = sl_nplayers(), ok = 0;
            for (int i = 0; i < n; i++) if (sl_player(i) == pl) ok = 1;
            if (!ok) pl = sl_player(0);
            en_sound(SOUND_THUMBSUP, 1.0f, 1.0f);
            if (pl) {
                if (g_use_active) g_use_active(pl, g_vend_van[s->i[1]].item, 0, 1, 0, 0);
                if (g_use_n < 8) g_use_log[g_use_n++] = g_vend_van[s->i[1]].item;
            }
        } else en_play(s, an);
    } else if (s->state == 4) {
        if (!strcmp(s->anim, "Death") && !en_finished(s, "Death")) en_play(s, "Death"); else en_play(s, "Broken");   /* Death, then Broken for good */
    }
}
