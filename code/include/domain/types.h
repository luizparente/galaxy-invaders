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
    STATE_DIFFICULTY_SELECT,
    STATE_SHIP_SELECT,
    STATE_GAME,
    STATE_PAUSE,
    STATE_GAME_OVER,
} GameStateId;

typedef enum PauseSelection {
    PAUSE_RESUME = 0,
    PAUSE_EXIT = 1,
} PauseSelection;

/* Chosen on the difficulty-select screen (see update_difficulty_select in
 * usecases/game_logic.c) reached right after confirming START GAME on the
 * main menu, and kept for the whole run - see GameState.selected_difficulty
 * and usecases/difficulty.c for how each level maps to actual spawn/fire
 * tuning. Ordered easiest to hardest so the enum value alone can drive both
 * menu cursor navigation and the difficulty curve's steepness. */
typedef enum Difficulty {
    DIFFICULTY_BABY = 0,
    DIFFICULTY_EASY,
    DIFFICULTY_NORMAL,
    DIFFICULTY_HARD,
    DIFFICULTY_INSANE,
    DIFFICULTY_COUNT,
} Difficulty;

/* Chosen on the ship-select screen (see update_ship_select in
 * usecases/game_logic.c) reached right after confirming a difficulty, and
 * kept for the whole run - see GameState.selected_ship and usecases/ship.c
 * for how each ship's Speed/Strength ratings translate into real gameplay
 * multipliers. Only SHIP_B20 and SHIP_C24 are implemented; the ship-select
 * grid has room for more (adapters/sdl_renderer.c) but every slot past
 * SHIP_COUNT renders as a locked placeholder, not a real Ship value. */
typedef enum Ship {
    SHIP_B20 = 0,
    SHIP_C24,
    SHIP_COUNT,
} Ship;

/* Every shooting pattern any ship can fire, switched with the 1-5 number
 * keys (see InputCommand). Which of these a given ship actually has, and
 * which key selects which, is per-ship - not every ship offers every mode,
 * and the same key can mean a different mode on a different ship (see
 * ship_shoot_mode_for_slot/ship_shoot_mode_slot_count in usecases/ship.h,
 * and update_shoot_mode_switch/reset_run in usecases/game_logic.c, which
 * are the only places a ShootMode value gets assigned to Player.shoot_mode).
 * The HUD's mode indicator (adapters/sdl_renderer.c) draws
 * ship_shoot_mode_slot_count(selected_ship) dots, not SHOOT_MODE_COUNT - a
 * new mode only needs a slot in some ship's own table (usecases/ship.c) to
 * show up there. */
