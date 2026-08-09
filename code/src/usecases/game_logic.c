#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "usecases/game_logic.h"
#include "usecases/collision.h"
#include "usecases/difficulty.h"
#include "usecases/ship.h"
#include "usecases/spawner.h"
#include "domain/constants.h"

static float frand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static bool within_radius(float ax, float ay, float bx, float by, float r) {
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy <= r * r;
}

/* The player's laser is always this exact color for the whole run, never
 * rerolled or varied by score, mode, or anything else - every one of
 * B-20's 5 modes reads pr->color from gs->player.laser_color (see
 * spawn_player_shot), so a single fixed value here is what keeps every
 * mode's shots visually identical and predictable across an entire run. */
static const Color kDefaultLaserColor = {255, 240, 120, 255};

/* Every spatial constant in domain/constants.h is tuned at DESIGN_W x
 * DESIGN_H. Multiplying by gs->scale (uniform in x and y) carries that
 * same proportion onto whatever the real screen measures, so shapes grow
 * or shrink together instead of stretching. */
static float scaled(const GameState *gs, float design_value) {
    return design_value * gs->scale;
}

/* The hitbox a given player shot should be tested against: PROJECTILE_KIND_POWER
 * gets a bigger round hitbox matching its bigger sprite, and a horizontal
 * (side-beam) shot has its width/height swapped so the hitbox stays aligned
 * with its direction of travel instead of the vertical bolt's shape. Shared
 * by check_collisions (enemy/boss/orb hit tests) and update_projectiles (the
 * off-screen despawn margin) so the two can never drift apart. */
static void player_shot_half_extents(const GameState *gs, const Projectile *pr, float *half_w, float *half_h) {
    /* Every one of C-24's shots is a sphere regardless of which mode fired
     * it (see draw_c24_sphere_shot in adapters/sdl_renderer.c) - checked
     * before ProjectileKind below so a C-24 power-cannon-style shot (still
     * PROJECTILE_KIND_POWER, for its damage multiplier and explode-on-
     * contact behavior - see check_collisions) hit-tests at C-24's own
     * sphere size instead of B-20's POWER_CANNON_PROJECTILE_RADIUS - just
     * C-24's own *bigger* sphere (SHIP_C24_POWER_MODE_RADIUS, 8x its other
     * two modes' SHIP_C24_PROJECTILE_RADIUS), matching how much heavier
     * this shot already is. Keyed off the shot's own style_ship, not
     * gs->selected_ship directly, so a C-24-kind ChildShip's own shots
     * still hit-test correctly while selected_ship is SHIP_MOTHERSHIP. */
    if (pr->style_ship == SHIP_C24) {
        float r = scaled(gs, pr->kind == PROJECTILE_KIND_POWER ? SHIP_C24_POWER_MODE_RADIUS : SHIP_C24_PROJECTILE_RADIUS);
        *half_w = r;
        *half_h = r;
        return;
    }
    if (pr->kind == PROJECTILE_KIND_POWER) {
        float r = scaled(gs, POWER_CANNON_PROJECTILE_RADIUS);
        *half_w = r;
        *half_h = r;
        return;
    }
    float w = scaled(gs, PLAYER_PROJECTILE_W) / 2.0f;
    float h = scaled(gs, PLAYER_PROJECTILE_H) / 2.0f;
    if (pr->horizontal) {
        *half_w = h;
        *half_h = w;
    } else {
        *half_w = w;
        *half_h = h;
    }
}

/* The hitbox for a given enemy shot: ORB-shaped shots (triburst/omni) get a
 * round hitbox sized by their own radius (half_len, reused for both axes);
 * BEAM-shaped shots (thin/long/trishot) get the exact bounding box of their
 * own oriented rectangle - half_len along the travel direction, half_wid
 * across it - so an angled trishot beam's hitbox actually lines up with
 * what's drawn instead of assuming it's always vertical. Shared by
 * check_collisions, update_projectiles (off-screen despawn margin) and
 * update_super_beam (enemy shot sweep) so the three can never drift apart -
 * the enemy-shot counterpart to player_shot_half_extents above. */
static void enemy_shot_half_extents(const Projectile *pr, float *half_w, float *half_h) {
    if (pr->enemy_kind == ENEMY_PROJECTILE_ORB) {
        *half_w = pr->half_len;
        *half_h = pr->half_len;
        return;
    }
    float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
    float dx = speed > 0.0f ? pr->vx / speed : 0.0f;
    float dy = speed > 0.0f ? pr->vy / speed : 1.0f;
    *half_w = pr->half_len * fabsf(dx) + pr->half_wid * fabsf(dy);
    *half_h = pr->half_len * fabsf(dy) + pr->half_wid * fabsf(dx);
}

static void init_stars(GameState *gs) {
    for (int i = 0; i < MAX_STARS; i++) {
        Star *s = &gs->stars[i];
        s->x = frand01() * (float)gs->screen_w;
        s->y = frand01() * (float)gs->screen_h;
        s->speed = scaled(gs, STAR_MIN_SPEED + frand01() * STAR_SPEED_RANGE);
        /* Wide spread, completely at random, so some stars read as barely-
         * there and others as sharply bright rather than a narrow band of
         * similar shine. */
        s->brightness = (unsigned char)(25 + rand() % 231);
    }
}

static void update_stars(GameState *gs, float dt) {
    for (int i = 0; i < MAX_STARS; i++) {
        Star *s = &gs->stars[i];
        s->y += s->speed * dt;
        if (s->y > (float)gs->screen_h) {
            s->y = 0.0f;
            s->x = frand01() * (float)gs->screen_w;
            s->brightness = (unsigned char)(25 + rand() % 231);
        }
    }
}

static void randomize_background_cloud(GameState *gs, BackgroundCloud *c) {
    c->x = frand01() * (float)gs->screen_w;
    c->radius = scaled(gs, BACKGROUND_CLOUD_MIN_RADIUS + frand01() * (BACKGROUND_CLOUD_MAX_RADIUS - BACKGROUND_CLOUD_MIN_RADIUS));
    c->speed = scaled(gs, BACKGROUND_CLOUD_MIN_SPEED + frand01() * (BACKGROUND_CLOUD_MAX_SPEED - BACKGROUND_CLOUD_MIN_SPEED));
    c->wobble_seed = frand01() * 6.2831853f;
    c->wobble_speed = BACKGROUND_CLOUD_WOBBLE_MIN_SPEED + frand01() * (BACKGROUND_CLOUD_WOBBLE_MAX_SPEED - BACKGROUND_CLOUD_WOBBLE_MIN_SPEED);
    c->wobble_amplitude = scaled(gs, BACKGROUND_CLOUD_WOBBLE_MIN_AMPLITUDE +
                                          frand01() * (BACKGROUND_CLOUD_WOBBLE_MAX_AMPLITUDE - BACKGROUND_CLOUD_WOBBLE_MIN_AMPLITUDE));
}

/* The background smoke's own drift counterpart to init_stars - scattered
 * across the whole screen height at startup (rather than all starting
 * above it) so the effect is already in view on the very first frame,
 * same reasoning as init_stars' own y placement. */
static void init_background_clouds(GameState *gs) {
    for (int i = 0; i < MAX_BACKGROUND_CLOUDS; i++) {
        BackgroundCloud *c = &gs->background_clouds[i];
        randomize_background_cloud(gs, c);
        c->y = frand01() * (float)gs->screen_h;
    }
}

/* Drifts every cloud straight down at its own slow, fixed speed - "flowing
 * top to bottom" - and once one has fully cleared the bottom edge (its own
 * radius included, so it doesn't visibly pop out of existence mid-fade),
 * wraps it back above the screen with every field freshly rolled, the same
 * wrap-and-reroll convention update_stars uses for its own y. The side-to-
 * side wobble isn't integrated here at all - see BackgroundCloud's own doc
 * comment for why it's instead derived purely from time_elapsed in the
 * renderer. */
static void update_background_clouds(GameState *gs, float dt) {
    for (int i = 0; i < MAX_BACKGROUND_CLOUDS; i++) {
        BackgroundCloud *c = &gs->background_clouds[i];
        c->y += c->speed * dt;
        if (c->y - c->radius > (float)gs->screen_h) {
            randomize_background_cloud(gs, c);
            c->y = -c->radius;
        }
    }
}

static void spawn_explosion(GameState *gs, float x, float y, float max_radius) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        Explosion *e = &gs->explosions[i];
        if (e->alive) continue;
        e->alive = true;
        e->x = x;
        e->y = y;
        e->age = 0.0f;
        e->max_age = EXPLOSION_DURATION;
        e->max_radius = max_radius;
        return;
    }
}

/* One puff of engine exhaust at (x, y) - see update_player_trail for the
 * steady emission that calls this, and TrailParticle in domain/types.h for
 * what each field drives. Drifts backward (away from the ship's nose) with
 * a little random sideways wobble so the stream reads as flickering
 * exhaust rather than a rigid line. */
static void spawn_trail_particle(GameState *gs, float x, float y) {
    for (int i = 0; i < MAX_TRAIL_PARTICLES; i++) {
        TrailParticle *t = &gs->trail_particles[i];
        if (t->alive) continue;
        t->alive = true;
        t->x = x;
        t->y = y;
        t->vx = (frand01() - 0.5f) * scaled(gs, TRAIL_PARTICLE_JITTER_SPEED);
        t->vy = scaled(gs, TRAIL_PARTICLE_SPEED) * (0.7f + frand01() * 0.6f);
        t->age = 0.0f;
        t->max_age = TRAIL_PARTICLE_LIFETIME;
        t->size = scaled(gs, TRAIL_PARTICLE_BASE_SIZE) * (0.7f + frand01() * 0.6f);
        return;
    }
}

/* The enemy/boss counterpart to spawn_trail_particle above, into the
 * separate enemy_trail_particles pool (see EnemyTrailParticle in
 * domain/types.h for why it's kept apart from the player's own). Unlike
 * the player, neither enemies nor the boss rotate to face their direction
 * of travel (their sprites are always drawn upright - see draw_sprite), so
 * "backward" is a fixed upward drift/offset here too, just mirrored
 * vertically from the player's downward one, matching their fixed-down
 * visual orientation regardless of which way they're actually moving.
 * size_mult and alpha_cap let a single shared routine serve both ordinary
 * enemies (1x size, ~5% visible) and the bigger, brighter boss (3x size,
 * ~15% visible, see update_enemy_and_boss_trails). (x, y) should already
 * be the emitter's own back (its top edge), not its center. */
static void spawn_enemy_trail_particle(GameState *gs, float x, float y, float size_mult, unsigned char alpha_cap) {
    for (int i = 0; i < MAX_ENEMY_TRAIL_PARTICLES; i++) {
        EnemyTrailParticle *t = &gs->enemy_trail_particles[i];
        if (t->alive) continue;
        t->alive = true;
        t->x = x;
        t->y = y;
        t->vx = (frand01() - 0.5f) * scaled(gs, TRAIL_PARTICLE_JITTER_SPEED);
        t->vy = -scaled(gs, TRAIL_PARTICLE_SPEED) * (0.7f + frand01() * 0.6f);
        t->age = 0.0f;
        t->max_age = TRAIL_PARTICLE_LIFETIME;
        t->size = scaled(gs, TRAIL_PARTICLE_BASE_SIZE) * size_mult * (0.7f + frand01() * 0.6f);
        t->alpha_cap = alpha_cap;
        return;
    }
}

