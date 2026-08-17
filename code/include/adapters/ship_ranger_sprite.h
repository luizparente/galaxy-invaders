#ifndef GALAXY_INVADERS_ADAPTERS_SHIP_RANGER_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_SHIP_RANGER_SPRITE_H

#include <stdint.h>

/* Pixel-art data for Ranger, reproduced from the reference artwork the same
 * way as adapters/ship_buckler_sprite and adapters/ship_samurai_sprite:
 * background removed (flood fill from the border, since the source had no
 * real alpha channel of its own - just an opaque near-black background
 * baked in), then fit into a 64x64 logical-pixel grid preserving the
 * source's own aspect ratio (padded to square, then Lanczos-downsampled
 * with premultiplied alpha to avoid a dark fringe at the silhouette edge) -
 * the standard 64x64 grid, matching Ranger's own stock render size
 * (ship_size_multiplier(SHIP_RANGER) is 1.0). The reference art was already
 * drawn nose-up (crowned head at the top, twin side-boosters at the flanks,
 * engine glow at the bottom), matching every other orientation convention in
 * the game (B-20, Shine, Samurai) already - no vertical flip needed.
 * Row-major, top-left origin. Each entry is a packed 0xRRGGBBAA color; a
 * fully zero entry (alpha 0) is background and should not be drawn. */

#define SHIP_RANGER_SPRITE_SIZE 64

extern const uint32_t kShipRangerSpritePixels[SHIP_RANGER_SPRITE_SIZE * SHIP_RANGER_SPRITE_SIZE];

#endif
