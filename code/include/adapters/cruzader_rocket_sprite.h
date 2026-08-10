#ifndef GALAXY_INVADERS_ADAPTERS_CRUZADER_ROCKET_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_CRUZADER_ROCKET_SPRITE_H

#include <stdint.h>

/* Pixel-art data for Cruzader's own mode 3 rocket (SHOOT_MODE_CRUZADER_ROCKETS),
 * reproduced from the reference artwork the same way as the ship sprites
 * (adapters/ship_shine_sprite etc.): background removed (flood fill from
 * the border, since the source had no real alpha channel of its own - just
 * an opaque near-black background baked in), then fit into an
 * 18x36-texel grid (width x height) preserving the source's own aspect
 * ratio as closely as that grid allows (padded to a 1:2 canvas, then
 * Lanczos-downsampled with premultiplied alpha to avoid a dark fringe at
 * the silhouette edge) - a non-square grid, unlike every ship sprite so
 * far, since a rocket is a naturally slim projectile rather than a
 * roughly-square ship. The reference art was already drawn nose-up, and
 * draw_cruzader_rocket (adapters/sdl_renderer.c) rotates this grid to match
 * the rocket's own actual travel direction every frame (it homes toward
 * the closest enemy, so it can point any way) rather than a fixed
 * axis-aligned blit like draw_ship_sprite uses. Row-major, top-left
 * origin, row 0 = the nose. Each entry is a packed 0xRRGGBBAA color; a
 * fully zero entry (alpha 0) is background and should not be drawn. */

#define CRUZADER_ROCKET_SPRITE_COLS 18
#define CRUZADER_ROCKET_SPRITE_ROWS 36

extern const uint32_t kCruzaderRocketSpritePixels[CRUZADER_ROCKET_SPRITE_COLS * CRUZADER_ROCKET_SPRITE_ROWS];

#endif
