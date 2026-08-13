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

/* The ship-select screen's cursor grid (adapters/sdl_renderer.c draws it,
 * usecases/game_logic.c's update_ship_select navigates it with all 4 arrow
 * keys) - shared here so the two can never drift apart, same rationale as
 * every other cross-layer shared constant in this file (e.g.
 * BOSS_MENACE_RING_RATIO). Slots past SHIP_COUNT render as locked
 * placeholders rather than real ships - see the Ship enum's own doc
 * comment in domain/types.h. */
#define SHIP_SELECT_GRID_COLS 4
#define SHIP_SELECT_GRID_ROWS 4

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
/* Star speed (see init_stars in usecases/game_logic.c) is
 * STAR_MIN_SPEED + rand * STAR_SPEED_RANGE. Named (rather than left as
 * inline literals) so BACKGROUND_CLOUD_MIN/MAX_SPEED below can be defined
 * as an exact fraction of the star field's own speed, instead of
 * independently-tuned numbers that only coincidentally match. */
#define STAR_MIN_SPEED 20.0f
#define STAR_SPEED_RANGE 70.0f
/* Kept small - each one is a huge source of influence over the pixelated
 * background smoke grid (see BackgroundCloud in domain/types.h), not a
 * cheap dot like a star, so a handful already gives every cell on screen
 * something drifting past it. */
#define MAX_BACKGROUND_CLOUDS 6
#define MAX_EVENTS 16

/* --- Background smoke (see BackgroundCloud in domain/types.h and
 * draw_background_smoke in adapters/sdl_renderer.c) --- */
#define BACKGROUND_CLOUD_MIN_RADIUS 50.0f
#define BACKGROUND_CLOUD_MAX_RADIUS 500.0f
/* Exactly half the star field's own speed range (see STAR_MIN_SPEED/
 * STAR_SPEED_RANGE above) - big clouds drift, they don't rush, and should
 * always read as further away / slower than the stars in front of them. */
#define BACKGROUND_CLOUD_SPEED_RATIO 0.9f
#define BACKGROUND_CLOUD_MIN_SPEED                                             \
  (STAR_MIN_SPEED * BACKGROUND_CLOUD_SPEED_RATIO)
#define BACKGROUND_CLOUD_MAX_SPEED                                             \
  ((STAR_MIN_SPEED + STAR_SPEED_RANGE) * BACKGROUND_CLOUD_SPEED_RATIO)
/* Side-to-side wobble (see BackgroundCloud.wobble_seed/speed/amplitude) -
 * what keeps the whole field visibly reshaping over time as clouds drift
 * past and through each other, rather than just uniformly sliding down. */
#define BACKGROUND_CLOUD_WOBBLE_MIN_AMPLITUDE 10.0f
#define BACKGROUND_CLOUD_WOBBLE_MAX_AMPLITUDE 25.0f
#define BACKGROUND_CLOUD_WOBBLE_MIN_SPEED 0.15f
#define BACKGROUND_CLOUD_WOBBLE_MAX_SPEED 0.9f
/* Design-space edge length of one blocky smoke cell (see
 * draw_background_smoke) - deliberately chunky, matching the game's own
 * pixel-art scale, rather than a smooth gradient. */
#define BACKGROUND_SMOKE_CELL_SIZE 5.0f
/* How much combined cloud influence a cell needs before it's shaded at
 * all (LIGHT) or shaded at the denser tier (DARK) - both tiers land
 * darker than the flat background, never lighter (see kSmokeLight/
 * kSmokeDark in adapters/sdl_renderer.c); the gap between the two
 * thresholds is what keeps a single cloud passing through from reading as
 * a hard-edged disc. */
#define BACKGROUND_SMOKE_LIGHT_THRESHOLD 0.35f
#define BACKGROUND_SMOKE_DARK_THRESHOLD 0.75f
/* [0, 1] - the single contrast knob: how far a shaded cell's color departs
 * from the flat background color, toward black (see kSmokeLight/kSmokeDark
 * in adapters/sdl_renderer.c, both lerp_color'd toward black - kSmokeDark
 * just goes further - so raising this can only ever make the smoke darker,
 * never introduce a lighter shade). Deliberately tiny - under 5% - so the
 * effect stays barely-there. */
#define BACKGROUND_SMOKE_CONTRAST 0.25f

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
#define PROJECTILE_TRAIL_SPAWN_INTERVAL                                        \
  0.035f /* ~29 particles/sec per projectile */
#define PROJECTILE_TRAIL_LIFETIME 0.35f
#define PROJECTILE_TRAIL_BASE_SIZE 2.2f
#define PROJECTILE_TRAIL_SIZE_GROWTH 2.2f
/* ~8% of 255 - half of the previous 40, for a subtler wake. Every shot's
 * trail reads at this same visibility, player and enemy alike - deliberately
 * a single flat constant, not something any one ship's weapon mode can
 * override, with exactly one deliberate exception: Cruzader's own mode 3
 * rockets (see CRUZADER_ROCKET_TRAIL_MAX_ALPHA). */
