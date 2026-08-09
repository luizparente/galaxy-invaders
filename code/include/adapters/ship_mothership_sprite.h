#ifndef GALAXY_INVADERS_ADAPTERS_SHIP_MOTHERSHIP_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_SHIP_MOTHERSHIP_SPRITE_H

#include <stdint.h>

/* Pixel-art data for The Mothership, reproduced from the reference artwork
 * the same way as adapters/player_sprite (B-20) and adapters/ship_c24_sprite
 * (C-24): background removed (flood fill from the border, since the source
 * had no real alpha channel of its own - just an opaque near-black
 * background baked in), then fit into a square logical-pixel grid
 * preserving the source's own aspect ratio (padded to square, then
 * Lanczos-downsampled with premultiplied alpha to avoid a dark fringe at
 * the silhouette edge). Deliberately 128x128 - double B-20/C-24's own 64x64
 * grid - because ship_size_multiplier(SHIP_MOTHERSHIP) renders her at 2x
 * their size (see usecases/ship.c): a 64x64 grid stretched to 2x reads as
 * blocky/pixelated next to them, where this doubled grid lands each of her
 * own logical pixels at very nearly the same on-screen size as theirs.
 * Unlike C-24's reference art, this one was already drawn nose-up (engine
 * flames at the bottom), matching every other orientation convention in the
 * game (B-20, enemies) already - no vertical flip needed. Row-major,
 * top-left origin. Each entry is a packed 0xRRGGBBAA color; a fully zero
 * entry (alpha 0) is background and should not be drawn. */

#define SHIP_MOTHERSHIP_SPRITE_SIZE 128

extern const uint32_t kShipMothershipSpritePixels[SHIP_MOTHERSHIP_SPRITE_SIZE * SHIP_MOTHERSHIP_SPRITE_SIZE];

#endif
