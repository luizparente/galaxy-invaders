#ifndef GALAXY_INVADERS_ADAPTERS_SHIP_SPRITES_H
#define GALAXY_INVADERS_ADAPTERS_SHIP_SPRITES_H

#include <stdint.h>
#include "domain/types.h"

/* One square RGBA pixel grid per playable Ship (domain/types.h), same
 * packed-0xRRGGBBAA/row-major/top-left-origin convention as
 * adapters/player_sprite and adapters/enemy_sprites - see
 * adapters/ship_c24_sprite.h for how C-24's own grid was produced. Indexed
 * by Ship so both the in-game player sprite (draw_player) and the
 * ship-select screen's icons/preview (draw_ship_select_screen) can share
 * one lookup instead of hand-picking a sprite per call site. */

typedef struct ShipSpriteSheet {
    const uint32_t *pixels;
    int size;
} ShipSpriteSheet;

extern const ShipSpriteSheet kShipSprites[SHIP_COUNT];

#endif
