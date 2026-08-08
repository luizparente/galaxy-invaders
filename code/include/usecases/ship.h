#ifndef GALAXY_INVADERS_USECASES_SHIP_H
#define GALAXY_INVADERS_USECASES_SHIP_H

#include "domain/types.h"

/* Each ship's own Speed/Strength/Attack rating (0-10, shown on the
 * ship-select screen - see adapters/sdl_renderer.c) and the two generalized
 * formulas that turn Speed/Strength into real gameplay numbers. Isolated
 * here (Single Responsibility), same rationale as usecases/difficulty, so
 * the ratings can be re-tuned or a new ship added without touching
 * orchestration code in game_logic, and unit-tested in isolation.
 *
 * B-20 is the tuning baseline every spatial/life constant in
 * domain/constants.h (PLAYER_SPEED, PLAYER_LIFE_LOSS_PER_HIT) was already
 * balanced around, so both formulas below are anchored to B-20's own
 * rating and always yield exactly 100% (1.0x) for B-20 itself. Every future
 * ship's Speed/Strength rating plugs into the same two formulas - nothing
 * ship-specific needs touching outside kShipSpeedRating/kShipStrengthRating
 * in usecases/ship.c.
 *
 * --- Speed: direct proportion, floor at B-20's own rating ---
 * A ship's move speed, as a percentage of B-20's own PLAYER_SPEED, is
 * exactly the ratio of its Speed rating to B-20's:
 *     speed_percent(ship) = 100 * speed_rating(ship) / speed_rating(B-20)
 * B-20 (Speed 7) is 100% by construction. C-24 (Speed 5) is
 * 100 * 5/7 = 71.43%. Each single point of Speed is worth 100/7 = 14.29
 * percentage points of B-20's own speed - a fixed step, since the formula
 * is linear through the origin.
 *
 * --- Strength: inverse proportion, anchored at B-20's own life-loss ---
 * Strength doesn't add up directly - it's protective, so a higher rating
 * must divide the damage taken, not multiply it. The percentage of life
 * PLAYER_LIFE_LOSS_PER_HIT lost to a single enemy-projectile hit is:
 *     life_loss_percent(ship) =
 *         (strength_rating(B-20) * PLAYER_LIFE_LOSS_PER_HIT) / strength_rating(ship)
 * B-20 (Strength 5) plugs in as (5 * 10) / 5 = 10%, i.e. exactly
 * PLAYER_LIFE_LOSS_PER_HIT unchanged, by construction. C-24 (Strength 7) is
 * (5 * 10) / 7 = 7.14% - less than B-20's 10%, so C-24 survives more hits,
 * exactly as its higher Strength rating promises. Unlike Speed, a Strength
 * point's value in percentage points is NOT constant (this is a reciprocal
 * curve, not a line) - each additional point buys a smaller absolute
 * reduction than the one before it, the same diminishing-returns shape
 * "more armor" naturally has.
 *
 * Contact with an enemy ship or the boss's menace ring stays unconditionally
 * fatal regardless of Strength, same as it already is regardless of
 * remaining life (see kill_player in usecases/game_logic.c) - Strength only
 * ever softens *projectile* hits (damage_player), never ship-to-ship
 * contact. A ship with a lower Speed rating still dodges less often and so
 * still gets hit more overall, even though each individual projectile hit
 * lands lighter. */

int ship_speed_rating(Ship ship);
int ship_strength_rating(Ship ship);
int ship_attack_rating(Ship ship);

/* Multiplies PLAYER_SPEED (see update_player in usecases/game_logic.c) -
 * how fast the ship actually flies around the playfield. Equivalent to
 * speed_percent(ship) / 100 from the formula above. */
float ship_speed_multiplier(Ship ship);

/* Multiplies PLAYER_LIFE_LOSS_PER_HIT (see damage_player in
 * usecases/game_logic.c). Equivalent to life_loss_percent(ship) /
 * PLAYER_LIFE_LOSS_PER_HIT from the formula above - so damage_player only
 * has to multiply, never repeat the ratio itself. */
float ship_damage_taken_multiplier(Ship ship);

/* Convenience wrappers exposing the two formulas above directly in the
 * units they're specced in (percent of B-20's speed; percent of life lost
 * per hit) rather than as a bare multiplier - for the ship-select screen,
 * tooling, or anyone eyeballing whether a new ship's numbers behave as
 * intended, without doing the multiplication themselves. */
float ship_speed_percent(Ship ship);
float ship_life_loss_percent_per_hit(Ship ship);

/* Multiplies PLAYER_WIDTH/PLAYER_HEIGHT - a fixed, spec'd render/hitbox
 * size bump (The Mothership only, so far), not derived from Speed/Strength
 * like the two formulas above. 1.0 for every ship without one. Never
 * applied to a ChildShip (see domain/types.h) regardless of which ship
 * dispatched it. */
float ship_size_multiplier(Ship ship);

/* Each ship's own moveset: which ShootMode the 1-5 number keys reach, in
 * order, and how many of those 5 keys actually do anything. B-20 has all 5
 * of its own original modes in their original key order; C-24 has only 3
 * (pressing 4 or 5 does nothing - see update_shoot_mode_switch in
 * usecases/game_logic.c, the only reader of these). A future ship with its
 * own distinct moveset just needs its own slot table in usecases/ship.c;
 * nothing outside it needs to change. */
int ship_shoot_mode_slot_count(Ship ship);
/* slot is 0-based (key 1 is slot 0); must be < ship_shoot_mode_slot_count(ship). */
ShootMode ship_shoot_mode_for_slot(Ship ship, int slot);
/* Reverse lookup for the HUD's mode indicator (draw_shoot_mode_indicator in
 * adapters/sdl_renderer.c): which slot, if any, holds `mode` on this ship's
 * table. Returns -1 if `mode` isn't reachable on this ship at all - can't
 * happen for whatever ship is actually flying (Player.shoot_mode is only
 * ever set from this same table - see reset_run/update_shoot_mode_switch),
 * but the HUD has no other way to know that without asking. */
int ship_shoot_mode_slot_index(Ship ship, ShootMode mode);

#endif
