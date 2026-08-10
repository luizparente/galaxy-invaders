#ifndef GALAXY_INVADERS_ADAPTERS_MENU_SHIP_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_MENU_SHIP_SPRITE_H

#include <stdint.h>

/* Pixel-art data for the decorative B-20 hero ship shown on the main
 * menu's right-hand side (see draw_menu_ship in adapters/sdl_renderer.c),
 * reproduced the same way as adapters/menu_ship_c24_sprite's own C-24
 * hero: background removed via flood fill from the border (the source had
 * no real alpha channel of its own - just an opaque black background baked
 * in), then Lanczos-downsampled with premultiplied alpha (to avoid a dark
 * fringe at the silhouette edge) from the source raster, preserving its
 * own aspect ratio. Row-major, top-left origin. Each entry is a packed
 * 0xRRGGBBAA color; a fully zero entry (alpha 0) is background and should
 * not be drawn. */

#define MENU_SHIP_SPRITE_W 800
#define MENU_SHIP_SPRITE_H 656

extern const uint32_t kMenuShipSpritePixels[MENU_SHIP_SPRITE_W * MENU_SHIP_SPRITE_H];

#endif
