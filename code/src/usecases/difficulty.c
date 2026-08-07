#include <math.h>
#include "usecases/difficulty.h"
#include "domain/constants.h"

/* One base spawn-rate multiplier per Difficulty (domain/types.h), ordered
 * to match: baby, easy, normal, hard, insane. Higher means slower spawns
 * (see difficulty_spawn_interval). Baby and insane are the tuned endpoints;
 * easy/normal/hard sit on a geometric progression between them (each
 * roughly 0.61x the previous) rather than arbitrary steps, so the curve
 * feels evenly paced across all 5 levels instead of lopsided around one
 * anchor. */
static const float kDifficultyBaseSpawnRateMultiplier[DIFFICULTY_COUNT] = {
    6.5f, 4.0f, 2.4f, 1.5f, 0.9f,
};

/* One base enemy fire chance per second per Difficulty, same ordering and
 * same "tuned endpoints, geometric progression between them" reasoning as
 * above - each level's fire chance is roughly 2.15x the previous. */
static const float kDifficultyBaseFireChancePerSec[DIFFICULTY_COUNT] = {
    0.035f, 0.075f, 0.16f, 0.35f, 0.75f,
};

float difficulty_spawn_rate_multiplier(Difficulty difficulty, float time_elapsed) {
    float steps = floorf(time_elapsed / SPAWN_RATE_RAMP_INTERVAL);
    float multiplier = kDifficultyBaseSpawnRateMultiplier[difficulty] - steps * SPAWN_RATE_RAMP_STEP;
    return multiplier < SPAWN_RATE_MULTIPLIER_MIN ? SPAWN_RATE_MULTIPLIER_MIN : multiplier;
}

float difficulty_enemy_fire_chance_per_sec(Difficulty difficulty, float time_elapsed) {
    float steps = floorf(time_elapsed / FIRE_CHANCE_RAMP_INTERVAL);
    float chance = kDifficultyBaseFireChancePerSec[difficulty] + steps * FIRE_CHANCE_RAMP_STEP;
    return chance > ENEMY_FIRE_CHANCE_MAX ? ENEMY_FIRE_CHANCE_MAX : chance;
}

float difficulty_spawn_interval(int score, Difficulty difficulty, float time_elapsed) {
    float steps = (float)score / SPAWN_INTERVAL_STEP_SCORE;
    float interval = BASE_SPAWN_INTERVAL * powf(SPAWN_INTERVAL_STEP_FACTOR, steps);
    if (interval < MIN_SPAWN_INTERVAL) interval = MIN_SPAWN_INTERVAL;
    return interval * difficulty_spawn_rate_multiplier(difficulty, time_elapsed);
}

float difficulty_enemy_speed(int score) {
    float speed = ENEMY_BASE_SPEED + ((float)score / 1000.0f) * ENEMY_SPEED_PER_1000_SCORE;
    return speed > ENEMY_MAX_SPEED ? ENEMY_MAX_SPEED : speed;
}

float difficulty_score_multiplier(int score) {
    float steps = floorf((float)score / SCORE_MULTIPLIER_STEP);
    return 1.0f + steps * SCORE_MULTIPLIER_INCREMENT;
}

float difficulty_normalized(float time_elapsed) {
    float t = time_elapsed / 180.0f; /* ramps to max difficulty feel over ~3 minutes */
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}
