#ifndef GALAXY_INVADERS_ADAPTERS_SHIP_CRUZADER_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_SHIP_CRUZADER_SPRITE_H

#include <stdint.h>

/* Pixel-art data for Cruzader, reproduced from the reference artwork the
 * same way as adapters/player_sprite (B-20) and adapters/ship_shine_sprite
 * (Shine): background removed (flood fill from the border, since the
 * source had no real alpha channel of its own - just an opaque near-black
 * background baked in), then fit into a 64x64 logical-pixel grid preserving
 * the source's own aspect ratio (padded to square, then Lanczos-downsampled
 * with premultiplied alpha to avoid a dark fringe at the silhouette edge) -
 * the standard 64x64 grid, not Mothership's doubled 128x128 one, since
 * Cruzader's own +50% render size (ship_size_multiplier(SHIP_CRUZADER) is
 * 1.5, applied at draw time, not baked into the sprite grid) doesn't need
 * the extra pixel density Mothership's own 2x size used. The reference art
 * was already drawn nose-up (twin engine flames at the bottom), matching
 * every other orientation convention in the game (B-20, Shine, enemies)
 * already - no vertical flip needed. Row-major, top-left origin. Each entry
 * is a packed 0xRRGGBBAA color; a fully zero entry (alpha 0) is background
 * and should not be drawn. */

#define SHIP_CRUZADER_SPRITE_SIZE 64

extern const uint32_t kShipCruzaderSpritePixels[SHIP_CRUZADER_SPRITE_SIZE * SHIP_CRUZADER_SPRITE_SIZE];

#endif
