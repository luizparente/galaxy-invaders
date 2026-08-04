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
    gs->score_since_last_boss = 0; /* the next one needs a full fresh BOSS_SCORE_STEP */

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

    event_queue_push_sfx(events, SFX_BOSS_ARRIVED);
}

/* Deterministic (unlike the orb's coin flip): a boss appears once
 * gs->score_since_last_boss reaches BOSS_SCORE_STEP - a counter that
 * resets to zero on every appearance (see spawn_boss) and only resumes
 * counting once the current boss is defeated, so each one always takes a
 * full fresh BOSS_SCORE_STEP of points to bring in, never less. */
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
    gs->score_since_last_boss += delta;

    if (gs->score / LASER_COLOR_SCORE_STEP > old_score / LASER_COLOR_SCORE_STEP) {
        gs->player.laser_color = random_vivid_color();
    }
    maybe_trigger_orb_spawn(gs, old_score, gs->score);
    maybe_trigger_boss_spawn(gs, events);
}

/* Shared by both ways the boss can take a hit (a direct laser shot, and
 * the super beam touching it) so the defeat/bonus/event logic can't drift
 * out of sync between the two. */
static void damage_boss(GameState *gs, EventQueue *events) {
    Boss *b = &gs->boss;
    b->hits_taken++;
    if (b->hits_taken >= b->hits_required) {
        spawn_explosion(gs, b->x, b->y, b->size * 1.4f);
        int bonus = b->hits_required * BOSS_KILL_SCORE_MULTIPLIER;
        b->alive = false;
        apply_score_delta(gs, events, bonus);
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

    if (p->fire_cooldown > 0.0f) p->fire_cooldown -= dt;

    /* While the super beam is active it replaces normal shots entirely -
     * see update_super_beam, which fires automatically every frame. */
    if (input->fire_held && p->fire_cooldown <= 0.0f && p->super_beam_timer <= 0.0f) {
        for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
            Projectile *pr = &gs->player_shots[i];
            if (pr->alive) continue;
            pr->alive = true;
            pr->x = p->x;
            pr->y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
            pr->vx = 0.0f;
            pr->vy = -scaled(gs, PLAYER_PROJECTILE_SPEED);
            pr->color = p->laser_color;
            p->fire_cooldown = PLAYER_FIRE_COOLDOWN;
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
            break;
        }
    }
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
        float pr_half_w = scaled(gs, ENEMY_PROJECTILE_W) / 2.0f;
        if (fabsf(pr->x - p->x) <= beam_half_w + pr_half_w) {
            pr->alive = false;
        }
    }

    if (gs->boss.alive) {
        bool boss_in_beam = gs->boss.y < p->y &&
                             fabsf(gs->boss.x - p->x) <= beam_half_w + gs->boss.size / 2.0f;
        if (boss_in_beam) {
            if (gs->boss.beam_contact_timer <= 0.0f) {
                damage_boss(gs, events);
                gs->boss.beam_contact_timer = BEAM_BOSS_HIT_INTERVAL;
            }
        } else {
            gs->boss.beam_contact_timer = 0.0f;
        }
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

        e->fire_timer -= dt;
        if (e->fire_timer <= 0.0f) {
            e->fire_timer = mean_fire_interval * (0.6f + frand01() * 0.8f);
            for (int j = 0; j < MAX_ENEMY_PROJECTILES; j++) {
                Projectile *pr = &gs->enemy_shots[j];
                if (pr->alive) continue;
                pr->alive = true;
                pr->x = e->x;
                pr->y = e->y + half;
                pr->vx = 0.0f;
                pr->vy = scaled(gs, ENEMY_PROJECTILE_SPEED);
                pr->color = e->color;
                pr->inert = false; /* slot may have been left inert by a past boss fight */
                pr->inert_age = 0.0f;
                break;
            }
        }
    }
}

static void update_projectiles(GameState *gs, float dt) {
    float player_shot_h = scaled(gs, PLAYER_PROJECTILE_H);
    float enemy_shot_h = scaled(gs, ENEMY_PROJECTILE_H);

    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        Projectile *pr = &gs->player_shots[i];
        if (!pr->alive) continue;
        pr->x += pr->vx * dt;
        pr->y += pr->vy * dt;
        if (pr->y < -player_shot_h) pr->alive = false;
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

        if (pr->y > (float)gs->screen_h + enemy_shot_h) pr->alive = false;
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

static void check_collisions(GameState *gs, EventQueue *events) {
    float player_shot_half_w = scaled(gs, PLAYER_PROJECTILE_W) / 2.0f;
    float player_shot_half_h = scaled(gs, PLAYER_PROJECTILE_H) / 2.0f;
    float enemy_shot_half_w = scaled(gs, ENEMY_PROJECTILE_W) / 2.0f;
    float enemy_shot_half_h = scaled(gs, ENEMY_PROJECTILE_H) / 2.0f;
    float player_half_w = scaled(gs, PLAYER_WIDTH) / 2.0f;
    float player_half_h = scaled(gs, PLAYER_HEIGHT) / 2.0f;

    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        Projectile *pr = &gs->player_shots[i];
        if (!pr->alive) continue;

        for (int j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &gs->enemies[j];
            if (!e->alive) continue;

            if (collision_aabb_overlap(pr->x, pr->y, player_shot_half_w, player_shot_half_h,
                                        e->x, e->y, e->size / 2.0f, e->size / 2.0f)) {
                pr->alive = false;
                destroy_enemy_for_score(gs, events, e);
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
            if (!collision_aabb_overlap(pr->x, pr->y, player_shot_half_w, player_shot_half_h,
                                         gs->boss.x, gs->boss.y, boss_half, boss_half)) {
                continue;
            }
            pr->alive = false;
            damage_boss(gs, events);
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
                gs->boss.alive = false;
                event_queue_push_sfx(events, SFX_BOSS_DEFEATED);
                kill_player(gs, events);
            }
        }
    }

    if (gs->orb.alive) {
        float orb_half = gs->orb.size / 2.0f;

        /* Shooting the orb: it detonates and neutralizes nearby enemies,
         * but does not grant the super beam. */
        for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
            Projectile *pr = &gs->player_shots[i];
            if (!pr->alive) continue;
            if (!collision_aabb_overlap(pr->x, pr->y, player_shot_half_w, player_shot_half_h,
                                         gs->orb.x, gs->orb.y, orb_half, orb_half)) {
                continue;
            }

            pr->alive = false;
            gs->orb.alive = false;
            spawn_explosion(gs, gs->orb.x, gs->orb.y, gs->orb.size * 1.8f);

            float neutralize_radius = scaled(gs, ORB_EXPLOSION_RADIUS);
            for (int j = 0; j < MAX_ENEMIES; j++) {
                Enemy *e = &gs->enemies[j];
                if (!e->alive) continue;
                if (within_radius(e->x, e->y, gs->orb.x, gs->orb.y, neutralize_radius)) {
                    e->alive = false;
                    spawn_explosion(gs, e->x, e->y, e->size);
                }
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
    if (!gs->boss.alive) spawner_update(gs, dt); /* no ordinary spawns during a boss fight */
    update_enemies(gs, dt);
    update_boss(gs, dt);
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
