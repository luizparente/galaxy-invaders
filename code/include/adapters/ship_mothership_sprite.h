#ifndef GALAXY_INVADERS_ADAPTERS_SHIP_MOTHERSHIP_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_SHIP_MOTHERSHIP_SPRITE_H

#include <stdint.h>

/* Pixel-art data for The Mothership, reproduced from the reference artwork
 * the same way as adapters/player_sprite (B-20) and adapters/ship_c24_sprite
 * (C-24): background removed (flood fill from the border, since the source
 * had no real alpha channel of its own - just an opaque near-black
 * background baked in), then fit into a 64x64 logical-pixel grid preserving
 * the source's own aspect ratio (padded to square, then box-downsampled
 * with premultiplied alpha to avoid a dark fringe at the silhouette edge).
 * Unlike C-24's reference art, this one was already drawn nose-up (engine
 * flames at the bottom), matching every other orientation convention in the
 * game (B-20, enemies) already - no vertical flip needed. Row-major,
 * top-left origin. Each entry is a packed 0xRRGGBBAA color; a fully zero
 * entry (alpha 0) is background and should not be drawn. */

#define SHIP_MOTHERSHIP_SPRITE_SIZE 64

extern const uint32_t kShipMothershipSpritePixels[SHIP_MOTHERSHIP_SPRITE_SIZE * SHIP_MOTHERSHIP_SPRITE_SIZE];

#endif