#define PROJECTILE_TRAIL_MAX_ALPHA 20

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
/* Boss fights replace the score-step mechanic above entirely (see
 * maybe_trigger_orb_spawn's own boss.alive guard in usecases/game_logic.c)
 * with a flat chance rolled on every enemy kill instead (see
 * destroy_enemy_for_score) - a fight's own kill count is small and
 * unpredictable, so a 200-point step would make orbs either flood in or
 * never show up depending on how the fight goes. */
#define BOSS_FIGHT_ORB_SPAWN_CHANCE 0.05f
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
#define RAPID_FIRE_LOCKOUT_DURATION 20.0f
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
/* C-24's mode 2 (the power-cannon reuse) blows up nearby enemies in a 50%
 * bigger damage radius than B-20's own mode 3 gets from
 * POWER_CANNON_EXPLOSION_RADIUS_RATIO alone - applied on top of that ratio
 * in trigger_power_cannon_explosion (usecases/game_logic.c), gated on the
 * triggering shot's own Projectile.style_ship so B-20's mode 3 (and a
 * B-20-kind ChildShip's own mode 2) are untouched regardless of which ship
 * is actually selected. Purely the enemies-caught-in-the-blast radius, not
 * the shot's own rendered/hitbox size (SHIP_C24_POWER_MODE_RADIUS,
 * unchanged) or the blast's visual explosion sprite. */
#define SHIP_C24_POWER_MODE_EXPLOSION_RADIUS_MULTIPLIER 1.5f
/* Degrees/sec C-24's own sphere shots cycle hue at (see
 * draw_c24_sphere_shot) - the same "continuously cycling color" effect the
 * power orb's own hue does (ORB_HUE_CYCLE_SPEED, update_orb), just computed
 * independently per shot from its own phase_seed rather than driven by one
 * shared, stored, incrementally-updated Orb.hue. Purely cosmetic. */
#define SHIP_C24_PROJECTILE_HUE_CYCLE_SPEED 180.0f

/* --- The Mothership + her CPU-flown ChildShip escorts (see
 * usecases/ship.c for her own 2-slot moveset, GameState.children, and
 * update_mothership_dispatch/update_children/update_child_firing in
 * usecases/game_logic.c) --- */

/* How many escorts can be alive at once - past this, holding fire is a
 * no-op until one dies. Deliberately its own named constant (not inlined)
 * so it's a one-line retune, per how it was specced. */
#define MOTHERSHIP_MAX_CHILDREN 3
/* Paces dispatch the same way every other single-shot mode paces its own
 * fire_cooldown (see update_normal_fire/update_power_cannon) - slower than
 * PLAYER_FIRE_COOLDOWN since each "shot" here is a whole extra ship, not a
 * projectile. */
#define MOTHERSHIP_DISPATCH_COOLDOWN 0.6f
/* "The equivalent of 20% of the life the spaceships would have if they
 * were controlled by the player" (2 of B-20's own PLAYER_LIFE_LOSS_PER_HIT
 * shots to die) - PLAYER_LIFE_MAX itself, not a kind-specific value, since
 * a ship's Strength rating already maps to *damage taken per hit*
 * (ship_damage_taken_multiplier), not a bigger life pool - see
 * usecases/ship.c. Applies the same regardless of a child's own
 * SHIP_B20/SHIP_C24 kind - a C-24-kind child's own higher Strength still
 * softens each individual hit, so it takes a bit more than 2 to die. */
#define MOTHERSHIP_CHILD_LIFE_MAX (PLAYER_LIFE_MAX * 0.20f)
/* A dispatched child's own weapon mode (see update_mothership_dispatch) is
 * fixed at its kind's own mode #1 (slot 0) the overwhelming majority of the
 * time - only a 5% chance rolls a uniformly random mode from the rest of
 * that kind's own moveset instead. A B-20-kind child rolled into mode #2
 * (SHOOT_MODE_RAPID) permanently falls back to mode #1 the instant its one
 * burst ends and never returns to mode #2 - see update_child_firing. */
#define MOTHERSHIP_CHILD_RANDOM_MODE_CHANCE 0.05f

/* The brief post-dispatch launch kick (see update_mothership_dispatch):
 * "their first movement being flying left or right at random, until the
 * CPU takes over" - LAUNCH_SPEED reuses PLAYER_SPEED itself for a brisk
 * shove clear of the Mothership before AI (wander/formation) takes over. */
#define MOTHERSHIP_CHILD_LAUNCH_DURATION 0.35f
#define MOTHERSHIP_CHILD_LAUNCH_SPEED PLAYER_SPEED

