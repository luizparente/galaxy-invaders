#ifndef GALAXY_INVADERS_ADAPTERS_PIXEL_FONT_H
#define GALAXY_INVADERS_ADAPTERS_PIXEL_FONT_H

#include <SDL2/SDL.h>
#include "domain/types.h"

/* Draws text using a programmatic 5x7 dot-matrix font. (x, y) is the
 * top-left corner. pixel_size is the edge length of one font dot. */
void pf_draw_text(SDL_Renderer *r, float x, float y, float pixel_size, Color c, const char *text);

/* Same glyphs, drawn as a neon sign instead of a solid block: a soft outer
 * glow, a drop shadow, then a hollow "tube" (bright edge cells, dimmer
 * fill cells) - used for the menu title. Same layout as pf_draw_text, so
 * pf_text_width still applies. `shadow` should be a dark but still hued
 * color (not near-black) - the background itself is already near-black,
 * so a black shadow is invisible against it; a dark tint of the sign's
 * own color is what actually reads as a drop shadow. */
void pf_draw_text_neon(SDL_Renderer *r, float x, float y, float pixel_size,
                        Color glow, Color edge, Color fill, Color shadow, const char *text);

/* Width in pixels that pf_draw_text would occupy for the given text. */
float pf_text_width(const char *text, float pixel_size);

#endif
