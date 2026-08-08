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
/* Sized for MAX_ENEMIES all firing at once, including the shooting styles
 * that spawn several shots per trigger (ENEMY_SHOOT_TRISHOT: 3,
 * ENEMY_SHOOT_OMNI: 8 - see EnemyShootStyle in domain/types.h) - comfortably
 * bigger than 64 (the old single-shot-per-enemy baseline) so a full pool
 * stays rare enough that enemy_shot_slots_available's all-or-nothing guard
 * (usecases/game_logic.c) rarely has anything to gate. */
#define MAX_ENEMY_PROJECTILES 160
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

/* The same exhaust effect as above, applied to enemies and the boss too -
 * see EnemyTrailParticle in domain/types.h. Shares TRAIL_PARTICLE_LIFETIME/
 * BASE_SIZE/SIZE_GROWTH/SPEED/JITTER_SPEED above ("the same effect"); only
 * visibility differs, plus the boss reads bigger. MAX_ENEMY_TRAIL_PARTICLES
 * is one pool shared by every enemy and the boss (never the player's own
 * trail_particles, so the two can never compete for slots).
 * ENEMY_TRAIL_SPAWN_INTERVAL is deliberately far sparser than the player's
 * TRAIL_SPAWN_INTERVAL - up to MAX_ENEMIES emitters share that one pool at
 * once, where the player is always exactly one, so each has to emit far
 * less often to keep the total population sane; that's a density/
 * performance knob, not a visual difference, so it doesn't apply to the
 * boss (always exactly one on screen at a time, like the player). */
#define MAX_ENEMY_TRAIL_PARTICLES 160
#define ENEMY_TRAIL_SPAWN_INTERVAL 0.25f /* ~4 particles/sec per enemy */
#define ENEMY_TRAIL_MAX_ALPHA 26         /* ~10% of 255 */
#define BOSS_TRAIL_SPAWN_INTERVAL                                              \
  0.05f                         /* ~20 particles/sec - a single big emitter */
#define BOSS_TRAIL_MAX_ALPHA 38 /* ~15% of 255 */
#define BOSS_TRAIL_SIZE_MULTIPLIER                                             \
  3.0f /* "bigger" - on top of TRAIL_PARTICLE_BASE_SIZE */

/* The same smoke-puff effect as above, trailing every projectile - player
 * and enemy shots alike - instead of a ship; see ProjectileTrailParticle
 * in domain/types.h. Unlike the ship trails (TRAIL_PARTICLE_* /
 * ENEMY_TRAIL_*), which start fire-colored and cool into gray smoke, these
 * keep the exact color of the projectile that spawned them for their whole
 * life - only alpha/size still follow the same fade/grow curve. One pool
 * shared by both player_shots and enemy_shots (up to
 * MAX_PLAYER_PROJECTILES + MAX_ENEMY_PROJECTILES = 192 emitters, far more
 * than the single ship/handful of enemies the ship trails budget for). At
 * steady state each emitter keeps roughly
 * PROJECTILE_TRAIL_LIFETIME / PROJECTILE_TRAIL_SPAWN_INTERVAL (~10) puffs
 * alive at once, so the pool needs real headroom over that 192-emitter
 * worst case - too small a pool (a former 256 was not enough) means
 * spawn_projectile_trail_particle silently drops puffs once it's full
 * (same "just does nothing" convention every spawn_* here follows), which
 * read as some projectiles randomly missing their trail entirely during
 * busy scenes rather than merely a shorter trail. */
#define MAX_PROJECTILE_TRAIL_PARTICLES 768
#define PROJECTILE_TRAIL_SPAWN_INTERVAL 0.035f /* ~29 particles/sec per projectile */
#define PROJECTILE_TRAIL_LIFETIME 0.35f
#define PROJECTILE_TRAIL_BASE_SIZE 2.2f
#define PROJECTILE_TRAIL_SIZE_GROWTH 2.2f
/* ~16% of 255 - half of the previous 80, for a subtler wake. Every shot's
 * trail reads at this same visibility, player and enemy alike - deliberately
 * a single flat constant, not something any one ship's weapon mode can
 * override. */
#define PROJECTILE_TRAIL_MAX_ALPHA 40

#define PLAYER_PROJECTILE_W 3.0f
#define PLAYER_PROJECTILE_H 14.0f
#define PLAYER_PROJECTILE_SPEED 520.0f

