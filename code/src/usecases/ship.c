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

/* B-20 keeps its original 5-mode lineup in its original key order. */
static const ShootMode kB20ShootModeSlots[] = {
    SHOOT_MODE_NORMAL, SHOOT_MODE_RAPID, SHOOT_MODE_POWER, SHOOT_MODE_DOUBLE, SHOOT_MODE_SIDE,
};

/* C-24's own moveset: B-20's double barrel and power cannon, reused as-is
 * (same fire rate/damage - see usecases/game_logic.c's spawn call sites),
 * just under different keys, plus a ship-exclusive third mode. Every one of
 * these renders/hit-tests differently for C-24 than for B-20 despite
 * sharing a ShootMode value - see player_shot_half_extents and
 * draw_c24_sphere_shot, both of which branch on GameState.selected_ship
 * directly rather than on ShootMode, since the same mode looks and hits
 * different depending on which ship fired it. */
static const ShootMode kC24ShootModeSlots[] = {
    SHOOT_MODE_DOUBLE, SHOOT_MODE_POWER, SHOOT_MODE_OMNI,
};

typedef struct ShipShootModeSlots {
    const ShootMode *modes;
    int count;
} ShipShootModeSlots;

static const ShipShootModeSlots kShipShootModeSlots[SHIP_COUNT] = {
    [SHIP_B20] = {kB20ShootModeSlots, 5},
    [SHIP_C24] = {kC24ShootModeSlots, 3},
};

int ship_shoot_mode_slot_count(Ship ship) {
    return kShipShootModeSlots[ship].count;
}

ShootMode ship_shoot_mode_for_slot(Ship ship, int slot) {
    return kShipShootModeSlots[ship].modes[slot];
}

int ship_shoot_mode_slot_index(Ship ship, ShootMode mode) {
    const ShipShootModeSlots *slots = &kShipShootModeSlots[ship];
    for (int i = 0; i < slots->count; i++) {
        if (slots->modes[i] == mode) return i;
    }
    return -1;
}
