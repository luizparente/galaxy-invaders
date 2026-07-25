#ifndef GALAXY_INVADERS_ADAPTERS_PIXEL_FONT_H
#define GALAXY_INVADERS_ADAPTERS_PIXEL_FONT_H

#include <SDL2/SDL.h>
#include "domain/types.h"

/* Draws text using a programmatic 5x7 dot-matrix font. (x, y) is the
 * top-left corner. pixel_size is the edge length of one font dot. */
void pf_draw_text(SDL_Renderer *r, float x, float y, float pixel_size, Color c, const char *text);

/* Width in pixels that pf_draw_text would occupy for the given text. */
float pf_text_width(const char *text, float pixel_size);

#endif
