#ifndef GALAXY_INVADERS_ADAPTERS_SDL_INPUT_H
#define GALAXY_INVADERS_ADAPTERS_SDL_INPUT_H

#include "ports/input_port.h"

/* Creates an SDL2-backed InputPort: polls the SDL event queue and keyboard
 * state, then translates raw scancodes into the semantic InputCommand the
 * use-case layer understands. Returns NULL on failure. The caller owns
 * the returned pointer and must eventually call port->destroy(port->self)
 * followed by free(port). */
InputPort *sdl_input_create(void);

#endif