#define ENEMY_PROJECTILE_SPEED 260.0f
/* Per-difficulty base enemy fire chance and its time ramp live in
 * usecases/difficulty.c (difficulty_enemy_fire_chance_per_sec) - see the
 * "Difficulty levels" section below for the ramp constants. */

/* --- Enemy shooting styles (see EnemyShootStyle in domain/types.h) ---
 * Each of the 16 enemy designs (see kEnemyKindShootStyle in
 * usecases/spawner.c) is wired to exactly one of these 5 patterns; only the
 * pattern and projectile shape vary between them - every style still deals
 * PLAYER_LIFE_LOSS_PER_HIT per hit, so the difference is purely which shots
 * the player has to dodge, never how hard they hit. */

/* Style 1 (ENEMY_SHOOT_THIN_BEAM): a slim beam, like the player's own bolt
 * (PLAYER_PROJECTILE_W/H) but thinner. */
#define ENEMY_THIN_BEAM_HALF_LENGTH 5.0f
#define ENEMY_THIN_BEAM_HALF_WIDTH 1.8f

/* Style 2 (ENEMY_SHOOT_LONG_BEAM): the same beam, stretched much longer. */
#define ENEMY_LONG_BEAM_HALF_LENGTH 32.0f
#define ENEMY_LONG_BEAM_HALF_WIDTH 1.8f

/* Style 3 (ENEMY_SHOOT_TRIBURST): 3 small round shots fired back-to-back
 * rather than a single beam, then the normal enemy fire timer restarts. */
#define ENEMY_TRIBURST_SHOT_COUNT 3
#define ENEMY_TRIBURST_SHOT_INTERVAL 0.08f
#define ENEMY_TRIBURST_ORB_RADIUS 4.0f

/* Style 4 (ENEMY_SHOOT_TRISHOT): 3 beams per trigger - one straight ahead
 * and two angled diagonally (down-left/down-right, at 45 degrees). */
#define ENEMY_TRISHOT_HALF_LENGTH 6.0f
#define ENEMY_TRISHOT_HALF_WIDTH 2.0f

/* Style 5 (ENEMY_SHOOT_OMNI): 8 small round shots fired at once in every
 * direction, evenly spaced like the points of an octagon. Slower than a
 * normal straight shot since there are 8x the shots to dodge at once. */
#define ENEMY_OMNI_SHOT_COUNT 8
#define ENEMY_OMNI_ORB_RADIUS 4.0f
#define ENEMY_OMNI_SPEED_RATIO 0.75f

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

/* --- Difficulty levels (see Difficulty in domain/types.h) ---
 * Chosen on the difficulty-select screen reached from the main menu (see
 * update_difficulty_select in usecases/game_logic.c) and kept for the
 * whole run. Each level's own base spawn-rate multiplier and enemy fire
 * chance live in usecases/difficulty.c (kDifficultyBaseSpawnRateMultiplier/
 * kDifficultyBaseFireChancePerSec) - this domain layer only owns the knobs
 * that don't vary by level: how both ramp up over the course of a single
 * run (gs->time_elapsed, reset to 0 by reset_run) regardless of which
 * difficulty was picked, since every level should feel like it's escalating
 * the longer a run lasts, not just start harder and stay flat. */

/* The spawn-rate multiplier (see difficulty_spawn_interval -
 * multiplies the computed interval, so *lower* means *faster* spawns)
 * drops by this amount every SPAWN_RATE_RAMP_INTERVAL seconds, floored at
 * SPAWN_RATE_MULTIPLIER_MIN so a very long run never spawns absurdly fast
 * on top of MIN_SPAWN_INTERVAL's own floor. Values are starting points -
 * the actual "1 minute" cadence and step size are meant to be tuned by
 * feel. */
#define SPAWN_RATE_RAMP_INTERVAL 60.0f
#define SPAWN_RATE_RAMP_STEP 0.12f
#define SPAWN_RATE_MULTIPLIER_MIN 0.5f

/* The enemy fire chance (see difficulty_enemy_fire_chance_per_sec - higher
 * means enemies fire more often) climbs by this amount every
 * FIRE_CHANCE_RAMP_INTERVAL seconds, capped at ENEMY_FIRE_CHANCE_MAX.
 * Same "tune by feel" caveat as the spawn-rate ramp above. */
