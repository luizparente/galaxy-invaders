#ifndef GALAXY_INVADERS_ADAPTERS_ENEMY_SPRITES_H
#define GALAXY_INVADERS_ADAPTERS_ENEMY_SPRITES_H

#include <stdint.h>
#include "domain/constants.h"

/* Pixel-art data for the 16 enemy designs, reproduced from reference
 * artwork the same way as adapters/player_sprite: background removed via
 * flood fill, box-downsampled to a grid sized to preserve each source
 * image's aspect ratio (long side below), row-major, top-left origin.
 * Each entry is a packed 0xRRGGBBAA color; a fully zero entry (alpha 0)
 * is background and should not be drawn. */

typedef struct EnemySpriteSheet {
    const uint32_t *pixels;
    int grid_w;
    int grid_h;
} EnemySpriteSheet;

extern const EnemySpriteSheet kEnemySprites[ENEMY_KIND_COUNT];

#endif