/* The projectile counterpart to spawn_trail_particle/spawn_enemy_trail_particle
 * above, into the shared projectile_trails pool - see ProjectileTrailParticle
 * in domain/types.h for why its color never shifts the way the ship trails'
 * does. (back_dx, back_dy) is the unit vector pointing opposite the
 * projectile's own travel direction (its "backward"), so the puff drifts
 * away from the direction the shot is heading regardless of which way that
 * is - unlike the ship trails, a projectile can travel in any direction,
 * not just down or up. */
static void spawn_projectile_trail_particle(GameState *gs, float x, float y, float back_dx, float back_dy,
                                             Color color) {
    for (int i = 0; i < MAX_PROJECTILE_TRAIL_PARTICLES; i++) {
        ProjectileTrailParticle *t = &gs->projectile_trails[i];
        if (t->alive) continue;
        t->alive = true;
        t->x = x;
        t->y = y;
        float perp_x = -back_dy, perp_y = back_dx;
        float jitter = (frand01() - 0.5f) * scaled(gs, TRAIL_PARTICLE_JITTER_SPEED);
        float drift = scaled(gs, TRAIL_PARTICLE_SPEED) * (0.7f + frand01() * 0.6f);
        t->vx = back_dx * drift + perp_x * jitter;
        t->vy = back_dy * drift + perp_y * jitter;
        t->age = 0.0f;
        t->max_age = PROJECTILE_TRAIL_LIFETIME;
        t->size = scaled(gs, PROJECTILE_TRAIL_BASE_SIZE) * (0.7f + frand01() * 0.6f);
        t->color = color;
        return;
    }
}

/* Shared by every shooting mode's fire logic (see update_player_firing and
 * its per-mode helpers, and update_child_firing for a ChildShip's own
 * fire): claims the first free slot in gs->player_shots and fills it in.
 * Silently does nothing once the pool (MAX_PLAYER_PROJECTILES) is
 * exhausted, same as the original inline spawn it replaces. style_ship
 * tags which ship's rendering/behavior style this shot uses (see
 * Projectile.style_ship in domain/types.h) - always SHIP_B20 or SHIP_C24,
 * whether that's from the real player's own selected_ship or a child's own
 * kind. */
static void spawn_player_shot_styled(GameState *gs, float x, float y, float vx, float vy,
                                      ProjectileKind kind, bool horizontal, float damage,
                                      Ship style_ship) {
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        Projectile *pr = &gs->player_shots[i];
        if (pr->alive) continue;
        pr->alive = true;
        pr->x = x;
        pr->y = y;
        pr->vx = vx;
        pr->vy = vy;
        pr->color = gs->player.laser_color;
        pr->kind = kind;
        pr->horizontal = horizontal;
        pr->damage = damage;
        pr->inert = false;
        pr->inert_age = 0.0f;
        /* Staggered like Enemy.trail_emit_timer at spawn, so a burst of
         * shots fired the same frame doesn't all puff their first smoke
         * trail particle on the exact same frame too. */
        pr->trail_emit_timer = frand01() * PROJECTILE_TRAIL_SPAWN_INTERVAL;
        /* Only read by C-24's own sphere-shot rendering (see
         * draw_c24_sphere_shot in adapters/sdl_renderer.c) - harmless to
         * set unconditionally for every ship. */
        pr->phase_seed = frand01() * 6.2831853f;
        pr->style_ship = style_ship;
        return;
    }
}

/* The real player's own fire routines all still call this - a thin wrapper
 * defaulting style_ship to gs->selected_ship, so none of their 9 call
 * sites need to change. Only update_child_firing calls
 * spawn_player_shot_styled directly, tagging a child's own kind instead. */
static void spawn_player_shot(GameState *gs, float x, float y, float vx, float vy,
                               ProjectileKind kind, bool horizontal, float damage) {
    spawn_player_shot_styled(gs, x, y, vx, vy, kind, horizontal, damage, gs->selected_ship);
}

/* The enemy-shot counterpart to spawn_player_shot above: claims the first
 * free slot in gs->enemy_shots and fills it in, including resetting
 * inert/inert_age (a reused slot may have been left inert - and fully
 * faded - by a past boss fight; see
 * test_enemy_projectile_slot_reset_after_boss_fade). half_len/half_wid
 * should already be scaled by the caller, same convention vx/vy follow. */
static void spawn_enemy_shot(GameState *gs, float x, float y, float vx, float vy,
                              EnemyProjectileKind enemy_kind, float half_len, float half_wid,
                              Color color) {
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        Projectile *pr = &gs->enemy_shots[i];
        if (pr->alive) continue;
        pr->alive = true;
        pr->x = x;
        pr->y = y;
        pr->vx = vx;
        pr->vy = vy;
        pr->color = color;
        pr->enemy_kind = enemy_kind;
        pr->half_len = half_len;
        pr->half_wid = half_wid;
        pr->inert = false;
        pr->inert_age = 0.0f;
        pr->trail_emit_timer = frand01() * PROJECTILE_TRAIL_SPAWN_INTERVAL;
        return;
    }
}

/* The 8 unit direction vectors an all-directions burst fires along, evenly
 * spaced like the points of an octagon (N/NE/E/SE/S/SW/W/NW in screen
 * space, where +y is down) - written out rather than computed with
 * sinf/cosf since all 8 land on exact multiples of 45 degrees. Shared by
 * ENEMY_SHOOT_OMNI (fire_enemy_shot_style below) and C-24's own
 * SHOOT_MODE_OMNI (update_omni_burst below) - the player's version is
 * explicitly the same pattern, just centered on the player instead of an
 * enemy. */
static const float kOmniDirX[ENEMY_OMNI_SHOT_COUNT] = {
    0.0f, 0.70710678f, 1.0f, 0.70710678f, 0.0f, -0.70710678f, -1.0f, -0.70710678f,
};
static const float kOmniDirY[ENEMY_OMNI_SHOT_COUNT] = {
    1.0f, 0.70710678f, 0.0f, -0.70710678f, -1.0f, -0.70710678f, 0.0f, 0.70710678f,
};

static void spawn_orb(GameState *gs) {
    Orb *o = &gs->orb;
    o->alive = true;
    o->size = scaled(gs, ORB_SIZE);
    o->x = o->size * 0.5f + frand01() * ((float)gs->screen_w - o->size);
    o->y = -o->size;
    o->hue = frand01() * 360.0f;
    o->wobble_phase = frand01() * 6.2831853f;
    o->color = color_from_hsv(o->hue, 0.9f, 1.0f);
}

/* Called right after gs->score changes. Every ORB_SCORE_STEP crossed has a
 * coin-flip chance of dropping a new orb, but only if the last one has
 * already been resolved (captured, shot, or fallen off the bottom). */
static void maybe_trigger_orb_spawn(GameState *gs, int old_score, int new_score) {
    if (gs->orb.alive) return;
    if (new_score / ORB_SCORE_STEP <= old_score / ORB_SCORE_STEP) return;
    if (frand01() < ORB_SPAWN_CHANCE) spawn_orb(gs);
}

/* Existing enemies don't fight the boss - they bolt. Speeding up their
 * existing downward motion lets the normal off-screen cleanup in
 * update_enemies remove them without any extra per-enemy state. Their
 * projectiles keep flying but go inert (see update_projectiles) so they
 * can no longer land a hit while they fade out. */
static void flee_enemies_for_boss(GameState *gs) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;
        e->vy *= ENEMY_FLEE_SPEED_MULTIPLIER;
        e->fire_timer = 9999.0f;
    }
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        Projectile *pr = &gs->enemy_shots[i];
        if (!pr->alive) continue;
        pr->inert = true;
        pr->inert_age = 0.0f;
    }
}

/* The boss presents as "a randomly picked enemy" (same shape/color pool
 * spawner.c draws from for ordinary enemies) at BOSS_SIZE_MULTIPLIER the
 * size. Each appearance is a fresh pick, so - as requested - every boss
 * looks like a different monster even though they all behave the same. */
static void spawn_boss(GameState *gs, EventQueue *events) {
    flee_enemies_for_boss(gs);

    Boss *b = &gs->boss;
    gs->boss_count++;
    /* It has served its purpose bringing this boss in; nothing may
     * accumulate again until the encounter ends (see apply_score_delta,
     * which stops counting entirely while a boss is alive). */
    gs->score_since_last_boss = 0;

    float min_size = BOSS_BASE_MIN_SIZE * gs->scale;
    float max_size = BOSS_BASE_MAX_SIZE * gs->scale;
    float size = (min_size + frand01() * (max_size - min_size)) * BOSS_SIZE_MULTIPLIER;

    b->alive = true;
    b->size = size;
    b->x = (float)gs->screen_w / 2.0f;
    b->y = -size;
    b->kind = spawner_random_boss_kind();
    b->hits_taken = 0;
    b->hits_required = BOSS_HITS_INCREMENT * gs->boss_count;
    b->beam_contact_timer = 0.0f;
    b->trail_emit_timer = 0.0f;

    event_queue_push_sfx(events, SFX_BOSS_ARRIVED);
}

/* Deterministic (unlike the orb's coin flip): a boss appears once
 * gs->score_since_last_boss reaches BOSS_SCORE_STEP - a counter that is
 * zeroed the moment a boss encounter ends (see end_boss_encounter) and
 * only advances while no boss is on screen (see apply_score_delta), so
 * every boss takes a full fresh BOSS_SCORE_STEP of points to bring in,
 * never less, no matter how the previous one ended or what it paid out. */
static void maybe_trigger_boss_spawn(GameState *gs, EventQueue *events) {
    if (gs->boss.alive) return;
    if (gs->score_since_last_boss < BOSS_SCORE_STEP) return;
    spawn_boss(gs, events);
}

/* The single place gs->score is allowed to change, so every threshold
 * effect tied to score (laser recolor, orb drops, boss arrivals) reacts
 * identically no matter which gameplay path earned the points. */
static void apply_score_delta(GameState *gs, EventQueue *events, int delta) {
    int old_score = gs->score;
    gs->score += delta;
    /* Only points earned with the arena clear count toward bringing the
     * next boss in. Points scored during a fight - picking off enemies
     * still fleeing, and the defeat bonus itself (see damage_boss) - are
     * worth full score but must never shorten the gap to the next boss. */
    if (!gs->boss.alive) gs->score_since_last_boss += delta;

    maybe_trigger_orb_spawn(gs, old_score, gs->score);
    maybe_trigger_boss_spawn(gs, events);
}

/* The single place a boss leaves the screen, by either route it can go
 * (shot down here, or detonated by ring contact in check_collisions).
 * Restarting the counter at the END of an encounter - not at its start -
 * is what guarantees the full BOSS_SCORE_STEP gap before the next one:
 * from this instant every point has to be earned fresh, with the arena
 * clear. */
static void end_boss_encounter(GameState *gs) {
    gs->boss.alive = false;
    gs->score_since_last_boss = 0;
}

/* Shared by both ways the boss can take a hit (a direct laser shot, and
 * the super beam touching it) so the defeat/bonus/event logic can't drift
 * out of sync between the two. damage is in units of BASE_PLAYER_DAMAGE -
 * a direct shot passes its own Projectile.damage (which varies by shooting
 * mode, see the per-mode DAMAGE_MULTIPLIER constants), while the super
 * beam always passes a flat BASE_PLAYER_DAMAGE since it replaces every
 * mode's fire logic rather than being one itself. */