#define FIRE_CHANCE_RAMP_INTERVAL 180.0f
#define FIRE_CHANCE_RAMP_STEP 0.05f
#define ENEMY_FIRE_CHANCE_MAX 1.5f

#define SCORE_PER_KILL 10
#define SCORE_MULTIPLIER_STEP 500.0f
#define SCORE_MULTIPLIER_INCREMENT 0.1f

#define EXPLOSION_DURATION 0.50f

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

/* Mode 1's per-shot damage - every other mode's damage is defined relative
 * to this (see the per-mode DAMAGE_MULTIPLIER constants below), and
 * everything gets programmatically funneled through Projectile.damage
 * (set at spawn in each mode's fire function) so retuning the balance
 * never means touching collision code. Only meaningful against the boss
 * (see damage_boss) - every other target in the game dies to a single hit
 * regardless of which mode landed it, having no hit-point pool of its own
 * to scale against. */
#define BASE_PLAYER_DAMAGE 1.0f

/* Mode 2: rapid fire. A dedicated, independently tunable rate constant
 * (rather than deriving from PLAYER_FIRE_COOLDOWN) so the burst's speed can
 * be tuned without touching the normal mode's. */
#define RAPID_FIRE_SHOTS_PER_SEC 10.0f
#define RAPID_FIRE_SHOT_INTERVAL (1.0f / RAPID_FIRE_SHOTS_PER_SEC)
#define RAPID_FIRE_BURST_DURATION 3.0f
#define RAPID_FIRE_LOCKOUT_DURATION 4.0f
#define RAPID_FIRE_PROJECTILE_RADIUS 5.0f

/* Mode 3: power cannon. 3x a normal shot's damage, to go with its much
 * slower rate of fire and heavier, explosive shot. */
#define POWER_CANNON_DAMAGE_MULTIPLIER 3.0f
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
/* Mode 4 fires two shots per trigger pull instead of one, each at half a
 * normal shot's damage, so landing both on the same target costs the same
 * total damage as mode 1 - the tradeoff is being able to split them across
 * two separate targets instead. */
#define DOUBLE_BARREL_DAMAGE_MULTIPLIER 0.5f

/* --- C-24's own weapon (see usecases/ship.c for its 3-slot moveset -
 * B-20's double barrel and power cannon reused as-is at their own damage
 * above, plus this ship-exclusive third mode) --- */

/* Mode 3 (SHOOT_MODE_OMNI): the same 8-direction, fire-all-at-once pattern
 * as ENEMY_SHOOT_OMNI (see ENEMY_OMNI_SHOT_COUNT above, reused directly -
 * it's the same shape, just from the player's own position), gated on a
 * single cooldown like every mode but rapid fire. "1 shot per second"
 * means the whole 8-pellet volley retriggers once a second, not that each
 * pellet fires individually. */
#define SHIP_C24_OMNI_FIRE_COOLDOWN 1.0f
/* Modes 1 and 3 render and hit-test as a sphere this size (see
 * player_shot_half_extents and draw_c24_sphere_shot in
 * adapters/sdl_renderer.c), deliberately matching ENEMY_OMNI_ORB_RADIUS
 * rather than B-20's own per-mode sizing (PLAYER_PROJECTILE_W/H) - a
 * kept-independent copy, not a shared constant, so retuning one ship's
 * projectile size can never silently retune the other's. */
#define SHIP_C24_PROJECTILE_RADIUS 4.0f
/* Mode 2 (the power-cannon reuse) is a much bigger sphere than C-24's other
 * two modes - "8x bigger" than SHIP_C24_PROJECTILE_RADIUS, matching how
 * much heavier the shot itself is (POWER_CANNON_DAMAGE_MULTIPLIER). */
#define SHIP_C24_POWER_MODE_RADIUS (SHIP_C24_PROJECTILE_RADIUS * 8.0f)
/* Degrees/sec C-24's own sphere shots cycle hue at (see
 * draw_c24_sphere_shot) - the same "continuously cycling color" effect the
 * power orb's own hue does (ORB_HUE_CYCLE_SPEED, update_orb), just computed
 * independently per shot from its own phase_seed rather than driven by one
 * shared, stored, incrementally-updated Orb.hue. Purely cosmetic. */
#define SHIP_C24_PROJECTILE_HUE_CYCLE_SPEED 180.0f

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
