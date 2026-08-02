#ifndef GALAXY_INVADERS_ADAPTERS_ENEMY_SPRITES_H
#define GALAXY_INVADERS_ADAPTERS_ENEMY_SPRITES_H

#include <stdint.h>
#include "domain/constants.h"

/* Pixel-art data for the 16 enemy designs, reproduced from reference
 * artwork the same way as adapters/player_sprite: background removed via
 * flood fill, then nearest-neighbor sampled (never blended/averaged, so
 * every grid cell is an exact copy of one source pixel's exact color) into
 * a grid preserving each source image's aspect ratio, row-major, top-left
 * origin. Each entry is a packed 0xRRGGBBAA color; a fully zero entry
 * (alpha 0) is background and should not be drawn.
 *
 * Two resolutions of the same 16 designs are kept: kEnemySprites is sized
 * for how big ordinary enemies render (a small grid comfortably exceeds
 * that in the same way the player ship's does); kBossSprites is each
 * design's *full native source resolution*, because the boss displays the
 * same art at up to ~10x the size (BOSS_SIZE_MULTIPLIER) and stretching
 * the small grid that far looked like a blown-up, blocky low-res image. */

typedef struct EnemySpriteSheet {
    const uint32_t *pixels;
    int grid_w;
    int grid_h;
} EnemySpriteSheet;

extern const EnemySpriteSheet kEnemySprites[ENEMY_KIND_COUNT];
extern const EnemySpriteSheet kBossSprites[ENEMY_KIND_COUNT];

#endif
