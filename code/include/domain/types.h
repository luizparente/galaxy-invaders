#ifndef GALAXY_INVADERS_DOMAIN_TYPES_H
#define GALAXY_INVADERS_DOMAIN_TYPES_H

#include <math.h>
#include <stdbool.h>
#include "domain/constants.h"

/* Pure domain entities and value objects. Nothing in this header may depend
 * on SDL or any other outward-layer library — see include/ports for the
 * abstractions that let outer layers plug in without this layer knowing. */

typedef struct Color {
    unsigned char r, g, b, a;
} Color;

/* h in degrees [0, 360), s and v in [0, 1]. A value-type operation on
 * Color, shared by usecases (e.g. rerolling the laser color, cycling the
 * orb's gradient) and the renderer (e.g. animating the super beam) so the
 * conversion math exists in exactly one place. */
static inline Color color_from_hsv(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r1, g1, b1;
    if (h < 60.0f) { r1 = c; g1 = x; b1 = 0.0f; }
    else if (h < 120.0f) { r1 = x; g1 = c; b1 = 0.0f; }
    else if (h < 180.0f) { r1 = 0.0f; g1 = c; b1 = x; }
    else if (h < 240.0f) { r1 = 0.0f; g1 = x; b1 = c; }
    else if (h < 300.0f) { r1 = x; g1 = 0.0f; b1 = c; }
    else { r1 = c; g1 = 0.0f; b1 = x; }

    return (Color){
        (unsigned char)((r1 + m) * 255.0f),
        (unsigned char)((g1 + m) * 255.0f),
        (unsigned char)((b1 + m) * 255.0f),
        255,
    };
}

typedef enum GameStateId {
    STATE_MENU,
    STATE_GAME,
    STATE_PAUSE,
    STATE_GAME_OVER,
} GameStateId;

typedef enum PauseSelection {
    PAUSE_RESUME = 0,
    PAUSE_EXIT = 1,
} PauseSelection;

/* The player's selectable shooting ability - switched with the 1-5 number
 * keys (see InputCommand). SHOOT_MODE_COUNT is also the indicator's dot
 * count in the HUD (adapters/sdl_renderer.c), so a new mode only needs to be
 * inserted before it to show up there automatically. */
typedef enum ShootMode {
    SHOOT_MODE_NORMAL = 0,
    SHOOT_MODE_RAPID,
    SHOOT_MODE_POWER,
    SHOOT_MODE_DOUBLE,
    SHOOT_MODE_SIDE,
    SHOOT_MODE_COUNT,
} ShootMode;

typedef struct Player {
    float x, y;
    bool alive;
    float fire_cooldown;
    Color laser_color;
    float super_beam_timer; /* seconds remaining; 0 = inactive */
    bool god_mode; /* toggled by Ctrl+G; ship turns gold and cannot die */
    float life; /* percentage, [0, PLAYER_LIFE_MAX]; hitting 0 kills the player */

    ShootMode shoot_mode;
    /* Rapid fire's own two-phase timer (see update_rapid_fire in
     * usecases/game_logic.c): rapid_burst_timer counts down the 3s of
     * automatic fire once triggered, then rapid_cooldown_timer counts down
     * the following 4s lockout. Both 0 means idle - free to fire normally
     * or switch modes. Only one of the two is ever nonzero at a time. */
    float rapid_burst_timer;
    float rapid_cooldown_timer;

    /* Counts down to the next engine trail particle emission (see
     * update_player_trail) - purely cosmetic, unrelated to fire_cooldown. */
    float trail_emit_timer;
} Player;

typedef struct Enemy {
    bool alive;
    float x, y;
    float vx, vy;
    float size;
    Color color; /* tints this enemy's projectiles; the sprite itself carries its own fixed colors */
    int kind; /* index into adapters/enemy_sprites' kEnemySprites, [0, ENEMY_KIND_COUNT) */
    float fire_timer;
    float wobble_phase;

    /* Set when a shot (not captured) power orb schedules this enemy to
     * detonate; orb_kill_timer counts down the random per-enemy delay
     * (see ORB_SHOT_EXPLOSION_WINDOW) before it actually happens. */
    bool orb_kill_pending;
    float orb_kill_timer;
} Enemy;

