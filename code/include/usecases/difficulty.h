#ifndef GALAXY_INVADERS_USECASES_DIFFICULTY_H
#define GALAXY_INVADERS_USECASES_DIFFICULTY_H

/* Pure functions mapping game progress to difficulty knobs. Isolated here
 * (Single Responsibility) so the curve can be re-tuned or swapped without
 * touching orchestration code in game_logic, and unit-tested in isolation. */

float difficulty_spawn_interval(int score);
float difficulty_enemy_speed(int score);
float difficulty_score_multiplier(int score);

/* Normalized 0..1 ramp used by the audio adapter to nudge music tempo. */
float difficulty_normalized(float time_elapsed);

#endif
