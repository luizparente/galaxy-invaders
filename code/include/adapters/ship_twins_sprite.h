#ifndef GALAXY_INVADERS_ADAPTERS_SHIP_TWINS_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_SHIP_TWINS_SPRITE_H

#include <stdint.h>

/* Pixel-art data for The Twins (a single twin's own rocket - duplicated at
 * render time, see draw_player's SHIP_TWINS branch in
 * adapters/sdl_renderer.c, not baked into the sprite grid itself), produced
 * the same way as every other ship's own sprite (B-20, Shine, Cruzader):
 * background removed (flood fill from the border, since the source had no
 * real alpha channel of its own - just an opaque near-black background
 * baked in), then fit into a 64x64 logical-pixel grid preserving the
 * source's own aspect ratio (padded to square, then Lanczos-downsampled
 * with premultiplied alpha to avoid a dark fringe at the silhouette edge).
 * The reference art was already drawn nose-up (engine flame at the
 * bottom), matching every other orientation convention in the game
 * already - no vertical flip needed. Row-major, top-left origin. Each
 * entry is a packed 0xRRGGBBAA color; a fully zero entry (alpha 0) is
 * background and should not be drawn. */

#define SHIP_TWINS_SPRITE_SIZE 64

extern const uint32_t kShipTwinsSpritePixels[SHIP_TWINS_SPRITE_SIZE * SHIP_TWINS_SPRITE_SIZE];

#endif
