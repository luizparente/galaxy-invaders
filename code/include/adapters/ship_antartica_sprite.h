#ifndef GALAXY_INVADERS_ADAPTERS_SHIP_ANTARTICA_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_SHIP_ANTARTICA_SPRITE_H

#include <stdint.h>

/* Pixel-art data for Antartica, produced the same way as every other ship's
 * own sprite (B-20, Shine, Cruzader, ...): background removed (flood fill
 * from the border, since the source had no real alpha channel of its own -
 * just an opaque near-black background baked in), then fit into a 64x64
 * logical-pixel grid preserving the source's own aspect ratio (padded to
 * square, then Lanczos-downsampled with premultiplied alpha to avoid a dark
 * fringe at the silhouette edge). The reference art was already drawn
 * nose-up, matching every other orientation convention in the game already -
 * no vertical flip needed. Row-major, top-left origin. Each entry is a
 * packed 0xRRGGBBAA color; a fully zero entry (alpha 0) is background and
 * should not be drawn. */

#define SHIP_ANTARTICA_SPRITE_SIZE 64

extern const uint32_t kShipAntarticaSpritePixels[SHIP_ANTARTICA_SPRITE_SIZE * SHIP_ANTARTICA_SPRITE_SIZE];

#endif
