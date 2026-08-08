#include "usecases/ship.h"
#include "domain/constants.h"

/* One Speed/Strength/Attack rating per Ship (domain/types.h), ordered to
 * match: B-20, C-24. B-20 is the versatile, fast baseline every skilled
 * pilot starts on; C-24 trades some of that speed for heavier plating,
 * per its own description on the ship-select screen. */
static const int kShipSpeedRating[SHIP_COUNT] = {7, 5};
static const int kShipStrengthRating[SHIP_COUNT] = {5, 7};
static const int kShipAttackRating[SHIP_COUNT] = {8, 8};

int ship_speed_rating(Ship ship) {
    return kShipSpeedRating[ship];
}

int ship_strength_rating(Ship ship) {
    return kShipStrengthRating[ship];
}

int ship_attack_rating(Ship ship) {
    return kShipAttackRating[ship];
}

float ship_speed_multiplier(Ship ship) {
    return (float)kShipSpeedRating[ship] / (float)kShipSpeedRating[SHIP_B20];
}

float ship_damage_taken_multiplier(Ship ship) {
    return (float)kShipStrengthRating[SHIP_B20] / (float)kShipStrengthRating[ship];
}

float ship_speed_percent(Ship ship) {
    return ship_speed_multiplier(ship) * 100.0f;
}

float ship_life_loss_percent_per_hit(Ship ship) {
    return PLAYER_LIFE_LOSS_PER_HIT * ship_damage_taken_multiplier(ship);
}