/* SHOOT_MODE_SWARM_WANDER: each child cruises toward a randomly rolled
 * point, re-rolling once it arrives or this many seconds pass, whichever
 * comes first (see update_children) - kept slower than the launch kick so
 * the hand-off from "flying off sideways" to "wandering" reads as a
 * deliberate slow-down, not a jarring speed-up. Bounds are screen-relative
 * ratios (not raw pixels) so wandering scales with whatever the real
 * screen measures, same convention as every other spatial ratio here
 * (e.g. POWER_CANNON_EXPLOSION_RADIUS_RATIO) - kept to the upper 65% of
 * the screen and off the side edges so children never wander down into
 * the Mothership's own lane at the bottom or clip off-screen. */
#define MOTHERSHIP_CHILD_WANDER_SPEED (PLAYER_SPEED * 0.55f)
#define MOTHERSHIP_CHILD_WANDER_RETARGET_MIN 1.5f
#define MOTHERSHIP_CHILD_WANDER_RETARGET_MAX 3.5f
#define MOTHERSHIP_CHILD_WANDER_ARRIVE_RADIUS 12.0f
#define MOTHERSHIP_CHILD_WANDER_X_MARGIN_RATIO 0.12f
#define MOTHERSHIP_CHILD_WANDER_Y_MIN_RATIO 0.08f
#define MOTHERSHIP_CHILD_WANDER_Y_MAX_RATIO 0.65f

/* SHOOT_MODE_SWARM_FORMATION: a 3-point wedge ahead of the Mothership -
 * one child dead ahead of her (front), the other two at her flanks, both
 * a little closer to her than the front point so the whole shape reads as
 * a forward-pointing triangle rather than a straight line abreast. Offsets
 * are design-scale distances (like every player/enemy size constant),
 * scaled by GameState.scale same as everything else - "not too close"
 * per spec, so comfortably bigger than a ship-width. Which alive child
 * occupies which of the 3 slots is recomputed fresh every frame (see
 * update_children), not a fixed per-child identity, so a slot vacated by
 * a dead child is filled immediately rather than left empty. */
#define MOTHERSHIP_CHILD_FORMATION_SPEED (PLAYER_SPEED * 0.95f)
#define MOTHERSHIP_CHILD_FORMATION_FRONT_OFFSET 95.0f
#define MOTHERSHIP_CHILD_FORMATION_SIDE_OFFSET_X 75.0f
#define MOTHERSHIP_CHILD_FORMATION_SIDE_OFFSET_Y 45.0f

/* --- Shine's own weapon (see usecases/ship.c for her 3-slot moveset) ---
 * Every shot across all 3 modes is a crystal "shard" - a slim pointed
 * quad oriented along its own travel direction (see draw_shine_shard in
 * adapters/sdl_renderer.c), the same direction-vector construction
 * enemy beams already use (capsule_bolt) but tapered to points instead of
 * rounded caps, for the icicle look. Kept fully independent of
 * PLAYER_PROJECTILE_W/H/SPEED - same "kept-independent copies, not shared"
 * precedent as SHIP_C24_PROJECTILE_RADIUS elsewhere here - so retuning
 * Shine's shots can never silently retune B-20's own. */
#define SHINE_SHARD_SPEED PLAYER_PROJECTILE_SPEED
#define SHINE_SHARD_LENGTH 22.0f
#define SHINE_SHARD_WIDTH 7.0f
/* Mode 1 (default): twin shards, straight ahead, close together - "the
 * gap between them equals one shard's width" (SHINE_SHARD_WIDTH). Each
 * shard's center sits SHINE_SHARD_WIDTH out from the nose, which by
 * construction leaves exactly one shard-width of empty space between
 * their facing edges (2 * offset - width = 2*width - width = width). Each
 * shard's damage is halved, same "two shots cost the same total as one"
 * precedent DOUBLE_BARREL_DAMAGE_MULTIPLIER already sets for B-20's own
 * twin-shot mode. */
#define SHINE_SHARDS_FIRE_COOLDOWN 0.18f
#define SHINE_TWIN_SHARD_OFFSET_X SHINE_SHARD_WIDTH
#define SHINE_TWIN_SHARD_DAMAGE_MULTIPLIER 0.5f
/* Mode 2: not a mode the player ever stays in - key 2 instantly fires this
 * 12-way burst (SHINE_OMNI_SHOT_COUNT, half again as many as every other
 * ship's own 8-way SHOOT_MODE_OMNI/ENEMY_SHOOT_OMNI) and puts shoot_mode
 * straight back to mode 1 (see trigger_shine_omni_burst in
 * usecases/game_logic.c). Each shard deals a full, unreduced hit - same as
 * C-24's own 8-way OMNI mode already does despite also being a multi-shot
 * burst, so a multi-directional volley's per-shot damage doesn't
 * universally get nerfed just for having many pellets. */
