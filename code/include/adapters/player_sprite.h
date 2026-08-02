#ifndef GALAXY_INVADERS_ADAPTERS_PLAYER_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_PLAYER_SPRITE_H

#include <stdint.h>

/* Pixel-art data for the player ship, reproduced from the reference
 * artwork at a 64x64 logical-pixel grid (background removed via
 * flood fill, box-downsampled from the source raster). Row-major,
 * top-left origin. Each entry is a packed 0xRRGGBBAA color; a fully
 * zero entry (alpha 0) is background and should not be drawn. */

#define PLAYER_SPRITE_SIZE 64

extern const uint32_t kPlayerSpritePixels[PLAYER_SPRITE_SIZE * PLAYER_SPRITE_SIZE];

#endif
