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

typedef struct Player {
    float x, y;
    bool alive;
    float fire_cooldown;
    Color laser_color;
    float super_beam_timer; /* seconds remaining; 0 = inactive */
    bool god_mode; /* toggled by Ctrl+G; ship turns gold and cannot die */
    float life; /* percentage, [0, PLAYER_LIFE_MAX]; hitting 0 kills the player */
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
} Enemy;

typedef struct Projectile {
    bool alive;
    float x, y;
    float vx, vy;
    Color color;

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
    Star stars[MAX_STARS];
    Orb orb;
    Boss boss;
    int boss_count; /* how many bosses have appeared so far this run */
    int score_since_last_boss; /* resets to 0 on every boss appearance; next one needs BOSS_SCORE_STEP more */

    int score;
    int last_game_score;
    float time_elapsed;
    float spawn_timer;
    float menu_blink_timer;

    bool quit_requested;
} GameState;

#endif