#define SHINE_OMNI_SHOT_COUNT 12
#define SHINE_OMNI_COOLDOWN 20.0f
/* Mode 3: a single longer shard (SHINE_SPIRAL_SHARD_LENGTH, longer than
 * the twin/omni shards' own SHINE_SHARD_LENGTH - length only, its width is
 * the same SHINE_SHARD_WIDTH as those) fired straight ahead, "2 shots per
 * second" per spec, that visually spins in place (SHINE_SPIRAL_SPIN_SPEED,
 * degrees/sec) as it flies - a cosmetic-only rotation computed from
 * GameState.time_elapsed plus the shot's own phase_seed (draw_shine_shard),
 * the same "per-instance seed plus the global clock" convention
 * Projectile.phase_seed already establishes for C-24's hue cycling - travel
 * direction itself is unaffected, still straight up. Triples its damage
 * (SHINE_SPIRAL_DAMAGE_MULTIPLIER) - the tradeoff for being a single,
 * slower-cadence shot rather than modes 1/2's many smaller ones. */
#define SHINE_SPIRAL_FIRE_COOLDOWN 0.5f
#define SHINE_SPIRAL_SHARD_LENGTH 60.0f
#define SHINE_SPIRAL_SHARD_WIDTH SHINE_SHARD_WIDTH
#define SHINE_SPIRAL_SPIN_SPEED 360.0f
#define SHINE_SPIRAL_DAMAGE_MULTIPLIER 3.0f

/* --- Cruzader's own weapon (see usecases/ship.c for his 3-slot moveset) ---
 * Kept fully independent of PLAYER_PROJECTILE_W/H/SPEED and the Power
 * Cannon's own constants below, same "kept-independent copies, not shared"
 * precedent as Shine's own block above. */
#define CRUZADER_SIZE_MULTIPLIER                                               \
  1.5f /* "50% bigger than B-20" - usecases/ship.c's kShipSizeMultiplier */

/* Mode 1 (default): B-20's DOUBLE pattern (two wingtip shots), recolored
 * green with blue accents (see draw_cruzader_bolt in
 * adapters/sdl_renderer.c), at 1.5 shots/sec - reuses B-20's own
 * DOUBLE_BARREL_DAMAGE_MULTIPLIER, matching "same as B-20's #4" per spec. */
#define CRUZADER_TWIN_FIRE_COOLDOWN (1.0f / 1.5f)
#define CRUZADER_BOLT_LENGTH                                                   \
  26.0f /* oriented bounding box, same construction as Shine's own shard */
#define CRUZADER_BOLT_WIDTH                                                    \
  4.5f /* 50% slimmer than the original 9.0f - "too thick" per feedback */

/* Mode 2: the deflector orb - see SHOOT_MODE_CRUZADER_ORB's own doc comment
 * (domain/types.h) and trigger_cruzader_orb/check_collisions in
 * usecases/game_logic.c. CRUZADER_ORB_RADIUS is deliberately bigger than
 * Cruzader's own (already 1.5x) hitbox - "no projectiles can penetrate the
 * orb" reads as a shield around the ship, not just at its own edges. */
#define CRUZADER_ORB_DURATION 5.0f
#define CRUZADER_ORB_COOLDOWN 20.0f
#define CRUZADER_ORB_RADIUS (PLAYER_WIDTH * CRUZADER_SIZE_MULTIPLIER * 1.4f)
#define CRUZADER_REFLECTED_SHOT_DAMAGE BASE_PLAYER_DAMAGE

/* Mode 3: homing rockets - "1 shot per 2 seconds," slower than a normal
 * bolt so the homing curve reads as a rocket correcting its own course
 * rather than an instant hit. Explosion radius reuses B-20's own Power
 * Cannon radius (POWER_CANNON_EXPLOSION_RADIUS_RATIO, see
 * trigger_power_cannon_explosion) unscaled - confirmed with the user as
 * what "B-20's #2" meant, since B-20's actual key 2 (Rapid Fire) has no
 * explosion of its own. */
#define CRUZADER_ROCKET_FIRE_COOLDOWN 2.0f
#define CRUZADER_ROCKET_SPEED (PLAYER_PROJECTILE_SPEED * 0.6f)
#define CRUZADER_ROCKET_DAMAGE                                                 \
  (BASE_PLAYER_DAMAGE * POWER_CANNON_DAMAGE_MULTIPLIER)
#define CRUZADER_ROCKET_LENGTH 24.0f
#define CRUZADER_ROCKET_WIDTH                                                  \
  5.0f /* 50% slimmer than the original 10.0f - "too thick" per feedback */
/* Denser, more visible, blue-tinted smoke than every other shot's own
 * PROJECTILE_TRAIL_SPAWN_INTERVAL/PROJECTILE_TRAIL_MAX_ALPHA - "increase the
 * visibility of the smoke... make it blue" per feedback, scoped to this one
 * shooting mode on this one ship only (see update_projectile_trails in
 * usecases/game_logic.c) - every other projectile's trail (every other
 * Cruzader mode included) is completely untouched. Color is a plain Color
 * literal, not a #define, next to its two size/cadence siblings here. */
#define CRUZADER_ROCKET_TRAIL_SPAWN_INTERVAL                                   \
  (PROJECTILE_TRAIL_SPAWN_INTERVAL * 0.4f)
#define CRUZADER_ROCKET_TRAIL_MAX_ALPHA                                        \
  90 /* ~4.5x PROJECTILE_TRAIL_MAX_ALPHA's 20 */