static void damage_boss(GameState *gs, EventQueue *events, float damage) {
    Boss *b = &gs->boss;
    b->hits_taken += damage;
    if (b->hits_taken >= (float)b->hits_required) {
        spawn_explosion(gs, b->x, b->y, b->size * 1.4f);
        int bonus = b->hits_required * BOSS_KILL_SCORE_MULTIPLIER;
        /* Order matters: the bonus is awarded while the boss still counts
         * as alive, so it lands in gs->score alone and is excluded from
         * score_since_last_boss (see apply_score_delta). Awarding it after
         * clearing the flag is what used to chain bosses back to back -
         * the bonus is hits_required * BOSS_KILL_SCORE_MULTIPLIER, which
         * from the third boss on (150 * 4 = 600) exceeds BOSS_SCORE_STEP
         * all by itself and so triggered the next arrival on the very
         * frame this one died. */
        apply_score_delta(gs, events, bonus);
        end_boss_encounter(gs);
        event_queue_push_sfx(events, SFX_BOSS_DEFEATED);
    } else {
        event_queue_push_sfx(events, SFX_BOSS_HIT);
    }
}

/* Shared by both ways an enemy can be destroyed for points (direct laser
 * hit, and the super beam sweeping over it) so every score-triggered
 * effect stays in sync between the two. */
static void destroy_enemy_for_score(GameState *gs, EventQueue *events, Enemy *e) {
    e->alive = false;
    spawn_explosion(gs, e->x, e->y, e->size);

    float mult = difficulty_score_multiplier(gs->score);
    apply_score_delta(gs, events, (int)((float)SCORE_PER_KILL * mult));

    event_queue_push_sfx(events, SFX_ENEMY_DESTROYED);
}

/* Mode 3 (power cannon): a shot explodes on contact, and every ordinary
 * enemy within POWER_CANNON_EXPLOSION_RADIUS_RATIO of the screen's shorter
 * dimension, centered on the contact point, explodes with it - mirroring
 * the orb's shot-to-detonate sweep (check_collisions) but instant rather
 * than staggered, and scoped to gs->enemies only (never the boss), same as
 * that sweep. destroy_enemy_for_score already spawns each enemy's own
 * explosion and awards score, so this only needs to add the single big
 * blast at the contact point on top of that. */
static void trigger_power_cannon_explosion(GameState *gs, EventQueue *events, float x, float y,
                                            Ship style_ship) {
    float radius = POWER_CANNON_EXPLOSION_RADIUS_RATIO * fminf((float)gs->screen_w, (float)gs->screen_h);
    spawn_explosion(gs, x, y, radius);

    /* The visual blast above stays B-20's own size for both ships - only
     * the enemies-caught-in-the-blast test below grows for C-24's mode 2
     * (see SHIP_C24_POWER_MODE_EXPLOSION_RADIUS_MULTIPLIER), gated on the
     * triggering shot's own style_ship (not gs->selected_ship - a
     * C-24-kind ChildShip's own power-cannon-reuse shot still needs this
     * bonus while selected_ship is SHIP_MOTHERSHIP) so B-20's mode 3 (and a
     * B-20-kind child's own mode 2) are byte-for-byte unaffected. */
    float damage_radius = radius;
    if (style_ship == SHIP_C24) {
        damage_radius *= SHIP_C24_POWER_MODE_EXPLOSION_RADIUS_MULTIPLIER;
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;
        if (!within_radius(x, y, e->x, e->y, damage_radius)) continue;
        destroy_enemy_for_score(gs, events, e);
    }
}

static void update_orb(GameState *gs, float dt) {
    Orb *o = &gs->orb;
    if (!o->alive) return;

    o->hue += ORB_HUE_CYCLE_SPEED * dt;
    if (o->hue >= 360.0f) o->hue -= 360.0f;
    o->color = color_from_hsv(o->hue, 0.9f, 1.0f);

    o->wobble_phase += dt * ORB_DRIFT_ANGULAR_SPEED;
    float drift = scaled(gs, ORB_DRIFT_SPEED);
    float fall = scaled(gs, ORB_FALL_SPEED);
    o->x += cosf(o->wobble_phase * 0.8f) * drift * dt;
    o->y += (fall + sinf(o->wobble_phase) * drift * 0.35f) * dt;

    float half = o->size / 2.0f;
    if (o->x < half) o->x = half;
    if (o->x > (float)gs->screen_w - half) o->x = (float)gs->screen_w - half;

    if (o->y - half > (float)gs->screen_h) {
        o->alive = false; /* fell off the bottom, unclaimed */
    }
}

/* A game of tag: the boss relentlessly closes in on the player's exact,
 * current position at a constant speed, every frame, with no idle state
 * and no waypoint it can "arrive at" and stop - if it ever isn't touching
 * the player, it is moving directly toward them. */
static void update_boss(GameState *gs, float dt) {
    Boss *b = &gs->boss;
    if (!b->alive) return;

    if (b->beam_contact_timer > 0.0f) b->beam_contact_timer -= dt;

    /* No boundary clamp: the boss always steps directly toward the
     * player's exact position, capped so it can't overshoot past it (see
     * below), and the player's own position is always kept on-screen by
     * update_player - so the boss's target is always valid and clamping
     * the boss separately only gets in the way (it used to cap the boss
     * short of a player standing near the bottom margin, making it look
     * like the boss had simply stopped chasing). */
    float speed = scaled(gs, PLAYER_SPEED) * BOSS_SPEED_MULTIPLIER;
    float dx = gs->player.x - b->x;
    float dy = gs->player.y - b->y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist > 0.0001f) {
        float step = speed * dt;
        if (step > dist) step = dist; /* don't overshoot past the player */
        b->x += dx / dist * step;
        b->y += dy / dist * step;
    }
}

static void reset_run(GameState *gs) {
    memset(&gs->children, 0, sizeof(gs->children));
    memset(&gs->enemies, 0, sizeof(gs->enemies));
    memset(&gs->player_shots, 0, sizeof(gs->player_shots));
    memset(&gs->enemy_shots, 0, sizeof(gs->enemy_shots));
    memset(&gs->explosions, 0, sizeof(gs->explosions));
    memset(&gs->trail_particles, 0, sizeof(gs->trail_particles));
    memset(&gs->enemy_trail_particles, 0, sizeof(gs->enemy_trail_particles));
    memset(&gs->projectile_trails, 0, sizeof(gs->projectile_trails));
    memset(&gs->orb, 0, sizeof(gs->orb));
    memset(&gs->boss, 0, sizeof(gs->boss));
    gs->boss_count = 0;
    gs->score_since_last_boss = 0;

    gs->player.x = (float)gs->screen_w / 2.0f;
    gs->player.y = (float)gs->screen_h - scaled(gs, PLAYER_BOTTOM_MARGIN);
    gs->player.alive = true;
    gs->player.fire_cooldown = 0.0f;
    gs->player.laser_color = kDefaultLaserColor;
    gs->player.super_beam_timer = 0.0f;
    gs->player.god_mode = false;
    gs->player.life = PLAYER_LIFE_MAX;
    gs->player.shoot_mode = ship_shoot_mode_for_slot(gs->selected_ship, 0);
    gs->player.rapid_burst_timer = 0.0f;
    gs->player.rapid_cooldown_timer = 0.0f;
    gs->player.trail_emit_timer = 0.0f;

    gs->score = 0;
    gs->time_elapsed = 0.0f;
    gs->spawn_timer = 0.5f;
    gs->pause_selection = PAUSE_RESUME;
    gs->state = STATE_GAME;
}

void game_init(GameState *gs, int screen_w, int screen_h) {
    memset(gs, 0, sizeof(*gs));
    gs->screen_w = screen_w;
    gs->screen_h = screen_h;

    float scale = (float)screen_h / (float)DESIGN_H;
    if (scale < 0.5f) scale = 0.5f;
    if (scale > 4.0f) scale = 4.0f;
    gs->scale = scale;

    gs->state = STATE_MENU;
    gs->selected_difficulty = DIFFICULTY_NORMAL;
    gs->selected_ship = SHIP_B20;
    init_stars(gs);
    init_background_clouds(gs);
}

static void handle_global_back(GameState *gs, const InputCommand *input, EventQueue *events) {
    if (!input->back_pressed) return;

    switch (gs->state) {
        case STATE_GAME:
            gs->state = STATE_PAUSE;
            gs->pause_selection = PAUSE_RESUME;
            event_queue_push_sfx(events, SFX_MENU_SELECT);
            break;
        case STATE_PAUSE:
            gs->state = STATE_GAME;
            event_queue_push_sfx(events, SFX_MENU_SELECT);
            break;
        case STATE_MENU:
            gs->quit_requested = true;
            break;
        case STATE_DIFFICULTY_SELECT:
            gs->state = STATE_MENU;
            event_queue_push_sfx(events, SFX_MENU_SELECT);
            break;
        case STATE_SHIP_SELECT:
            gs->state = STATE_DIFFICULTY_SELECT;
            event_queue_push_sfx(events, SFX_MENU_SELECT);
            break;
        case STATE_GAME_OVER:
            break;
    }
}

static void update_menu(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    gs->menu_blink_timer += dt;
    if (input->confirm_pressed) {
        event_queue_push_sfx(events, SFX_MENU_SELECT);
        gs->state = STATE_DIFFICULTY_SELECT;
    }
}

/* The difficulty-select screen reached right after confirming START GAME -
 * up/down moves the cursor (gs->selected_difficulty doubles as both the
 * cursor position and, once confirmed, the run's actual difficulty - same
 * "selection is the state" pattern as PauseSelection), clamped rather than
 * wrapping at the ends of the DIFFICULTY_BABY..DIFFICULTY_INSANE range.
 * Confirming moves on to the ship-select screen (update_ship_select below)
 * rather than starting the run directly - reset_run only fires once a ship
 * is confirmed too. */
static void update_difficulty_select(GameState *gs, const InputCommand *input, EventQueue *events) {
    if (input->nav_up_pressed && gs->selected_difficulty > 0) {
        gs->selected_difficulty--;
        event_queue_push_sfx(events, SFX_MENU_SELECT);
    }
    if (input->nav_down_pressed && gs->selected_difficulty < DIFFICULTY_COUNT - 1) {
        gs->selected_difficulty++;
        event_queue_push_sfx(events, SFX_MENU_SELECT);
    }
    if (input->confirm_pressed) {
        event_queue_push_sfx(events, SFX_MENU_SELECT);
        gs->state = STATE_SHIP_SELECT;
    }
}

/* The ship-select screen reached right after confirming a difficulty -
 * left/right moves the cursor across the row of unlocked ships
 * (gs->selected_ship doubles as both the cursor position and, once
 * confirmed, the run's actual ship, same "selection is the state" pattern
 * as gs->selected_difficulty above), clamped at SHIP_B20/the last
 * implemented ship rather than wrapping. Everything past SHIP_COUNT in the
 * ship-select grid is a locked placeholder the renderer draws directly
 * (adapters/sdl_renderer.c) - there's no cursor state for those slots to
 * land on. Confirming starts the run via reset_run, same as the old direct
 * STATE_DIFFICULTY_SELECT -> reset_run path this screen was inserted in
 * front of. */
