#ifndef GALAXY_INVADERS_ADAPTERS_MENU_SHIP_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_MENU_SHIP_SPRITE_H

#include <stdint.h>

/* Pixel-art data for the decorative hero ship shown on the main menu,
 * reproduced from reference artwork the same way as adapters/player_sprite:
 * background removed via flood fill, then box-downsampled (averaged, not
 * nearest-neighbor - the source is smooth painted shading, not flat blocky
 * pixel art) from the source raster, preserving its aspect ratio. Row-major,
 * top-left origin. Each entry is a packed 0xRRGGBBAA color; a fully zero
 * entry (alpha 0) is background and should not be drawn. */

#define MENU_SHIP_SPRITE_W 800
#define MENU_SHIP_SPRITE_H 650

extern const uint32_t kMenuShipSpritePixels[MENU_SHIP_SPRITE_W * MENU_SHIP_SPRITE_H];

#endif