#define CRUZADER_ROCKET_TRAIL_SIZE_MULTIPLIER 1.6f

/* Passive (always on, no key of its own): a 50% chance to bounce an
 * incoming enemy shot back instead of taking a full hit - half the usual
 * life loss on a successful reflect (the shot still grazes the ship on its
 * way out), same reflected-shot mechanics/damage as the orb above, just at
 * a much smaller "shield" - only the shot that would've hit the player's
 * own hitbox, not everything in CRUZADER_ORB_RADIUS, and only while the
 * orb itself isn't already handling that shot for free. */
#define CRUZADER_PASSIVE_REFLECT_CHANCE 0.5f
#define CRUZADER_PASSIVE_REFLECT_DAMAGE_MULTIPLIER 0.5f

/* Notes: Cruzader never explodes from touching an ordinary enemy (see
 * damage_cruzader_on_enemy_contact in usecases/game_logic.c), but isn't
 * fully unscathed either - a flat, fixed life-loss penalty, deliberately
 * NOT run through ship_damage_taken_multiplier the way a projectile hit
 * (damage_player) is: this is a trade-off for surviving contact at all,
 * not a hit the normal damage system should be able to soften further. */
#define CRUZADER_ENEMY_CONTACT_LIFE_LOSS 10.0f

/* --- The Twins' own weapon/flight (see usecases/ship.c for their 2-slot
 * moveset, usecases/game_logic.c's SHIP_TWINS branches for how these get
 * used) - "50% bigger than B-20" would be TWINS_SIZE_MULTIPLIER 1.5, but
 * per spec The Twins are only 25% bigger. */
#define TWINS_SIZE_MULTIPLIER 1.25f
/* Distance kept between the two twins' own centers while flying in rigid
 * formation (mode 1, SHOOT_MODE_TWINS_ALTERNATE) - wide enough that their
 * own (already 1.25x) sprites never overlap. */
#define TWINS_FORMATION_GAP (PLAYER_WIDTH * TWINS_SIZE_MULTIPLIER * 1.3f)
/* How fast each twin closes the distance back to its own formation slot
 * (center +/- half TWINS_FORMATION_GAP) after switching back to mode 1
 * from mode 2 - an eased "fly toward each other" reunion (see
 * update_player's own SHIP_TWINS branch), not an instant snap, same
 * steer-toward-target technique as MOTHERSHIP_CHILD_FORMATION_SPEED. A
 * little faster than the ship's own base PLAYER_SPEED so the reunion
 * reads as snappy rather than sluggish. */
#define TWINS_FORMATION_REJOIN_SPEED (PLAYER_SPEED * 1.5f)
/* "Each twin shoots 2 projectiles per second... together 4 total per
 * second" per spec - a flat rate specific to The Twins, not derived from
 * PLAYER_FIRE_COOLDOWN or C-24's own double-barrel cooldown (the mode 1
 * pattern is only reused loosely: one shot per activation, alternating
 * muzzle, not two shots at once - see update_twins_alternating_fire). */
#define TWINS_ALTERNATE_FIRE_COOLDOWN 0.25f
#define TWINS_BOLT_LENGTH 22.0f
#define TWINS_BOLT_WIDTH 4.5f

/* --- Antartica's own weapon + Frosty's own passive weapon (see
 * usecases/ship.c for her 3-slot moveset) - kept fully independent of
 * every other ship's own constants (SHINE_SHARD_*, SHIP_C24_PROJECTILE_*,
 * SUPER_BEAM_*), same "kept-independent copies, not shared" precedent as
 * every other ship's own block above, so retuning Antartica/Frosty can
 * never silently retune anyone else's numbers. */
#define ANTARTICA_FROSTY_SIZE_MULTIPLIER                                       \
  0.75f /* "25% smaller than Antartica" */
/* Frosty's own starting position (see reset_run in usecases/game_logic.c) is
 * TWINS_FORMATION_GAP to Antartica's own left - "the same distance between
 * the two when the game begins" as The Twins' own starting gap, per spec,
 * reused directly rather than duplicated as an independent copy (an actual
 * shared starting-distance convention, not a tunable weapon/size number of
 * Antartica's own that happens to coincide with Twins'). */

/* Mode 1 (default): the same twin-shard pattern as Shine's own mode 1
 * (SHINE_SHARD_*), just "made of ice" - accent-recolored light blue instead
 * of Shine's white/grey (see draw_antartica_shard in
 * adapters/sdl_renderer.c) with "a tad more" white trailing smoke
 * (ANTARTICA_SHARD_TRAIL_* below, see update_projectile_trails). */
