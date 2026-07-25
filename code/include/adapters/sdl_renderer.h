#ifndef GALAXY_INVADERS_ADAPTERS_SDL_RENDERER_H
#define GALAXY_INVADERS_ADAPTERS_SDL_RENDERER_H

#include "ports/renderer_port.h"

/* Creates an SDL2-backed RendererPort: opens a borderless fullscreen window
 * at the desktop's own resolution and draws every frame purely from
 * computed pixel geometry (see adapters/graphics_primitives and
 * adapters/pixel_font) - no image assets. `fallback_w`/`fallback_h` are
 * used only if the real display size can't be determined. The actual
 * playfield size in real pixels is written to out_screen_w and out_screen_h
 * so the caller can hand it to game_init - the playfield always matches
 * the physical screen exactly, no letterboxing or stretching.
 * Returns NULL on failure. The caller owns the returned pointer and must
 * eventually call port->destroy(port->self) followed by free(port). */
RendererPort *sdl_renderer_create(const char *title, int fallback_w, int fallback_h,
                                   int *out_screen_w, int *out_screen_h);

#endif