static void update_ship_select(GameState *gs, const InputCommand *input, EventQueue *events) {
    if (input->nav_left_pressed && gs->selected_ship > 0) {
        gs->selected_ship--;
        event_queue_push_sfx(events, SFX_MENU_SELECT);
    }
    if (input->nav_right_pressed && gs->selected_ship < SHIP_COUNT - 1) {
        gs->selected_ship++;
        event_queue_push_sfx(events, SFX_MENU_SELECT);
    }
    if (input->confirm_pressed) {
        event_queue_push_sfx(events, SFX_MENU_SELECT);
        reset_run(gs);
    }
}

static void update_pause(GameState *gs, const InputCommand *input, EventQueue *events) {
    if (input->nav_up_pressed || input->nav_down_pressed) {
        gs->pause_selection = (gs->pause_selection == PAUSE_RESUME) ? PAUSE_EXIT : PAUSE_RESUME;
        event_queue_push_sfx(events, SFX_MENU_SELECT);
    }
    if (input->confirm_pressed) {
        event_queue_push_sfx(events, SFX_MENU_SELECT);
        gs->state = (gs->pause_selection == PAUSE_RESUME) ? STATE_GAME : STATE_MENU;
    }
}

static void update_game_over(GameState *gs, const InputCommand *input, EventQueue *events) {
    if (input->confirm_pressed) {
        event_queue_push_sfx(events, SFX_MENU_SELECT);
        gs->state = STATE_MENU;
    }
}

static void kill_player(GameState *gs, EventQueue *events) {
    if (!gs->player.alive) return;
    if (gs->player.super_beam_timer > 0.0f) return; /* invincible for the duration of the beam */
    if (gs->player.god_mode) return; /* invincible until Ctrl+G is pressed again */
    gs->player.alive = false;
    gs->player.life = 0.0f;
    spawn_explosion(gs, gs->player.x, gs->player.y, scaled(gs, PLAYER_WIDTH));
    event_queue_push_sfx(events, SFX_PLAYER_DESTROYED);
    gs->last_game_score = gs->score;
    gs->state = STATE_GAME_OVER;
}

/* An enemy projectile's hit, as opposed to ship/boss contact: it only
 * drains life instead of exploding the player outright, and only kills
 * once life is fully spent. Immunity (super beam, god mode) is checked
 * here too so a grazed-but-invincible player takes no life loss at all,
 * not just no death - kill_player re-checks the same conditions since it's
 * also reached directly by the always-fatal contact paths. */
static void damage_player(GameState *gs, EventQueue *events, float amount) {
    Player *p = &gs->player;
    if (!p->alive) return;
    if (p->super_beam_timer > 0.0f) return;
    if (p->god_mode) return;

    p->life -= amount * ship_damage_taken_multiplier(gs->selected_ship);
    if (p->life <= 0.0f) {
        p->life = 0.0f;
        kill_player(gs, events);
    }
}

/* Mode switching (1-5 number keys) is locked out for the whole duration of
 * a rapid-fire burst - the player is committed to that automatic volley
 * (see update_rapid_fire). Once the burst ends, update_rapid_fire itself
 * auto-switches away to slot 0 and starts the cooldown; from then on
 * switching is free again except back into mode 2 itself (SHOOT_MODE_RAPID),
 * which stays unselectable for the rest of the cooldown - everywhere else,
 * switching is free even mid-flight of other shots. */
/* Which key (1-5) selects which ShootMode is entirely per-ship (see
 * ship_shoot_mode_for_slot in usecases/ship.h) - a key past that ship's own
 * ship_shoot_mode_slot_count (e.g. 4 or 5 for C-24, which only has 3) does
 * nothing, same as pressing an already-active key does nothing. */
static void update_shoot_mode_switch(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (p->rapid_burst_timer > 0.0f) return;

    int slot = -1;
    if (input->shoot_mode_1_pressed) slot = 0;
    else if (input->shoot_mode_2_pressed) slot = 1;
    else if (input->shoot_mode_3_pressed) slot = 2;
    else if (input->shoot_mode_4_pressed) slot = 3;
    else if (input->shoot_mode_5_pressed) slot = 4;
    if (slot < 0 || slot >= ship_shoot_mode_slot_count(gs->selected_ship)) return;

    ShootMode requested = ship_shoot_mode_for_slot(gs->selected_ship, slot);
    if (requested == SHOOT_MODE_RAPID && p->rapid_cooldown_timer > 0.0f) return;
    if (requested == p->shoot_mode) return;
    p->shoot_mode = requested;
    event_queue_push_sfx(events, SFX_MENU_SELECT);
}

/* B-20's mode 1 (default): a single shot from the nose, unchanged from
 * before this ability existed. Not part of C-24's own moveset. */
