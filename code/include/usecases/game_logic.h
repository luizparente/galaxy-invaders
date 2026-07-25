#ifndef GALAXY_INVADERS_USECASES_GAME_LOGIC_H
#define GALAXY_INVADERS_USECASES_GAME_LOGIC_H

#include "domain/types.h"
#include "domain/events.h"
#include "ports/input_port.h"

/* The use-case layer's whole public surface: reset the world, and advance
 * it by one frame. Everything else in usecases/ is a private helper these
 * two functions orchestrate. Neither function nor anything they call
 * touches SDL, a renderer, or an audio device - side effects the outside
 * world must react to (sounds, in particular) are recorded into `events`
 * instead, per the dependency-inversion boundary described in
 * domain/events.h. */

void game_init(GameState *gs);
void game_update(GameState *gs, const InputCommand *input, float dt, EventQueue *events);

#endif