typedef enum ShootMode {
    SHOOT_MODE_NORMAL = 0,
    SHOOT_MODE_RAPID,
    SHOOT_MODE_POWER,
    SHOOT_MODE_DOUBLE,
    SHOOT_MODE_SIDE,
    /* 8 shots fired at once in every direction, evenly spaced like the
     * points of an octagon - the same pattern ENEMY_SHOOT_OMNI uses (see
     * EnemyShootStyle below), just from the player's own position. Not part
     * of B-20's own moveset - only ships whose own slot table includes it
     * (currently just C-24) can ever reach it. */
    SHOOT_MODE_OMNI,
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

/* Which of the 5 shooting patterns an enemy design fires - see
 * kEnemyKindShootStyle in usecases/spawner.c for which of the 16 designs
 * (Enemy.kind) uses which, and the per-style constants in domain/constants.h.
 * Purely a difference in what the player has to dodge, not how hard it
 * hits: every style still deals PLAYER_LIFE_LOSS_PER_HIT per contact. */
typedef enum EnemyShootStyle {
    ENEMY_SHOOT_THIN_BEAM = 0, /* a slim beam, like the player's own but thinner */
    ENEMY_SHOOT_LONG_BEAM,     /* the same beam, stretched much longer */
    ENEMY_SHOOT_TRIBURST,      /* 3 small round shots fired back-to-back */
    ENEMY_SHOOT_TRISHOT,       /* 3 beams per trigger: forward + both diagonals */
    ENEMY_SHOOT_OMNI,          /* 8 small round shots fired at once, all directions */
} EnemyShootStyle;

typedef struct Enemy {
    bool alive;
    float x, y;
    float vx, vy;
    float size;
    Color color; /* a random color rolled at spawn time (see spawner.c); tints this enemy's projectiles - the sprite itself carries its own fixed colors */
    int kind; /* index into adapters/enemy_sprites' kEnemySprites, [0, ENEMY_KIND_COUNT); also picks this enemy's EnemyShootStyle, see kEnemyKindShootStyle */
    float fire_timer;
    float wobble_phase;

    /* ENEMY_SHOOT_TRIBURST's own state: burst_shots_remaining counts down
     * shots left in the in-progress burst (0 = idle, waiting on
     * fire_timer like every other style); burst_shot_timer paces the short
     * gap between each shot within a burst. Unused by every other style. */
    int burst_shots_remaining;
    float burst_shot_timer;

    /* Set when a shot (not captured) power orb schedules this enemy to
     * detonate; orb_kill_timer counts down the random per-enemy delay
     * (see ORB_SHOT_EXPLOSION_WINDOW) before it actually happens. */
    bool orb_kill_pending;
    float orb_kill_timer;

    /* Counts down to this enemy's next engine trail particle emission -
     * see update_enemy_and_boss_trails, the enemy/boss counterpart to the
     * player's own update_player_trail. Purely cosmetic. */
    float trail_emit_timer;
} Enemy;

/* Drives the player shot's rendering (adapters/sdl_renderer.c) and, for
 * PROJECTILE_KIND_POWER, its explode-on-contact behavior in check_collisions.
 * Unused (left NORMAL) by enemy shots. */
typedef enum ProjectileKind {
    PROJECTILE_KIND_NORMAL = 0,
    PROJECTILE_KIND_RAPID,
    PROJECTILE_KIND_POWER,
} ProjectileKind;

/* Drives an enemy shot's rendering (adapters/sdl_renderer.c) and hitbox
 * (enemy_shot_half_extents in usecases/game_logic.c) - BEAM shots (styles
 * thin/long/trishot) are a slim bolt oriented along their own travel
 * direction, sized by Projectile.half_len/half_wid; ORB shots (styles
 * triburst/omni) are a glowing sphere, sized by half_len alone (its
 * radius; half_wid unused). Unused by player shots (see ProjectileKind). */
typedef enum EnemyProjectileKind {
    ENEMY_PROJECTILE_BEAM = 0,
    ENEMY_PROJECTILE_ORB,
} EnemyProjectileKind;

typedef struct Projectile {
    bool alive;
    float x, y;
    float vx, vy;
    Color color;
    ProjectileKind kind;
    /* How much of the boss's hit pool this shot consumes on contact (see
     * damage_boss) - BASE_PLAYER_DAMAGE for most modes, scaled by that
     * mode's own *_DAMAGE_MULTIPLIER constant otherwise. Every other
     * target dies to any hit regardless of this value. */
    float damage;
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

    /* Enemy shots only (see EnemyProjectileKind above and
     * enemy_shot_half_extents in usecases/game_logic.c): half_len is a
     * beam's half-length along its travel direction or an orb's radius;
     * half_wid is a beam's half-width across its travel direction (unused
     * by orbs). Already scaled by GameState.scale at spawn time, same
     * convention spawn_player_shot's vx/vy already follow. */
    EnemyProjectileKind enemy_kind;
    float half_len;
    float half_wid;

    /* Counts down to this shot's next smoke-trail puff emission - see
     * spawn_projectile_trail_particle/update_projectile_trails in
     * usecases/game_logic.c, the projectile counterpart to
     * Player.trail_emit_timer/Enemy.trail_emit_timer. Shared by both
     * player_shots and enemy_shots, at the same PROJECTILE_TRAIL_MAX_ALPHA
     * visibility for every shot regardless of source - player and enemy
     * projectiles read identically here by design. Purely cosmetic. */
    float trail_emit_timer;
    /* Random per-shot phase seed, radians [0, 2*pi) - set at spawn (see
     * spawn_player_shot) and read only by C-24's own sphere-shot rendering
     * (draw_c24_sphere_shot in adapters/sdl_renderer.c, reached whenever
     * GameState.selected_ship is SHIP_C24): offsets that shot's own hue-
     * cycling phase so simultaneous shots - a double-barrel pair, all 8 of
     * an omni burst - don't cycle color in lockstep. Unused by every other
     * shot. */
    float phase_seed;
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

/* The same fire/smoke exhaust as TrailParticle above, applied to enemies
 * and the boss instead of the player - see spawn_enemy_trail_particle and
 * update_enemy_and_boss_trails in usecases/game_logic.c. A separate pool
 * (rather than sharing the player's trail_particles) so a screen full of
 * enemies can never starve the player's own trail of slots, and so the
 * player's existing trail code stays completely untouched by this.
 * alpha_cap bakes in each source's max visibility at spawn time (~5% for
 * enemies, ~15% for the boss, see draw_enemy_trail_particle) so the
 * renderer doesn't need to know which kind of ship a particle came from,
 * just how to draw one. */
typedef struct EnemyTrailParticle {
    bool alive;
    float x, y;
    float vx, vy;
    float age;
    float max_age;
    float size;
    unsigned char alpha_cap;
} EnemyTrailParticle;

/* The same smoke-puff mechanics as TrailParticle/EnemyTrailParticle above
 * (drift, drag, grow, fade - see draw_projectile_trail_particle), trailing
 * every projectile - player and enemy shots alike - instead of a ship; see
 * spawn_projectile_trail_particle and update_projectile_trails in
 * usecases/game_logic.c. Unlike the ship trails, which start fire-colored
 * and cool into gray smoke as they age, color is captured once at spawn
 * from the exact Projectile.color that emitted it and never shifts -
 * only alpha (fade) and size (growth) animate over the puff's life, so the
 * trail always reads as "this projectile's own color," never a generic
 * fire/smoke tone. A single pool shared by both player_shots and
 * enemy_shots (each Projectile carries its own trail_emit_timer) since
 * projectiles of either side get identical treatment. */
typedef struct ProjectileTrailParticle {
    bool alive;
    float x, y;
    float vx, vy;
    float age;
    float max_age;
    float size;
    Color color;
} ProjectileTrailParticle;

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

    /* A running total of Projectile.damage landed so far, not a literal
     * shot count - float because some modes deal fractional multiples of
     * BASE_PLAYER_DAMAGE (see damage_boss). hits_required stays a whole
     * number: the size of the pool, tuned in units of one normal-mode hit. */
    float hits_taken;
    int hits_required;

    /* The super beam can still whittle the boss down over sustained
     * contact (unlike the player, it isn't fatal to it); this timer
     * paces those repeat hits. Breaking contact resets it so the next
     * beam touch deals damage instantly again. */
    float beam_contact_timer;

    /* Counts down to the boss's next engine trail particle emission - see
     * update_enemy_and_boss_trails. Purely cosmetic. */
    float trail_emit_timer;
} Boss;

typedef struct GameState {
    GameStateId state;
    PauseSelection pause_selection;
    /* The cursor on the difficulty-select screen (see
     * update_difficulty_select in usecases/game_logic.c) and, once
     * confirmed, the difficulty the run in progress was started at - kept
     * across STATE_MENU/STATE_DIFFICULTY_SELECT round trips (reset_run
     * deliberately never touches it) so the last choice is remembered
     * instead of resetting every time. Defaults to DIFFICULTY_NORMAL at
     * startup (see game_init). */
    Difficulty selected_difficulty;

    /* The cursor on the ship-select screen (see update_ship_select in
     * usecases/game_logic.c), reached right after confirming a difficulty,
     * and once confirmed, the ship the run in progress was started with -
     * kept across STATE_MENU/STATE_DIFFICULTY_SELECT/STATE_SHIP_SELECT
     * round trips (reset_run deliberately never touches it), same
     * "selection is the state" pattern as selected_difficulty above.
     * Defaults to SHIP_B20 at startup (see game_init). */
    Ship selected_ship;

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
    EnemyTrailParticle enemy_trail_particles[MAX_ENEMY_TRAIL_PARTICLES];
    ProjectileTrailParticle projectile_trails[MAX_PROJECTILE_TRAIL_PARTICLES];
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