static void update_normal_fire(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    spawn_player_shot(gs, p->x, p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f,
                       0.0f, -scaled(gs, PLAYER_PROJECTILE_SPEED), PROJECTILE_KIND_NORMAL, false,
                       BASE_PLAYER_DAMAGE);
    p->fire_cooldown = PLAYER_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* B-20's mode 2 (not part of C-24's own moveset): one press (or hold) commits to a full RAPID_FIRE_BURST_DURATION
 * seconds of automatic fire at RAPID_FIRE_SHOTS_PER_SEC, regardless of
 * whether the key is still held. The instant the burst ends, shoot_mode is
 * kicked back to slot 0 (B-20's normal shot) and a RAPID_FIRE_LOCKOUT_DURATION
 * cooldown starts - the player is free to fire and switch among every other
 * mode during that cooldown, just not back into mode 2 itself
 * (update_shoot_mode_switch enforces that one restriction). rapid_burst_timer
 * and rapid_cooldown_timer are mutually exclusive: exactly one is nonzero
 * while a burst is in flight or recovering, both are zero while idle. */
static void update_rapid_fire(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    Player *p = &gs->player;

    if (p->rapid_burst_timer > 0.0f) {
        p->rapid_burst_timer -= dt;
        if (p->rapid_burst_timer <= 0.0f) {
            p->rapid_burst_timer = 0.0f;
            p->rapid_cooldown_timer = RAPID_FIRE_LOCKOUT_DURATION;
            p->shoot_mode = ship_shoot_mode_for_slot(gs->selected_ship, 0);
            return;
        }
        if (p->fire_cooldown <= 0.0f) {
            spawn_player_shot(gs, p->x, p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f,
                               0.0f, -scaled(gs, PLAYER_PROJECTILE_SPEED), PROJECTILE_KIND_RAPID, false,
                               BASE_PLAYER_DAMAGE);
            p->fire_cooldown = RAPID_FIRE_SHOT_INTERVAL;
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
        }
        return;
    }

    /* rapid_cooldown_timer itself ticks down in update_player_firing,
     * unconditionally on dt regardless of shoot_mode - this function is
     * only ever reached while shoot_mode is still SHOOT_MODE_RAPID, which
     * the auto-switch above guarantees is never true while the cooldown
     * is running. */
    if (input->fire_held) {
        p->rapid_burst_timer = RAPID_FIRE_BURST_DURATION;
        p->fire_cooldown = 0.0f; /* fire the first shot of the burst immediately */
    }
}

/* B-20's mode 3, reused as C-24's own mode 2 at the exact same rate/damage
 * (see ship_shoot_mode_for_slot) - a slow, heavy single shot; see
 * trigger_power_cannon_explosion (check_collisions) for what happens when
 * it actually lands, unaffected by which ship fired it. C-24 renders and
 * hit-tests this one at its own bigger sphere size - see
 * player_shot_half_extents. */
static void update_power_cannon(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float speed = scaled(gs, PLAYER_PROJECTILE_SPEED) * POWER_CANNON_PROJECTILE_SPEED_MULTIPLIER;
    spawn_player_shot(gs, p->x, p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f,
                       0.0f, -speed, PROJECTILE_KIND_POWER, false,
                       BASE_PLAYER_DAMAGE * POWER_CANNON_DAMAGE_MULTIPLIER);
    p->fire_cooldown = POWER_CANNON_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* B-20's mode 4, reused as C-24's own mode 1 at the exact same rate/damage
 * (see ship_shoot_mode_for_slot): two shots fired abreast from the
 * wingtips instead of one from the nose. */
static void update_double_barrel(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float wing_x = scaled(gs, PLAYER_WING_OFFSET_X);
    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
    float vy = -scaled(gs, PLAYER_PROJECTILE_SPEED);
    float damage = BASE_PLAYER_DAMAGE * DOUBLE_BARREL_DAMAGE_MULTIPLIER;
    spawn_player_shot(gs, p->x - wing_x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, damage);
    spawn_player_shot(gs, p->x + wing_x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, damage);
    p->fire_cooldown = PLAYER_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* B-20's mode 5 (not part of C-24's own moveset): same wingtip pair as
 * mode 4, fired sideways (away from the ship) instead of forward. */
static void update_side_beams(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float wing_x = scaled(gs, PLAYER_WING_OFFSET_X);
    float speed = scaled(gs, PLAYER_PROJECTILE_SPEED);
    spawn_player_shot(gs, p->x - wing_x, p->y, -speed, 0.0f, PROJECTILE_KIND_NORMAL, true, BASE_PLAYER_DAMAGE);
    spawn_player_shot(gs, p->x + wing_x, p->y, speed, 0.0f, PROJECTILE_KIND_NORMAL, true, BASE_PLAYER_DAMAGE);
    p->fire_cooldown = PLAYER_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* C-24's own mode 3 (SHOOT_MODE_OMNI, not reachable by any other ship - see
 * ship_shoot_mode_for_slot): the same 8-direction, fire-all-at-once burst as
 * ENEMY_SHOOT_OMNI (see kOmniDirX/kOmniDirY above), from the player's own
 * position at the player's own baseline projectile speed/damage. Gated on a
 * single cooldown like every mode but rapid fire - "1 shot per second"
 * means the whole 8-pellet volley retriggers once a second, not that each
 * pellet fires individually. */
static void update_omni_burst(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float speed = scaled(gs, PLAYER_PROJECTILE_SPEED);
    for (int k = 0; k < ENEMY_OMNI_SHOT_COUNT; k++) {
        spawn_player_shot(gs, p->x, p->y, kOmniDirX[k] * speed, kOmniDirY[k] * speed,
                           PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE);
    }
    p->fire_cooldown = SHIP_C24_OMNI_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* A ChildShip's own weapon, driven by update_children every frame
 * regardless of its current movement AI/launch phase (firing and moving
 * are independent concerns) - a condensed mirror of
 * update_normal_fire/update_rapid_fire/update_power_cannon/
 * update_double_barrel/update_side_beams/update_omni_burst above, sourced
 * from the child's own x/y/kind/shoot_mode and writing into its own
 * fire_cooldown/rapid_burst_timer instead of the player's, always "firing
 * held" since a CPU escort never releases the trigger. Kept as an
 * independent copy rather than sharing code with the already-tested player
 * routines - same "kept-independent copies, not shared" precedent as
 * SHIP_C24_PROJECTILE_RADIUS elsewhere in this file. A SHOOT_MODE_RAPID
 * child (the rare 5% dispatch roll - see MOTHERSHIP_CHILD_RANDOM_MODE_CHANCE)
 * fires exactly one burst and then permanently falls back to mode #1 the
 * instant it ends, never bursting again - unlike the player's own mode 2,
 * there's no cooldown/re-arm cycle here at all. */
static void update_child_firing(GameState *gs, ChildShip *c, float dt, EventQueue *events) {
    if (c->fire_cooldown > 0.0f) c->fire_cooldown -= dt;

    float nose_y = c->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
    float speed = scaled(gs, PLAYER_PROJECTILE_SPEED);

    switch (c->shoot_mode) {
        case SHOOT_MODE_RAPID:
            if (c->rapid_burst_timer > 0.0f) {
                c->rapid_burst_timer -= dt;
                if (c->rapid_burst_timer <= 0.0f) {
                    c->rapid_burst_timer = 0.0f;
                    /* Permanently back to mode #1 - see this function's own
                     * doc comment. ship_shoot_mode_for_slot(c->kind, 0) is
                     * always B-20's mode #1 here since only a B-20-kind
                     * child's own moveset ever includes SHOOT_MODE_RAPID at
                     * all (see ship_shoot_mode_for_slot(SHIP_B20, 1) in
                     * usecases/ship.c). */
                    c->shoot_mode = ship_shoot_mode_for_slot(c->kind, 0);
                    break;
                }
                if (c->fire_cooldown <= 0.0f) {
                    spawn_player_shot_styled(gs, c->x, nose_y, 0.0f, -speed, PROJECTILE_KIND_RAPID, false,
                                              BASE_PLAYER_DAMAGE, c->kind);
                    c->fire_cooldown = RAPID_FIRE_SHOT_INTERVAL;
                    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
                }
                break;
            }
            /* Reached exactly once per SHOOT_MODE_RAPID child - the very
             * first update_child_firing call after dispatch, since every
             * later call either lands in the burst branch above or (once
             * the burst ends) shoot_mode has already moved on to mode #1,
             * so this case is never reached again at all. */
            c->rapid_burst_timer = RAPID_FIRE_BURST_DURATION;
            c->fire_cooldown = 0.0f;
            break;

        case SHOOT_MODE_POWER:
            if (c->fire_cooldown > 0.0f) break;
            {
                float pspeed = speed * POWER_CANNON_PROJECTILE_SPEED_MULTIPLIER;
                spawn_player_shot_styled(gs, c->x, nose_y, 0.0f, -pspeed, PROJECTILE_KIND_POWER, false,
                                          BASE_PLAYER_DAMAGE * POWER_CANNON_DAMAGE_MULTIPLIER, c->kind);
            }
            c->fire_cooldown = POWER_CANNON_FIRE_COOLDOWN;
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
            break;

        case SHOOT_MODE_DOUBLE:
            if (c->fire_cooldown > 0.0f) break;
            {
                float wing_x = scaled(gs, PLAYER_WING_OFFSET_X);
                float damage = BASE_PLAYER_DAMAGE * DOUBLE_BARREL_DAMAGE_MULTIPLIER;
                spawn_player_shot_styled(gs, c->x - wing_x, nose_y, 0.0f, -speed, PROJECTILE_KIND_NORMAL, false,
                                          damage, c->kind);
                spawn_player_shot_styled(gs, c->x + wing_x, nose_y, 0.0f, -speed, PROJECTILE_KIND_NORMAL, false,
                                          damage, c->kind);
            }
            c->fire_cooldown = PLAYER_FIRE_COOLDOWN;
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
            break;

        case SHOOT_MODE_SIDE:
            if (c->fire_cooldown > 0.0f) break;
            {
                float wing_x = scaled(gs, PLAYER_WING_OFFSET_X);
                spawn_player_shot_styled(gs, c->x - wing_x, c->y, -speed, 0.0f, PROJECTILE_KIND_NORMAL, true,
                                          BASE_PLAYER_DAMAGE, c->kind);
                spawn_player_shot_styled(gs, c->x + wing_x, c->y, speed, 0.0f, PROJECTILE_KIND_NORMAL, true,
                                          BASE_PLAYER_DAMAGE, c->kind);
            }
            c->fire_cooldown = PLAYER_FIRE_COOLDOWN;
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
            break;

        case SHOOT_MODE_OMNI:
            if (c->fire_cooldown > 0.0f) break;
            for (int k = 0; k < ENEMY_OMNI_SHOT_COUNT; k++) {
                spawn_player_shot_styled(gs, c->x, c->y, kOmniDirX[k] * speed, kOmniDirY[k] * speed,
                                          PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE, c->kind);
            }
            c->fire_cooldown = SHIP_C24_OMNI_FIRE_COOLDOWN;
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
            break;

        case SHOOT_MODE_NORMAL:
        case SHOOT_MODE_SWARM_WANDER:
        case SHOOT_MODE_SWARM_FORMATION:
        case SHOOT_MODE_COUNT:
        default:
            /* A child's own kind is always SHIP_B20 or SHIP_C24 (see
             * update_mothership_dispatch), so its rolled shoot_mode is
             * always one of the cases above - this default is only ever
             * B-20's own mode 1 (SHOOT_MODE_NORMAL) in practice; the
             * SWARM_* cases can't happen here at all (no child's own kind
             * ever has them in its moveset) and only appear so the switch
             * is exhaustive. */
            if (c->fire_cooldown > 0.0f) break;
            spawn_player_shot_styled(gs, c->x, nose_y, 0.0f, -speed, PROJECTILE_KIND_NORMAL, false,
                                      BASE_PLAYER_DAMAGE, c->kind);
            c->fire_cooldown = PLAYER_FIRE_COOLDOWN;
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
            break;
    }
}

/* Where in front of/beside the Mothership's *current* position the
 * SHOOT_MODE_SWARM_FORMATION slot for the alive_index-th currently-alive
 * child sits (see update_children) - a forward-pointing triangle: 0 is the
 * lead point straight ahead of her, 1/2 are the left/right flanks, both
 * closer to her than the lead point. Recomputed fresh every frame from
 * alive_index (not a persistent per-child identity), and only ever called
 * with alive_index in [0, MOTHERSHIP_MAX_CHILDREN) - the two are
 * intentionally coupled (a triangle has exactly 3 points), so retuning the
 * cap past 3 would need this reworked too. */
static void mothership_formation_slot(const GameState *gs, int alive_index, float *tx, float *ty) {
    const Player *p = &gs->player;
    float front = scaled(gs, MOTHERSHIP_CHILD_FORMATION_FRONT_OFFSET);
    float side_x = scaled(gs, MOTHERSHIP_CHILD_FORMATION_SIDE_OFFSET_X);
    float side_y = scaled(gs, MOTHERSHIP_CHILD_FORMATION_SIDE_OFFSET_Y);
    switch (alive_index) {
        case 0:
            *tx = p->x;
            *ty = p->y - front;
            break;
        case 1:
            *tx = p->x - side_x;
            *ty = p->y - side_y;
            break;
        default:
            *tx = p->x + side_x;
            *ty = p->y - side_y;
            break;
    }
}

/* Every alive ChildShip's own per-frame update: launch kick, then AI
 * movement (wander or formation, re-read fresh from
 * GameState.player.shoot_mode every frame - see the ShootMode enum's own
 * doc comment), clamped to the screen same as the real player, then its
 * own weapon fire. Called from update_running right after update_player
 * (which is what actually dispatches new children - see
 * update_mothership_dispatch) and before check_collisions, so a child
 * fired this frame is already in its final position before hit-testing. */
static void update_children(GameState *gs, float dt, EventQueue *events) {
    int alive_index = 0;
    for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
        ChildShip *c = &gs->children[i];
        if (!c->alive) continue;
        int my_alive_index = alive_index++;

        if (c->launch_timer > 0.0f) {
            c->launch_timer -= dt;
            c->x += c->vx * dt;
            c->y += c->vy * dt;
        } else if (gs->player.shoot_mode == SHOOT_MODE_SWARM_FORMATION) {
            float tx, ty;
            mothership_formation_slot(gs, my_alive_index, &tx, &ty);
            float dx = tx - c->x, dy = ty - c->y;
            float dist = sqrtf(dx * dx + dy * dy);
            float step = scaled(gs, MOTHERSHIP_CHILD_FORMATION_SPEED) * dt;
            if (dist > step) {
                c->x += dx / dist * step;
                c->y += dy / dist * step;
            } else {
                c->x = tx;
                c->y = ty;
            }
        } else { /* SHOOT_MODE_SWARM_WANDER */
            c->wander_retarget_timer -= dt;
            float dx0 = c->wander_target_x - c->x, dy0 = c->wander_target_y - c->y;
            float dist0 = sqrtf(dx0 * dx0 + dy0 * dy0);
            if (c->wander_retarget_timer <= 0.0f || dist0 <= scaled(gs, MOTHERSHIP_CHILD_WANDER_ARRIVE_RADIUS)) {
                float x_margin = (float)gs->screen_w * MOTHERSHIP_CHILD_WANDER_X_MARGIN_RATIO;
                float y_min = (float)gs->screen_h * MOTHERSHIP_CHILD_WANDER_Y_MIN_RATIO;
                float y_max = (float)gs->screen_h * MOTHERSHIP_CHILD_WANDER_Y_MAX_RATIO;
                c->wander_target_x = x_margin + frand01() * ((float)gs->screen_w - 2.0f * x_margin);
                c->wander_target_y = y_min + frand01() * (y_max - y_min);
                c->wander_retarget_timer = MOTHERSHIP_CHILD_WANDER_RETARGET_MIN +
                                            frand01() * (MOTHERSHIP_CHILD_WANDER_RETARGET_MAX -
                                                          MOTHERSHIP_CHILD_WANDER_RETARGET_MIN);
            }
            float dx = c->wander_target_x - c->x, dy = c->wander_target_y - c->y;
            float dist = sqrtf(dx * dx + dy * dy);
            float step = scaled(gs, MOTHERSHIP_CHILD_WANDER_SPEED) * dt;
            if (dist > step && dist > 0.0001f) {
                c->x += dx / dist * step;
                c->y += dy / dist * step;
            }
        }

        float half_w = scaled(gs, PLAYER_WIDTH) / 2.0f;
        float half_h = scaled(gs, PLAYER_HEIGHT) / 2.0f;
        if (c->x < half_w) c->x = half_w;
        if (c->x > (float)gs->screen_w - half_w) c->x = (float)gs->screen_w - half_w;
        if (c->y < half_h) c->y = half_h;
        if (c->y > (float)gs->screen_h - half_h) c->y = (float)gs->screen_h - half_h;

        update_child_firing(gs, c, dt, events);
    }
}

/* Both of The Mothership's own modes (SHOOT_MODE_SWARM_WANDER/
 * SHOOT_MODE_SWARM_FORMATION) dispatch a new escort identically, paced by
 * MOTHERSHIP_DISPATCH_COOLDOWN the same way every other single-shot mode
 * paces its own fire_cooldown - which of the two is active only ever
 * steers update_children's own AI for every already-alive child, never
 * this spawn logic. She never fires a projectile of her own. */
static void update_mothership_dispatch(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    int slot = -1;
    for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
        if (!gs->children[i].alive) {
            slot = i;
            break;
        }
    }
    /* At capacity - no cooldown spent, so dispatch resumes the instant a
     * slot frees rather than making the player wait out an extra cooldown
     * on top of that. */
    if (slot < 0) return;

    ChildShip *c = &gs->children[slot];
    *c = (ChildShip){0};
    c->alive = true;
    c->kind = (frand01() < 0.5f) ? SHIP_B20 : SHIP_C24;
    /* Underneath her, never in front or behind - see
     * MOTHERSHIP_CHILD_LAUNCH_DURATION's own doc comment. */
    c->x = p->x;
    c->y = p->y + scaled(gs, PLAYER_HEIGHT) * ship_size_multiplier(SHIP_MOTHERSHIP) * 0.5f;
    c->vx = (frand01() < 0.5f ? -1.0f : 1.0f) * scaled(gs, MOTHERSHIP_CHILD_LAUNCH_SPEED);
    c->vy = 0.0f;
    c->launch_timer = MOTHERSHIP_CHILD_LAUNCH_DURATION;
    c->life = MOTHERSHIP_CHILD_LIFE_MAX;

    /* Fixed at mode #1 (slot 0) the overwhelming majority of the time - see
     * MOTHERSHIP_CHILD_RANDOM_MODE_CHANCE's own doc comment. */
    int mode_slot = 0;
    int mode_count = ship_shoot_mode_slot_count(c->kind);
    if (mode_count > 1 && frand01() < MOTHERSHIP_CHILD_RANDOM_MODE_CHANCE) {
        int extra_slot = (int)(frand01() * (float)(mode_count - 1));
        if (extra_slot >= mode_count - 1) extra_slot = mode_count - 2;
        mode_slot = extra_slot + 1; /* uniformly random slot in [1, mode_count) */
    }
    c->shoot_mode = ship_shoot_mode_for_slot(c->kind, mode_slot);

    p->fire_cooldown = MOTHERSHIP_DISPATCH_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Dispatches to whichever mode is currently active. fire_cooldown is
 * decremented once here regardless of mode - every mode but rapid fire
 * (which drives its own pair of timers) gates its shot on it, the same
 * single-timer pattern the original normal-only fire logic used.
 * rapid_cooldown_timer is decremented here too, unconditionally on dt
 * and regardless of shoot_mode - it has to run independently of
 * update_rapid_fire now that shoot_mode is auto-switched away to slot 0
 * the instant the burst ends (see update_rapid_fire), so update_rapid_fire
 * itself is never reached again while the cooldown is actually running. */
static void update_player_firing(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    Player *p = &gs->player;
    if (p->fire_cooldown > 0.0f) p->fire_cooldown -= dt;
    if (p->rapid_cooldown_timer > 0.0f) {
        p->rapid_cooldown_timer -= dt;
        if (p->rapid_cooldown_timer < 0.0f) p->rapid_cooldown_timer = 0.0f;
    }

    /* While the super beam is active it replaces every shooting mode
     * entirely - see update_super_beam, which fires automatically every
     * frame on its own. */
    if (p->super_beam_timer > 0.0f) return;

    switch (p->shoot_mode) {
        case SHOOT_MODE_RAPID: update_rapid_fire(gs, input, dt, events); break;
        case SHOOT_MODE_POWER: update_power_cannon(gs, input, events); break;
        case SHOOT_MODE_DOUBLE: update_double_barrel(gs, input, events); break;
        case SHOOT_MODE_SIDE: update_side_beams(gs, input, events); break;
        case SHOOT_MODE_OMNI: update_omni_burst(gs, input, events); break;
        case SHOOT_MODE_SWARM_WANDER:
        case SHOOT_MODE_SWARM_FORMATION: update_mothership_dispatch(gs, input, events); break;
        case SHOOT_MODE_NORMAL:
        case SHOOT_MODE_COUNT:
        default: update_normal_fire(gs, input, events); break;
    }
}

static void update_player(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    Player *p = &gs->player;
    if (!p->alive) return;

    float dx = 0.0f, dy = 0.0f;
    if (input->move_left) dx -= 1.0f;
    if (input->move_right) dx += 1.0f;
    if (input->move_up) dy -= 1.0f;
    if (input->move_down) dy += 1.0f;
    if (dx != 0.0f && dy != 0.0f) {
        const float inv_sqrt2 = 0.70710678f;
        dx *= inv_sqrt2;
        dy *= inv_sqrt2;
    }

    float speed = scaled(gs, PLAYER_SPEED) * ship_speed_multiplier(gs->selected_ship);
    if (p->super_beam_timer > 0.0f) speed *= SUPER_BEAM_SPEED_MULTIPLIER;
    p->x += dx * speed * dt;
    p->y += dy * speed * dt;

    float size_mult = ship_size_multiplier(gs->selected_ship);
    float half_w = scaled(gs, PLAYER_WIDTH) * size_mult / 2.0f;
    float min_y = scaled(gs, PLAYER_HEIGHT) * size_mult / 2.0f; /* free to roam the whole screen, not just the lower band */
    float max_y = (float)gs->screen_h - scaled(gs, PLAYER_BOTTOM_MARGIN);
    if (p->x < half_w) p->x = half_w;
    if (p->x > (float)gs->screen_w - half_w) p->x = (float)gs->screen_w - half_w;
    if (p->y < min_y) p->y = min_y;
    if (p->y > max_y) p->y = max_y;

    update_shoot_mode_switch(gs, input, events);
    update_player_firing(gs, input, dt, events);
}

/* The super beam is a continuous column running from the ship straight up
 * to the top of the screen. While active it needs no fire input: it just
 * sweeps every enemy and enemy projectile inside its width off the board,
 * every frame, for its whole duration. */
static void update_super_beam(GameState *gs, float dt, EventQueue *events) {
    Player *p = &gs->player;
    if (p->super_beam_timer <= 0.0f) {
        /* Not active: make sure a later reactivation always deals damage
         * instantly, regardless of where the boss was sitting when this
         * beam session ended. */
        gs->boss.beam_contact_timer = 0.0f;
        return;
    }

    p->super_beam_timer -= dt;
    if (p->super_beam_timer < 0.0f) p->super_beam_timer = 0.0f;
    if (!p->alive) return;

    float beam_half_w = scaled(gs, PLAYER_PROJECTILE_W) * SUPER_BEAM_WIDTH_MULTIPLIER / 2.0f;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;
        if (e->y >= p->y) continue;
        if (fabsf(e->x - p->x) <= beam_half_w + e->size / 2.0f) {
            destroy_enemy_for_score(gs, events, e);
        }
    }

    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        Projectile *pr = &gs->enemy_shots[i];
        if (!pr->alive) continue;
        if (pr->y >= p->y) continue;
        float pr_half_w, pr_half_h;
        enemy_shot_half_extents(pr, &pr_half_w, &pr_half_h);
        if (fabsf(pr->x - p->x) <= beam_half_w + pr_half_w) {
            pr->alive = false;
        }
    }

    if (gs->boss.alive) {
        bool boss_in_beam = gs->boss.y < p->y &&
                             fabsf(gs->boss.x - p->x) <= beam_half_w + gs->boss.size / 2.0f;
        if (boss_in_beam) {
            if (gs->boss.beam_contact_timer <= 0.0f) {
                damage_boss(gs, events, BASE_PLAYER_DAMAGE);
                gs->boss.beam_contact_timer = BEAM_BOSS_HIT_INTERVAL;
            }
        } else {
            gs->boss.beam_contact_timer = 0.0f;
        }
    }
}

