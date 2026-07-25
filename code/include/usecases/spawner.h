#ifndef GALAXY_INVADERS_USECASES_SPAWNER_H
#define GALAXY_INVADERS_USECASES_SPAWNER_H

#include "domain/types.h"

/* Advances the spawn countdown and materializes new enemies into free
 * slots of gs->enemies when it elapses. Spawn rate and enemy speed come
 * from usecases/difficulty so this stays focused on *when/where/what to
 * spawn*, not *how hard the game should currently be*. */
void spawner_update(GameState *gs, float dt);

#endif
