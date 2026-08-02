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
Color spawner_enemy_kind_accent_color(int kind);

#endif