/* True if at least `count` slots in gs->enemy_shots are currently free -
 * checked before firing a same-frame multi-shot pattern (ENEMY_SHOOT_TRISHOT,
 * ENEMY_SHOOT_OMNI) so a volley always lands as its whole recognizable shape
 * or not at all, rather than silently losing individual shots to pool
 * contention when many enemies are onscreen at once (spawn_enemy_shot just
 * drops a shot once the pool is exhausted) - losing, say, the second
 * diagonal of a 3-way spread would otherwise read as a broken pattern
 * instead of a merely skipped volley (the enemy's fire_timer already retries
 * on its own next interval). Single-shot styles don't need this: losing one
 * shot to a full pool is imperceptible either way. */
static bool enemy_shot_slots_available(const GameState *gs, int count) {
    int free_slots = 0;
    for (int i = 0; i < MAX_ENEMY_PROJECTILES && free_slots < count; i++) {
        if (!gs->enemy_shots[i].alive) free_slots++;
    }
    return free_slots >= count;
}

/* Fires one shooting style's non-bursting pattern (see EnemyShootStyle) -
 * everything except ENEMY_SHOOT_TRIBURST, whose 3 shots are staggered over
 * time instead of landing in the same frame and so are driven separately
 * by burst_shots_remaining/burst_shot_timer in update_enemies below. half
 * is the enemy's own half-size (e->size / 2), used to spawn beams just off
 * its leading edge. */
static void fire_enemy_shot_style(GameState *gs, Enemy *e, EnemyShootStyle style, float half) {
    float speed = scaled(gs, ENEMY_PROJECTILE_SPEED);

    switch (style) {
        case ENEMY_SHOOT_LONG_BEAM:
            spawn_enemy_shot(gs, e->x, e->y + half, 0.0f, speed, ENEMY_PROJECTILE_BEAM,
                              scaled(gs, ENEMY_LONG_BEAM_HALF_LENGTH),
                              scaled(gs, ENEMY_LONG_BEAM_HALF_WIDTH), e->color);
            break;

        case ENEMY_SHOOT_TRISHOT: {
            if (!enemy_shot_slots_available(gs, 3)) break;
            float diag = 0.70710678f;
            float half_len = scaled(gs, ENEMY_TRISHOT_HALF_LENGTH);
            float half_wid = scaled(gs, ENEMY_TRISHOT_HALF_WIDTH);
            spawn_enemy_shot(gs, e->x, e->y + half, 0.0f, speed,
                              ENEMY_PROJECTILE_BEAM, half_len, half_wid, e->color);
            spawn_enemy_shot(gs, e->x, e->y + half, -speed * diag, speed * diag,
                              ENEMY_PROJECTILE_BEAM, half_len, half_wid, e->color);
            spawn_enemy_shot(gs, e->x, e->y + half, speed * diag, speed * diag,
                              ENEMY_PROJECTILE_BEAM, half_len, half_wid, e->color);
            break;
        }

        case ENEMY_SHOOT_OMNI: {
            if (!enemy_shot_slots_available(gs, ENEMY_OMNI_SHOT_COUNT)) break;
            float omni_speed = speed * ENEMY_OMNI_SPEED_RATIO;
            float radius = scaled(gs, ENEMY_OMNI_ORB_RADIUS);
            for (int k = 0; k < ENEMY_OMNI_SHOT_COUNT; k++) {
                spawn_enemy_shot(gs, e->x, e->y, kOmniDirX[k] * omni_speed, kOmniDirY[k] * omni_speed,
                                  ENEMY_PROJECTILE_ORB, radius, 0.0f, e->color);
            }
            break;
        }

        case ENEMY_SHOOT_TRIBURST:
            /* driven separately, over several frames - see update_enemies */
            break;

        case ENEMY_SHOOT_THIN_BEAM:
        default:
            spawn_enemy_shot(gs, e->x, e->y + half, 0.0f, speed, ENEMY_PROJECTILE_BEAM,
                              scaled(gs, ENEMY_THIN_BEAM_HALF_LENGTH),
                              scaled(gs, ENEMY_THIN_BEAM_HALF_WIDTH), e->color);
            break;
    }
}