#define ANTARTICA_SHARD_SPEED PLAYER_PROJECTILE_SPEED
#define ANTARTICA_SHARD_LENGTH 22.0f
#define ANTARTICA_SHARD_WIDTH 7.0f
#define ANTARTICA_SHARDS_FIRE_COOLDOWN 0.18f
#define ANTARTICA_TWIN_SHARD_OFFSET_X ANTARTICA_SHARD_WIDTH
#define ANTARTICA_TWIN_SHARD_DAMAGE_MULTIPLIER 0.5f
/* "A tad more" than every other shot's own PROJECTILE_TRAIL_BASE_SIZE/
 * PROJECTILE_TRAIL_MAX_ALPHA - a small bump, not the dramatic one Cruzader's
 * own rockets get (CRUZADER_ROCKET_TRAIL_*). Rendered fixed white
 * (see update_projectile_trails), not the shard's own light-blue accent
 * color, per "trailing WHITE smoke" - same "captured, not the projectile's
 * own color" carve-out precedent as Cruzader's own rocket trail. */
#define ANTARTICA_SHARD_TRAIL_SIZE_MULTIPLIER 1.25f
#define ANTARTICA_SHARD_TRAIL_MAX_ALPHA 30

/* Mode 2: Ice Storm - 16 shards (ANTARTICA_ICE_STORM_SHOT_COUNT) spanning
 * Antartica's own full frontal 180 degrees (ANTARTICA_ICE_STORM_SPREAD_DEG),
 * evenly spaced, centered straight up - like Shine's own omni burst
 * (SHOOT_MODE_SHINE_OMNI), never persists as Player.shoot_mode: pressing
 * key 2 fires the volley directly (see trigger_antartica_ice_storm in
 * usecases/game_logic.c) and immediately reverts to mode 1, gated on its
 * own 20s cooldown ("this has a 20 seconds cooldown" per spec, the same
 * duration as Shine's own SHINE_OMNI_COOLDOWN). Each shard deals a full,
 * unreduced hit - same precedent as every other multi-shot burst (Shine's
 * own omni, C-24's own OMNI). */
#define ANTARTICA_ICE_STORM_SHOT_COUNT 16
#define ANTARTICA_ICE_STORM_SPREAD_DEG 180.0f
#define ANTARTICA_ICE_STORM_COOLDOWN 20.0f

/* Mode 3: Super Ice Beam - both Antartica and Frosty fire a continuous
 * freezing beam, white/light-blue only, the same column-sweep mechanics as
 * the power orb's own super beam (update_super_beam) but WITHOUT its heal
 * or invincibility - see update_antartica_freezing_beam in
 * usecases/game_logic.c. Never persists as Player.shoot_mode, same
 * "trigger + immediate revert" pattern as Ice Storm above and Cruzader's
 * own deflector orb - key 3 starts the 5s active window
 * (antartica_freeze_beam_timer) immediately, and the moment that ends, the
 * 30s cooldown (antartica_freeze_beam_cooldown_timer) begins. */
#define ANTARTICA_FREEZE_BEAM_DURATION 5.0f
#define ANTARTICA_FREEZE_BEAM_COOLDOWN 30.0f
#define ANTARTICA_FREEZE_BEAM_WIDTH_MULTIPLIER 2.0f

/* Frosty's own passive weapon - "2 shots per second," fires automatically
 * (see update_frosty_fire), never gated on the fire key or Antartica's own
 * shoot_mode the way every other ship's own moveset is. Visually "like
 * C-24's projectiles" (draw_c24_sphere_shot's layered glow/body/hot-core/
 * glint sphere), just "white and light blue only" instead of hue-cycling
 * (see draw_frosty_snowball) plus "slightly increased" white trailing
 * smoke (FROSTY_SNOWBALL_TRAIL_* below). */
#define FROSTY_SNOWBALL_FIRE_COOLDOWN 0.5f /* 2 shots/sec */
#define FROSTY_SNOWBALL_SPEED PLAYER_PROJECTILE_SPEED
#define FROSTY_SNOWBALL_RADIUS 4.0f
#define FROSTY_SNOWBALL_TRAIL_SIZE_MULTIPLIER 1.3f
#define FROSTY_SNOWBALL_TRAIL_MAX_ALPHA 32

/* --- Buckler's own weapon (see usecases/ship.c for her single-slot
 * moveset, and SHOOT_MODE_BUCKLER_CANNON's own doc comment in
 * domain/types.h) - kept fully independent of every other ship's own
 * constants, same "kept-independent copies, not shared" precedent as every
 * other ship's own block above. */
#define BUCKLER_SIZE_MULTIPLIER 1.0f /* same render/hitbox size as B-20 */

/* The only mode: 5 fixed-direction cannons (west/northwest/north/northeast/
 * east - see kBucklerCannonDir in usecases/game_logic.c), spanning her own
 * frontal 180 degrees, one active at a time - "2 shots per second" per
 * spec while a cannon is held. Speed/damage reuse B-20's own baseline
 * unscaled - only the direction and the round-ball look are unique to her. */
