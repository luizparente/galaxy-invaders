#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "usecases/game_logic.h"
#include "usecases/collision.h"
#include "usecases/difficulty.h"
#include "usecases/spawner.h"
#include "domain/constants.h"

static float frand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static bool within_radius(float ax, float ay, float bx, float by, float r) {
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy <= r * r;
}

static const Color kDefaultLaserColor = {255, 240, 120, 255};

/* A random fully-saturated hue, so each reroll is clearly a different
 * color rather than a subtle tint of the last one. */
static Color random_vivid_color(void) {
    float h = frand01() * 360.0f;
    float s = 0.75f + frand01() * 0.25f;
    float v = 0.9f + frand01() * 0.1f;
    return color_from_hsv(h, s, v);
}

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
        s->speed = scaled(gs, 20.0f + frand01() * 70.0f);
        s->brightness = (unsigned char)(90 + rand() % 165);
    }
}

static void update_stars(GameState *gs, float dt) {
    for (int i = 0; i < MAX_STARS; i++) {
        Star *s = &gs->stars[i];
        s->y += s->speed * dt;
        if (s->y > (float)gs->screen_h) {
            s->y = 0.0f;
            s->x = frand01() * (float)gs->screen_w;
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

/* Shared by every shooting mode's fire logic (see update_player_firing and
 * its per-mode helpers): claims the first free slot in gs->player_shots and
 * fills it in. Silently does nothing once the pool (MAX_PLAYER_PROJECTILES)
 * is exhausted, same as the original inline spawn it replaces. */
static void spawn_player_shot(GameState *gs, float x, float y, float vx, float vy,
                               ProjectileKind kind, bool horizontal, float damage) {
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
        return;
    }
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
        return;
    }
}

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

    if (gs->score / LASER_COLOR_SCORE_STEP > old_score / LASER_COLOR_SCORE_STEP) {
        gs->player.laser_color = random_vivid_color();
    }
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
static void trigger_power_cannon_explosion(GameState *gs, EventQueue *events, float x, float y) {
    float radius = POWER_CANNON_EXPLOSION_RADIUS_RATIO * fminf((float)gs->screen_w, (float)gs->screen_h);
    spawn_explosion(gs, x, y, radius);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;
        if (!within_radius(x, y, e->x, e->y, radius)) continue;
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
    memset(&gs->enemies, 0, sizeof(gs->enemies));
    memset(&gs->player_shots, 0, sizeof(gs->player_shots));
    memset(&gs->enemy_shots, 0, sizeof(gs->enemy_shots));
    memset(&gs->explosions, 0, sizeof(gs->explosions));
    memset(&gs->trail_particles, 0, sizeof(gs->trail_particles));
    memset(&gs->enemy_trail_particles, 0, sizeof(gs->enemy_trail_particles));
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
    gs->player.shoot_mode = SHOOT_MODE_NORMAL;
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
    init_stars(gs);
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
        case STATE_GAME_OVER:
            break;
    }
}

static void update_menu(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    gs->menu_blink_timer += dt;
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

    p->life -= amount;
    if (p->life <= 0.0f) {
        p->life = 0.0f;
        kill_player(gs, events);
    }
}

/* Mode switching (1-5 number keys) is locked out for the whole duration of
 * a rapid-fire burst and its following cooldown (see update_rapid_fire) -
 * everywhere else, switching is free even mid-flight of other shots. */
static void update_shoot_mode_switch(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (p->rapid_burst_timer > 0.0f || p->rapid_cooldown_timer > 0.0f) return;

    ShootMode requested = p->shoot_mode;
    if (input->shoot_mode_1_pressed) requested = SHOOT_MODE_NORMAL;
    else if (input->shoot_mode_2_pressed) requested = SHOOT_MODE_RAPID;
    else if (input->shoot_mode_3_pressed) requested = SHOOT_MODE_POWER;
    else if (input->shoot_mode_4_pressed) requested = SHOOT_MODE_DOUBLE;
    else if (input->shoot_mode_5_pressed) requested = SHOOT_MODE_SIDE;

    if (requested == p->shoot_mode) return;
    p->shoot_mode = requested;
    event_queue_push_sfx(events, SFX_MENU_SELECT);
}

/* Mode 1 (default): a single shot from the nose, unchanged from before this
 * ability existed. */
static void update_normal_fire(GameState *gs, const InputCommand *input, EventQueue *events) {
    Player *p = &gs->player;
    if (!(input->fire_held && p->fire_cooldown <= 0.0f)) return;

    spawn_player_shot(gs, p->x, p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f,
                       0.0f, -scaled(gs, PLAYER_PROJECTILE_SPEED), PROJECTILE_KIND_NORMAL, false,
                       BASE_PLAYER_DAMAGE);
    p->fire_cooldown = PLAYER_FIRE_COOLDOWN;
    event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
}

