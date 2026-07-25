#ifndef GALAXY_INVADERS_DOMAIN_TYPES_H
#define GALAXY_INVADERS_DOMAIN_TYPES_H

#include <stdbool.h>
#include "domain/constants.h"

/* Pure domain entities and value objects. Nothing in this header may depend
 * on SDL or any other outward-layer library — see include/ports for the
 * abstractions that let outer layers plug in without this layer knowing. */

typedef struct Color {
    unsigned char r, g, b, a;
} Color;

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
} Player;

typedef enum EnemyShape {
    ENEMY_SHAPE_INVADER = 0,
    ENEMY_SHAPE_SAUCER = 1,
} EnemyShape;

typedef struct Enemy {
    bool alive;
    float x, y;
    float vx, vy;
    float size;
    Color color;
    EnemyShape shape;
    float fire_timer;
    float wobble_phase;
} Enemy;

typedef struct Projectile {
    bool alive;
    float x, y;
    float vx, vy;
    Color color;
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

    int score;
    int last_game_score;
    float time_elapsed;
    float spawn_timer;
    float menu_blink_timer;

    bool quit_requested;
} GameState;

#endif