#define BUCKLER_CANNON_FIRE_COOLDOWN 0.5f /* 2 shots/sec */
#define BUCKLER_CANNON_PROJECTILE_SPEED PLAYER_PROJECTILE_SPEED
#define BUCKLER_CANNON_PROJECTILE_RADIUS 6.0f
/* How far from the ship's own center each cannon's shot spawns - offset
 * along that cannon's own firing direction, so a shot visibly leaves from
 * the rim of the hull rather than its exact center. */
#define BUCKLER_CANNON_MUZZLE_OFFSET                                           \
  (PLAYER_WIDTH * BUCKLER_SIZE_MULTIPLIER * 0.5f)

/* Spacebar: the protective orb - same defensive behavior/duration/cooldown
 * as Cruzader's own deflector orb (CRUZADER_ORB_DURATION/COOLDOWN), just
 * blocking incoming fire outright (see check_collisions' own SHIP_BUCKLER
 * branch) instead of reflecting it back at the enemies, and with no passive
 * always-on chance the way Cruzader's own CRUZADER_PASSIVE_REFLECT_CHANCE
 * is - Buckler's orb is entirely spacebar-gated. */
#define BUCKLER_ORB_DURATION 5.0f
#define BUCKLER_ORB_COOLDOWN 20.0f
#define BUCKLER_ORB_RADIUS (PLAYER_WIDTH * BUCKLER_SIZE_MULTIPLIER * 1.4f)

/* --- Samurai's own weapon (see usecases/ship.c for her 3-slot moveset, and
 * the SHOOT_MODE_SAMURAI_... / PROJECTILE_KIND_SAMURAI_SHURIKEN doc comments
 * in domain/types.h) - kept fully independent of every other ship's own
 * constants, same "kept-independent copies, not shared" precedent as every
 * other ship's own block above. */
#define SAMURAI_SIZE_MULTIPLIER 1.0f /* same render/hitbox size as B-20 */

/* Every one of Samurai's own shots (modes 1/2 alike) - "shiny white
 * shurikens" per spec, drawn as a spinning 4-point star (see
 * draw_samurai_shuriken in adapters/sdl_renderer.c). Speed reuses B-20's own
 * baseline unscaled. */
#define SAMURAI_SHURIKEN_SPEED PLAYER_PROJECTILE_SPEED
#define SAMURAI_SHURIKEN_RADIUS 7.0f
#define SAMURAI_SHURIKEN_SPIN_SPEED 480.0f /* degrees/sec, purely cosmetic */
/* "2 pts damage on bosses" - BASE_PLAYER_DAMAGE is worth 1 pt, so this is
 * exactly double a normal B-20 mode-1 shot's own damage. */
#define SAMURAI_SHURIKEN_DAMAGE (BASE_PLAYER_DAMAGE * 2.0f)

/* Mode 1 (default): "bursts of 3 shuriken stars" - staggered over time
 * rather than landing in the same frame, the same ENEMY_SHOOT_TRIBURST
 * pattern (ENEMY_TRIBURST_SHOT_INTERVAL) reused for the player's own mode 1
 * (see Player.samurai_burst_shots_remaining/samurai_burst_shot_timer and
 * update_samurai_shuriken in usecases/game_logic.c): 150ms between each of
 * the 3 shots within a burst, then a 550ms pause before the next burst can
 * start - an 850ms full cycle, not the rounder "once per second" a first
 * pass at this ship used, straight ahead from the nose like B-20's own
 * mode 1. */
#define SAMURAI_SHURIKEN_BURST_COUNT 3
#define SAMURAI_SHURIKEN_SHOT_INTERVAL 0.1f
#define SAMURAI_SHURIKEN_BURST_COOLDOWN 0.55f

/* Mode 2: the 180-degree sweep - see SHOOT_MODE_SAMURAI_OMNI's own doc
 * comment in domain/types.h. 8 shots total, "consistent angles of 180/8
 * degrees" per spec (SAMURAI_OMNI_STEP_DEG), starting from the west and
 * sweeping toward the east over SAMURAI_OMNI_DURATION seconds, one shot
 * every SAMURAI_OMNI_SHOT_INTERVAL seconds - not simultaneous like Shine's
 * own omni burst or C-24's own OMNI mode, the one deliberate difference
 * "shot one at a time over a 1 second period" asks for. */
#define SAMURAI_OMNI_SHOT_COUNT 8
#define SAMURAI_OMNI_STEP_DEG 22.5f /* 180.0f / SAMURAI_OMNI_SHOT_COUNT */
#define SAMURAI_OMNI_DURATION 1.0f
#define SAMURAI_OMNI_SHOT_INTERVAL                                             \
  (SAMURAI_OMNI_DURATION / (float)SAMURAI_OMNI_SHOT_COUNT)
#define SAMURAI_OMNI_COOLDOWN 20.0f

/* Mode 3: stealth - see SHOOT_MODE_SAMURAI_STEALTH's own doc comment in
 * domain/types.h. No shot constants of its own - it doesn't fire at all,
 * only moves faster and becomes untouchable for its own duration. */
