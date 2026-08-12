#ifndef GALAXY_INVADERS_USECASES_SPAWNER_H
#define GALAXY_INVADERS_USECASES_SPAWNER_H

#include "domain/types.h"

/* Advances the spawn countdown and materializes new enemies into free
 * slots of gs->enemies when it elapses. Spawn rate and enemy speed come
 * from usecases/difficulty so this stays focused on *when/where/what to
 * spawn*, not *how hard the game should currently be*. */
void spawner_update(GameState *gs, float dt);

/* The same random "what does an enemy look like" pool spawner_update
 * draws from, exposed so the boss (usecases/game_logic.c) can present
 * itself as "a randomly picked enemy" too, just scaled way up. */
int spawner_random_enemy_kind(void);

/* Which of the 5 shooting patterns (see EnemyShootStyle in domain/types.h)
 * a given enemy design fires - a fixed per-kind lookup, same shape as the
 * old accent-color table this replaced now that projectile color is
 * rolled randomly per enemy instead (see spawn_one_enemy). */
EnemyShootStyle spawner_enemy_kind_shoot_style(int kind);

/* A separate, smaller pool for the boss: only the kinds that have a
 * dedicated richer, high-resolution redesign for the boss's ~10x size
 * (adapters/enemy_sprites' kBossSprites) - the rest still spawn as
 * ordinary enemies via spawner_random_enemy_kind, just never as a boss. */
int spawner_random_boss_kind(void);

/* The odds a newly-spawned ordinary enemy flies an erratic
 * (CIRCLE/SPIRAL/SINE/RANDOM) pattern instead of the original NORMAL
 * straight-fall-and-wobble one - ERRATIC_ENEMY_CHANCE_PER_BOSS_DEFEAT per
 * boss actually defeated so far this run, capped at 1.0 (100%). Exposed as
 * its own pure function (rather than inlined in spawn_one_enemy) so it can
 * be pinned down directly in tests, same reasoning as usecases/difficulty's
 * own pure functions. */
float spawner_erratic_enemy_chance(int bosses_defeated);

/* How long the boss waits between dispatching enemies (see
 * update_boss_dispatch in usecases/game_logic.c), given how many boss
 * encounters have occurred so far this run (gs->boss_count, 1 during the
 * first). Starts at BOSS_DISPATCH_INTERVAL_START and gets
 * BOSS_DISPATCH_INTERVAL_STEP shorter per encounter since, floored at
 * BOSS_DISPATCH_INTERVAL_MIN - exposed as its own pure function, same
 * "pin the formula down directly in tests" reasoning as
 * spawner_erratic_enemy_chance above. */
float spawner_boss_dispatch_interval(int boss_count);

/* Spawns a fresh ordinary enemy directly beneath the boss (gs->boss.x/y)
 * into the first free gs->enemies slot, exactly like spawn_one_enemy would
 * (random kind/color/timers, and - once it lands - the same
 * spawner_erratic_enemy_chance-gated movement style roll) except it starts
 * in-flight toward a random point elsewhere on screen instead of falling
 * from the top - see the Enemy.boss_dispatch_flying field's own doc
 * comment and update_enemy_movement's flying branch, which calls
 * spawner_land_boss_dispatched_enemy once it arrives. A no-op if every
 * slot is currently occupied (same "best-effort, no queued retry" choice
 * spawn_one_enemy already makes). */
void spawner_dispatch_enemy_from_boss(GameState *gs);

/* Finalizes a boss-dispatched enemy the instant it reaches its landing
 * point (see update_enemy_movement) - rerolls vx/vy/fire_timer exactly as
 * spawn_one_enemy would for a fresh spawn, then rolls its real
 * movement_style (roll_enemy_movement_style), since only the flight phase
 * needed to suppress those. Not meant to be called on an enemy that isn't
 * currently boss_dispatch_flying. */
void spawner_land_boss_dispatched_enemy(GameState *gs, Enemy *e);

#endif
