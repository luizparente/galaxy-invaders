#include "adapters/ship_sprites.h"
#include "adapters/player_sprite.h"
#include "adapters/ship_c24_sprite.h"

const ShipSpriteSheet kShipSprites[SHIP_COUNT] = {
    [SHIP_B20] = {kPlayerSpritePixels, PLAYER_SPRITE_SIZE},
    [SHIP_C24] = {kShipC24SpritePixels, SHIP_C24_SPRITE_SIZE},
};
