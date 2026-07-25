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

static void reset_run(GameState *gs) {
    memset(&gs->enemies, 0, sizeof(gs->enemies));
    memset(&gs->player_shots, 0, sizeof(gs->player_shots));
    memset(&gs->enemy_shots, 0, sizeof(gs->enemy_shots));
    memset(&gs->explosions, 0, sizeof(gs->explosions));

    gs->player.x = (float)gs->screen_w / 2.0f;
    gs->player.y = (float)gs->screen_h - scaled(gs, PLAYER_BOTTOM_MARGIN);
    gs->player.alive = true;
    gs->player.fire_cooldown = 0.0f;

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
    gs->player.alive = false;
    spawn_explosion(gs, gs->player.x, gs->player.y, scaled(gs, PLAYER_WIDTH));
    event_queue_push_sfx(events, SFX_PLAYER_DESTROYED);
    gs->last_game_score = gs->score;
    gs->state = STATE_GAME_OVER;
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

    p->x += dx * scaled(gs, PLAYER_SPEED) * dt;
    p->y += dy * scaled(gs, PLAYER_SPEED) * dt;

    float half_w = scaled(gs, PLAYER_WIDTH) / 2.0f;
    float min_y = (float)gs->screen_h * PLAYER_MIN_Y_RATIO;
    float max_y = (float)gs->screen_h - scaled(gs, PLAYER_BOTTOM_MARGIN);
    if (p->x < half_w) p->x = half_w;
    if (p->x > (float)gs->screen_w - half_w) p->x = (float)gs->screen_w - half_w;
    if (p->y < min_y) p->y = min_y;
    if (p->y > max_y) p->y = max_y;

    if (p->fire_cooldown > 0.0f) p->fire_cooldown -= dt;

    if (input->fire_held && p->fire_cooldown <= 0.0f) {
        for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
            Projectile *pr = &gs->player_shots[i];
            if (pr->alive) continue;
            pr->alive = true;
            pr->x = p->x;
            pr->y = p->y - scaled(gs, PLAYER_HEIGHT) / 2.0f;
            pr->vx = 0.0f;
            pr->vy = -scaled(gs, PLAYER_PROJECTILE_SPEED);
            pr->color = (Color){255, 240, 120, 255};
            p->fire_cooldown = PLAYER_FIRE_COOLDOWN;
            event_queue_push_sfx(events, SFX_PLAYER_SHOOT);
            break;
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
                e->alive = false;
                spawn_explosion(gs, e->x, e->y, e->size);
                float mult = difficulty_score_multiplier(gs->score);
                gs->score += (int)((float)SCORE_PER_KILL * mult);
                event_queue_push_sfx(events, SFX_ENEMY_DESTROYED);
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
            if (!pr->alive) continue;
            if (collision_aabb_overlap(pr->x, pr->y, enemy_shot_half_w, enemy_shot_half_h,
                                        gs->player.x, gs->player.y, player_half_w, player_half_h)) {
                pr->alive = false;
                kill_player(gs, events);
                break;
            }
        }
    }
}

static void update_running(GameState *gs, const InputCommand *input, float dt, EventQueue *events) {
    gs->time_elapsed += dt;

    update_player(gs, input, dt, events);
    spawner_update(gs, dt);
    update_enemies(gs, dt);
    update_projectiles(gs, dt);
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
