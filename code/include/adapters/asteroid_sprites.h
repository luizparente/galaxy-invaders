#ifndef GALAXY_INVADERS_ADAPTERS_ASTEROID_SPRITES_H
#define GALAXY_INVADERS_ADAPTERS_ASTEROID_SPRITES_H

#include <stdint.h>
#include "domain/constants.h"

/* Pixel-art data for the 26 asteroid designs, reproduced from reference
 * artwork the same way as adapters/ship_ranger_sprite: background removed
 * via flood fill from the border (the source had no real alpha channel of
 * its own - just an opaque near-black/grey/tinted-grey background baked
 * in, varying by reference image), padded to square, then Lanczos-
 * downsampled with premultiplied alpha (avoids a dark fringe at the
 * silhouette edge). Row-major, top-left origin. Each entry is a packed
 * 0xRRGGBBAA color; a fully zero entry (alpha 0) is background and should
 * not be drawn.
 *
 * Split into 3 size tiers - small (11 designs, 64x64), medium (10, 96x96),
 * large (5, 160x160) - per "vary the asteroid sizes" feedback: each tier
 * gets its own bigger on-screen size range (ASTEROID_SIZE_SMALL/MEDIUM/
 * LARGE_MIN/MAX in domain/constants.h) and, since the large tier's own
 * reference art is far more detailed and renders far bigger, a
 * correspondingly higher native texture resolution (same "grid resolution
 * scales with how big the art actually renders" reasoning
 * adapters/enemy_sprites' kBossSprites already established) - kept as
 * three different native resolutions within one flat array purely because
 * every entry stores its own grid_w/grid_h; draw_asteroid
 * (adapters/sdl_renderer.c) already scales whatever texture it's given
 * into the spawned Asteroid's own on-screen a->size, so no renderer code
 * needs to know about tiers at all. kAsteroidSprites is laid out small
 * tier first, then medium, then large - see spawner_spawn_asteroid in
 * usecases/spawner.c, the only place a tier and an index within it are
 * both rolled together (ASTEROID_SPRITE_COUNT/ASTEROID_SMALL_SPRITE_COUNT/
 * ASTEROID_MEDIUM_SPRITE_COUNT/ASTEROID_LARGE_SPRITE_COUNT all live in
 * domain/constants.h rather than an adapters-only enum, same "usecases
 * needs the count too" precedent ENEMY_KIND_COUNT/kEnemySprites already
 * sets). */

typedef struct AsteroidSprite {
    const uint32_t *pixels;
    int grid_w;
    int grid_h;
} AsteroidSprite;

extern const AsteroidSprite kAsteroidSprites[ASTEROID_SPRITE_COUNT];

#endif
