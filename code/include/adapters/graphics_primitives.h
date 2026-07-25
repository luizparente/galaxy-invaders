#ifndef GALAXY_INVADERS_ADAPTERS_GRAPHICS_PRIMITIVES_H
#define GALAXY_INVADERS_ADAPTERS_GRAPHICS_PRIMITIVES_H

#include <SDL2/SDL.h>
#include "domain/types.h"

/* Thin, SDL-specific drawing helpers. Every shape is computed and drawn
 * from geometry at call time (rects, lines, triangles via SDL_RenderGeometry,
 * scanline-filled circles/ellipses) - there is no image or bitmap asset
 * anywhere in this pipeline. Used only by adapters/sdl_renderer, never by
 * usecases or app. */

void gp_fill_rect(SDL_Renderer *r, float x, float y, float w, float h, Color c);
void gp_draw_line(SDL_Renderer *r, float x0, float y0, float x1, float y1, Color c);
void gp_fill_triangle(SDL_Renderer *r, float x0, float y0, float x1, float y1,
                       float x2, float y2, Color c);
void gp_fill_quad(SDL_Renderer *r, float x0, float y0, float x1, float y1,
                   float x2, float y2, float x3, float y3, Color c);
void gp_fill_circle(SDL_Renderer *r, float cx, float cy, float radius, Color c);
void gp_draw_circle_outline(SDL_Renderer *r, float cx, float cy, float radius, Color c);
void gp_fill_ellipse(SDL_Renderer *r, float cx, float cy, float rx, float ry, Color c);

#endif
