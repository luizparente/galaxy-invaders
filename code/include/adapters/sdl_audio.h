#ifndef GALAXY_INVADERS_ADAPTERS_SDL_AUDIO_H
#define GALAXY_INVADERS_ADAPTERS_SDL_AUDIO_H

#include "ports/audio_port.h"

/* Creates an SDL2-backed AudioPort. All sound - the background chiptune
 * loop and every sound effect - is synthesized sample-by-sample from
 * oscillators inside the audio callback; nothing is loaded from a file.
 * Returns NULL on failure. The caller owns the returned pointer and must
 * eventually call port->destroy(port->self) followed by free(port). */
AudioPort *sdl_audio_create(void);

#endif