/* Drives the player shot's rendering (adapters/sdl_renderer.c) and, for
 * PROJECTILE_KIND_POWER, its explode-on-contact behavior in check_collisions.
 * Unused (left NORMAL) by enemy shots. */
typedef enum ProjectileKind {
    PROJECTILE_KIND_NORMAL = 0,
    PROJECTILE_KIND_RAPID,
    PROJECTILE_KIND_POWER,
} ProjectileKind;

typedef struct Projectile {
    bool alive;
    float x, y;
    float vx, vy;
    Color color;
    ProjectileKind kind;
    /* True only for side-beam shots (ShootMode SHOOT_MODE_SIDE): the shot
     * travels sideways instead of upward, so its visual and hitbox are
     * elongated along x instead of y (see draw_projectile and
     * player_shot_half_extents). */
    bool horizontal;

    /* Set true only on enemy shots caught out when a boss arrives: they
     * keep drifting and visually fade (see inert_age) but can no longer
     * harm the player. Unused by player shots. */
    bool inert;
    float inert_age;
} Projectile;

typedef struct Explosion {
    bool alive;
    float x, y;
    float age;
    float max_age;
    float max_radius;
} Explosion;

/* One puff of the player ship's engine exhaust - a soft dot that starts
 * fire-colored and cools into smoke as it ages (see draw_trail_particle),
 * spawned continuously from the back of the ship (update_player_trail) and
 * released to drift for TRAIL_PARTICLE_LIFETIME seconds. Purely cosmetic:
 * never collides with anything, never affects gameplay. */
typedef struct TrailParticle {
    bool alive;
    float x, y;
    float vx, vy;
    float age;
    float max_age;
    float size; /* base radius at spawn, already scaled by GameState.scale */
} TrailParticle;

typedef struct Star {
    float x, y;
    float speed;
    unsigned char brightness;
} Star;

/* A rare falling power-up. Captured by the player's ship it grants the
 * super beam; shot by the player's laser it just detonates. */
typedef struct Orb {
    bool alive;
    float x, y;
    float size;
    float hue; /* degrees, 0-360; drives the cycling gradient color below */
    float wobble_phase;
    Color color;
} Orb;

/* A recurring heavyweight encounter: a normal enemy's look scaled way up,
 * that has to be shot down over many hits instead of one, and threatens
 * the player by ramming it rather than shooting at it. It relentlessly
 * seeks the player's exact position - a game of tag, not a stationary
 * turret - so it never idles even if the player stops moving. If its
 * visible danger ring ever reaches the player, both explode instantly:
 * there is no health bar for that, only avoidance. */
typedef struct Boss {
    bool alive;
    float x, y;
    float size;
    int kind; /* index into adapters/enemy_sprites' kEnemySprites, [0, ENEMY_KIND_COUNT) */

    int hits_taken;
    int hits_required;

    /* The super beam can still whittle the boss down over sustained
     * contact (unlike the player, it isn't fatal to it); this timer
     * paces those repeat hits. Breaking contact resets it so the next
     * beam touch deals damage instantly again. */
    float beam_contact_timer;
} Boss;

typedef struct GameState {
    GameStateId state;
    PauseSelection pause_selection;

    /* Real playfield size in pixels (matches the physical screen exactly -
     * see domain/constants.h) and the uniform factor every design-baseline
     * size/speed constant is multiplied by so shapes scale without
     * distortion. */
    int screen_w, screen_h;
    float scale;

    Player player;
    Enemy enemies[MAX_ENEMIES];
    Projectile player_shots[MAX_PLAYER_PROJECTILES];
    Projectile enemy_shots[MAX_ENEMY_PROJECTILES];
    Explosion explosions[MAX_EXPLOSIONS];
    TrailParticle trail_particles[MAX_TRAIL_PARTICLES];
    Star stars[MAX_STARS];
    Orb orb;
    Boss boss;
    int boss_count; /* how many bosses have appeared so far this run */
    /* Points earned with the arena clear since the last boss encounter
     * ended. Frozen while a boss is alive and zeroed the moment one leaves,
     * so the next arrival always costs a full fresh BOSS_SCORE_STEP. */
    int score_since_last_boss;

    int score;
    int last_game_score;
    float time_elapsed;
    float spawn_timer;
    float menu_blink_timer;

    bool quit_requested;
} GameState;

#endif
