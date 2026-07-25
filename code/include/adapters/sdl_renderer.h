#ifndef GALAXY_INVADERS_ADAPTERS_SDL_RENDERER_H
#define GALAXY_INVADERS_ADAPTERS_SDL_RENDERER_H

#include "ports/renderer_port.h"

/* Creates an SDL2-backed RendererPort: owns the SDL window and renderer,
 * and draws every frame purely from computed pixel geometry (see
 * adapters/graphics_primitives and adapters/pixel_font) - no image assets.
 * Returns NULL on failure. The caller owns the returned pointer and must
 * eventually call port->destroy(port->self) followed by free(port). */
RendererPort *sdl_renderer_create(const char *title, int logical_w, int logical_h);

#endif
