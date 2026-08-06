#ifndef GALAXY_INVADERS_DOMAIN_CONSTANTS_H
#define GALAXY_INVADERS_DOMAIN_CONSTANTS_H

/* Pure domain constants. No dependency on any rendering or platform library. */

/* Baseline design canvas the numbers below were tuned against. At runtime
 * the actual playfield is exactly whatever the real screen measures (see
 * GameState.screen_w/screen_h) so the game always fills it edge to edge
 * with the screen's own aspect ratio - no letterboxing, no stretching.
 * Every *spatial* constant here (sizes, speeds) is a value "at design
 * scale" and must be multiplied by GameState.scale before use, which keeps
 * width and height scaled by the same factor so nothing gets distorted. */
#define DESIGN_W 480
#define DESIGN_H 640

#define TARGET_FPS 60

#define MAX_PLAYER_PROJECTILES 32
#define MAX_ENEMY_PROJECTILES 64
#define MAX_ENEMIES 48
#define MAX_EXPLOSIONS 32
#define MAX_TRAIL_PARTICLES 64
#define MAX_STARS 80
#define MAX_EVENTS 16

#define PLAYER_WIDTH 32.0f
#define PLAYER_HEIGHT 32.0f
#define PLAYER_SPEED 240.0f
#define PLAYER_FIRE_COOLDOWN 0.18f
#define PLAYER_BOTTOM_MARGIN 40.0f

/* Life is tracked as a percentage, [0, 100]. Contact with an enemy
 * spaceship or the boss's menace ring is always instantly fatal regardless
 * of life remaining; only being hit by an enemy projectile drains life by
 * this amount, and reaching 0 is what actually kills the player that way.
 * Capturing the orb (see ORB_SCORE_STEP) refills life to 100 alongside
 * granting the super beam. */
#define PLAYER_LIFE_MAX 100.0f
#define PLAYER_LIFE_LOSS_PER_HIT 10.0f
/* At or below this percentage the life bar switches from yellow to red. */
#define PLAYER_LIFE_LOW_THRESHOLD 20.0f

/* Cosmetic engine exhaust trailing from the back of the ship - see
 * TrailParticle in domain/types.h and update_player_trail in
 * usecases/game_logic.c. Purely visual, never touched by collision. */
#define TRAIL_PARTICLE_LIFETIME                                                \
  1.0f /* "remain on screen for about 1 second"                                \
        */
#define TRAIL_SPAWN_INTERVAL                                                   \
  0.02f /* ~50 particles/sec, a steady but sparse stream */
#define TRAIL_PARTICLE_BASE_SIZE 3.0f
#define TRAIL_PARTICLE_SIZE_GROWTH                                             \
  2.5f /* radius multiplier reached by end of life, like dispersing smoke */
#define TRAIL_PARTICLE_SPEED 30.0f        /* base backward drift speed */
#define TRAIL_PARTICLE_JITTER_SPEED 18.0f /* random sideways wobble range */
/* "subtle...25% visibility": alpha never exceeds this even at a particle's
 * brightest instant, out of 255. */
#define TRAIL_PARTICLE_MAX_ALPHA 32

#define PLAYER_PROJECTILE_W 3.0f
#define PLAYER_PROJECTILE_H 14.0f
#define PLAYER_PROJECTILE_SPEED 520.0f

#define ENEMY_PROJECTILE_W 6.0f
#define ENEMY_PROJECTILE_H 8.0f
#define ENEMY_PROJECTILE_SPEED 260.0f
#define ENEMY_FIRE_CHANCE_PER_SEC 0.35f

#define ENEMY_KIND_COUNT 16

#define ENEMY_MIN_SIZE PLAYER_WIDTH
#define ENEMY_MAX_SIZE PLAYER_WIDTH
#define ENEMY_BASE_SPEED 55.0f
#define ENEMY_SPEED_PER_1000_SCORE 15.0f
#define ENEMY_MAX_SPEED 220.0f

#define BASE_SPAWN_INTERVAL 1.10f
#define MIN_SPAWN_INTERVAL 0.22f
#define SPAWN_INTERVAL_STEP_SCORE 500.0f
#define SPAWN_INTERVAL_STEP_FACTOR 0.92f
/* Single knob to tune overall spawn frequency without touching the curve
 * above: multiplies the final interval difficulty_spawn_interval returns,
 * so >1 spreads spawns out (slower) and <1 bunches them up (faster). 2.0
 * halves the spawn rate. */
#define SPAWN_RATE_MULTIPLIER 2.0f

