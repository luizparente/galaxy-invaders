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

static float deg_to_rad(float deg) {
    return deg * 0.017453293f;
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
    /* Every one of Shine's own shots is an elongated shard oriented along
     * its own travel direction (see draw_shine_shard) - modes 1/3 always
     * fly straight up, but mode 2's 12-way omni burst fires in every
     * direction, so (unlike B-20's own SIDE mode, which only ever needs to
     * pick between "vertical" and "horizontal") this can't just swap
     * width/height on a bool. Instead it's the exact axis-aligned bounding
     * box of the shard's own length x width rectangle rotated to match its
     * unit travel direction (dx, dy) - fabsf(dx)/fabsf(dy) are exactly the
     * projection of that rotation onto each axis, so this collapses to the
     * same "swap on vertical vs horizontal" result for straight shots and
     * stays exact for every diagonal in between. PROJECTILE_KIND_SHINE_SPIRAL
     * (mode 3) is the longer of the two shard lengths. */
    if (pr->style_ship == SHIP_SHINE) {
        float length = scaled(gs, pr->kind == PROJECTILE_KIND_SHINE_SPIRAL ? SHINE_SPIRAL_SHARD_LENGTH
                                                                            : SHINE_SHARD_LENGTH);
        float width = scaled(gs, pr->kind == PROJECTILE_KIND_SHINE_SPIRAL ? SHINE_SPIRAL_SHARD_WIDTH
                                                                           : SHINE_SHARD_WIDTH);
        float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
        float dx = speed > 0.0f ? pr->vx / speed : 0.0f;
        float dy = speed > 0.0f ? pr->vy / speed : -1.0f;
        *half_w = fabsf(dx) * length / 2.0f + fabsf(dy) * width / 2.0f;
        *half_h = fabsf(dy) * length / 2.0f + fabsf(dx) * width / 2.0f;
        return;
    }
    /* Cruzader's own shots (twin bolts and rockets - reflected shots never
     * reach here, they stay in gs->enemy_shots with their original enemy
     * design untouched, see reflect_enemy_shot) use the same
     * oriented-bounding-box construction as Shine's own branch above rather
     * than a simple vertical/horizontal swap, since a rocket's homing curve
     * can bend it to any angle. */
    if (pr->style_ship == SHIP_CRUZADER) {
        float length = scaled(gs, pr->kind == PROJECTILE_KIND_CRUZADER_ROCKET ? CRUZADER_ROCKET_LENGTH
                                                                                : CRUZADER_BOLT_LENGTH);
        float width = scaled(gs, pr->kind == PROJECTILE_KIND_CRUZADER_ROCKET ? CRUZADER_ROCKET_WIDTH
                                                                               : CRUZADER_BOLT_WIDTH);
        float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
        float dx = speed > 0.0f ? pr->vx / speed : 0.0f;
        float dy = speed > 0.0f ? pr->vy / speed : -1.0f;
        *half_w = fabsf(dx) * length / 2.0f + fabsf(dy) * width / 2.0f;
        *half_h = fabsf(dy) * length / 2.0f + fabsf(dx) * width / 2.0f;
        return;
    }
    /* The Twins' own bolt (always fired straight up, but built the same
     * oriented-bounding-box way as Shine/Cruzader above rather than a
     * simple vertical swap, for consistency). */
    if (pr->style_ship == SHIP_TWINS) {
        float length = scaled(gs, TWINS_BOLT_LENGTH);
        float width = scaled(gs, TWINS_BOLT_WIDTH);
        float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
        float dx = speed > 0.0f ? pr->vx / speed : 0.0f;
        float dy = speed > 0.0f ? pr->vy / speed : -1.0f;
        *half_w = fabsf(dx) * length / 2.0f + fabsf(dy) * width / 2.0f;
        *half_h = fabsf(dy) * length / 2.0f + fabsf(dx) * width / 2.0f;
        return;
    }
    /* Antartica's own shots: Frosty's own snowball (PROJECTILE_KIND_FROSTY_SNOWBALL)
     * is a sphere, same round-hitbox construction as C-24's own sphere shots
     * above; Antartica's own ice shards (every other kind she fires) use the
     * same oriented-bounding-box construction as Shine/Cruzader/Twins above. */
    if (pr->style_ship == SHIP_ANTARTICA) {
        if (pr->kind == PROJECTILE_KIND_FROSTY_SNOWBALL) {
            float r = scaled(gs, FROSTY_SNOWBALL_RADIUS);
            *half_w = r;
            *half_h = r;
            return;
        }
        float length = scaled(gs, ANTARTICA_SHARD_LENGTH);
        float width = scaled(gs, ANTARTICA_SHARD_WIDTH);
        float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
        float dx = speed > 0.0f ? pr->vx / speed : 0.0f;
        float dy = speed > 0.0f ? pr->vy / speed : -1.0f;
        *half_w = fabsf(dx) * length / 2.0f + fabsf(dy) * width / 2.0f;
        *half_h = fabsf(dy) * length / 2.0f + fabsf(dx) * width / 2.0f;
        return;
    }
    /* Buckler's own shot is a round ball fired in any of 5 fixed directions
     * (see update_buckler_cannon_fire) - a simple fixed-radius sphere hitbox,
     * same construction as C-24's/Frosty's own round shots above, needing no
     * travel-direction math since it's never elongated. */
    if (pr->style_ship == SHIP_BUCKLER) {
        float r = scaled(gs, BUCKLER_CANNON_PROJECTILE_RADIUS);
        *half_w = r;
        *half_h = r;
        return;
    }
    /* Every one of Samurai's own shuriken (modes 1/2 alike) - a simple
     * fixed-radius sphere hitbox, same "round despite its pointed silhouette"
     * convention Buckler's own cannon ball above already uses, needing no
     * travel-direction math even for mode 2's own fanned-out sweep shots. */
    if (pr->style_ship == SHIP_SAMURAI) {
        float r = scaled(gs, SAMURAI_SHURIKEN_RADIUS);
        *half_w = r;
        *half_h = r;
        return;
    }
    /* Ranger's own shots: modes 1/2 are always fired straight up, so unlike
     * Shine/Cruzader/Twins/Antartica's own oriented-bounding-box shots
     * above, no travel-direction math is needed - a simple fixed vertical
     * bolt hitbox, just RANGER_BEAM_LENGTH/WIDTH's own bigger "long laser
     * beam" size instead of B-20's own PLAYER_PROJECTILE_W/H. Mode 3's own
     * arch-shaped wave (PROJECTILE_KIND_RANGER_ARC) gets its own wide,
     * shallow hitbox instead - wide enough to sweep most of a vertical
     * lane's worth of enemies at once, per its own "arch-shaped" spec. */
    if (pr->style_ship == SHIP_RANGER) {
        if (pr->kind == PROJECTILE_KIND_RANGER_ARC) {
            *half_w = scaled(gs, RANGER_ARC_WAVE_WIDTH) / 2.0f;
            *half_h = scaled(gs, RANGER_ARC_WAVE_HEIGHT) / 2.0f;
            return;
        }
        *half_w = scaled(gs, RANGER_BEAM_WIDTH) / 2.0f;
        *half_h = scaled(gs, RANGER_BEAM_LENGTH) / 2.0f;
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
        t->style_ship = gs->selected_ship;
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
/* size_mult/alpha_cap default to 1.0f/PROJECTILE_TRAIL_MAX_ALPHA for every
 * ordinary shot (see update_projectile_trails' own call site) - Cruzader's
 * mode 3 rockets are the one deliberate exception, passing
 * CRUZADER_ROCKET_TRAIL_SIZE_MULTIPLIER/CRUZADER_ROCKET_TRAIL_MAX_ALPHA
 * instead for a bigger, more visible puff, per feedback. */
static void spawn_projectile_trail_particle(GameState *gs, float x, float y, float back_dx, float back_dy,
                                             Color color, float size_mult, unsigned char alpha_cap) {
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
        t->size = scaled(gs, PROJECTILE_TRAIL_BASE_SIZE) * (0.7f + frand01() * 0.6f) * size_mult;
        t->color = color;
        t->alpha_cap = alpha_cap;
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
/* The one place every player shot actually gets filled in - color is an
 * explicit parameter rather than always pulled from gs->player.laser_color,
 * so Ranger's own per-shot random color reroll (see
 * SHOOT_MODE_RANGER_TRIBEAM's own doc comment in domain/types.h) can bypass
 * B-20's "never rerolled" laser_color entirely instead of temporarily
 * stomping that shared field. spawn_player_shot_styled just below is a thin
 * wrapper passing gs->player.laser_color through unchanged, so every
 * existing call site keeps B-20's own convention untouched. */
static void spawn_player_shot_colored(GameState *gs, float x, float y, float vx, float vy,
                                       ProjectileKind kind, bool horizontal, float damage,
                                       Ship style_ship, Color color) {
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        Projectile *pr = &gs->player_shots[i];
        if (pr->alive) continue;
        pr->alive = true;
        pr->x = x;
        pr->y = y;
        pr->vx = vx;
        pr->vy = vy;
        pr->color = color;
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
        /* PROJECTILE_KIND_RANGER_ARC only - see its own doc comment in
         * domain/types.h. Harmless to set unconditionally for every other
         * kind, same as phase_seed above. */
        pr->ranger_arc_hit_boss = false;
        return;
    }
}

static void spawn_player_shot_styled(GameState *gs, float x, float y, float vx, float vy,
                                      ProjectileKind kind, bool horizontal, float damage,
                                      Ship style_ship) {
    spawn_player_shot_colored(gs, x, y, vx, vy, kind, horizontal, damage, style_ship, gs->player.laser_color);
}

/* The real player's own fire routines all still call this - a thin wrapper
 * defaulting style_ship to gs->selected_ship, so none of their 9 call
 * sites need to change. Only update_child_firing calls
 * spawn_player_shot_styled directly, tagging a child's own kind instead. */
static void spawn_player_shot(GameState *gs, float x, float y, float vx, float vy,
                               ProjectileKind kind, bool horizontal, float damage) {
    spawn_player_shot_styled(gs, x, y, vx, vy, kind, horizontal, damage, gs->selected_ship);
}

/* Ranger's own random-per-shot laser color - "each time Ranger shoots the
 * projectile takes on a different color at random" per spec, a genuinely
 * random hue every trigger (unlike B-20's own fixed-for-the-whole-run
 * kDefaultLaserColor above), at high saturation/value so it always reads as
 * a vivid, saturated beam rather than a washed-out pastel one. */
static Color ranger_random_shot_color(void) {
    return color_from_hsv(frand01() * 360.0f, 0.85f, 1.0f);
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
        pr->reflected = false;
        pr->trail_emit_timer = frand01() * PROJECTILE_TRAIL_SPAWN_INTERVAL;
        return;
    }
}

/* Cruzader's own reflect mechanics (his passive 50% chance and his
 * deflector orb - see check_collisions): bounces an incoming enemy shot
 * back the way it came, in place - only vx/vy (negated) and damage/
 * reflected change, so the shot keeps its exact original design (color,
 * beam vs orb shape, size) rather than turning into one of Cruzader's own
 * bolts. Stays in gs->enemy_shots (never moved into player_shots); a
 * separate pass in check_collisions tests reflected shots against
 * gs->enemies/gs->boss instead of the player. damage is stashed in
 * Projectile.damage, the same field a player shot already uses to carry
 * its own boss-damage amount, so the reflected-vs-boss test below can read
 * it the same way. */
static void reflect_enemy_shot(Projectile *pr, float damage) {
    pr->vx = -pr->vx;
    pr->vy = -pr->vy;
    pr->damage = damage;
    pr->reflected = true;
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

/* Shine's own mode 2 (SHOOT_MODE_SHINE_OMNI): the same idea as kOmniDirX/Y
 * above, just SHINE_OMNI_SHOT_COUNT (12, not 8) directions evenly spaced
 * every 30 degrees - still written out rather than computed with sinf/cosf,
 * since multiples of 30 degrees land on the same small set of exact values
 * (0, 0.5, sqrt(3)/2, 1) that 45-degree multiples do. Kept independent of
 * kOmniDirX/Y (not a generalized N-direction generator) so retuning one
 * ship's burst can never accidentally retune another's or an enemy's. */
static const float kShineOmniDirX[SHINE_OMNI_SHOT_COUNT] = {
    0.0f, 0.5f, 0.86602540f, 1.0f, 0.86602540f, 0.5f,
    0.0f, -0.5f, -0.86602540f, -1.0f, -0.86602540f, -0.5f,
};
static const float kShineOmniDirY[SHINE_OMNI_SHOT_COUNT] = {
    1.0f, 0.86602540f, 0.5f, 0.0f, -0.5f, -0.86602540f,
    -1.0f, -0.86602540f, -0.5f, 0.0f, 0.5f, 0.86602540f,
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
 * already been resolved (captured, shot, or fallen off the bottom).
 *
 * Skipped entirely while a boss is alive: a fight's own kill count is too
 * small and unpredictable to land on round score steps the way a normal
 * clear does, so boss fights use a flat per-kill chance instead - see
 * BOSS_FIGHT_ORB_SPAWN_CHANCE in destroy_enemy_for_score below. */
static void maybe_trigger_orb_spawn(GameState *gs, int old_score, int new_score) {
    if (gs->boss.alive) return;
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
    /* boss_count was just incremented above, so this already reflects the
     * encounter that's starting - see spawner_boss_dispatch_interval. */
    b->dispatch_timer = spawner_boss_dispatch_interval(gs->boss_count);

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

/* Keeps gs->boss_warning (see its own comment in domain/types.h) in sync
 * every frame, regardless of which path last changed score_since_last_boss
 * or boss.alive - cheaper and less error-prone than trying to update it
 * from every one of those mutation sites individually. */
static void update_boss_warning(GameState *gs) {
    gs->boss_warning = !gs->boss.alive &&
                        gs->score_since_last_boss >= BOSS_SCORE_STEP - BOSS_WARNING_SCORE_GAP;
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
 * (shot down here, or detonated by ring contact in check_collisions) -
 * both of which are always a defeat (there's no "boss just leaves"
 * outcome), so this is also the single place bosses_defeated advances.
 * Restarting score_since_last_boss at the END of an encounter - not at its
 * start - is what guarantees the full BOSS_SCORE_STEP gap before the next
 * one: from this instant every point has to be earned fresh, with the
 * arena clear. */
static void end_boss_encounter(GameState *gs) {
    gs->boss.alive = false;
    gs->score_since_last_boss = 0;
    gs->bosses_defeated++;
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

    /* Boss fights use this flat per-kill chance instead of the score-step
     * mechanic maybe_trigger_orb_spawn normally drives (skipped entirely
     * while a boss is alive - see its own guard). */
    if (gs->boss.alive && !gs->orb.alive && frand01() < BOSS_FIGHT_ORB_SPAWN_CHANCE) {
        spawn_orb(gs);
    }

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

/* Paces the boss's own periodic enemy dispatch (see
 * spawner_dispatch_enemy_from_boss) - the interval only ever changes
 * between encounters (spawner_boss_dispatch_interval keyed on
 * gs->boss_count), never mid-fight, so this just needs to keep resetting
 * dispatch_timer to whatever it was already set to. */
static void update_boss_dispatch(GameState *gs, float dt) {
    Boss *b = &gs->boss;
    if (!b->alive) return;

    b->dispatch_timer -= dt;
    if (b->dispatch_timer > 0.0f) return;

    b->dispatch_timer = spawner_boss_dispatch_interval(gs->boss_count);
    spawner_dispatch_enemy_from_boss(gs);
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
    gs->bosses_defeated = 0;
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
    gs->player.shine_omni_cooldown_timer = 0.0f;
    gs->player.cruzader_orb_timer = 0.0f;
    gs->player.cruzader_orb_cooldown_timer = 0.0f;
    gs->player.buckler_active_cannon = 0;
    gs->player.buckler_orb_timer = 0.0f;
    gs->player.buckler_orb_cooldown_timer = 0.0f;
    gs->player.samurai_burst_shots_remaining = 0;
    gs->player.samurai_burst_shot_timer = 0.0f;
    gs->player.samurai_omni_burst_timer = 0.0f;
    gs->player.samurai_omni_next_shot_index = 0;
    gs->player.samurai_omni_shot_timer = 0.0f;
    gs->player.samurai_omni_cooldown_timer = 0.0f;
    gs->player.samurai_stealth_timer = 0.0f;
    gs->player.samurai_stealth_cooldown_timer = 0.0f;
    gs->player.ranger_next_muzzle = 0;
    gs->player.trail_emit_timer = 0.0f;
    gs->player.twins_right_life = PLAYER_LIFE_MAX;
    gs->player.twins_left_life = PLAYER_LIFE_MAX;
    gs->player.twins_right_alive = true;
    gs->player.twins_left_alive = true;
    gs->player.twins_next_shot_is_right = true;
    gs->player.twins_right_x = gs->player.x + scaled(gs, TWINS_FORMATION_GAP) / 2.0f;
    gs->player.twins_left_x = gs->player.x - scaled(gs, TWINS_FORMATION_GAP) / 2.0f;
    gs->player.twins_mirror_center_x = gs->player.x;

    gs->player.antartica_life = PLAYER_LIFE_MAX;
    gs->player.frosty_life = PLAYER_LIFE_MAX;
    gs->player.antartica_alive = true;
    gs->player.frosty_alive = true;
    gs->player.frosty_fire_cooldown = 0.0f;
    gs->player.antartica_ice_storm_cooldown_timer = 0.0f;
    gs->player.antartica_freeze_beam_timer = 0.0f;
    gs->player.antartica_freeze_beam_cooldown_timer = 0.0f;
    gs->player.antartica_freeze_beam_boss_hit_timer = 0.0f;
    gs->player.frosty_x = gs->player.x - scaled(gs, TWINS_FORMATION_GAP);
    gs->player.frosty_y = gs->player.y;

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

/* The ship-select screen reached right after confirming a difficulty - all
 * 4 arrow keys move the cursor across the grid of unlocked ships
 * (gs->selected_ship doubles as both the cursor position and, once
 * confirmed, the run's actual ship, same "selection is the state" pattern
 * as gs->selected_difficulty above): left/right step by 1, up/down step by
 * a full SHIP_SELECT_GRID_COLS-wide row, all clamped at SHIP_B20/the last
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
    /* Up/down step by a full grid row (SHIP_SELECT_GRID_COLS, shared with
     * the renderer's own grid layout), same row/col math
     * draw_ship_select_screen uses to place each icon - clamped rather than
     * wrapping, and clamped at SHIP_COUNT - 1 (the last real ship) rather
     * than SHIP_SELECT_GRID_SLOTS - 1, so the cursor can never land on one
     * of the locked placeholder slots past it. */
    if (input->nav_up_pressed && gs->selected_ship >= SHIP_SELECT_GRID_COLS) {
        gs->selected_ship -= SHIP_SELECT_GRID_COLS;
        event_queue_push_sfx(events, SFX_MENU_SELECT);
    }
    if (input->nav_down_pressed && gs->selected_ship + SHIP_SELECT_GRID_COLS <= SHIP_COUNT - 1) {
        gs->selected_ship += SHIP_SELECT_GRID_COLS;
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

/* Whether the super beam currently shields the player from every death/
 * damage hazard below (kill_player and its per-ship-part counterparts) -
 * true outside a boss encounter (its original behavior, unchanged), false
 * while gs->boss.alive. Capturing the orb mid-fight still grants the full
 * beam otherwise - offense, healing, the speed boost (see
 * update_player_firing/update_player) - just never this immunity: a boss
 * on screen must always be able to end the run via an ordinary enemy or
 * its own ring, the same way it already ignores god mode's own protection
 * for nothing (god_mode is untouched by this and still blocks everything
 * unconditionally, checked as its own separate condition right after every
 * call to this). */
static bool super_beam_shields_player(const GameState *gs) {
    return gs->player.super_beam_timer > 0.0f && !gs->boss.alive;
}

/* Samurai's own mode 3 (SHOOT_MODE_SAMURAI_STEALTH) - true for the whole 3s
 * active window, during which check_collisions' own SHIP_SAMURAI branches
 * skip every attack/collision entirely (not just block damage the way
 * super_beam_shields_player does - "no collision, no deaths on either
 * side" per spec, so an ordinary enemy touched during stealth must survive
 * too, unlike every other immunity in the game). Also read by update_player
 * for the speed bonus. Unlike super_beam_shields_player above, this is
 * never suspended during a boss fight - stealth's whole point per spec is
 * unconditional immunity for its duration. */
static bool samurai_stealth_active(const GameState *gs) {
    return gs->selected_ship == SHIP_SAMURAI && gs->player.samurai_stealth_timer > 0.0f;
}

static void kill_player(GameState *gs, EventQueue *events) {
    if (!gs->player.alive) return;
    if (super_beam_shields_player(gs)) return; /* invincible for the duration of the beam, outside a boss fight */
    if (gs->player.god_mode) return; /* invincible until Ctrl+G is pressed again */
    gs->player.alive = false;
    gs->player.life = 0.0f;
    /* Harmless no-op for every ship but SHIP_TWINS - see kill_twin, the
     * only other place these get cleared. */
    gs->player.twins_right_alive = false;
    gs->player.twins_left_alive = false;
    /* Harmless no-op for every ship but SHIP_ANTARTICA - see
     * kill_antartica_body/kill_frosty, the only other places these get
     * cleared. */
    gs->player.antartica_alive = false;
    gs->player.frosty_alive = false;
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
    if (super_beam_shields_player(gs)) return;
    if (p->god_mode) return;

    p->life -= amount * ship_damage_taken_multiplier(gs->selected_ship);
    if (p->life <= 0.0f) {
        p->life = 0.0f;
        kill_player(gs, events);
    }
}

/* Cruzader's own carve-out from the always-fatal player-enemy contact rule
 * (see check_collisions): instead of kill_player, a flat
 * CRUZADER_ENEMY_CONTACT_LIFE_LOSS life-loss penalty - deliberately not run
 * through ship_damage_taken_multiplier the way damage_player's projectile
 * hits are (see that constant's own doc comment) - still gated on the same
 * super beam/god mode immunity every other form of damage respects. */
static void damage_cruzader_on_enemy_contact(GameState *gs, EventQueue *events) {
    Player *p = &gs->player;
    if (!p->alive) return;
    if (super_beam_shields_player(gs)) return;
    if (p->god_mode) return;

    p->life -= CRUZADER_ENEMY_CONTACT_LIFE_LOSS;
    if (p->life <= 0.0f) {
        p->life = 0.0f;
        kill_player(gs, events);
    }
}

/* The Twins' own per-twin death (see damage_twin below for the only other
 * caller) - modeled on kill_player above, but only that one twin's own
 * alive flag/explosion, and the control-transfer step: if the other twin
 * is still standing, p->x (the single shared control point - see
 * update_player's own SHIP_TWINS branch) snaps onto its current cached
 * position so it keeps flying from exactly where it already was instead of
 * jumping, and from this point on is driven directly (no more
 * mirroring/formation offset) - the "reverse control" the surviving twin
 * needs, since a bare direct mapping is exactly what makes the right key
 * move it right regardless of which mode was active when its sibling
 * died. Once both twins are down, the whole Player dies via kill_player. */
static void kill_twin(GameState *gs, EventQueue *events, bool right) {
    Player *p = &gs->player;
    bool *alive_flag = right ? &p->twins_right_alive : &p->twins_left_alive;
    if (!*alive_flag) return;
    if (!p->alive) return;
    if (super_beam_shields_player(gs)) return;
    if (p->god_mode) return;

    *alive_flag = false;
    /* A flat-damage/contact kill (kill_player_hitbox's own else-branch)
     * never routed through damage_twin's own life<=0 zeroing, so it's
     * zeroed unconditionally here instead - the dead twin's own life bar
     * (draw_twins_life_bars) must always read exactly 0, regardless of
     * which path killed it. */
    if (right) p->twins_right_life = 0.0f;
    else p->twins_left_life = 0.0f;
    float dead_x = right ? p->twins_right_x : p->twins_left_x;
    spawn_explosion(gs, dead_x, p->y, scaled(gs, PLAYER_WIDTH) * TWINS_SIZE_MULTIPLIER);
    event_queue_push_sfx(events, SFX_PLAYER_DESTROYED);

    if (right && p->twins_left_alive) {
        p->x = p->twins_left_x;
    } else if (!right && p->twins_right_alive) {
        p->x = p->twins_right_x;
    }

    /* Losing a twin immediately forces mode 1 (formation) - the survivor's
     * own actual position is untouched by this (update_player's own SHIP_TWINS
     * solo branch never reads shoot_mode at all, it always drives x directly),
     * this purely corrects the HUD's own mode indicator/label and, together
     * with update_shoot_mode_switch's own guard below, locks mode 2 out
     * entirely until the run ends - a lone twin has nothing left to mirror. */
    p->shoot_mode = SHOOT_MODE_TWINS_ALTERNATE;

    if (!p->twins_right_alive && !p->twins_left_alive) {
        kill_player(gs, events);
    }
}

/* The Twins' own per-twin damage_player counterpart - same immunity guards
 * and ship_damage_taken_multiplier scaling, just against that one twin's
 * own life field instead of the shared Player.life (unused by SHIP_TWINS -
 * see the Player struct's own doc comment). */
static void damage_twin(GameState *gs, EventQueue *events, bool right, float amount) {
    Player *p = &gs->player;
    if (!p->alive) return;
    if (super_beam_shields_player(gs)) return;
    if (p->god_mode) return;
    if (!(right ? p->twins_right_alive : p->twins_left_alive)) return;

    float *life = right ? &p->twins_right_life : &p->twins_left_life;
    *life -= amount * ship_damage_taken_multiplier(SHIP_TWINS);
    if (*life <= 0.0f) {
        *life = 0.0f;
        kill_twin(gs, events, right);
    }
}

/* Antartica's own per-body death (Antartica herself, as opposed to Frosty -
 * see kill_frosty below) - modeled on kill_twin, but the two Antartica
 * bodies never share a control point or a weapon the way the Twins do:
 * Frosty keeps flying (its own independent WASD control) and firing (its
 * own autonomous snowballs, see update_frosty_fire) even after Antartica
 * herself dies here, and Antartica's own 3-mode arsenal simply goes silent
 * once she's the one who died (see the p->antartica_alive guards in
 * update_antartica_shards/trigger_antartica_ice_storm/
 * trigger_antartica_freeze_beam). Only once BOTH are gone does the whole
 * Player die via kill_player - same "both down" rule as the Twins. */
static void kill_antartica_body(GameState *gs, EventQueue *events) {
    Player *p = &gs->player;
    if (!p->antartica_alive) return;
    if (!p->alive) return;
    if (super_beam_shields_player(gs)) return;
    if (p->god_mode) return;

    p->antartica_alive = false;
    p->antartica_life = 0.0f;
    spawn_explosion(gs, p->x, p->y, scaled(gs, PLAYER_WIDTH) * ship_size_multiplier(SHIP_ANTARTICA));
    event_queue_push_sfx(events, SFX_PLAYER_DESTROYED);

    if (!p->antartica_alive && !p->frosty_alive) {
        kill_player(gs, events);
    }
}

/* Antartica's own per-body damage_player counterpart - same immunity guards
 * and ship_damage_taken_multiplier scaling (keyed on SHIP_ANTARTICA's own
 * Strength rating for both bodies alike, same "one ship-level rating shared
 * by both hitboxes" precedent SHIP_TWINS already sets) as damage_twin, just
 * against antartica_life instead of Player.life (unused by SHIP_ANTARTICA -
 * see the Player struct's own doc comment). */
static void damage_antartica_body(GameState *gs, EventQueue *events, float amount) {
    Player *p = &gs->player;
    if (!p->alive) return;
    if (super_beam_shields_player(gs)) return;
    if (p->god_mode) return;
    if (!p->antartica_alive) return;

    p->antartica_life -= amount * ship_damage_taken_multiplier(SHIP_ANTARTICA);
    if (p->antartica_life <= 0.0f) {
        p->antartica_life = 0.0f;
        kill_antartica_body(gs, events);
    }
}

/* Frosty's own death - the counterpart to kill_antartica_body above.
 * Antartica's own 3-mode arsenal is entirely unaffected by Frosty dying
 * (she keeps flying and firing on her own); only Frosty's own autonomous
 * snowball fire stops (see update_frosty_fire's own p->frosty_alive
 * guard). */
static void kill_frosty(GameState *gs, EventQueue *events) {
    Player *p = &gs->player;
    if (!p->frosty_alive) return;
    if (!p->alive) return;
    if (super_beam_shields_player(gs)) return;
    if (p->god_mode) return;

    p->frosty_alive = false;
    p->frosty_life = 0.0f;
    spawn_explosion(gs, p->frosty_x, p->frosty_y,
                     scaled(gs, PLAYER_WIDTH) * ship_size_multiplier(SHIP_ANTARTICA) * ANTARTICA_FROSTY_SIZE_MULTIPLIER);
    event_queue_push_sfx(events, SFX_PLAYER_DESTROYED);

    if (!p->antartica_alive && !p->frosty_alive) {
        kill_player(gs, events);
    }
}

static void damage_frosty(GameState *gs, EventQueue *events, float amount) {
    Player *p = &gs->player;
    if (!p->alive) return;
    if (super_beam_shields_player(gs)) return;
    if (p->god_mode) return;
    if (!p->frosty_alive) return;

    p->frosty_life -= amount * ship_damage_taken_multiplier(SHIP_ANTARTICA);
    if (p->frosty_life <= 0.0f) {
        p->frosty_life = 0.0f;
        kill_frosty(gs, events);
    }
}

/* Shine's own mode 2 (SHOOT_MODE_SHINE_OMNI): fires SHINE_OMNI_SHOT_COUNT
 * shards in every direction at once, gated on Player.shine_omni_cooldown_timer
 * (decremented unconditionally in update_player_firing, the same pattern
 * B-20's own rapid_cooldown_timer already uses). Unlike every other mode,
 * this never assigns SHOOT_MODE_SHINE_OMNI to p->shoot_mode at all - it's
 * triggered directly from update_shoot_mode_switch below and immediately
 * leaves shoot_mode at mode 1 (SHOOT_MODE_SHINE_SHARDS), regardless of
 * whatever mode was active before the key was pressed. On cooldown, pressing
 * key 2 is simply a no-op - no sfx, no state change - same as every other
 * mode's own switch already does nothing when it's not actually available. */
static void trigger_shine_omni_burst(GameState *gs, EventQueue *events) {
    Player *p = &gs->player;
    if (p->shine_omni_cooldown_timer > 0.0f) return;

    float speed = scaled(gs, SHINE_SHARD_SPEED);
    for (int k = 0; k < SHINE_OMNI_SHOT_COUNT; k++) {
        spawn_player_shot(gs, p->x, p->y, kShineOmniDirX[k] * speed, kShineOmniDirY[k] * speed,
                           PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE);
    }
    p->shine_omni_cooldown_timer = SHINE_OMNI_COOLDOWN;
    p->shoot_mode = ship_shoot_mode_for_slot(SHIP_SHINE, 0);
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Cruzader's own mode 2 (SHOOT_MODE_CRUZADER_ORB): like
 * trigger_shine_omni_burst above, never actually assigns its own ShootMode
 * to p->shoot_mode - intercepted directly from update_shoot_mode_switch
 * below and immediately leaves shoot_mode at mode 1
 * (SHOOT_MODE_CRUZADER_TWIN). Unlike Shine's instant single burst, this
 * also starts the 5s active window (cruzader_orb_timer) check_collisions
 * reads to reflect incoming fire - see SHOOT_MODE_CRUZADER_ORB's own doc
 * comment in domain/types.h. No-ops (no sfx, no state change) while
 * already active or on cooldown, same "silently does nothing" convention
 * every other mode's own switch already follows when it's not actually
 * available. */
static void trigger_cruzader_orb(GameState *gs, EventQueue *events) {
    Player *p = &gs->player;
    if (p->cruzader_orb_timer > 0.0f || p->cruzader_orb_cooldown_timer > 0.0f) return;

    p->cruzader_orb_timer = CRUZADER_ORB_DURATION;
    p->shoot_mode = ship_shoot_mode_for_slot(SHIP_CRUZADER, 0);
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Antartica's own mode 2 (SHOOT_MODE_ANTARTICA_ICE_STORM): like
 * trigger_shine_omni_burst above, fires ANTARTICA_ICE_STORM_SHOT_COUNT
 * shards evenly spanning her own frontal ANTARTICA_ICE_STORM_SPREAD_DEG
 * degrees (centered straight up) all at once, gated on
 * Player.antartica_ice_storm_cooldown_timer, and never actually assigns its
 * own ShootMode to p->shoot_mode - intercepted directly from
 * update_shoot_mode_switch below and immediately leaves shoot_mode at mode 1
 * (SHOOT_MODE_ANTARTICA_SHARDS). Only Antartica herself fires this - Frosty
 * plays no part in it (contrast the freezing beam below). A no-op while
 * Antartica herself is dead or the burst is already on cooldown, same
 * "silently does nothing" convention every other mode's own switch already
 * follows when it's not actually available. */
static void trigger_antartica_ice_storm(GameState *gs, EventQueue *events) {
    Player *p = &gs->player;
    if (!p->antartica_alive) return;
    if (p->antartica_ice_storm_cooldown_timer > 0.0f) return;

    float speed = scaled(gs, ANTARTICA_SHARD_SPEED);
    float step_deg = ANTARTICA_ICE_STORM_SPREAD_DEG / (float)(ANTARTICA_ICE_STORM_SHOT_COUNT - 1);
    float start_deg = -ANTARTICA_ICE_STORM_SPREAD_DEG / 2.0f;
    for (int k = 0; k < ANTARTICA_ICE_STORM_SHOT_COUNT; k++) {
        float angle = deg_to_rad(start_deg + step_deg * (float)k);
        float dx = sinf(angle), dy = -cosf(angle);
        spawn_player_shot(gs, p->x, p->y, dx * speed, dy * speed, PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE);
    }
    p->antartica_ice_storm_cooldown_timer = ANTARTICA_ICE_STORM_COOLDOWN;
    p->shoot_mode = ship_shoot_mode_for_slot(SHIP_ANTARTICA, 0);
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Antartica's own mode 3 (SHOOT_MODE_ANTARTICA_FREEZE_BEAM): like
 * trigger_cruzader_orb above, starts a 5s active window
 * (antartica_freeze_beam_timer, see update_antartica_freezing_beam for the
 * actual beam sweep) and never actually assigns its own ShootMode to
 * p->shoot_mode - intercepted directly from update_shoot_mode_switch below
 * and immediately leaves shoot_mode at mode 1 (SHOOT_MODE_ANTARTICA_SHARDS).
 * Gated on Antartica herself being alive (same as Ice Storm above) - if
 * she's the one who died, only Frosty's own passive snowball fire remains,
 * her own 3-mode arsenal (freezing beam included) goes silent. */
static void trigger_antartica_freeze_beam(GameState *gs, EventQueue *events) {
    Player *p = &gs->player;
    if (!p->antartica_alive) return;
    if (p->antartica_freeze_beam_timer > 0.0f || p->antartica_freeze_beam_cooldown_timer > 0.0f) return;

    p->antartica_freeze_beam_timer = ANTARTICA_FREEZE_BEAM_DURATION;
    p->shoot_mode = ship_shoot_mode_for_slot(SHIP_ANTARTICA, 0);
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Buckler's own spacebar power: the protective orb - same defensive
 * behavior/duration/cooldown as Cruzader's own deflector orb
 * (trigger_cruzader_orb above), just triggered by the spacebar's own edge
 * (input->fire_pressed) instead of a shoot-mode key, and never reflecting
 * anything back at the enemies (see check_collisions' own SHIP_BUCKLER
 * branch, which destroys shots in range outright instead of calling
 * reflect_enemy_shot) - no passive always-on chance either, unlike
 * Cruzader's own CRUZADER_PASSIVE_REFLECT_CHANCE. Never touches
 * Player.shoot_mode - Buckler's own SHOOT_MODE_BUCKLER_CANNON never
 * changes for the whole run, unlike Cruzader's mode 1 revert. A no-op
 * (no sfx, no state change) while already active or on cooldown, same
 * "silently does nothing" convention every other unavailable-power press
 * already follows. */
static void trigger_buckler_orb(GameState *gs, EventQueue *events) {
    Player *p = &gs->player;
    if (p->buckler_orb_timer > 0.0f || p->buckler_orb_cooldown_timer > 0.0f) return;

    p->buckler_orb_timer = BUCKLER_ORB_DURATION;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Buckler's own (only) mode - see SHOOT_MODE_BUCKLER_CANNON's own doc
 * comment in domain/types.h. Indexed by cannon number (1-5) minus 1: west,
 * north-west, north, north-east, east - spanning her own frontal 180
 * degrees, matching kBucklerCannonDirX/Y's own direction below. */
static const float kBucklerCannonDirX[5] = {-1.0f, -0.70710678f, 0.0f, 0.70710678f, 1.0f};
static const float kBucklerCannonDirY[5] = {0.0f, -0.70710678f, -1.0f, -0.70710678f, 0.0f};

/* Buckler's own (only) mode: 5 fixed-direction cannons, one active at a
 * time - "the player can only shoot one cannon at a time; if two numbers
 * are pressed, only the one pressed first fires" per spec.
 * Player.buckler_active_cannon (0 = none, else 1-5) latches onto whichever
 * key is currently held the instant no cannon is already latched, and
 * un-latches the moment that same key's own held state goes false - a key
 * newly pressed while another is already latched is simply never looked at
 * until the latched one releases, which is what makes "the one pressed
 * first fires" hold even though this reads held state rather than tracking
 * press order explicitly. Keys 1-5 here are update_shoot_mode_switch's own
 * *_pressed edges everywhere else in the game; SHIP_BUCKLER is the one
 * ship that reads their *_held level state instead, and for firing rather
 * than mode-switching - see InputCommand's own shoot_mode_N_held doc
 * comment. */
static void update_buckler_cannon_fire(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;

    if (p->buckler_active_cannon != 0) {
        bool still_held;
        switch (p->buckler_active_cannon) {
            case 1: still_held = input->shoot_mode_1_held; break;
            case 2: still_held = input->shoot_mode_2_held; break;
            case 3: still_held = input->shoot_mode_3_held; break;
            case 4: still_held = input->shoot_mode_4_held; break;
            default: still_held = input->shoot_mode_5_held; break;
        }
        if (!still_held) p->buckler_active_cannon = 0;
    }

    if (p->buckler_active_cannon == 0) {
        if (input->shoot_mode_1_held) p->buckler_active_cannon = 1;
        else if (input->shoot_mode_2_held) p->buckler_active_cannon = 2;
        else if (input->shoot_mode_3_held) p->buckler_active_cannon = 3;
        else if (input->shoot_mode_4_held) p->buckler_active_cannon = 4;
        else if (input->shoot_mode_5_held) p->buckler_active_cannon = 5;
    }

    if (p->buckler_active_cannon == 0) return;
    if (p->fire_cooldown > 0.0f) return;

    int idx = p->buckler_active_cannon - 1;
    float dx = kBucklerCannonDirX[idx], dy = kBucklerCannonDirY[idx];
    float speed = scaled(gs, BUCKLER_CANNON_PROJECTILE_SPEED);
    float muzzle = scaled(gs, BUCKLER_CANNON_MUZZLE_OFFSET);
    spawn_player_shot(gs, p->x + dx * muzzle, p->y + dy * muzzle, dx * speed, dy * speed,
                       PROJECTILE_KIND_BUCKLER_ORB, false, BASE_PLAYER_DAMAGE);
    p->fire_cooldown = BUCKLER_CANNON_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Samurai's mode 1 (default): "bursts of 3 shuriken stars" - staggered one
 * at a time, SAMURAI_SHURIKEN_SHOT_INTERVAL (150ms) apart, the same
 * ENEMY_SHOOT_TRIBURST pattern (see update_enemies) reused for the player's
 * own fire, straight ahead from the nose like B-20's own mode 1. Each shot
 * deals SAMURAI_SHURIKEN_DAMAGE (2 pts - "6 total if all 3 in the burst
 * hit" a boss, per spec). While a burst is already in progress
 * (samurai_burst_shots_remaining > 0), this fires the next shot in it on
 * its own timer, completely ignoring fire_held/fire_cooldown - same as an
 * enemy's own triburst, once started a burst always finishes regardless of
 * what the trigger is doing. Only once the 3rd shot fires does
 * fire_cooldown get set (to SAMURAI_SHURIKEN_BURST_COOLDOWN, 550ms), which
 * is what actually paces one burst to the next while the key stays held. */
static void update_samurai_shuriken(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    Player *p = &gs->player;
    float speed = scaled(gs, SAMURAI_SHURIKEN_SPEED);
    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;

    if (p->samurai_burst_shots_remaining > 0) {
        p->samurai_burst_shot_timer -= dt;
        if (p->samurai_burst_shot_timer <= 0.0f) {
            p->samurai_burst_shot_timer = SAMURAI_SHURIKEN_SHOT_INTERVAL;
            p->samurai_burst_shots_remaining--;
            spawn_player_shot(gs, p->x, y, 0.0f, -speed, PROJECTILE_KIND_SAMURAI_SHURIKEN, false,
                               SAMURAI_SHURIKEN_DAMAGE);
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
            if (p->samurai_burst_shots_remaining == 0) {
                p->fire_cooldown = SAMURAI_SHURIKEN_BURST_COOLDOWN;
            }
        }
        return;
    }

    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    spawn_player_shot(gs, p->x, y, 0.0f, -speed, PROJECTILE_KIND_SAMURAI_SHURIKEN, false, SAMURAI_SHURIKEN_DAMAGE);
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
    p->samurai_burst_shots_remaining = SAMURAI_SHURIKEN_BURST_COUNT - 1;
    p->samurai_burst_shot_timer = SAMURAI_SHURIKEN_SHOT_INTERVAL;
}

/* Samurai's mode 2: the 180-degree sweep - see SHOOT_MODE_SAMURAI_OMNI's own
 * doc comment in domain/types.h. Unlike every other special mode in the
 * game, this one DOES stay Player.shoot_mode for its whole 1s active window
 * (samurai_omni_burst_timer, started the instant update_shoot_mode_switch
 * switches into this mode) - called every frame from update_player_firing's
 * own switch purely because shoot_mode currently equals this value, exactly
 * the same "called only while selected" convention update_rapid_fire
 * already uses for B-20's own burst. Fires SAMURAI_OMNI_SHOT_COUNT shots,
 * one every SAMURAI_OMNI_SHOT_INTERVAL seconds, sweeping from due west
 * (samurai_omni_next_shot_index 0) toward the east in SAMURAI_OMNI_STEP_DEG
 * increments. The instant the window ends, shoot_mode reverts to mode 1 and
 * SAMURAI_OMNI_COOLDOWN's own lockout begins - mirroring
 * update_rapid_fire's own auto-switch-back-and-cooldown structure exactly,
 * just with a sweep instead of a straight-up volley. */
static void update_samurai_omni_fire(GameState *gs, float dt, EventQueue *events) {
    Player *p = &gs->player;

    p->samurai_omni_burst_timer -= dt;
    if (p->samurai_omni_burst_timer <= 0.0f) {
        p->samurai_omni_burst_timer = 0.0f;
        p->samurai_omni_cooldown_timer = SAMURAI_OMNI_COOLDOWN;
        p->shoot_mode = ship_shoot_mode_for_slot(gs->selected_ship, 0);
        return;
    }

    if (p->samurai_omni_shot_timer > 0.0f) p->samurai_omni_shot_timer -= dt;
    if (p->samurai_omni_shot_timer <= 0.0f && p->samurai_omni_next_shot_index < SAMURAI_OMNI_SHOT_COUNT) {
        int i = p->samurai_omni_next_shot_index;
        float angle = deg_to_rad(-90.0f + (float)i * SAMURAI_OMNI_STEP_DEG);
        float dx = sinf(angle), dy = -cosf(angle);
        float speed = scaled(gs, SAMURAI_SHURIKEN_SPEED);
        spawn_player_shot(gs, p->x, p->y, dx * speed, dy * speed, PROJECTILE_KIND_SAMURAI_SHURIKEN, false,
                           SAMURAI_SHURIKEN_DAMAGE);
        p->samurai_omni_next_shot_index++;
        p->samurai_omni_shot_timer = SAMURAI_OMNI_SHOT_INTERVAL;
        event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
    }
}

/* Samurai's mode 3: stealth - see SHOOT_MODE_SAMURAI_STEALTH's own doc
 * comment in domain/types.h. Same "persists as shoot_mode for its own
 * active window, called every frame purely because it's currently selected"
 * shape as update_samurai_omni_fire above, just firing nothing at all - the
 * 50% transparency (adapters/sdl_renderer.c's own draw_player), the
 * SAMURAI_STEALTH_SPEED_MULTIPLIER speed bonus (update_player), and the
 * full attack/collision immunity (check_collisions' own SHIP_SAMURAI
 * branches) are every bit of this power's real effect, all keyed directly
 * off Player.samurai_stealth_timer > 0 rather than anything this function
 * does itself. Reverts to mode 1 and starts SAMURAI_STEALTH_COOLDOWN's own
 * lockout the instant the window ends, same auto-revert timing as mode 2. */
static void update_samurai_stealth(GameState *gs, float dt) {
    Player *p = &gs->player;

    p->samurai_stealth_timer -= dt;
    if (p->samurai_stealth_timer <= 0.0f) {
        p->samurai_stealth_timer = 0.0f;
        p->samurai_stealth_cooldown_timer = SAMURAI_STEALTH_COOLDOWN;
        p->shoot_mode = ship_shoot_mode_for_slot(gs->selected_ship, 0);
    }
}

/* Ranger's own mode 1 (default): all 3 heads (center/left/right) fire at
 * once, straight up, every trigger - see RANGER_SIDE_MUZZLE_OFFSET_X and
 * SHOOT_MODE_RANGER_TRIBEAM's own doc comment in domain/types.h. All 3
 * beams this trigger share one freshly rerolled random color
 * (ranger_random_shot_color), so the volley always reads as one cohesive
 * burst rather than 3 independently-colored beams. */
static void update_ranger_tribeam(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float side_x = scaled(gs, RANGER_SIDE_MUZZLE_OFFSET_X);
    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
    float vy = -scaled(gs, RANGER_BEAM_SPEED);
    Color color = ranger_random_shot_color();
    spawn_player_shot_colored(gs, p->x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE,
                               SHIP_RANGER, color);
    spawn_player_shot_colored(gs, p->x - side_x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE,
                               SHIP_RANGER, color);
    spawn_player_shot_colored(gs, p->x + side_x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE,
                               SHIP_RANGER, color);
    p->fire_cooldown = RANGER_TRIBEAM_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Ranger's own mode 2: the same 3 heads as mode 1, fired one at a time in a
 * fixed rotating order (center, then left, then right, repeating) instead
 * of all 3 at once - see Player.ranger_next_muzzle and
 * SHOOT_MODE_RANGER_ALTERNATE's own doc comment in domain/types.h. Each
 * individual shot rerolls its own random color independently, unlike mode
 * 1's one-color-per-volley sharing - there's only ever one shot in flight
 * from this mode at a time, so there's no volley to keep visually cohesive. */
static void update_ranger_alternate(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float side_x = scaled(gs, RANGER_SIDE_MUZZLE_OFFSET_X);
    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
    float vy = -scaled(gs, RANGER_BEAM_SPEED);
    float x = p->x;
    if (p->ranger_next_muzzle == 1) x = p->x - side_x;
    else if (p->ranger_next_muzzle == 2) x = p->x + side_x;

    spawn_player_shot_colored(gs, x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE, SHIP_RANGER,
                               ranger_random_shot_color());
    p->ranger_next_muzzle = (p->ranger_next_muzzle + 1) % 3;
    p->fire_cooldown = RANGER_ALTERNATE_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Ranger's own mode 3: the arch-shaped wave - fired straight up from the
 * nose like mode 1's center beam, but as a single PROJECTILE_KIND_RANGER_ARC
 * shot instead of a beam (see that kind's own doc comment in domain/types.h
 * for the pierce-through behavior check_collisions gives it). Far slower
 * cadence than modes 1/2 (RANGER_ARC_WAVE_FIRE_COOLDOWN) and a fresh random
 * color each time, same "colorful" random-hue reroll as every other Ranger
 * shot. */
static void update_ranger_arc_wave(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
    float vy = -scaled(gs, RANGER_ARC_WAVE_SPEED);
    spawn_player_shot_colored(gs, p->x, y, 0.0f, vy, PROJECTILE_KIND_RANGER_ARC, false, BASE_PLAYER_DAMAGE,
                               SHIP_RANGER, ranger_random_shot_color());
    p->fire_cooldown = RANGER_ARC_WAVE_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
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

    /* Buckler's own keys 1-5 aren't a mode switch at all - see
     * SHOOT_MODE_BUCKLER_CANNON's own doc comment in domain/types.h and
     * update_buckler_cannon_fire, which reads the same keys' *_held state
     * directly from update_player_firing's own switch instead. */
    if (gs->selected_ship == SHIP_BUCKLER) return;

    /* Samurai's own sweep (mode 2) and stealth (mode 3) lock out mode
     * switching entirely for their whole active window - the player is
     * committed, same "can't interrupt an in-progress special" rule
     * rapid_burst_timer's own guard above already gives B-20's own rapid
     * fire. */
    if (p->samurai_omni_burst_timer > 0.0f || p->samurai_stealth_timer > 0.0f) return;

    int slot = -1;
    if (input->shoot_mode_1_pressed) slot = 0;
    else if (input->shoot_mode_2_pressed) slot = 1;
    else if (input->shoot_mode_3_pressed) slot = 2;
    else if (input->shoot_mode_4_pressed) slot = 3;
    else if (input->shoot_mode_5_pressed) slot = 4;
    if (slot < 0 || slot >= ship_shoot_mode_slot_count(gs->selected_ship)) return;

    ShootMode requested = ship_shoot_mode_for_slot(gs->selected_ship, slot);
    if (requested == SHOOT_MODE_SHINE_OMNI) {
        trigger_shine_omni_burst(gs, events);
        return;
    }
    if (requested == SHOOT_MODE_CRUZADER_ORB) {
        trigger_cruzader_orb(gs, events);
        return;
    }
    if (requested == SHOOT_MODE_ANTARTICA_ICE_STORM) {
        trigger_antartica_ice_storm(gs, events);
        return;
    }
    if (requested == SHOOT_MODE_ANTARTICA_FREEZE_BEAM) {
        trigger_antartica_freeze_beam(gs, events);
        return;
    }
    if (requested == SHOOT_MODE_RAPID && p->rapid_cooldown_timer > 0.0f) return;
    if (requested == SHOOT_MODE_SAMURAI_OMNI && p->samurai_omni_cooldown_timer > 0.0f) return;
    if (requested == SHOOT_MODE_SAMURAI_STEALTH && p->samurai_stealth_cooldown_timer > 0.0f) return;
    /* Once one twin is down, mode 2 (mirroring the other twin's own
     * movement) is meaningless - there's nothing left to mirror. Locked
     * out entirely for the rest of the run, same "just a no-op" language
     * as every other unavailable-mode press (RAPID's own cooldown just
     * above) - see kill_twin's own doc comment for the forced switch back
     * to mode 1 the instant this happens, and draw_shoot_mode_indicator
     * for how this reads as a permanently red slot 2. */
    if (requested == SHOOT_MODE_TWINS_MIRROR &&
        !(p->twins_right_alive && p->twins_left_alive)) {
        return;
    }
    if (requested == p->shoot_mode) return;

    /* The Twins' own mode transitions re-anchor the shared control point
     * (x) and the mirror reflection axis (twins_mirror_center_x) to
     * wherever the twins actually currently are, right at the moment the
     * mode changes - never to some stale value left over from before the
     * ship last switched away from that mode. That's what keeps both
     * transitions jump-free: entering mirror mode (2) starts the right
     * twin's own free flight from its own current x, and the left twin
     * mirroring from the current midpoint (so it starts exactly where it
     * already is, not reflected around a screen-center formula it may
     * never have been symmetric around); entering formation mode (1) sets
     * the new formation center to the twins' own current midpoint too, so
     * update_player's own eased catch-up (TWINS_FORMATION_REJOIN_SPEED)
     * has them fly toward each other from wherever they actually are
     * instead of snapping onto (or racing toward) a stale, unrelated
     * center. */
    if (requested == SHOOT_MODE_TWINS_MIRROR) {
        p->twins_mirror_center_x = (p->twins_right_x + p->twins_left_x) / 2.0f;
        p->x = p->twins_right_x;
    } else if (requested == SHOOT_MODE_TWINS_ALTERNATE) {
        p->x = (p->twins_right_x + p->twins_left_x) / 2.0f;
    }

    p->shoot_mode = requested;

    /* Samurai's own mode 2/3 kick off their active-window state right at
     * the moment they're actually selected - unlike every "trigger +
     * immediate revert" mode above, requested == p->shoot_mode now stays
     * true for the whole window (see update_samurai_omni_fire/
     * update_samurai_stealth, both dispatched every frame from
     * update_player_firing's own switch purely because shoot_mode still
     * equals this value), so this is the one and only place either state
     * gets initialized. */
    if (requested == SHOOT_MODE_SAMURAI_OMNI) {
        p->samurai_omni_burst_timer = SAMURAI_OMNI_DURATION;
        p->samurai_omni_next_shot_index = 0;
        p->samurai_omni_shot_timer = 0.0f;
    } else if (requested == SHOOT_MODE_SAMURAI_STEALTH) {
        p->samurai_stealth_timer = SAMURAI_STEALTH_DURATION;
    }

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

/* The Twins' own weapon, shared by both of their moveset slots
 * (SHOOT_MODE_TWINS_ALTERNATE and SHOOT_MODE_TWINS_MIRROR - mode 2 changes
 * flight only, never firing, see update_player's own SHIP_TWINS branch):
 * one shot per activation, alternating muzzle between the two twins' own
 * cached x positions (twins_next_shot_is_right) so together they reach
 * TWINS_ALTERNATE_FIRE_COOLDOWN's combined 6 shots/sec, 3/sec from either
 * twin alone - loosely "C-24's own mode 1" in spirit (a wingtip-style
 * pattern from two source points), but a single alternating shot per
 * activation rather than a simultaneous pair, at Twins' own explicitly
 * specced rate instead of double-barrel's. Once one twin has died, every
 * shot simply comes from whichever twin survives - the toggle only flips
 * while both are still alive. */
static void update_twins_alternating_fire(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;
    if (!p->twins_right_alive && !p->twins_left_alive) return;

    bool fire_right = p->twins_next_shot_is_right;
    if (fire_right && !p->twins_right_alive) fire_right = false;
    if (!fire_right && !p->twins_left_alive) fire_right = true;

    float x = fire_right ? p->twins_right_x : p->twins_left_x;
    float y = p->y - scaled(gs, PLAYER_HEIGHT) * TWINS_SIZE_MULTIPLIER / 2.0f;
    float vy = -scaled(gs, PLAYER_PROJECTILE_SPEED);
    spawn_player_shot(gs, x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, BASE_PLAYER_DAMAGE);

    if (p->twins_right_alive && p->twins_left_alive) {
        p->twins_next_shot_is_right = !fire_right;
    }
    p->fire_cooldown = TWINS_ALTERNATE_FIRE_COOLDOWN;
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

/* Shine's own mode 1 (default): twin crystal shards fired straight from
 * the nose, deliberately close together rather than wingtip-spaced like
 * B-20's own DOUBLE mode - see SHINE_TWIN_SHARD_OFFSET_X's own doc comment
 * for the exact "one shard-width of gap" spacing. Each shard's damage is
 * halved (SHINE_TWIN_SHARD_DAMAGE_MULTIPLIER), the same "two shots cost
 * the same total as one" precedent DOUBLE_BARREL_DAMAGE_MULTIPLIER already
 * sets for a twin-shot mode. */
static void update_shine_shards(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float offset = scaled(gs, SHINE_TWIN_SHARD_OFFSET_X);
    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
    float vy = -scaled(gs, SHINE_SHARD_SPEED);
    float damage = BASE_PLAYER_DAMAGE * SHINE_TWIN_SHARD_DAMAGE_MULTIPLIER;
    spawn_player_shot(gs, p->x - offset, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, damage);
    spawn_player_shot(gs, p->x + offset, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, damage);
    p->fire_cooldown = SHINE_SHARDS_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Antartica's own mode 1 (default): the same twin-shard pattern as
 * update_shine_shards above (same spacing/damage-halving construction),
 * just her own kept-independent ANTARTICA_* constants and style_ship (see
 * draw_antartica_shard in adapters/sdl_renderer.c for the ice recolor).
 * Gated on Antartica herself being alive - if she's the one who died, her
 * own 3-mode arsenal goes silent (Frosty's own passive fire is unaffected,
 * see update_frosty_fire below). */
static void update_antartica_shards(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!p->antartica_alive) return;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float offset = scaled(gs, ANTARTICA_TWIN_SHARD_OFFSET_X);
    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
    float vy = -scaled(gs, ANTARTICA_SHARD_SPEED);
    float damage = BASE_PLAYER_DAMAGE * ANTARTICA_TWIN_SHARD_DAMAGE_MULTIPLIER;
    spawn_player_shot(gs, p->x - offset, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, damage);
    spawn_player_shot(gs, p->x + offset, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, damage);
    p->fire_cooldown = ANTARTICA_SHARDS_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Frosty's own passive weapon - fires automatically at
 * FROSTY_SNOWBALL_FIRE_COOLDOWN's flat rate ("2 shots per second" per spec)
 * whenever alive, completely independent of Antartica's own fire key or
 * shoot_mode (see update_player_firing, which calls this unconditionally
 * once past the gates every other ship's own fire dispatch respects -
 * the power orb's own super beam and Antartica's own freezing beam both
 * pause it, same as everything else). Snowballs, not shards - see
 * PROJECTILE_KIND_FROSTY_SNOWBALL/draw_frosty_snowball. */
static void update_frosty_fire(GameState *gs, float dt, EventQueue *events) {
    Player *p = &gs->player;
    if (p->frosty_fire_cooldown > 0.0f) p->frosty_fire_cooldown -= dt;
    if (!p->frosty_alive) return;
    if (p->frosty_fire_cooldown > 0.0f) return;

    float y = p->frosty_y - scaled(gs, PLAYER_HEIGHT) * ANTARTICA_FROSTY_SIZE_MULTIPLIER / 2.0f;
    spawn_player_shot(gs, p->frosty_x, y, 0.0f, -scaled(gs, FROSTY_SNOWBALL_SPEED),
                       PROJECTILE_KIND_FROSTY_SNOWBALL, false, BASE_PLAYER_DAMAGE);
    p->frosty_fire_cooldown = FROSTY_SNOWBALL_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Shine's own mode 3: a single longer shard (PROJECTILE_KIND_SHINE_SPIRAL)
 * fired straight from the nose at "2 shots per second"
 * (SHINE_SPIRAL_FIRE_COOLDOWN) - the same single-shot cadence pattern as
 * update_normal_fire, just this ship's own projectile kind/speed/cooldown,
 * at triple damage (SHINE_SPIRAL_DAMAGE_MULTIPLIER). The visual spin
 * (draw_shine_shard in adapters/sdl_renderer.c) is purely cosmetic - travel
 * is still straight up, only the drawn orientation rotates. */
static void update_shine_spiral(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    spawn_player_shot(gs, p->x, p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f,
                       0.0f, -scaled(gs, SHINE_SHARD_SPEED), PROJECTILE_KIND_SHINE_SPIRAL, false,
                       BASE_PLAYER_DAMAGE * SHINE_SPIRAL_DAMAGE_MULTIPLIER);
    p->fire_cooldown = SHINE_SPIRAL_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Cruzader's own mode 1 (default): B-20's own DOUBLE pattern (two wingtip
 * shots), just recolored (see draw_cruzader_bolt in adapters/sdl_renderer.c)
 * and at CRUZADER_TWIN_FIRE_COOLDOWN's own 1.5 shots/sec instead of
 * PLAYER_FIRE_COOLDOWN - "same as B-20's #4" per spec, so this reuses
 * B-20's own DOUBLE_BARREL_DAMAGE_MULTIPLIER rather than a Cruzader-specific
 * one. Wingtip/nose offsets are scaled by Cruzader's own
 * ship_size_multiplier (1.5x) - the first firing ship bigger than B-20's
 * own baseline, so muzzle points need to track the bigger sprite's actual
 * wingtips instead of assuming the stock PLAYER_WING_OFFSET_X/HEIGHT. */
static void update_cruzader_twin(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float size_mult = ship_size_multiplier(SHIP_CRUZADER);
    float wing_x = scaled(gs, PLAYER_WING_OFFSET_X) * size_mult;
    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f * size_mult;
    float vy = -scaled(gs, PLAYER_PROJECTILE_SPEED);
    float damage = BASE_PLAYER_DAMAGE * DOUBLE_BARREL_DAMAGE_MULTIPLIER;
    spawn_player_shot(gs, p->x - wing_x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, damage);
    spawn_player_shot(gs, p->x + wing_x, y, 0.0f, vy, PROJECTILE_KIND_NORMAL, false, damage);
    p->fire_cooldown = CRUZADER_TWIN_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Cruzader's own mode 3: a single slow rocket from the nose (offset by his
 * own size multiplier, same reasoning as update_cruzader_twin above) every
 * CRUZADER_ROCKET_FIRE_COOLDOWN (1 shot/2s per spec) - see
 * update_cruzader_rocket_homing (update_projectiles) for the actual
 * closest-enemy homing, and check_collisions for its Power-Cannon-style
 * explosion on contact. Initial heading is straight up, same as every
 * other single-shot mode; homing takes over from the very next frame. */
static void update_cruzader_rockets(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    float size_mult = ship_size_multiplier(SHIP_CRUZADER);
    float y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f * size_mult;
    float speed = scaled(gs, CRUZADER_ROCKET_SPEED);
    spawn_player_shot(gs, p->x, y, 0.0f, -speed, PROJECTILE_KIND_CRUZADER_ROCKET, false, CRUZADER_ROCKET_DAMAGE);
    p->fire_cooldown = CRUZADER_ROCKET_FIRE_COOLDOWN;
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
 * rapid_cooldown_timer and shine_omni_cooldown_timer are decremented here
 * too, unconditionally on dt and regardless of shoot_mode - the former has
 * to run independently of update_rapid_fire now that shoot_mode is
 * auto-switched away to slot 0 the instant the burst ends (see
 * update_rapid_fire), so update_rapid_fire itself is never reached again
 * while the cooldown is actually running; the latter never had a mode of
 * its own to run inside in the first place (see SHOOT_MODE_SHINE_OMNI's
 * own doc comment) - this is the only place it ever ticks down at all. */
static void update_player_firing(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    Player *p = &gs->player;
    if (p->fire_cooldown > 0.0f) p->fire_cooldown -= dt;
    if (p->rapid_cooldown_timer > 0.0f) {
        p->rapid_cooldown_timer -= dt;
        if (p->rapid_cooldown_timer < 0.0f) p->rapid_cooldown_timer = 0.0f;
    }
    if (p->shine_omni_cooldown_timer > 0.0f) {
        p->shine_omni_cooldown_timer -= dt;
        if (p->shine_omni_cooldown_timer < 0.0f) p->shine_omni_cooldown_timer = 0.0f;
    }
    /* cruzader_orb_timer/cruzader_orb_cooldown_timer tick unconditionally,
     * same as shine_omni_cooldown_timer above - not gated on shoot_mode,
     * since the orb stays active while shoot_mode has already reverted to
     * mode 1 (see trigger_cruzader_orb). The instant the active window
     * expires, the following cooldown starts immediately - mutually
     * exclusive, same convention as rapid_burst_timer/rapid_cooldown_timer. */
    if (p->cruzader_orb_timer > 0.0f) {
        p->cruzader_orb_timer -= dt;
        if (p->cruzader_orb_timer <= 0.0f) {
            p->cruzader_orb_timer = 0.0f;
            p->cruzader_orb_cooldown_timer = CRUZADER_ORB_COOLDOWN;
        }
    } else if (p->cruzader_orb_cooldown_timer > 0.0f) {
        p->cruzader_orb_cooldown_timer -= dt;
        if (p->cruzader_orb_cooldown_timer < 0.0f) p->cruzader_orb_cooldown_timer = 0.0f;
    }
    /* buckler_orb_timer/buckler_orb_cooldown_timer tick the same
     * unconditional, two-phase way cruzader_orb_timer/cooldown do just
     * above - unused by every other ship. */
    if (p->buckler_orb_timer > 0.0f) {
        p->buckler_orb_timer -= dt;
        if (p->buckler_orb_timer <= 0.0f) {
            p->buckler_orb_timer = 0.0f;
            p->buckler_orb_cooldown_timer = BUCKLER_ORB_COOLDOWN;
        }
    } else if (p->buckler_orb_cooldown_timer > 0.0f) {
        p->buckler_orb_cooldown_timer -= dt;
        if (p->buckler_orb_cooldown_timer < 0.0f) p->buckler_orb_cooldown_timer = 0.0f;
    }
    if (gs->selected_ship == SHIP_BUCKLER && input->fire_pressed) {
        trigger_buckler_orb(gs, events);
    }
    /* samurai_omni_cooldown_timer/samurai_stealth_cooldown_timer tick the
     * same unconditional way as shine_omni_cooldown_timer above - unused by
     * every other ship. Their own active-window timers (samurai_omni_
     * burst_timer/samurai_stealth_timer) are NOT ticked here: unlike every
     * other special mode's own active timer, they only ever count down
     * while shoot_mode still actually equals that mode (see
     * update_samurai_omni_fire/update_samurai_stealth, both reached only
     * through the switch below), since - unlike Cruzader's orb or
     * Antartica's freeze beam - Samurai's own modes 2/3 stay selected for
     * their entire window rather than reverting the instant they're
     * triggered. */
    if (p->samurai_omni_cooldown_timer > 0.0f) {
        p->samurai_omni_cooldown_timer -= dt;
        if (p->samurai_omni_cooldown_timer < 0.0f) p->samurai_omni_cooldown_timer = 0.0f;
    }
    if (p->samurai_stealth_cooldown_timer > 0.0f) {
        p->samurai_stealth_cooldown_timer -= dt;
        if (p->samurai_stealth_cooldown_timer < 0.0f) p->samurai_stealth_cooldown_timer = 0.0f;
    }
    /* antartica_ice_storm_cooldown_timer ticks the same unconditional way as
     * shine_omni_cooldown_timer above - unused by every other ship, so this
     * is a harmless no-op for them. antartica_freeze_beam_timer/cooldown
     * tick the same two-phase way cruzader_orb_timer/cooldown do just
     * above, for the same reason (the beam stays active while shoot_mode
     * has already reverted to mode 1 - see trigger_antartica_freeze_beam). */
    if (p->antartica_ice_storm_cooldown_timer > 0.0f) {
        p->antartica_ice_storm_cooldown_timer -= dt;
        if (p->antartica_ice_storm_cooldown_timer < 0.0f) p->antartica_ice_storm_cooldown_timer = 0.0f;
    }
    if (p->antartica_freeze_beam_timer > 0.0f) {
        p->antartica_freeze_beam_timer -= dt;
        if (p->antartica_freeze_beam_timer <= 0.0f) {
            p->antartica_freeze_beam_timer = 0.0f;
            p->antartica_freeze_beam_cooldown_timer = ANTARTICA_FREEZE_BEAM_COOLDOWN;
        }
    } else if (p->antartica_freeze_beam_cooldown_timer > 0.0f) {
        p->antartica_freeze_beam_cooldown_timer -= dt;
        if (p->antartica_freeze_beam_cooldown_timer < 0.0f) p->antartica_freeze_beam_cooldown_timer = 0.0f;
    }

    /* While the super beam is active it replaces every shooting mode
     * entirely - see update_super_beam, which fires automatically every
     * frame on its own. */
    if (p->super_beam_timer > 0.0f) return;

    /* Antartica's own freezing beam (mode 3) replaces her own shard fire the
     * same way the super beam replaces every mode above, for as long as it
     * stays active (see update_antartica_freezing_beam, called separately
     * from update_running - the actual sweep runs regardless of this early
     * return). Frosty's own passive snowball fire pauses too while either
     * beam is up (it's busy emitting the freezing beam alongside Antartica
     * during that one); otherwise it fires on its own timer regardless of
     * Antartica's own shoot_mode. */
    if (gs->selected_ship == SHIP_ANTARTICA) {
        if (p->antartica_freeze_beam_timer > 0.0f) return;
        update_frosty_fire(gs, dt, events);
    }

    switch (p->shoot_mode) {
        case SHOOT_MODE_RAPID: update_rapid_fire(gs, input, dt, events); break;
        case SHOOT_MODE_POWER: update_power_cannon(gs, input, events); break;
        case SHOOT_MODE_DOUBLE: update_double_barrel(gs, input, events); break;
        case SHOOT_MODE_SIDE: update_side_beams(gs, input, events); break;
        case SHOOT_MODE_OMNI: update_omni_burst(gs, input, events); break;
        case SHOOT_MODE_SWARM_WANDER:
        case SHOOT_MODE_SWARM_FORMATION: update_mothership_dispatch(gs, input, events); break;
        case SHOOT_MODE_SHINE_SHARDS: update_shine_shards(gs, input, events); break;
        case SHOOT_MODE_SHINE_SPIRAL: update_shine_spiral(gs, input, events); break;
        case SHOOT_MODE_CRUZADER_TWIN: update_cruzader_twin(gs, input, events); break;
        case SHOOT_MODE_CRUZADER_ROCKETS: update_cruzader_rockets(gs, input, events); break;
        case SHOOT_MODE_TWINS_ALTERNATE:
        case SHOOT_MODE_TWINS_MIRROR: update_twins_alternating_fire(gs, input, events); break;
        case SHOOT_MODE_ANTARTICA_SHARDS: update_antartica_shards(gs, input, events); break;
        case SHOOT_MODE_BUCKLER_CANNON: update_buckler_cannon_fire(gs, input, events); break;
        case SHOOT_MODE_SAMURAI_SHURIKEN: update_samurai_shuriken(gs, input, dt, events); break;
        case SHOOT_MODE_SAMURAI_OMNI: update_samurai_omni_fire(gs, dt, events); break;
        case SHOOT_MODE_SAMURAI_STEALTH: update_samurai_stealth(gs, dt); break;
        case SHOOT_MODE_RANGER_TRIBEAM: update_ranger_tribeam(gs, input, events); break;
        case SHOOT_MODE_RANGER_ALTERNATE: update_ranger_alternate(gs, input, events); break;
        case SHOOT_MODE_RANGER_ARC: update_ranger_arc_wave(gs, input, events); break;
        case SHOOT_MODE_SHINE_OMNI: /* never persists as the active mode - see its own doc comment */
        case SHOOT_MODE_CRUZADER_ORB: /* never persists as the active mode - see its own doc comment */
        case SHOOT_MODE_ANTARTICA_ICE_STORM: /* never persists as the active mode - see its own doc comment */
        case SHOOT_MODE_ANTARTICA_FREEZE_BEAM: /* never persists as the active mode - see its own doc comment */
        case SHOOT_MODE_NORMAL:
        case SHOOT_MODE_COUNT:
        default: update_normal_fire(gs, input, events); break;
    }
}

/* Steers a single scalar toward target by at most max_step - the 1D
 * analogue of mothership_formation_slot's own dx/dy steering above, used by
 * The Twins' own formation catch-up (see update_player's SHIP_TWINS
 * branch): snaps to target outright once within one step of it, otherwise
 * moves max_step in target's direction. */
static float ease_toward_1d(float current, float target, float max_step) {
    float diff = target - current;
    if (fabsf(diff) <= max_step) return target;
    return current + (diff > 0.0f ? max_step : -max_step);
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
    if (samurai_stealth_active(gs)) speed *= SAMURAI_STEALTH_SPEED_MULTIPLIER;

    float size_mult = ship_size_multiplier(gs->selected_ship);
    float half_w = scaled(gs, PLAYER_WIDTH) * size_mult / 2.0f;
    float min_y = scaled(gs, PLAYER_HEIGHT) * size_mult / 2.0f; /* free to roam the whole screen, not just the lower band */
    float max_y = (float)gs->screen_h - scaled(gs, PLAYER_BOTTOM_MARGIN);

    if (gs->selected_ship == SHIP_TWINS) {
        /* y is always shared - both twins move vertically together. x is
         * the single input-driven control point; how it maps onto each
         * twin's own actual position depends on flight mode and on whether
         * both twins are still alive (see the Player struct's own doc
         * comment and kill_twin's control-transfer step). */
        p->y += dy * speed * dt;
        if (p->y < min_y) p->y = min_y;
        if (p->y > max_y) p->y = max_y;

        bool both_alive = p->twins_right_alive && p->twins_left_alive;
        if (both_alive && p->shoot_mode == SHOOT_MODE_TWINS_MIRROR) {
            /* Mode 2: the right twin is directly, fully input-driven
             * across the whole screen width; the left twin mirrors its
             * position around twins_mirror_center_x - re-anchored to the
             * twins' own current midpoint the instant this mode activates
             * (see update_shoot_mode_switch), not always screen-center, so
             * the left twin always starts mirroring from wherever it
             * actually already was. */
            p->x += dx * speed * dt;
            if (p->x < half_w) p->x = half_w;
            if (p->x > (float)gs->screen_w - half_w) p->x = (float)gs->screen_w - half_w;
            p->twins_right_x = p->x;
            float mirrored = 2.0f * p->twins_mirror_center_x - p->x;
            if (mirrored < half_w) mirrored = half_w;
            if (mirrored > (float)gs->screen_w - half_w) mirrored = (float)gs->screen_w - half_w;
            p->twins_left_x = mirrored;
        } else if (both_alive) {
            /* Mode 1 (default): rigid formation - x is the shared,
             * input-driven formation center, kept TWINS_FORMATION_GAP
             * apart. The twins' own actual positions EASE toward that
             * center +/- half the gap (TWINS_FORMATION_REJOIN_SPEED)
             * rather than snapping there outright, so returning from mode
             * 2 (where they may have drifted far apart) reads as flying
             * toward each other - same steer-toward-target technique as
             * mothership_formation_slot's own child catch-up. Once
             * they're already at their slot (the overwhelmingly common
             * case - every frame that isn't the first one or two after a
             * mode switch) this is a no-op, identical to the direct
             * assignment it replaces. */
            float half_gap = scaled(gs, TWINS_FORMATION_GAP) / 2.0f;
            float min_x = half_w + half_gap;
            float max_x = (float)gs->screen_w - half_w - half_gap;
            p->x += dx * speed * dt;
            if (p->x < min_x) p->x = min_x;
            if (p->x > max_x) p->x = max_x;
            float rejoin_step = scaled(gs, TWINS_FORMATION_REJOIN_SPEED) * dt;
            p->twins_right_x = ease_toward_1d(p->twins_right_x, p->x + half_gap, rejoin_step);
            p->twins_left_x = ease_toward_1d(p->twins_left_x, p->x - half_gap, rejoin_step);
        } else {
            /* Solo: whichever twin survives is now under direct control -
             * kill_twin already snapped x onto its position, so plain
             * integration from here on just naturally makes the right key
             * move it right, regardless of which mode/offset was active
             * when its sibling died. */
            p->x += dx * speed * dt;
            if (p->x < half_w) p->x = half_w;
            if (p->x > (float)gs->screen_w - half_w) p->x = (float)gs->screen_w - half_w;
            if (p->twins_right_alive) p->twins_right_x = p->x;
            if (p->twins_left_alive) p->twins_left_x = p->x;
        }
    } else if (gs->selected_ship == SHIP_ANTARTICA) {
        /* Antartica's own sidekick Frosty is independently, fully
         * controlled by WASD, while arrow keys alone drive Antartica
         * herself - the one ship in the fleet with two separate,
         * simultaneously-usable control schemes instead of a single shared
         * input stream (contrast The Twins above, whose two bodies share
         * one control point). Each body clamps to the screen and moves on
         * its own; a dead body's own controls simply stop mattering, since
         * nothing reads that body's position once its own alive flag is
         * false - EXCEPT p->x/p->y themselves, kept mirrored onto whoever's
         * actually still alive below, since those two fields are still what
         * every ship-agnostic single-position consumer reads as "the
         * player" (the boss's own chase target in update_boss, the
         * whole-player death explosion in kill_player). Without this,
         * killing Antartica alone would freeze p->x/p->y at her own last
         * position forever - the boss would keep chasing a spot Frosty
         * isn't anywhere near, and the power orb's own super beam would
         * keep drawing Frosty's column starting from Antartica's old y
         * (see player_beam_origins below, which reads Frosty's own
         * frosty_x/frosty_y directly and so is unaffected by this either
         * way - this mirroring is for every OTHER generic consumer that
         * only ever reads gs->player.x/y). Same rationale as kill_twin's
         * own control-transfer step for a lone surviving twin, just applied
         * every frame here (via a fallback sync) rather than once at the
         * moment of death, since Antartica/Frosty don't share one
         * continuously-driven control point to begin with. */
        float adx = 0.0f, ady = 0.0f;
        if (input->arrow_left) adx -= 1.0f;
        if (input->arrow_right) adx += 1.0f;
        if (input->arrow_up) ady -= 1.0f;
        if (input->arrow_down) ady += 1.0f;
        if (adx != 0.0f && ady != 0.0f) {
            const float inv_sqrt2 = 0.70710678f;
            adx *= inv_sqrt2;
            ady *= inv_sqrt2;
        }
        if (p->antartica_alive) {
            p->x += adx * speed * dt;
            p->y += ady * speed * dt;
            if (p->x < half_w) p->x = half_w;
            if (p->x > (float)gs->screen_w - half_w) p->x = (float)gs->screen_w - half_w;
            if (p->y < min_y) p->y = min_y;
            if (p->y > max_y) p->y = max_y;
        }

        float fdx = 0.0f, fdy = 0.0f;
        if (input->wasd_left) fdx -= 1.0f;
        if (input->wasd_right) fdx += 1.0f;
        if (input->wasd_up) fdy -= 1.0f;
        if (input->wasd_down) fdy += 1.0f;
        if (fdx != 0.0f && fdy != 0.0f) {
            const float inv_sqrt2 = 0.70710678f;
            fdx *= inv_sqrt2;
            fdy *= inv_sqrt2;
        }
        if (p->frosty_alive) {
            float frosty_half_w = half_w * ANTARTICA_FROSTY_SIZE_MULTIPLIER;
            float frosty_min_y = min_y * ANTARTICA_FROSTY_SIZE_MULTIPLIER;
            p->frosty_x += fdx * speed * dt;
            p->frosty_y += fdy * speed * dt;
            if (p->frosty_x < frosty_half_w) p->frosty_x = frosty_half_w;
            if (p->frosty_x > (float)gs->screen_w - frosty_half_w) p->frosty_x = (float)gs->screen_w - frosty_half_w;
            if (p->frosty_y < frosty_min_y) p->frosty_y = frosty_min_y;
            if (p->frosty_y > max_y) p->frosty_y = max_y;
        }

        if (!p->antartica_alive && p->frosty_alive) {
            p->x = p->frosty_x;
            p->y = p->frosty_y;
        }
    } else {
        p->x += dx * speed * dt;
        p->y += dy * speed * dt;
        if (p->x < half_w) p->x = half_w;
        if (p->x > (float)gs->screen_w - half_w) p->x = (float)gs->screen_w - half_w;
        if (p->y < min_y) p->y = min_y;
        if (p->y > max_y) p->y = max_y;
    }

    update_shoot_mode_switch(gs, input, events);
    update_player_firing(gs, input, dt, events);
}

/* The super beam is a continuous column running from the ship straight up
 * to the top of the screen. While active it needs no fire input: it just
 * sweeps every enemy and enemy projectile inside its width off the board,
 * every frame, for its whole duration. */
/* The super beam's own column origin(s) for this frame: 1 entry (p->x/p->y,
 * same as ever) for every ship but SHIP_TWINS/SHIP_ANTARTICA, or one per
 * currently-alive body for those two - so the orb's super beam sweeps a
 * column from each twin (or each of Antartica/Frosty) still standing, not
 * just a single column from whatever p->x/p->y happens to mean for the
 * ship's own current flight mode. Both x AND y are per-origin (not a single
 * shared y) - The Twins always share y so that made no practical
 * difference, but Antartica and Frosty can be at very different heights
 * (independent WASD/arrow control - see update_player's own SHIP_ANTARTICA
 * branch), so each column has to sweep from its own actual body's y, same
 * "each body has its own beam" construction update_antartica_freezing_beam
 * already uses. Every other ship's own single-column behavior is completely
 * unchanged (out_x[0]/out_y[0] = p->x/p->y, count 1, identical to reading
 * p->x/p->y directly). */
static int player_beam_origins(const GameState *gs, float out_x[2], float out_y[2]) {
    const Player *p = &gs->player;
    if (gs->selected_ship == SHIP_TWINS) {
        int n = 0;
        if (p->twins_right_alive) { out_x[n] = p->twins_right_x; out_y[n] = p->y; n++; }
        if (p->twins_left_alive) { out_x[n] = p->twins_left_x; out_y[n] = p->y; n++; }
        return n;
    }
    if (gs->selected_ship == SHIP_ANTARTICA) {
        int n = 0;
        if (p->antartica_alive) { out_x[n] = p->x; out_y[n] = p->y; n++; }
        if (p->frosty_alive) { out_x[n] = p->frosty_x; out_y[n] = p->frosty_y; n++; }
        return n;
    }
    out_x[0] = p->x;
    out_y[0] = p->y;
    return 1;
}

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
    float origin_x[2], origin_y[2];
    int origin_count = player_beam_origins(gs, origin_x, origin_y);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;
        for (int o = 0; o < origin_count; o++) {
            if (e->y >= origin_y[o]) continue;
            if (fabsf(e->x - origin_x[o]) <= beam_half_w + e->size / 2.0f) {
                destroy_enemy_for_score(gs, events, e);
                break;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        Projectile *pr = &gs->enemy_shots[i];
        if (!pr->alive) continue;
        float pr_half_w, pr_half_h;
        enemy_shot_half_extents(pr, &pr_half_w, &pr_half_h);
        for (int o = 0; o < origin_count; o++) {
            if (pr->y >= origin_y[o]) continue;
            if (fabsf(pr->x - origin_x[o]) <= beam_half_w + pr_half_w) {
                pr->alive = false;
                break;
            }
        }
    }

    if (gs->boss.alive) {
        bool boss_in_beam = false;
        for (int o = 0; o < origin_count; o++) {
            if (gs->boss.y < origin_y[o] &&
                fabsf(gs->boss.x - origin_x[o]) <= beam_half_w + gs->boss.size / 2.0f) {
                boss_in_beam = true;
                break;
            }
        }
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

/* Antartica's own mode 3 (SHOOT_MODE_ANTARTICA_FREEZE_BEAM) - "both
 * Antartica and Frosty fire a continuous freezing beam" per spec: the same
 * column-sweep mechanics as update_super_beam above, but from up to two
 * independent origins (each with its OWN y, since - unlike The Twins -
 * Antartica and Frosty can be at different heights, see update_player's own
 * SHIP_ANTARTICA branch) and deliberately WITHOUT touching super_beam_timer
 * or any immunity guard: this beam neither heals nor grants invincibility,
 * only the sweep itself. antartica_freeze_beam_boss_hit_timer paces repeat
 * boss damage, the same role Boss.beam_contact_timer plays for the power
 * orb's own super beam above, kept as its own separate field so the two
 * beams' boss-contact pacing can never interfere with each other. */
static void update_antartica_freezing_beam(GameState *gs, float dt, EventQueue *events) {
    Player *p = &gs->player;
    if (p->antartica_freeze_beam_boss_hit_timer > 0.0f) p->antartica_freeze_beam_boss_hit_timer -= dt;

    if (p->antartica_freeze_beam_timer <= 0.0f) {
        p->antartica_freeze_beam_boss_hit_timer = 0.0f;
        return;
    }
    if (!p->alive) return;

    float beam_half_w = scaled(gs, PLAYER_PROJECTILE_W) * ANTARTICA_FREEZE_BEAM_WIDTH_MULTIPLIER / 2.0f;
    float origin_x[2], origin_y[2];
    int origin_count = 0;
    if (p->antartica_alive) {
        origin_x[origin_count] = p->x;
        origin_y[origin_count] = p->y;
        origin_count++;
    }
    if (p->frosty_alive) {
        origin_x[origin_count] = p->frosty_x;
        origin_y[origin_count] = p->frosty_y;
        origin_count++;
    }
    if (origin_count == 0) return;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;
        for (int o = 0; o < origin_count; o++) {
            if (e->y >= origin_y[o]) continue;
            if (fabsf(e->x - origin_x[o]) <= beam_half_w + e->size / 2.0f) {
                destroy_enemy_for_score(gs, events, e);
                break;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        Projectile *pr = &gs->enemy_shots[i];
        if (!pr->alive) continue;
        float pr_half_w, pr_half_h;
        enemy_shot_half_extents(pr, &pr_half_w, &pr_half_h);
        for (int o = 0; o < origin_count; o++) {
            if (pr->y >= origin_y[o]) continue;
            if (fabsf(pr->x - origin_x[o]) <= beam_half_w + pr_half_w) {
                pr->alive = false;
                break;
            }
        }
    }

    if (gs->boss.alive) {
        bool boss_in_beam = false;
        for (int o = 0; o < origin_count; o++) {
            if (gs->boss.y < origin_y[o] &&
                fabsf(gs->boss.x - origin_x[o]) <= beam_half_w + gs->boss.size / 2.0f) {
                boss_in_beam = true;
                break;
            }
        }
        if (boss_in_beam) {
            if (p->antartica_freeze_beam_boss_hit_timer <= 0.0f) {
                damage_boss(gs, events, BASE_PLAYER_DAMAGE);
                p->antartica_freeze_beam_boss_hit_timer = BEAM_BOSS_HIT_INTERVAL;
            }
        } else {
            p->antartica_freeze_beam_boss_hit_timer = 0.0f;
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

/* Moves one enemy according to its own EnemyMovementStyle (rolled once at
 * spawn - see roll_enemy_movement_style in usecases/spawner.c), writing
 * into e->x/e->y same as the original NORMAL-only formula this replaced.
 * CIRCLE/SPIRAL/SINE never touch e->vx/e->vy after spawn - they orbit/wave
 * around orbit_center_x/orbit_center_y, which drifts by the enemy's own
 * (possibly ERRATIC_ENEMY_SPEED_MULTIPLIER-boosted) vx/vy exactly the way
 * a NORMAL enemy's raw x/y would, guaranteeing every style still clears
 * the bottom of the screen on its own, no separate despawn logic needed. */
static void update_enemy_movement(GameState *gs, Enemy *e, float dt) {
    /* A boss-dispatched enemy ignores its own (not yet rolled)
     * movement_style entirely until it reaches its landing point - see the
     * Enemy.boss_dispatch_flying field's own doc comment. */
    if (e->boss_dispatch_flying) {
        float dx = e->boss_dispatch_target_x - e->x;
        float dy = e->boss_dispatch_target_y - e->y;
        float dist = sqrtf(dx * dx + dy * dy);
        float step = scaled(gs, BOSS_DISPATCH_ENEMY_FLIGHT_SPEED) * dt;
        if (dist > step && dist > 0.0001f) {
            e->x += dx / dist * step;
            e->y += dy / dist * step;
        } else {
            e->x = e->boss_dispatch_target_x;
            e->y = e->boss_dispatch_target_y;
            spawner_land_boss_dispatched_enemy(gs, e);
        }
        return;
    }

    switch (e->movement_style) {
        case ENEMY_MOVEMENT_CIRCLE:
        case ENEMY_MOVEMENT_SPIRAL: {
            float angular_speed = deg_to_rad(e->movement_style == ENEMY_MOVEMENT_SPIRAL
                                                  ? ERRATIC_ENEMY_SPIRAL_ANGULAR_SPEED
                                                  : ERRATIC_ENEMY_CIRCLE_ANGULAR_SPEED);
            e->wobble_phase += angular_speed * dt;
            if (e->movement_style == ENEMY_MOVEMENT_SPIRAL) {
                e->erratic_radius += scaled(gs, ERRATIC_ENEMY_SPIRAL_RADIUS_GROWTH) * dt;
                float max_r = scaled(gs, ERRATIC_ENEMY_SPIRAL_RADIUS_MAX);
                if (e->erratic_radius > max_r) e->erratic_radius = max_r;
            }
            e->orbit_center_x += e->vx * dt;
            e->orbit_center_y += e->vy * dt;
            e->x = e->orbit_center_x + cosf(e->wobble_phase) * e->erratic_radius;
            e->y = e->orbit_center_y + sinf(e->wobble_phase) * e->erratic_radius;
            break;
        }

        case ENEMY_MOVEMENT_SINE:
            e->wobble_phase += deg_to_rad(ERRATIC_ENEMY_SINE_ANGULAR_SPEED) * dt;
            e->orbit_center_x += e->vx * dt;
            e->orbit_center_y += e->vy * dt;
            e->x = e->orbit_center_x + sinf(e->wobble_phase) * e->erratic_radius;
            e->y = e->orbit_center_y;
            break;

        case ENEMY_MOVEMENT_RANDOM:
            e->wobble_phase -= dt;
            if (e->wobble_phase <= 0.0f) {
                float speed = scaled(gs, ERRATIC_ENEMY_RANDOM_SPEED) * ERRATIC_ENEMY_SPEED_MULTIPLIER;
                float angle = frand01() * 6.2831853f;
                e->vx = cosf(angle) * speed;
                /* Guarantee a real downward component (never purely
                 * sideways/upward) so a RANDOM enemy still reliably clears
                 * the bottom of the screen like every other style. */
                e->vy = fabsf(sinf(angle)) * speed + scaled(gs, ENEMY_MIN_SIZE) * 0.5f;
                e->wobble_phase = ERRATIC_ENEMY_RANDOM_RETARGET_MIN +
                                   frand01() * (ERRATIC_ENEMY_RANDOM_RETARGET_MAX - ERRATIC_ENEMY_RANDOM_RETARGET_MIN);
            }
            e->x += e->vx * dt;
            e->y += e->vy * dt;
            break;

        case ENEMY_MOVEMENT_NORMAL:
        default:
            e->wobble_phase += dt * 3.0f;
            e->x += (e->vx + sinf(e->wobble_phase) * scaled(gs, 12.0f)) * dt;
            e->y += e->vy * dt;
            break;
    }
}

static void update_enemies(GameState *gs, float dt) {
    float fire_chance = difficulty_enemy_fire_chance_per_sec(gs->selected_difficulty, gs->time_elapsed);
    float mean_fire_interval = 1.0f / fire_chance;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;

        update_enemy_movement(gs, e, dt);

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

/* Cruzader's own mode 3 rockets (PROJECTILE_KIND_CRUZADER_ROCKET): every
 * frame, before position integration, snap each alive rocket's heading to
 * point exactly at the closest currently-alive Enemy, preserving its
 * current speed - guaranteed-impact homing ("auto-flying towards the
 * closest enemy's direction to ensure impact" per spec), not a gradual
 * turn-rate steer. A rocket with no enemies left alive just keeps flying
 * on whatever heading it already had. */
static void update_cruzader_rocket_homing(GameState *gs) {
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        Projectile *pr = &gs->player_shots[i];
        if (!pr->alive || pr->kind != PROJECTILE_KIND_CRUZADER_ROCKET) continue;

        Enemy *closest = NULL;
        float best_dist2 = 0.0f;
        for (int j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &gs->enemies[j];
            if (!e->alive) continue;
            float dx = e->x - pr->x, dy = e->y - pr->y;
            float dist2 = dx * dx + dy * dy;
            if (!closest || dist2 < best_dist2) {
                closest = e;
                best_dist2 = dist2;
            }
        }
        if (!closest) continue;

        float dx = closest->x - pr->x, dy = closest->y - pr->y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= 0.0001f) continue;
        float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
        pr->vx = dx / dist * speed;
        pr->vy = dy / dist * speed;
    }
}

static void update_projectiles(GameState *gs, float dt) {
    update_cruzader_rocket_homing(gs);
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
            /* Ranger's own trail re-arms on a much shorter interval than
             * every other ship's shared TRAIL_SPAWN_INTERVAL - still one
             * particle per tick, just far more often, so consecutive
             * particles overlap into one unbroken stream instead of reading
             * as discrete puffs (draw_trail_particle gives the color/alpha
             * its own blue-with-white-accents treatment on top of this). */
            p->trail_emit_timer =
                gs->selected_ship == SHIP_RANGER ? RANGER_TRAIL_SPAWN_INTERVAL : TRAIL_SPAWN_INTERVAL;
            float back_y = p->y + scaled(gs, PLAYER_HEIGHT) / 2.0f;
            float jitter_x = (frand01() - 0.5f) * scaled(gs, PLAYER_WIDTH) * 0.3f;
            if (gs->selected_ship == SHIP_TWINS) {
                /* One puff behind each currently-alive twin instead of a
                 * single one behind p->x - matches the two-rockets look. */
                if (p->twins_right_alive) spawn_trail_particle(gs, p->twins_right_x + jitter_x, back_y);
                if (p->twins_left_alive) spawn_trail_particle(gs, p->twins_left_x + jitter_x, back_y);
            } else {
                spawn_trail_particle(gs, p->x + jitter_x, back_y);
            }
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
                /* Cruzader's rockets get a denser, bigger, more visible,
                 * blue-tinted smoke trail than every other shot - "increase
                 * the visibility of the smoke... make it blue" per feedback
                 * - scoped to this exact kind/ship combination; every other
                 * shot (every other Cruzader mode included) keeps the
                 * ordinary color/cadence/size/alpha untouched. */
                bool is_cruzader_rocket =
                    pr->style_ship == SHIP_CRUZADER && pr->kind == PROJECTILE_KIND_CRUZADER_ROCKET;
                /* Antartica's own ice shards get "a tad more" white trailing
                 * smoke, and Frosty's own snowballs get a "slightly
                 * increased" white trailing smoke - both scoped to this one
                 * ship/kind combination only, same "kept independent"
                 * carve-out precedent as Cruzader's own rocket trail above. */
                bool is_antartica_shard =
                    pr->style_ship == SHIP_ANTARTICA && pr->kind != PROJECTILE_KIND_FROSTY_SNOWBALL;
                bool is_frosty_snowball =
                    pr->style_ship == SHIP_ANTARTICA && pr->kind == PROJECTILE_KIND_FROSTY_SNOWBALL;
                /* Ranger's own mode 3 (SHOOT_MODE_RANGER_ARC): unlike every
                 * other shot's own trail (one puff drifting from a single
                 * point), the wave spans a wide arch, so its trail has to
                 * span that same arch - RANGER_ARC_WAVE_TRAIL_POINTS puffs
                 * spawned at once, evenly spread across the wave's own
                 * current width and each riding the same RANGER_ARC_WAVE_
                 * BULGE curve draw_ranger_arc_wave (adapters/sdl_renderer.c)
                 * renders, so the trail visibly covers the wave's entire
                 * length (left edge to right edge) instead of trailing from
                 * just its center. */
                bool is_ranger_arc = pr->style_ship == SHIP_RANGER && pr->kind == PROJECTILE_KIND_RANGER_ARC;
                /* Ranger's own modes 1/2 (SHOOT_MODE_RANGER_TRIBEAM/
                 * _ALTERNATE) - "much more visible" per feedback, same
                 * RANGER_PROJECTILE_TRAIL_* boost mode 3's own arc-wave
                 * trail gets below, just from a single point (like every
                 * other ship's own beam) rather than spread across a
                 * width. */
                bool is_ranger_beam = pr->style_ship == SHIP_RANGER && pr->kind != PROJECTILE_KIND_RANGER_ARC;
                pr->trail_emit_timer = is_cruzader_rocket  ? CRUZADER_ROCKET_TRAIL_SPAWN_INTERVAL
                                        : (is_ranger_arc || is_ranger_beam) ? RANGER_PROJECTILE_TRAIL_SPAWN_INTERVAL
                                                                             : PROJECTILE_TRAIL_SPAWN_INTERVAL;
                float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
                float back_dx = speed > 0.0f ? -pr->vx / speed : 0.0f;
                float back_dy = speed > 0.0f ? -pr->vy / speed : -1.0f;
                if (is_ranger_arc) {
                    float half_w = scaled(gs, RANGER_ARC_WAVE_WIDTH) / 2.0f;
                    float bulge = scaled(gs, RANGER_ARC_WAVE_BULGE);
                    for (int k = 0; k < RANGER_ARC_WAVE_TRAIL_POINTS; k++) {
                        float t = (float)k / (float)(RANGER_ARC_WAVE_TRAIL_POINTS - 1);
                        float x = pr->x + (t - 0.5f) * 2.0f * half_w;
                        float y = pr->y - sinf(deg_to_rad(180.0f * t)) * bulge;
                        spawn_projectile_trail_particle(gs, x, y, 0.0f, 1.0f, pr->color,
                                                         RANGER_ARC_WAVE_TRAIL_SIZE_MULTIPLIER,
                                                         RANGER_ARC_WAVE_TRAIL_MAX_ALPHA);
                    }
                } else if (is_ranger_beam) {
                    spawn_projectile_trail_particle(gs, pr->x, pr->y, back_dx, back_dy, pr->color,
                                                     RANGER_PROJECTILE_TRAIL_SIZE_MULTIPLIER,
                                                     RANGER_PROJECTILE_TRAIL_MAX_ALPHA);
                } else if (is_cruzader_rocket) {
                    static const Color kCruzaderRocketSmokeBlue = {70, 150, 255, 255};
                    spawn_projectile_trail_particle(gs, pr->x, pr->y, back_dx, back_dy, kCruzaderRocketSmokeBlue,
                                                     CRUZADER_ROCKET_TRAIL_SIZE_MULTIPLIER,
                                                     CRUZADER_ROCKET_TRAIL_MAX_ALPHA);
                } else if (is_antartica_shard) {
                    static const Color kAntarticaSmokeWhite = {255, 255, 255, 255};
                    spawn_projectile_trail_particle(gs, pr->x, pr->y, back_dx, back_dy, kAntarticaSmokeWhite,
                                                     ANTARTICA_SHARD_TRAIL_SIZE_MULTIPLIER,
                                                     ANTARTICA_SHARD_TRAIL_MAX_ALPHA);
                } else if (is_frosty_snowball) {
                    static const Color kFrostySmokeWhite = {255, 255, 255, 255};
                    spawn_projectile_trail_particle(gs, pr->x, pr->y, back_dx, back_dy, kFrostySmokeWhite,
                                                     FROSTY_SNOWBALL_TRAIL_SIZE_MULTIPLIER,
                                                     FROSTY_SNOWBALL_TRAIL_MAX_ALPHA);
                } else {
                    spawn_projectile_trail_particle(gs, pr->x, pr->y, back_dx, back_dy, pr->color, 1.0f,
                                                     PROJECTILE_TRAIL_MAX_ALPHA);
                }
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

/* The player's own hitbox(es) for this frame: 1 entry (gs->player.x/y, same
 * as ever) for every ship but SHIP_TWINS, or one entry per currently-alive
 * twin (0, 1, or 2) for SHIP_TWINS - see the Player struct's own doc
 * comment for what twins_right_x/twins_left_x mean. Every collision site in
 * check_collisions that used to test a single player hitbox loops over this
 * instead, so Twins' two independent bodies plug into every one of them
 * without changing behavior for any other ship (hb_count is always 1
 * there, identical to the single test each of those sites used to run
 * directly). */
typedef struct PlayerHitbox {
    float x, y, half_w, half_h;
    bool right; /* only meaningful for SHIP_TWINS */
    bool is_frosty; /* only meaningful for SHIP_ANTARTICA - false means Antartica herself */
} PlayerHitbox;

static int player_hitboxes(const GameState *gs, PlayerHitbox out[2]) {
    if (!gs->player.alive) return 0;
    float half_h = scaled(gs, PLAYER_HEIGHT) * ship_size_multiplier(gs->selected_ship) / 2.0f;
    float half_w = scaled(gs, PLAYER_WIDTH) * ship_size_multiplier(gs->selected_ship) / 2.0f;
    if (gs->selected_ship == SHIP_TWINS) {
        int n = 0;
        if (gs->player.twins_right_alive) {
            out[n++] = (PlayerHitbox){gs->player.twins_right_x, gs->player.y, half_w, half_h, true, false};
        }
        if (gs->player.twins_left_alive) {
            out[n++] = (PlayerHitbox){gs->player.twins_left_x, gs->player.y, half_w, half_h, false, false};
        }
        return n;
    }
    /* SHIP_ANTARTICA's own two independent bodies - same "one entry per
     * currently-alive body" convention as SHIP_TWINS above, just at two
     * different positions/sizes instead of one shared position (Frosty is
     * ANTARTICA_FROSTY_SIZE_MULTIPLIER smaller, per spec). */
    if (gs->selected_ship == SHIP_ANTARTICA) {
        int n = 0;
        if (gs->player.antartica_alive) {
            out[n++] = (PlayerHitbox){gs->player.x, gs->player.y, half_w, half_h, false, false};
        }
        if (gs->player.frosty_alive) {
            out[n++] = (PlayerHitbox){gs->player.frosty_x, gs->player.frosty_y,
                                       half_w * ANTARTICA_FROSTY_SIZE_MULTIPLIER,
                                       half_h * ANTARTICA_FROSTY_SIZE_MULTIPLIER, false, true};
        }
        return n;
    }
    out[0] = (PlayerHitbox){gs->player.x, gs->player.y, half_w, half_h, false, false};
    return 1;
}

/* Routes to kill_twin/damage_twin for SHIP_TWINS (using PlayerHitbox.right
 * to say which twin), kill_antartica_body/kill_frosty (or their damage_
 * counterparts) for SHIP_ANTARTICA (using PlayerHitbox.is_frosty to say
 * which body), or straight to kill_player/damage_player otherwise - so
 * every other ship's own call site is functionally identical to calling
 * kill_player/damage_player directly, just through one extra layer. */
static void kill_player_hitbox(GameState *gs, EventQueue *events, const PlayerHitbox *hb) {
    if (gs->selected_ship == SHIP_TWINS) {
        kill_twin(gs, events, hb->right);
    } else if (gs->selected_ship == SHIP_ANTARTICA) {
        if (hb->is_frosty) kill_frosty(gs, events);
        else kill_antartica_body(gs, events);
    } else {
        kill_player(gs, events);
    }
}

static void damage_player_hitbox(GameState *gs, EventQueue *events, const PlayerHitbox *hb, float amount) {
    if (gs->selected_ship == SHIP_TWINS) {
        damage_twin(gs, events, hb->right, amount);
    } else if (gs->selected_ship == SHIP_ANTARTICA) {
        if (hb->is_frosty) damage_frosty(gs, events, amount);
        else damage_antartica_body(gs, events, amount);
    } else {
        damage_player(gs, events, amount);
    }
}

static void check_collisions(GameState *gs, EventQueue *events) {
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
                /* Ranger's own mode 3 (SHOOT_MODE_RANGER_ARC) pierces
                 * through instead of being consumed by the first enemy it
                 * touches - see PROJECTILE_KIND_RANGER_ARC's own doc
                 * comment in domain/types.h. pr stays alive and the inner
                 * loop keeps going (no break), so a single wave can destroy
                 * every enemy it overlaps this same frame, and every frame
                 * after, all the way up until it clears the top of the
                 * screen (update_projectiles' own ordinary off-screen
                 * despawn). Each enemy it touches still only ever takes one
                 * hit - it dies outright, same as any other shot - so there
                 * is no repeat-hit bookkeeping needed here the way the
                 * boss's own hit pool needs further down. */
                if (pr->kind == PROJECTILE_KIND_RANGER_ARC) {
                    destroy_enemy_for_score(gs, events, e);
                    continue;
                }
                pr->alive = false;
                if (pr->kind == PROJECTILE_KIND_POWER || pr->kind == PROJECTILE_KIND_CRUZADER_ROCKET) {
                    trigger_power_cannon_explosion(gs, events, pr->x, pr->y, pr->style_ship);
                } else {
                    destroy_enemy_for_score(gs, events, e);
                }
                break;
            }
        }
    }

    /* Cruzader never explodes from touching an ordinary enemy - the enemy
     * still dies (e->alive = false; spawn_explosion, both unconditional
     * below), but takes a flat life-loss penalty instead of the usual
     * instant kill_player (see damage_cruzader_on_enemy_contact's own doc
     * comment). Contact with the boss's own danger ring stays fatal (see
     * the ring-touch block further down) - this carve-out is scoped to
     * ordinary enemies only, per spec.
     *
     * Samurai's own stealth (mode 3) skips this whole block entirely while
     * active - "no collision, no deaths on either side" per spec, so unlike
     * Cruzader's own carve-out above (which still kills the enemy), the
     * touched enemy must survive too, not just the player. */
    if (gs->player.alive && !samurai_stealth_active(gs)) {
        PlayerHitbox hbs[2];
        int hb_count = player_hitboxes(gs, hbs);
        for (int h = 0; h < hb_count; h++) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                Enemy *e = &gs->enemies[j];
                if (!e->alive) continue;
                if (collision_aabb_overlap(hbs[h].x, hbs[h].y, hbs[h].half_w, hbs[h].half_h,
                                            e->x, e->y, e->size / 2.0f, e->size / 2.0f)) {
                    e->alive = false;
                    spawn_explosion(gs, e->x, e->y, e->size);
                    if (gs->selected_ship == SHIP_CRUZADER) {
                        damage_cruzader_on_enemy_contact(gs, events);
                    } else {
                        kill_player_hitbox(gs, events, &hbs[h]);
                    }
                    break;
                }
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

    /* Cruzader's deflector orb (mode 2): while active, every enemy shot
     * within CRUZADER_ORB_RADIUS is fully reflected - no player damage at
     * all, unlike the passive 50% chance below - and unlike that loop,
     * doesn't stop at the first one: the whole point is nothing gets
     * through while the orb is up. A shot stays in gs->enemy_shots after
     * being reflected (see reflect_enemy_shot's own doc comment), so
     * pr->reflected must be checked here too - otherwise a shot that
     * lingers inside the radius after bouncing back would get flipped
     * again next frame, and again the frame after that. */
    if (gs->player.alive && gs->selected_ship == SHIP_CRUZADER && gs->player.cruzader_orb_timer > 0.0f) {
        float orb_radius = scaled(gs, CRUZADER_ORB_RADIUS);
        for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
            Projectile *pr = &gs->enemy_shots[i];
            if (!pr->alive || pr->inert || pr->reflected) continue;
            if (!within_radius(pr->x, pr->y, gs->player.x, gs->player.y, orb_radius)) continue;
            reflect_enemy_shot(pr, CRUZADER_REFLECTED_SHOT_DAMAGE);
        }
    }

    /* Buckler's own protective orb: while active, every enemy shot within
     * BUCKLER_ORB_RADIUS is simply destroyed - no player damage, same as
     * Cruzader's own deflector orb above, but unlike that one, never
     * reflected back at the enemies (see BUCKLER_ORB_RADIUS's own doc
     * comment in domain/constants.h - Buckler's orb is purely defensive). */
    if (gs->player.alive && gs->selected_ship == SHIP_BUCKLER && gs->player.buckler_orb_timer > 0.0f) {
        float orb_radius = scaled(gs, BUCKLER_ORB_RADIUS);
        for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
            Projectile *pr = &gs->enemy_shots[i];
            if (!pr->alive || pr->inert) continue;
            if (!within_radius(pr->x, pr->y, gs->player.x, gs->player.y, orb_radius)) continue;
            pr->alive = false;
        }
    }

    /* Samurai's own stealth: "projectiles fly right through it" - every
     * enemy shot simply keeps flying, untouched (not even consumed), while
     * active. */
    if (gs->player.alive && !samurai_stealth_active(gs)) {
        PlayerHitbox hbs[2];
        int hb_count = player_hitboxes(gs, hbs);
        for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
            Projectile *pr = &gs->enemy_shots[i];
            /* inert = fading out after a boss arrived; harmless. A shot
             * already marked reflected must never re-enter this test - it's
             * now a friendly projectile flying away from the player, not an
             * incoming threat, even if it's slow enough to still overlap
             * the player's hitbox for a frame or two right after bouncing
             * back (see the identical guard in the orb loop above). */
            if (!pr->alive || pr->inert || pr->reflected) continue;
            float enemy_shot_half_w, enemy_shot_half_h;
            enemy_shot_half_extents(pr, &enemy_shot_half_w, &enemy_shot_half_h);
            for (int h = 0; h < hb_count; h++) {
                if (collision_aabb_overlap(pr->x, pr->y, enemy_shot_half_w, enemy_shot_half_h,
                                            hbs[h].x, hbs[h].y, hbs[h].half_w, hbs[h].half_h)) {
                    /* Cruzader's passive: 50% chance to bounce the shot back
                     * (see reflect_enemy_shot) instead of taking a full hit -
                     * only reached here when the orb above isn't already
                     * handling every shot in range for free. A reflected shot
                     * stays alive (it keeps flying, now away from the player)
                     * so the pass below can still land it on an enemy/boss. */
                    if (gs->selected_ship == SHIP_CRUZADER && frand01() < CRUZADER_PASSIVE_REFLECT_CHANCE) {
                        reflect_enemy_shot(pr, CRUZADER_REFLECTED_SHOT_DAMAGE);
                        damage_player_hitbox(gs, events, &hbs[h],
                                              PLAYER_LIFE_LOSS_PER_HIT * CRUZADER_PASSIVE_REFLECT_DAMAGE_MULTIPLIER);
                    } else {
                        pr->alive = false;
                        damage_player_hitbox(gs, events, &hbs[h], PLAYER_LIFE_LOSS_PER_HIT);
                    }
                    break;
                }
            }
        }
    }

    /* Reflected enemy shots (Cruzader's passive/orb, just above) now fly
     * back toward the enemies instead of the player, in their own
     * unmodified original design - test them against ordinary enemies and
     * the boss the same way a player shot would, then consume them on
     * contact. A separate pass (rather than folded into the player-shot
     * loops above) since these are never moved out of gs->enemy_shots. */
    if (gs->selected_ship == SHIP_CRUZADER) {
        for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
            Projectile *pr = &gs->enemy_shots[i];
            if (!pr->alive || pr->inert || !pr->reflected) continue;
            float half_w, half_h;
            enemy_shot_half_extents(pr, &half_w, &half_h);

            bool consumed = false;
            for (int j = 0; j < MAX_ENEMIES; j++) {
                Enemy *e = &gs->enemies[j];
                if (!e->alive) continue;
                if (collision_aabb_overlap(pr->x, pr->y, half_w, half_h, e->x, e->y, e->size / 2.0f,
                                            e->size / 2.0f)) {
                    destroy_enemy_for_score(gs, events, e);
                    consumed = true;
                    break;
                }
            }
            if (!consumed && gs->boss.alive) {
                float boss_half = gs->boss.size / 2.0f;
                if (collision_aabb_overlap(pr->x, pr->y, half_w, half_h, gs->boss.x, gs->boss.y, boss_half,
                                            boss_half)) {
                    damage_boss(gs, events, pr->damage);
                    consumed = true;
                }
            }
            if (consumed) pr->alive = false;
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
            /* Ranger's own mode 3 (SHOOT_MODE_RANGER_ARC): pierces through
             * instead of being consumed here, same as against ordinary
             * enemies above (see PROJECTILE_KIND_RANGER_ARC's own doc
             * comment), and deals the boss exactly one hit total
             * (ranger_arc_hit_boss) rather than repeatedly draining its hit
             * pool every frame it's still overlapping during its slow
             * multi-frame crossing. Never breaks the loop below - unlike
             * every other kind, it isn't consumed by this contact, so it
             * shouldn't claim the "only one shot damages the boss per
             * frame" budget that break enforces for everything else. */
            if (pr->kind == PROJECTILE_KIND_RANGER_ARC) {
                if (!pr->ranger_arc_hit_boss) {
                    pr->ranger_arc_hit_boss = true;
                    damage_boss(gs, events, pr->damage);
                }
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
            if (pr->kind == PROJECTILE_KIND_POWER || pr->kind == PROJECTILE_KIND_CRUZADER_ROCKET) {
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
         * just defeated it this same frame. Cruzader's deflector orb blocks
         * this whole consequence block entirely while active (not just
         * kill_player) - the ring simply does nothing that frame, boss
         * keeps chasing, no free kill either. Outside the orb window, a
         * boss ring touch is fatal to Cruzader same as any other ship.
         *
         * kill_player_hitbox is called before end_boss_encounter, not
         * after: super_beam_shields_player reads gs->boss.alive to decide
         * whether the beam's immunity applies (it doesn't during a boss
         * fight - see that function's own doc comment), so the player's
         * own death has to be resolved while the boss this ring belongs to
         * is still (from its perspective) alive, not after end_boss_
         * encounter has already zeroed the flag out from under it. */
        bool cruzader_orb_blocks_ring = gs->selected_ship == SHIP_CRUZADER && gs->player.cruzader_orb_timer > 0.0f;
        /* Buckler's own protective orb blocks the ring the same
         * unconditional way Cruzader's own does above - see that flag's
         * own doc comment just above for why this has to stay
         * unconditional too. */
        bool buckler_orb_blocks_ring = gs->selected_ship == SHIP_BUCKLER && gs->player.buckler_orb_timer > 0.0f;
        /* Samurai's own stealth blocks the ring the same unconditional way
         * Cruzader's/Buckler's own orbs do above - "fly through... bosses
         * as if they did not exist" per spec. */
        bool samurai_stealth_blocks_ring = samurai_stealth_active(gs);
        if (gs->boss.alive && gs->player.alive && !cruzader_orb_blocks_ring && !buckler_orb_blocks_ring &&
            !samurai_stealth_blocks_ring) {
            float ring_radius = gs->boss.size * BOSS_MENACE_RING_RATIO;
            /* Fatal to whichever body actually touched it - kill_player_hitbox
             * (same dispatcher every other hazard in this function already
             * uses) routes to kill_twin/kill_antartica_body/kill_frosty for
             * SHIP_TWINS/SHIP_ANTARTICA, so touching the ring with only one
             * of a pair's two bodies no longer takes the other down with it;
             * the survivor keeps flying, same "one can die while the other
             * keeps going" rule every other hazard already respects (see
             * PlayerHitbox's own doc comment). Every other ship's own single
             * hitbox still routes straight to kill_player, unchanged. The
             * boss's own detonation just below stays unconditional either
             * way - still checked against every currently-alive body's own
             * hitbox for detection. */
            PlayerHitbox hbs[2];
            int hb_count = player_hitboxes(gs, hbs);
            for (int h = 0; h < hb_count; h++) {
                float player_radius = fmaxf(hbs[h].half_w, hbs[h].half_h);
                if (within_radius(hbs[h].x, hbs[h].y, gs->boss.x, gs->boss.y, ring_radius + player_radius)) {
                    spawn_explosion(gs, gs->boss.x, gs->boss.y, gs->boss.size * 1.4f);
                    kill_player_hitbox(gs, events, &hbs[h]);
                    end_boss_encounter(gs);
                    event_queue_push_sfx(events, SFX_BOSS_DEFEATED);
                    break;
                }
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
        PlayerHitbox hbs[2];
        int hb_count = player_hitboxes(gs, hbs);
        for (int h = 0; h < hb_count; h++) {
            if (collision_aabb_overlap(hbs[h].x, hbs[h].y, hbs[h].half_w, hbs[h].half_h,
                                        gs->orb.x, gs->orb.y, orb_half, orb_half)) {
                gs->orb.alive = false;
                gs->player.super_beam_timer = SUPER_BEAM_DURATION;
                if (gs->selected_ship == SHIP_TWINS) {
                    /* Only heals whichever twin(s) are still alive - a dead
                     * twin's own life bar must keep reading 0 (see
                     * kill_twin), never get resurrected back to full by a
                     * later orb capture. */
                    if (gs->player.twins_right_alive) gs->player.twins_right_life = PLAYER_LIFE_MAX;
                    if (gs->player.twins_left_alive) gs->player.twins_left_life = PLAYER_LIFE_MAX;
                } else if (gs->selected_ship == SHIP_ANTARTICA) {
                    /* Same "only heals whoever's still alive" rule as
                     * SHIP_TWINS above - restores both Antartica's and
                     * Frosty's health independently. */
                    if (gs->player.antartica_alive) gs->player.antartica_life = PLAYER_LIFE_MAX;
                    if (gs->player.frosty_alive) gs->player.frosty_life = PLAYER_LIFE_MAX;
                } else {
                    gs->player.life = PLAYER_LIFE_MAX;
                }
                event_queue_push_sfx(events, SFX_ORB_CAPTURED);
                break;
            }
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
    update_boss_dispatch(gs, dt);
    update_enemy_and_boss_trails(gs, dt);
    update_orb(gs, dt);
    update_projectiles(gs, dt);
    update_projectile_trails(gs, dt);
    update_super_beam(gs, dt, events);
    update_antartica_freezing_beam(gs, dt, events);
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

    /* Runs after the switch above (not before) so it reflects score/boss
     * changes update_running just made this same frame, instead of
     * lagging a frame behind - the frame a boss actually spawns must see
     * boss_warning already false, not still true from the update_running
     * call that flipped boss.alive. */
    update_boss_warning(gs);

    if (input->quit_requested) gs->quit_requested = true;
}
