#ifndef GALAXY_INVADERS_ADAPTERS_FROSTY_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_FROSTY_SPRITE_H

#include <stdint.h>

/* Pixel-art data for Frosty, Antartica's own sidekick - not a playable Ship
 * of its own, so it isn't part of adapters/ship_sprites' kShipSprites table;
 * draw_player's own SHIP_ANTARTICA branch (adapters/sdl_renderer.c) reaches
 * this directly. Produced the same way as every other sprite in the game -
 * see ship_antartica_sprite.h's own doc comment for the exact background-
 * removal/downsampling pipeline. Row-major, top-left origin, packed
 * 0xRRGGBBAA, alpha 0 = background. */

#define FROSTY_SPRITE_SIZE 64

extern const uint32_t kFrostySpritePixels[FROSTY_SPRITE_SIZE * FROSTY_SPRITE_SIZE];

#endif
