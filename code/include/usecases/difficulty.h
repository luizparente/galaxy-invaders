#ifndef GALAXY_INVADERS_USECASES_DIFFICULTY_H
#define GALAXY_INVADERS_USECASES_DIFFICULTY_H

#include "domain/types.h"

/* Pure functions mapping game progress to difficulty knobs. Isolated here
 * (Single Responsibility) so the curve can be re-tuned or swapped without
 * touching orchestration code in game_logic, and unit-tested in isolation. */

/* The selected difficulty's own base spawn-rate multiplier (see
 * kDifficultyBaseSpawnRateMultiplier in usecases/difficulty.c), ramped down
 * over the run's own time_elapsed per SPAWN_RATE_RAMP_INTERVAL/STEP
 * (domain/constants.h) and floored at SPAWN_RATE_MULTIPLIER_MIN. Exposed
 * mainly for difficulty_spawn_interval below and for tests. */
float difficulty_spawn_rate_multiplier(Difficulty difficulty, float time_elapsed);

/* The selected difficulty's own base enemy fire chance per second, ramped
 * up over time_elapsed per FIRE_CHANCE_RAMP_INTERVAL/STEP and capped at
 * ENEMY_FIRE_CHANCE_MAX - see update_enemies in usecases/game_logic.c for
 * where this drives the mean time between enemy shots. */
float difficulty_enemy_fire_chance_per_sec(Difficulty difficulty, float time_elapsed);

float difficulty_spawn_interval(int score, Difficulty difficulty, float time_elapsed);
float difficulty_enemy_speed(int score);
float difficulty_score_multiplier(int score);

/* Normalized 0..1 ramp used by the audio adapter to nudge music tempo. */
float difficulty_normalized(float time_elapsed);

#endif
