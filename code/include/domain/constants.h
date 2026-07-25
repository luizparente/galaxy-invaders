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
#define MAX_STARS 80
#define MAX_EVENTS 16

#define PLAYER_WIDTH 32.0f
#define PLAYER_HEIGHT 22.0f
#define PLAYER_SPEED 240.0f
#define PLAYER_FIRE_COOLDOWN 0.18f
#define PLAYER_MIN_Y_RATIO 0.45f
#define PLAYER_BOTTOM_MARGIN 40.0f

#define PLAYER_PROJECTILE_W 3.0f
#define PLAYER_PROJECTILE_H 14.0f
#define PLAYER_PROJECTILE_SPEED 520.0f

#define ENEMY_PROJECTILE_W 6.0f
#define ENEMY_PROJECTILE_H 8.0f
#define ENEMY_PROJECTILE_SPEED 260.0f
#define ENEMY_FIRE_CHANCE_PER_SEC 0.35f

#define ENEMY_MIN_SIZE 15.0f
#define ENEMY_MAX_SIZE 25.0f
#define ENEMY_BASE_SPEED 55.0f
#define ENEMY_SPEED_PER_1000_SCORE 15.0f
#define ENEMY_MAX_SPEED 220.0f

#define BASE_SPAWN_INTERVAL 1.10f
#define MIN_SPAWN_INTERVAL 0.22f
#define SPAWN_INTERVAL_STEP_SCORE 500.0f
#define SPAWN_INTERVAL_STEP_FACTOR 0.92f

#define SCORE_PER_KILL 10
#define SCORE_MULTIPLIER_STEP 500.0f
#define SCORE_MULTIPLIER_INCREMENT 0.1f

#define EXPLOSION_DURATION 0.28f

#define LASER_COLOR_SCORE_STEP 100

#define ORB_SCORE_STEP 200
#define ORB_SPAWN_CHANCE 0.5f
#define ORB_SIZE 22.0f
#define ORB_FALL_SPEED 26.0f
#define ORB_DRIFT_SPEED 55.0f
#define ORB_DRIFT_ANGULAR_SPEED 1.4f
#define ORB_HUE_CYCLE_SPEED 140.0f
#define ORB_EXPLOSION_RADIUS 90.0f

#define SUPER_BEAM_DURATION 5.0f
#define SUPER_BEAM_WIDTH_MULTIPLIER 2.0f
#define SUPER_BEAM_SPEED_MULTIPLIER 2.0f

/* Purely cosmetic animation - the beam's *gameplay* width (what it
 * actually neutralizes) stays fixed at SUPER_BEAM_WIDTH_MULTIPLIER; only
 * what's drawn pulses and cycles color, so a target's fate never depends
 * on the animation's phase. */
#define SUPER_BEAM_WIDTH_PULSE_SPEED 16.0f
#define SUPER_BEAM_WIDTH_PULSE_AMOUNT 0.35f
#define SUPER_BEAM_COLOR_CYCLE_SPEED 420.0f

#endif
