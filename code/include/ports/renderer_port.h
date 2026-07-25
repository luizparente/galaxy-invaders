#ifndef GALAXY_INVADERS_PORTS_RENDERER_PORT_H
#define GALAXY_INVADERS_PORTS_RENDERER_PORT_H

#include "domain/types.h"

/* Output boundary the use-case/app layer draws through. Concrete
 * implementations (e.g. adapters/sdl_renderer) own every rendering-library
 * detail behind this vtable; nothing above this port ever names SDL. */
typedef struct RendererPort {
    void *self;
    void (*render)(void *self, const GameState *state);
    void (*destroy)(void *self);
} RendererPort;

#endif