#define SAMURAI_STEALTH_DURATION 3.0f
#define SAMURAI_STEALTH_COOLDOWN 20.0f
#define SAMURAI_STEALTH_SPEED_MULTIPLIER 2.0f
#define SAMURAI_STEALTH_OPACITY 0.5f /* "50% transparency" per spec */

#define BOSS_SCORE_STEP 500
/* How many points before BOSS_SCORE_STEP the "boss incoming" warning
 * kicks in - see update_boss_warning in usecases/game_logic.c, which
 * drives both the red star fade (draw_stars in adapters/sdl_renderer.c)
 * and the early start of the boss soundtrack (app.c's audio->update
 * call), both of which last until the boss is actually defeated. */
#define BOSS_WARNING_SCORE_GAP 50
/* Radians/sec fed to sinf(gs->time_elapsed * ...) to pulse the warning
 * star field between white and red - see draw_stars. Distinct from (and
 * slower than) SUPER_BEAM_WIDTH_PULSE_SPEED, which pulses something
 * already close-up and urgent rather than a background ambient cue. */
#define BOSS_WARNING_STAR_PULSE_SPEED 3.0f
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

/* How often the boss dispatches a fresh ordinary enemy from directly
 * beneath itself to a random point on screen (see update_boss_dispatch and
 * spawner_dispatch_enemy_from_boss) - starts at BOSS_DISPATCH_INTERVAL_START
 * seconds for the very first encounter (gs->boss_count == 1) and gets
 * BOSS_DISPATCH_INTERVAL_STEP seconds shorter with every encounter since,
 * bottoming out at BOSS_DISPATCH_INTERVAL_MIN - see
 * spawner_boss_dispatch_interval, the single place this formula lives. */
#define BOSS_DISPATCH_INTERVAL_START 3.0f
#define BOSS_DISPATCH_INTERVAL_STEP 0.5f
#define BOSS_DISPATCH_INTERVAL_MIN 1.0f
/* How fast a dispatched enemy flies from beneath the boss to its landing
 * point (see the boss_dispatch_flying branch of update_enemy_movement) -
 * same baseline PLAYER_SPEED precedent MOTHERSHIP_CHILD_LAUNCH_SPEED
 * already uses for "a freshly dispatched entity's own travel speed." */
#define BOSS_DISPATCH_ENEMY_FLIGHT_SPEED PLAYER_SPEED

/* --- Erratic enemy movement (see EnemyMovementStyle in domain/types.h and
 * update_enemies in usecases/game_logic.c) - CIRCLE/SPIRAL/SINE/RANDOM
 * instead of the original straight fall + small wobble, unlocked
 * progressively as bosses are defeated. --- */
/* Each ordinary enemy rolls this chance at spawn time (see spawn_one_enemy
 * in usecases/spawner.c), multiplied by GameState.bosses_defeated and
 * capped at 100% - 0% before the first boss is defeated, 10% after the
 * first, 20% after the second, and so on. A hit picks uniformly among the
 * 3 non-NORMAL styles. */
#define ERRATIC_ENEMY_CHANCE_PER_BOSS_DEFEAT 0.10f
/* Every erratic style's underlying downward drift (the speed its own
 * orbit_center_y/y falls at) is this much faster than a normal enemy's own
 * difficulty-driven fall speed - "maybe even with higher speeds", per
 * spec. */
#define ERRATIC_ENEMY_SPEED_MULTIPLIER 1.35f
#define ERRATIC_ENEMY_CIRCLE_RADIUS_MIN 24.0f
#define ERRATIC_ENEMY_CIRCLE_RADIUS_MAX 55.0f
#define ERRATIC_ENEMY_CIRCLE_ANGULAR_SPEED 220.0f /* degrees/sec */
/* SPIRAL starts at radius 0 (dead center) and grows outward at this rate,
 * capped at RADIUS_MAX, tracing a widening spiral rather than CIRCLE's own
 * fixed loop. */
#define ERRATIC_ENEMY_SPIRAL_RADIUS_GROWTH 30.0f /* design px/sec */
#define ERRATIC_ENEMY_SPIRAL_RADIUS_MAX 110.0f
#define ERRATIC_ENEMY_SPIRAL_ANGULAR_SPEED 200.0f /* degrees/sec */
#define ERRATIC_ENEMY_SINE_AMPLITUDE_MIN 50.0f
#define ERRATIC_ENEMY_SINE_AMPLITUDE_MAX 110.0f
#define ERRATIC_ENEMY_SINE_ANGULAR_SPEED 160.0f /* degrees/sec */
/* RANDOM re-rolls a fresh heading within this speed budget every
 * RANDOM_RETARGET_MIN..MAX seconds - always with a downward component
 * (never purely sideways/upward) so it still reliably clears the bottom
 * of the screen like every other style, just on a lumpier path. */
#define ERRATIC_ENEMY_RANDOM_SPEED 150.0f /* design px/sec */
#define ERRATIC_ENEMY_RANDOM_RETARGET_MIN 0.35f
#define ERRATIC_ENEMY_RANDOM_RETARGET_MAX 0.9f

#endif
