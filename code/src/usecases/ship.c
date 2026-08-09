#include "usecases/ship.h"
#include "domain/constants.h"

/* One Speed/Strength/Attack rating per Ship (domain/types.h), ordered to
 * match: B-20, C-24, SHIP_MOTHERSHIP, SHIP_SHINE. B-20 is the versatile,
 * fast baseline every skilled pilot starts on; C-24 trades some of that
 * speed for heavier plating; The Mothership trades the most speed of all
 * for the heaviest plating in the fleet; Shine trades the most plating of
 * all for the fastest ship in the fleet - faster even than B-20 itself -
 * per each one's own description on the ship-select screen. */
static const int kShipSpeedRating[SHIP_COUNT] = {7, 5, 2, 8};
static const int kShipStrengthRating[SHIP_COUNT] = {5, 7, 10, 4};
static const int kShipAttackRating[SHIP_COUNT] = {8, 7, 10, 6};

/* Fixed per-ship render/hitbox size multiplier - unlike Speed/Strength,
 * this isn't derived from a rating formula, it's spec'd directly (The
 * Mothership is "100% bigger than the other player spaceships" - double
 * size; Shine is explicitly "the same size as B-20"). Applied everywhere
 * PLAYER_WIDTH/PLAYER_HEIGHT drive the real player's own size -
 * draw_player, update_player's movement clamp, and check_collisions'
 * player half-extents (all in usecases/game_logic.c and
 * adapters/sdl_renderer.c). Never applied to a ChildShip, which always
 * renders/collides at the stock size regardless of which ship dispatched
 * it. */
static const float kShipSizeMultiplier[SHIP_COUNT] = {1.0f, 1.0f, 2.0f, 1.0f};

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

float ship_size_multiplier(Ship ship) {
    return kShipSizeMultiplier[ship];
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
 * draw_c24_sphere_shot, both of which branch on the shot's own
 * Projectile.style_ship rather than on ShootMode, since the same mode
 * looks and hits different depending on which ship fired it. */
static const ShootMode kC24ShootModeSlots[] = {
    SHOOT_MODE_DOUBLE, SHOOT_MODE_POWER, SHOOT_MODE_OMNI,
};

/* The Mothership's own moveset: she never fires a projectile of her own
 * under either mode - both dispatch a new ChildShip escort identically
 * (see update_mothership_dispatch in usecases/game_logic.c), key 1/slot 0
 * ordered first per how the two modes were specced. */
static const ShootMode kMothershipShootModeSlots[] = {
    SHOOT_MODE_SWARM_WANDER, SHOOT_MODE_SWARM_FORMATION,
};

/* Shine's own moveset - see the SHOOT_MODE_SHINE_* entries' own doc
 * comments in domain/types.h. Slot 1 (key 2) is the odd one out: it's
 * listed here purely so ship_shoot_mode_slot_count/for_slot and the HUD
 * indicator (adapters/sdl_renderer.c) know about it at all - it's never
 * actually assigned to Player.shoot_mode (update_shoot_mode_switch
 * intercepts it before that point). */
static const ShootMode kShineShootModeSlots[] = {
    SHOOT_MODE_SHINE_SHARDS, SHOOT_MODE_SHINE_OMNI, SHOOT_MODE_SHINE_SPIRAL,
};

typedef struct ShipShootModeSlots {
    const ShootMode *modes;
    int count;
} ShipShootModeSlots;

static const ShipShootModeSlots kShipShootModeSlots[SHIP_COUNT] = {
    [SHIP_B20] = {kB20ShootModeSlots, 5},
    [SHIP_C24] = {kC24ShootModeSlots, 3},
    [SHIP_MOTHERSHIP] = {kMothershipShootModeSlots, 2},
    [SHIP_SHINE] = {kShineShootModeSlots, 3},
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