#define SCORE_PER_KILL 10
#define SCORE_MULTIPLIER_STEP 500.0f
#define SCORE_MULTIPLIER_INCREMENT 0.1f

#define EXPLOSION_DURATION 0.50f

#define LASER_COLOR_SCORE_STEP 200

#define ORB_SCORE_STEP 200
#define ORB_SPAWN_CHANCE 0.5f
#define ORB_SIZE 22.0f
#define ORB_FALL_SPEED 26.0f
#define ORB_DRIFT_SPEED 55.0f
#define ORB_DRIFT_ANGULAR_SPEED 1.4f
#define ORB_HUE_CYCLE_SPEED 140.0f
/* Shooting (rather than capturing) the orb schedules every enemy alive on
 * screen at that instant - never the boss - to explode at its own random
 * moment within this many seconds, instead of all at once. */
#define ORB_SHOT_EXPLOSION_WINDOW 0.5f

#define SUPER_BEAM_DURATION 10.0f
#define SUPER_BEAM_WIDTH_MULTIPLIER 2.0f
#define SUPER_BEAM_SPEED_MULTIPLIER 2.0f

/* Purely cosmetic animation - the beam's *gameplay* width (what it
 * actually neutralizes) stays fixed at SUPER_BEAM_WIDTH_MULTIPLIER; only
 * what's drawn pulses and cycles color, so a target's fate never depends
 * on the animation's phase. */
#define SUPER_BEAM_WIDTH_PULSE_SPEED 16.0f
#define SUPER_BEAM_WIDTH_PULSE_AMOUNT 0.35f
#define SUPER_BEAM_COLOR_CYCLE_SPEED 420.0f

/* --- Player shooting modes (see ShootMode in domain/types.h) --- */

/* Mode 2: rapid fire. A dedicated, independently tunable rate constant
 * (rather than deriving from PLAYER_FIRE_COOLDOWN) so the burst's speed can
 * be tuned without touching the normal mode's. */
#define RAPID_FIRE_SHOTS_PER_SEC 10.0f
#define RAPID_FIRE_SHOT_INTERVAL (1.0f / RAPID_FIRE_SHOTS_PER_SEC)
#define RAPID_FIRE_BURST_DURATION 3.0f
#define RAPID_FIRE_LOCKOUT_DURATION 4.0f
#define RAPID_FIRE_PROJECTILE_RADIUS 5.0f

/* Mode 3: power cannon. */
#define POWER_CANNON_FIRE_COOLDOWN 1.0f
#define POWER_CANNON_PROJECTILE_RADIUS 20.0f
#define POWER_CANNON_PROJECTILE_SPEED_MULTIPLIER                               \
  0.6f /* "a little bit slower" than a normal shot */
/* "25% of the screen" - of the shorter screen dimension, so the blast
 * reads sanely regardless of the display's aspect ratio. */
#define POWER_CANNON_EXPLOSION_RADIUS_RATIO 0.25f

/* Modes 4 & 5: double barrel / side beams fire from the wingtips instead of
 * the nose - offset from center approximating where the ship sprite's wings
 * sit. */
#define PLAYER_WING_OFFSET_X (PLAYER_WIDTH * 0.42f)

#define BOSS_SCORE_STEP 500
#define BOSS_HITS_INCREMENT 50
/* The boss's own baseline size range, scaled up by BOSS_SIZE_MULTIPLIER -
 * kept independent of ENEMY_MIN_SIZE/MAX_SIZE (which enemies spawn at)
 * so resizing ordinary enemies never changes the boss encounter. */
#define BOSS_BASE_MIN_SIZE 15.0f
#define BOSS_BASE_MAX_SIZE 25.0f
#define BOSS_SIZE_MULTIPLIER 10.0f
#define BOSS_SPEED_MULTIPLIER                                                  \
  0.25f /* of PLAYER_SPEED - relentlessly chases, never idles */
#define BOSS_KILL_SCORE_MULTIPLIER 4
/* The visible danger ring drawn around the boss (adapters/sdl_renderer.c)
 * IS the contact hitbox - shared here so the two can never drift apart:
 * the moment that ring reaches the player, both explode and it's game
 * over, on the very first touch. */
#define BOSS_MENACE_RING_RATIO 0.58f
/* Only used by the super beam's sustained-contact damage against the
 * boss now (see update_super_beam) - ordinary ring contact with the
 * player is instantly fatal and needs no interval. */
#define BEAM_BOSS_HIT_INTERVAL 0.5f
#define ENEMY_FLEE_SPEED_MULTIPLIER 7.0f
#define ENEMY_SHOT_FADE_DURATION 0.6f

#endif
