#ifndef GALAXY_INVADERS_ADAPTERS_PIXEL_FONT_H
#define GALAXY_INVADERS_ADAPTERS_PIXEL_FONT_H

#include <SDL2/SDL.h>
#include "domain/types.h"

/* Draws text using a programmatic 5x7 dot-matrix font. (x, y) is the
 * top-left corner. pixel_size is the edge length of one font dot. Carries
 * a light drop shadow (a single down-right offset copy at moderate alpha)
 * for readability over the game's background art. */
void pf_draw_text(SDL_Renderer *r, float x, float y, float pixel_size, Color c, const char *text);

/* Same glyphs/layout as pf_draw_text, but with a much stronger shadow (a
 * full 8-direction, near-opaque outline) - reserved for
 * draw_ship_select_screen's own description panel, the one spot in the
 * game where small, dense text sits directly over the busiest background
 * art and the normal light shadow isn't enough. */
void pf_draw_text_strong_shadow(SDL_Renderer *r, float x, float y, float pixel_size, Color c, const char *text);

/* Same glyphs/layout as pf_draw_text, but with no shadow at all - both life
 * bars' percentage readouts (the player's top-left one and the boss
 * fight's top-center one) sit on their own solid-color fill rather than
 * over background art, so a shadow there is just visual noise. */
void pf_draw_text_plain(SDL_Renderer *r, float x, float y, float pixel_size, Color c, const char *text);

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
