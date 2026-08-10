#ifndef GALAXY_INVADERS_ADAPTERS_MENU_SHIP_C24_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_MENU_SHIP_C24_SPRITE_H

#include <stdint.h>

/* Pixel-art data for the decorative C-24 hero ship shown on the main menu's
 * left-hand side (see draw_menu_ship_c24 in adapters/sdl_renderer.c),
 * reproduced the same way as adapters/menu_ship_sprite's own B-20 hero:
 * background removed via flood fill from the border (the source had no
 * real alpha channel of its own - just an opaque black background baked
 * in), then Lanczos-downsampled with premultiplied alpha (to avoid a dark
 * fringe at the silhouette edge) from the source raster, preserving its
 * own aspect ratio - no padding to square, unlike the 64x64 in-game sprite
 * grids. Row-major, top-left origin. Each entry is a packed 0xRRGGBBAA
 * color; a fully zero entry (alpha 0) is background and should not be
 * drawn. */

#define MENU_SHIP_C24_SPRITE_W 700
#define MENU_SHIP_C24_SPRITE_H 584

extern const uint32_t kMenuShipC24SpritePixels[MENU_SHIP_C24_SPRITE_W * MENU_SHIP_C24_SPRITE_H];

#endif