/* Mode 2: one press (or hold) commits to a full RAPID_FIRE_BURST_DURATION
 * seconds of automatic fire at RAPID_FIRE_SHOTS_PER_SEC, regardless of
 * whether the key is still held, followed by a RAPID_FIRE_LOCKOUT_DURATION
 * cooldown during which this mode (see its early return below) fires
 * nothing at all - update_shoot_mode_switch enforces the matching "can't
 * switch mode either" half of that lockout. rapid_burst_timer and
 * rapid_cooldown_timer are mutually exclusive: exactly one is nonzero
 * while a burst is in flight or recovering, both are zero while idle. */
static void update_rapid_fire(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    Player *p = &gs->player;

    if (p->rapid_burst_timer > 0.0f) {
        p->rapid_burst_timer -= dt;
        if (p->rapid_burst_timer <= 0.0f) {
            p->rapid_burst_timer = 0.0f;
            p->rapid_cooldown_timer = RAPID_FIRE_LOCKOUT_DURATION;
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

    if (p->rapid_cooldown_timer > 0.0f) {
        p->rapid_cooldown_timer -= dt;
        if (p->rapid_cooldown_timer < 0.0f) p->rapid_cooldown_timer = 0.0f;
        return;
    }

    if (input->fire_held) {
        p->rapid_burst_timer = RAPID_FIRE_BURST_DURATION;
        p->fire_cooldown = 0.0f; /* fire the first shot of the burst immediately */
    }
}

/* Mode 3: a slow, heavy single shot - see trigger_power_cannon_explosion
 * (check_collisions) for what happens when it actually lands. */
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

/* Mode 4: identical rate and shot to mode 1, just two shots fired abreast
 * from the wingtips instead of one from the nose. */
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

/* Mode 5: same wingtip pair as mode 4, fired sideways (away from the ship)
 * instead of forward. */
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

/* Dispatches to whichever mode is currently active. fire_cooldown is
 * decremented once here regardless of mode - every mode but rapid fire
 * (which drives its own pair of timers) gates its shot on it, the same
 * single-timer pattern the original normal-only fire logic used. */
static void update_player_firing(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    Player *p = &gs->player;
    if (p->fire_cooldown > 0.0f) p->fire_cooldown -= dt;

    /* While the super beam is active it replaces every shooting mode
     * entirely - see update_super_beam, which fires automatically every
     * frame on its own. */
    if (p->super_beam_timer > 0.0f) return;

    switch (p->shoot_mode) {
        case SHOOT_MODE_RAPID: update_rapid_fire(gs, input, dt, events); break;
        case SHOOT_MODE_POWER: update_power_cannon(gs, input, events); break;
        case SHOOT_MODE_DOUBLE: update_double_barrel(gs, input, events); break;
        case SHOOT_MODE_SIDE: update_side_beams(gs, input, events); break;
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

    float speed = scaled(gs, PLAYER_SPEED);
    if (p->super_beam_timer > 0.0f) speed *= SUPER_BEAM_SPEED_MULTIPLIER;
    p->x += dx * speed * dt;
    p->y += dy * speed * dt;

    float half_w = scaled(gs, PLAYER_WIDTH) / 2.0f;
    float min_y = scaled(gs, PLAYER_HEIGHT) / 2.0f; /* free to roam the whole screen, not just the lower band */
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

/* The 8 unit direction vectors ENEMY_SHOOT_OMNI fires along, evenly spaced
 * like the points of an octagon (N/NE/E/SE/S/SW/W/NW in screen space,
 * where +y is down) - written out rather than computed with sinf/cosf
 * since all 8 land on exact multiples of 45 degrees. */
static const float kOmniDirX[ENEMY_OMNI_SHOT_COUNT] = {
    0.0f, 0.70710678f, 1.0f, 0.70710678f, 0.0f, -0.70710678f, -1.0f, -0.70710678f,
};
static const float kOmniDirY[ENEMY_OMNI_SHOT_COUNT] = {
    1.0f, 0.70710678f, 0.0f, -0.70710678f, -1.0f, -0.70710678f, 0.0f, 0.70710678f,
};

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
    float mean_fire_interval = 1.0f / ENEMY_FIRE_CHANCE_PER_SEC;

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

static void check_collisions(GameState *gs, EventQueue *events) {
    float player_half_w = scaled(gs, PLAYER_WIDTH) / 2.0f;
    float player_half_h = scaled(gs, PLAYER_HEIGHT) / 2.0f;

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
                    trigger_power_cannon_explosion(gs, events, pr->x, pr->y);
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
                trigger_power_cannon_explosion(gs, events, pr->x, pr->y);
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
    update_player_trail(gs, dt);
    if (!gs->boss.alive) spawner_update(gs, dt); /* no ordinary spawns during a boss fight */
    update_enemies(gs, dt);
    update_pending_orb_kills(gs, dt, events);
    update_boss(gs, dt);
    update_enemy_and_boss_trails(gs, dt);
    update_orb(gs, dt);
    update_projectiles(gs, dt);
    update_super_beam(gs, dt, events);
    update_explosions(gs, dt);
    check_collisions(gs, events);
}

void game_update(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    event_queue_clear(events);
    update_stars(gs, dt);
    handle_global_back(gs, input, events);

    switch (gs->state) {
        case STATE_MENU:
            update_menu(gs, input, dt, events);
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
