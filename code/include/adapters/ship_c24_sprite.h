#ifndef GALAXY_INVADERS_ADAPTERS_SHIP_C24_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_SHIP_C24_SPRITE_H

#include <stdint.h>

/* Pixel-art data for the C-24 ship, reproduced from the reference artwork
 * the same way as adapters/player_sprite (B-20): background removed via
 * flood fill, then fit into a 64x64 logical-pixel grid preserving the
 * source's own aspect ratio (box-downsampled), centered with transparent
 * padding on the shorter axis. The reference art itself was drawn nose-down
 * (engine flame at the top), so the grid is additionally flipped vertically
 * here so the nose faces up and the exhaust trails from the bottom, matching
 * every other orientation convention in the game (B-20, enemies) - draw_player
 * fires from a sprite's top edge and trails smoke from its bottom (see
 * update_player_trail in usecases/game_logic.c), so this sprite has to match
 * that or it reads as flying backwards. Row-major, top-left origin. Each
 * entry is a packed 0xRRGGBBAA color; a fully zero entry (alpha 0) is
 * background and should not be drawn. */

#define SHIP_C24_SPRITE_SIZE 64

extern const uint32_t kShipC24SpritePixels[SHIP_C24_SPRITE_SIZE * SHIP_C24_SPRITE_SIZE];

#endif