static void update_enemies(GameState *gs, float dt) {
    float fire_chance = difficulty_enemy_fire_chance_per_sec(gs->selected_difficulty, gs->time_elapsed);
    float mean_fire_interval = 1.0f / fire_chance;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;

        e->wobble_phase += dt * 3.0f;
        e->x += (e->vx + sinf(e->wobble_phase) * scaled(gs, 12.0f)) * dt;
        e->y += e->vy * dt;

        float half = e->size / 2.0f;
        if (e->x < half) e->x = half;
        if (e->x > (float)gs->screen_w - half) e->x = (float)gs->screen_w - half;

        if (e->y - half > (float)gs->screen_h) {
            e->alive = false;
            continue;
        }

        if (e->burst_shots_remaining > 0) {
            e->burst_shot_timer -= dt;
            if (e->burst_shot_timer <= 0.0f) {
                e->burst_shot_timer = ENEMY_TRIBURST_SHOT_INTERVAL;
                e->burst_shots_remaining--;
                spawn_enemy_shot(gs, e->x, e->y + half, 0.0f, scaled(gs, ENEMY_PROJECTILE_SPEED),
                                  ENEMY_PROJECTILE_ORB, scaled(gs, ENEMY_TRIBURST_ORB_RADIUS), 0.0f,
                                  e->color);
            }
        }

        e->fire_timer -= dt;
        if (e->fire_timer <= 0.0f && e->burst_shots_remaining == 0) {
            e->fire_timer = mean_fire_interval * (0.6f + frand01() * 0.8f);
            EnemyShootStyle style = spawner_enemy_kind_shoot_style(e->kind);
            if (style == ENEMY_SHOOT_TRIBURST) {
                e->burst_shots_remaining = ENEMY_TRIBURST_SHOT_COUNT;
                e->burst_shot_timer = 0.0f;
            } else {
                fire_enemy_shot_style(gs, e, style, half);
            }
        }
    }
}

/* Counts down every enemy a shot orb scheduled for a delayed explosion
 * (see check_collisions) and detonates it the moment its own random timer
 * runs out. Never awards score - mirroring the orb's old instant-neutralize
 * semantics, just spread out over ORB_SHOT_EXPLOSION_WINDOW instead of all
 * at once. Enemies that died some other way first (direct laser hit, super
 * beam, ...) are simply skipped via the alive check; no separate cleanup
 * needed since spawner.c resets these fields whenever a slot is reused. */
static void update_pending_orb_kills(GameState *gs, float dt, EventQueue *events) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive || !e->orb_kill_pending) continue;

        e->orb_kill_timer -= dt;
        if (e->orb_kill_timer > 0.0f) continue;

        e->alive = false;
        e->orb_kill_pending = false;
        spawn_explosion(gs, e->x, e->y, e->size);
        event_queue_push_sfx(events, SFX_ENEMY_DESTROYED);
    }
}

static void update_projectiles(GameState *gs, float dt) {
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        Projectile *pr = &gs->player_shots[i];
        if (!pr->alive) continue;
        pr->x += pr->vx * dt;
        pr->y += pr->vy * dt;

        /* Every mode but side beams (SHOOT_MODE_SIDE) only ever leaves via
         * the top, same as before this ability existed - checking all four
         * edges is what lets a horizontal shot despawn once it clears the
         * left/right edge instead of flying forever. */
        float half_w, half_h;
        player_shot_half_extents(gs, pr, &half_w, &half_h);
        if (pr->y < -half_h || pr->y > (float)gs->screen_h + half_h ||
            pr->x < -half_w || pr->x > (float)gs->screen_w + half_w) {
            pr->alive = false;
        }
    }
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        Projectile *pr = &gs->enemy_shots[i];
        if (!pr->alive) continue;
        pr->x += pr->vx * dt;
        pr->y += pr->vy * dt;

        if (pr->inert) {
            pr->inert_age += dt;
            if (pr->inert_age >= ENEMY_SHOT_FADE_DURATION) {
                pr->alive = false;
                continue;
            }
        }

        float half_w, half_h;
        enemy_shot_half_extents(pr, &half_w, &half_h);
        if (pr->y > (float)gs->screen_h + half_h) pr->alive = false;
    }
}

static void update_explosions(GameState *gs, float dt) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        Explosion *e = &gs->explosions[i];
        if (!e->alive) continue;
        e->age += dt;
        if (e->age >= e->max_age) e->alive = false;
    }
}

/* Continuously emits engine exhaust from the back of the ship (the end
 * opposite the nose - see spawn_player_shot's spawn point for the nose
 * itself) while the player is alive, and ages/drifts every particle
 * already in flight regardless. Emission stops the instant the player
 * dies, but particles already released keep drifting and fading out on
 * their own, same as an explosion outliving the enemy that spawned it. */
static void update_player_trail(GameState *gs, float dt) {
    Player *p = &gs->player;

    if (p->alive) {
        p->trail_emit_timer -= dt;
        if (p->trail_emit_timer <= 0.0f) {
            p->trail_emit_timer = TRAIL_SPAWN_INTERVAL;
            float back_y = p->y + scaled(gs, PLAYER_HEIGHT) / 2.0f;
            float jitter_x = (frand01() - 0.5f) * scaled(gs, PLAYER_WIDTH) * 0.3f;
            spawn_trail_particle(gs, p->x + jitter_x, back_y);
        }
    }

    for (int i = 0; i < MAX_TRAIL_PARTICLES; i++) {
        TrailParticle *t = &gs->trail_particles[i];
        if (!t->alive) continue;
        t->age += dt;
        if (t->age >= t->max_age) {
            t->alive = false;
            continue;
        }

        /* Exponential drag so the puff lingers and spreads like smoke
         * instead of shooting off in a straight line: at this rate it
         * loses about 85% of its speed every second. */
        float drag = powf(0.15f, dt);
        t->vx *= drag;
        t->vy *= drag;
        t->x += t->vx * dt;
        t->y += t->vy * dt;
    }
}

/* The enemy/boss counterpart to update_player_trail above: every alive
 * enemy and the boss (if present) each get their own emission cadence
 * (Enemy.trail_emit_timer / Boss.trail_emit_timer) into the shared
 * enemy_trail_particles pool, then every particle already in flight ages
 * and drifts exactly like the player's own (same drag physics) regardless
 * of whether its source is still alive - a boss shot down mid-trail
 * doesn't yank its existing puffs off screen, same as an explosion
 * outliving whatever spawned it. */
static void update_enemy_and_boss_trails(GameState *gs, float dt) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;
        e->trail_emit_timer -= dt;
        if (e->trail_emit_timer <= 0.0f) {
            e->trail_emit_timer = ENEMY_TRAIL_SPAWN_INTERVAL;
            float back_y = e->y - e->size / 2.0f;
            float jitter_x = (frand01() - 0.5f) * e->size * 0.3f;
            spawn_enemy_trail_particle(gs, e->x + jitter_x, back_y, 1.0f, ENEMY_TRAIL_MAX_ALPHA);
        }
    }

    Boss *b = &gs->boss;
    if (b->alive) {
        b->trail_emit_timer -= dt;
        if (b->trail_emit_timer <= 0.0f) {
            b->trail_emit_timer = BOSS_TRAIL_SPAWN_INTERVAL;
            float back_y = b->y - b->size / 2.0f;
            float jitter_x = (frand01() - 0.5f) * b->size * 0.3f;
            spawn_enemy_trail_particle(gs, b->x + jitter_x, back_y, BOSS_TRAIL_SIZE_MULTIPLIER, BOSS_TRAIL_MAX_ALPHA);
        }
    }

    for (int i = 0; i < MAX_ENEMY_TRAIL_PARTICLES; i++) {
        EnemyTrailParticle *t = &gs->enemy_trail_particles[i];
        if (!t->alive) continue;
        t->age += dt;
        if (t->age >= t->max_age) {
            t->alive = false;
            continue;
        }

        float drag = powf(0.15f, dt);
        t->vx *= drag;
        t->vy *= drag;
        t->x += t->vx * dt;
        t->y += t->vy * dt;
    }
}

/* One shared emitter loop for every projectile on screen - player_shots and
 * enemy_shots alike - into the one projectile_trails pool (see
 * ProjectileTrailParticle in domain/types.h). Each shot's own
 * trail_emit_timer paces its puffs independently, same convention as
 * Player.trail_emit_timer/Enemy.trail_emit_timer; the backward direction is
 * derived from the shot's own vx/vy (falling back to "straight up" if
 * somehow stationary, matching draw_enemy_beam's own guard) since unlike a
 * ship, a projectile can travel in any direction, not just down or up.
 * Runs after update_projectiles so it emits from this frame's fresh
 * position. Existing puffs age/drift the same way regardless of whether
 * their source projectile is still alive, same as every other trail here -
 * a shot that just hit something doesn't yank its trailing smoke away. */
static void update_projectile_trails(GameState *gs, float dt) {
    for (int side = 0; side < 2; side++) {
        Projectile *shots = side == 0 ? gs->player_shots : gs->enemy_shots;
        int count = side == 0 ? MAX_PLAYER_PROJECTILES : MAX_ENEMY_PROJECTILES;
        for (int i = 0; i < count; i++) {
            Projectile *pr = &shots[i];
            if (!pr->alive) continue;
            pr->trail_emit_timer -= dt;
            if (pr->trail_emit_timer <= 0.0f) {
                pr->trail_emit_timer = PROJECTILE_TRAIL_SPAWN_INTERVAL;
                float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
                float back_dx = speed > 0.0f ? -pr->vx / speed : 0.0f;
                float back_dy = speed > 0.0f ? -pr->vy / speed : -1.0f;
                spawn_projectile_trail_particle(gs, pr->x, pr->y, back_dx, back_dy, pr->color);
            }
        }
    }

    for (int i = 0; i < MAX_PROJECTILE_TRAIL_PARTICLES; i++) {
        ProjectileTrailParticle *t = &gs->projectile_trails[i];
        if (!t->alive) continue;
        t->age += dt;
        if (t->age >= t->max_age) {
            t->alive = false;
            continue;
        }

        float drag = powf(0.15f, dt);
        t->vx *= drag;
        t->vy *= drag;
        t->x += t->vx * dt;
        t->y += t->vy * dt;
    }
}

