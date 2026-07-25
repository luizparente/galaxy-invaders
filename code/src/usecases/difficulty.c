#include <math.h>
#include "usecases/difficulty.h"
#include "domain/constants.h"

float difficulty_spawn_interval(int score) {
    float steps = (float)score / SPAWN_INTERVAL_STEP_SCORE;
    float interval = BASE_SPAWN_INTERVAL * powf(SPAWN_INTERVAL_STEP_FACTOR, steps);
    return interval < MIN_SPAWN_INTERVAL ? MIN_SPAWN_INTERVAL : interval;
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
