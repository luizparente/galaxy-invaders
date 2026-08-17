#include "adapters/ship_sprites.h"
#include "adapters/player_sprite.h"
#include "adapters/ship_c24_sprite.h"
#include "adapters/ship_mothership_sprite.h"
#include "adapters/ship_shine_sprite.h"
#include "adapters/ship_cruzader_sprite.h"
#include "adapters/ship_twins_sprite.h"
#include "adapters/ship_antartica_sprite.h"
#include "adapters/ship_buckler_sprite.h"
#include "adapters/ship_samurai_sprite.h"
#include "adapters/ship_ranger_sprite.h"

const ShipSpriteSheet kShipSprites[SHIP_COUNT] = {
    [SHIP_B20] = {kPlayerSpritePixels, PLAYER_SPRITE_SIZE},
    [SHIP_C24] = {kShipC24SpritePixels, SHIP_C24_SPRITE_SIZE},
    [SHIP_MOTHERSHIP] = {kShipMothershipSpritePixels, SHIP_MOTHERSHIP_SPRITE_SIZE},
    [SHIP_SHINE] = {kShipShineSpritePixels, SHIP_SHINE_SPRITE_SIZE},
    [SHIP_CRUZADER] = {kShipCruzaderSpritePixels, SHIP_CRUZADER_SPRITE_SIZE},
    [SHIP_TWINS] = {kShipTwinsSpritePixels, SHIP_TWINS_SPRITE_SIZE},
    [SHIP_ANTARTICA] = {kShipAntarticaSpritePixels, SHIP_ANTARTICA_SPRITE_SIZE},
    [SHIP_BUCKLER] = {kShipBucklerSpritePixels, SHIP_BUCKLER_SPRITE_SIZE},
    [SHIP_SAMURAI] = {kShipSamuraiSpritePixels, SHIP_SAMURAI_SPRITE_SIZE},
    [SHIP_RANGER] = {kShipRangerSpritePixels, SHIP_RANGER_SPRITE_SIZE},
};