static void check_collisions(GameState *gs, EventQueue *events) {
    float player_size_mult = ship_size_multiplier(gs->selected_ship);
    float player_half_w = scaled(gs, PLAYER_WIDTH) * player_size_mult / 2.0f;
    float player_half_h = scaled(gs, PLAYER_HEIGHT) * player_size_mult / 2.0f;
    /* Children always collide at the stock (unmultiplied) size - see
     * ship_size_multiplier's own doc comment. */
    float child_half_w = scaled(gs, PLAYER_WIDTH) / 2.0f;
    float child_half_h = scaled(gs, PLAYER_HEIGHT) / 2.0f;

    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        Projectile *pr = &gs->player_shots[i];
        if (!pr->alive) continue;

        float shot_half_w, shot_half_h;
        player_shot_half_extents(gs, pr, &shot_half_w, &shot_half_h);

        for (int j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &gs->enemies[j];
            if (!e->alive) continue;

            if (collision_aabb_overlap(pr->x, pr->y, shot_half_w, shot_half_h,
                                        e->x, e->y, e->size / 2.0f, e->size / 2.0f)) {
                pr->alive = false;
                if (pr->kind == PROJECTILE_KIND_POWER) {
                    trigger_power_cannon_explosion(gs, events, pr->x, pr->y, pr->style_ship);
                } else {
                    destroy_enemy_for_score(gs, events, e);
                }
                break;
            }
        }
    }

    if (gs->player.alive) {
        for (int j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &gs->enemies[j];
            if (!e->alive) continue;
            if (collision_aabb_overlap(gs->player.x, gs->player.y, player_half_w, player_half_h,
                                        e->x, e->y, e->size / 2.0f, e->size / 2.0f)) {
                e->alive = false;
                spawn_explosion(gs, e->x, e->y, e->size);
                kill_player(gs, events);
                break;
            }
        }
    }

    /* A ChildShip touching an ordinary enemy is mutual destruction - same
     * symmetric "both die" rule as player-enemy contact above, just never
     * fatal to the run since it's an escort, not the player (see
     * destroy_enemy_for_score for the enemy's own explosion/score/sfx). */
    for (int k = 0; k < MOTHERSHIP_MAX_CHILDREN; k++) {
        ChildShip *c = &gs->children[k];
        if (!c->alive) continue;
        for (int j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &gs->enemies[j];
            if (!e->alive) continue;
            if (collision_aabb_overlap(c->x, c->y, child_half_w, child_half_h,
                                        e->x, e->y, e->size / 2.0f, e->size / 2.0f)) {
                c->alive = false;
                c->life = 0.0f;
                spawn_explosion(gs, c->x, c->y, scaled(gs, PLAYER_WIDTH));
                event_queue_push_sfx(events, SFX_PLAYER_DESTROYED);
                destroy_enemy_for_score(gs, events, e);
                break;
            }
        }
    }

    if (gs->player.alive) {
        for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
            Projectile *pr = &gs->enemy_shots[i];
            if (!pr->alive || pr->inert) continue; /* inert = fading out after a boss arrived; harmless */
            float enemy_shot_half_w, enemy_shot_half_h;
            enemy_shot_half_extents(pr, &enemy_shot_half_w, &enemy_shot_half_h);
            if (collision_aabb_overlap(pr->x, pr->y, enemy_shot_half_w, enemy_shot_half_h,
                                        gs->player.x, gs->player.y, player_half_w, player_half_h)) {
                pr->alive = false;
                damage_player(gs, events, PLAYER_LIFE_LOSS_PER_HIT);
                break;
            }
        }
    }

    /* Same enemy-shot life-loss rule as damage_player above, just against
     * each ChildShip's own (much smaller) MOTHERSHIP_CHILD_LIFE_MAX pool
     * and its own kind's ship_damage_taken_multiplier - a B-20-kind child
     * takes a full hit, a C-24-kind child takes less, same as those ships'
     * own Strength ratings already promise the real player. A separate
     * loop over gs->enemy_shots from the player's own above (rather than
     * one combined pass) so a shot that already hit the player this frame
     * (pr->alive now false) is simply skipped here, never double-applied. */
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        Projectile *pr = &gs->enemy_shots[i];
        if (!pr->alive || pr->inert) continue;
        float enemy_shot_half_w, enemy_shot_half_h;
        enemy_shot_half_extents(pr, &enemy_shot_half_w, &enemy_shot_half_h);
        for (int k = 0; k < MOTHERSHIP_MAX_CHILDREN; k++) {
            ChildShip *c = &gs->children[k];
            if (!c->alive) continue;
            if (!collision_aabb_overlap(pr->x, pr->y, enemy_shot_half_w, enemy_shot_half_h,
                                         c->x, c->y, child_half_w, child_half_h)) {
                continue;
            }
            pr->alive = false;
            c->life -= PLAYER_LIFE_LOSS_PER_HIT * ship_damage_taken_multiplier(c->kind);
            if (c->life <= 0.0f) {
                c->life = 0.0f;
                c->alive = false;
                spawn_explosion(gs, c->x, c->y, scaled(gs, PLAYER_WIDTH));
                event_queue_push_sfx(events, SFX_PLAYER_DESTROYED);
            }
            break;
        }
    }

    if (gs->boss.alive) {
        float boss_half = gs->boss.size / 2.0f;

        /* The player's laser chips away at the boss's hit pool. */
        for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
            Projectile *pr = &gs->player_shots[i];
            if (!pr->alive) continue;
            float shot_half_w, shot_half_h;
            player_shot_half_extents(gs, pr, &shot_half_w, &shot_half_h);
            if (!collision_aabb_overlap(pr->x, pr->y, shot_half_w, shot_half_h,
                                         gs->boss.x, gs->boss.y, boss_half, boss_half)) {
                continue;
            }
            pr->alive = false;
            damage_boss(gs, events, pr->damage);
            /* A power cannon shot still detonates on contact like it would
             * against any other target - trigger_power_cannon_explosion
             * only sweeps gs->enemies (never the boss, same as the orb's
             * shot-to-detonate sweep), so this is purely a bonus against
             * anything else caught in the blast radius, on top of the
             * boss's own damage_boss hit above. */
            if (pr->kind == PROJECTILE_KIND_POWER) {
                trigger_power_cannon_explosion(gs, events, pr->x, pr->y, pr->style_ship);
            }
            break;
        }

        /* The boss doesn't shoot - it rams the player instead. Its
         * visible danger ring (drawn at BOSS_MENACE_RING_RATIO * size in
         * adapters/sdl_renderer.c - the two share that constant so they
         * can never drift apart) detonates on the very first touch: the
         * boss explodes, and that explosion takes the player with it.
         *
         * The boss's own detonation is UNCONDITIONAL - deliberately not
         * gated on the player being killable. Gating it caused a
         * permanent stalemate: an invulnerable player (god mode, or an
         * active super beam) would have the boss glue itself to them at
         * zero distance forever with nothing happening at all. Only the
         * player's death is conditional, and kill_player already applies
         * those immunity rules itself.
         *
         * Re-check gs->boss.alive since the laser loop above may have
         * just defeated it this same frame. */
        if (gs->boss.alive && gs->player.alive) {
            float ring_radius = gs->boss.size * BOSS_MENACE_RING_RATIO;
            float player_radius = fmaxf(player_half_w, player_half_h);
            if (within_radius(gs->player.x, gs->player.y, gs->boss.x, gs->boss.y, ring_radius + player_radius)) {
                spawn_explosion(gs, gs->boss.x, gs->boss.y, gs->boss.size * 1.4f);
                end_boss_encounter(gs);
                event_queue_push_sfx(events, SFX_BOSS_DEFEATED);
                kill_player(gs, events);
            }
        }

        /* A ChildShip touching the same danger ring dies instantly too -
         * same hazard, so it can't just sit inside the boss unharmed - but
         * deliberately does NOT end the boss encounter the way the
         * player's own ring touch does just above: an escort is cheap to
         * re-dispatch, so letting one defeat the boss for free would be a
         * do-nothing-but-spam-children exploit. Re-check gs->boss.alive
         * (the ring-vs-player touch just above may have just ended the
         * encounter this same frame) - no break, since more than one child
         * can be caught in the ring at once and all of them die. */
        if (gs->boss.alive) {
            float ring_radius = gs->boss.size * BOSS_MENACE_RING_RATIO;
            float child_radius = fmaxf(child_half_w, child_half_h);
            for (int k = 0; k < MOTHERSHIP_MAX_CHILDREN; k++) {
                ChildShip *c = &gs->children[k];
                if (!c->alive) continue;
                if (!within_radius(c->x, c->y, gs->boss.x, gs->boss.y, ring_radius + child_radius)) continue;
                c->alive = false;
                c->life = 0.0f;
                spawn_explosion(gs, c->x, c->y, scaled(gs, PLAYER_WIDTH));
                event_queue_push_sfx(events, SFX_PLAYER_DESTROYED);
            }
        }
    }

    if (gs->orb.alive) {
        float orb_half = gs->orb.size / 2.0f;

        /* Shooting the orb (as opposed to capturing it): it detonates and
         * schedules every enemy alive on screen right now - never the
         * boss, which isn't part of gs->enemies - to explode at its own
         * random moment within ORB_SHOT_EXPLOSION_WINDOW seconds (see
         * update_pending_orb_kills), rather than granting the super beam.
         * Enemies that spawn afterward, while some of these delayed
         * explosions are still pending, are untouched - only what was
         * actually on screen at the instant of the shot counts. */
        for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
            Projectile *pr = &gs->player_shots[i];
            if (!pr->alive) continue;
            float shot_half_w, shot_half_h;
            player_shot_half_extents(gs, pr, &shot_half_w, &shot_half_h);
            if (!collision_aabb_overlap(pr->x, pr->y, shot_half_w, shot_half_h,
                                         gs->orb.x, gs->orb.y, orb_half, orb_half)) {
                continue;
            }

            pr->alive = false;
            gs->orb.alive = false;
            spawn_explosion(gs, gs->orb.x, gs->orb.y, gs->orb.size * 1.8f);

            for (int j = 0; j < MAX_ENEMIES; j++) {
                Enemy *e = &gs->enemies[j];
                if (!e->alive || e->orb_kill_pending) continue;
                e->orb_kill_pending = true;
                e->orb_kill_timer = frand01() * ORB_SHOT_EXPLOSION_WINDOW;
            }
            event_queue_push_sfx(events, SFX_ORB_DESTROYED);
            break;
        }
    }

    /* Capturing the orb (the ship physically touching it) grants the
     * super beam - re-check gs->orb.alive since the loop above may have
     * just detonated it this same frame. */
    if (gs->orb.alive && gs->player.alive) {
        float orb_half = gs->orb.size / 2.0f;
        if (collision_aabb_overlap(gs->player.x, gs->player.y, player_half_w, player_half_h,
                                    gs->orb.x, gs->orb.y, orb_half, orb_half)) {
            gs->orb.alive = false;
            gs->player.super_beam_timer = SUPER_BEAM_DURATION;
            gs->player.life = PLAYER_LIFE_MAX;
            event_queue_push_sfx(events, SFX_ORB_CAPTURED);
        }
    }
}

static void update_running(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    gs->time_elapsed += dt;

    if (input->god_mode_toggle_pressed) {
        gs->player.god_mode = !gs->player.god_mode;
        event_queue_push_sfx(events, SFX_MENU_SELECT);
    }

    update_player(gs, input, dt, events);
    update_children(gs, dt, events);
    update_player_trail(gs, dt);
    if (!gs->boss.alive) spawner_update(gs, dt); /* no ordinary spawns during a boss fight */
    update_enemies(gs, dt);
    update_pending_orb_kills(gs, dt, events);
    update_boss(gs, dt);
    update_enemy_and_boss_trails(gs, dt);
    update_orb(gs, dt);
    update_projectiles(gs, dt);
    update_projectile_trails(gs, dt);
    update_super_beam(gs, dt, events);
    update_explosions(gs, dt);
    check_collisions(gs, events);
}

void game_update(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    event_queue_clear(events);
    update_stars(gs, dt);
    update_background_clouds(gs, dt);
    handle_global_back(gs, input, events);

    switch (gs->state) {
        case STATE_MENU:
            update_menu(gs, input, dt, events);
            break;
        case STATE_DIFFICULTY_SELECT:
            update_difficulty_select(gs, input, events);
            break;
        case STATE_SHIP_SELECT:
            update_ship_select(gs, input, events);
            break;
        case STATE_GAME:
            update_running(gs, input, dt, events);
            break;
        case STATE_PAUSE:
            update_pause(gs, input, events);
            break;
        case STATE_GAME_OVER:
            update_game_over(gs, input, events);
            break;
    }

    if (input->quit_requested) gs->quit_requested = true;
}
