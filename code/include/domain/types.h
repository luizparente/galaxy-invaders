#ifndef GALAXY_INVADERS_DOMAIN_TYPES_H
#define GALAXY_INVADERS_DOMAIN_TYPES_H

#include <math.h>
#include <stdbool.h>
#include "domain/constants.h"

/* Pure domain entities and value objects. Nothing in this header may depend
 * on SDL or any other outward-layer library — see include/ports for the
 * abstractions that let outer layers plug in without this layer knowing. */

typedef struct Color {
    unsigned char r, g, b, a;
} Color;

/* h in degrees [0, 360), s and v in [0, 1]. A value-type operation on
 * Color, shared by usecases (e.g. rerolling the laser color, cycling the
 * orb's gradient) and the renderer (e.g. animating the super beam) so the
 * conversion math exists in exactly one place. */
static inline Color color_from_hsv(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r1, g1, b1;
    if (h < 60.0f) { r1 = c; g1 = x; b1 = 0.0f; }
    else if (h < 120.0f) { r1 = x; g1 = c; b1 = 0.0f; }
    else if (h < 180.0f) { r1 = 0.0f; g1 = c; b1 = x; }
    else if (h < 240.0f) { r1 = 0.0f; g1 = x; b1 = c; }
    else if (h < 300.0f) { r1 = x; g1 = 0.0f; b1 = c; }
    else { r1 = c; g1 = 0.0f; b1 = x; }

    return (Color){
        (unsigned char)((r1 + m) * 255.0f),
        (unsigned char)((g1 + m) * 255.0f),
        (unsigned char)((b1 + m) * 255.0f),
        255,
    };
}

typedef enum GameStateId {
    STATE_MENU,
    STATE_DIFFICULTY_SELECT,
    STATE_SHIP_SELECT,
    STATE_GAME,
    STATE_PAUSE,
    STATE_GAME_OVER,
} GameStateId;

typedef enum PauseSelection {
    PAUSE_RESUME = 0,
    PAUSE_EXIT = 1,
} PauseSelection;

/* Chosen on the difficulty-select screen (see update_difficulty_select in
 * usecases/game_logic.c) reached right after confirming START GAME on the
 * main menu, and kept for the whole run - see GameState.selected_difficulty
 * and usecases/difficulty.c for how each level maps to actual spawn/fire
 * tuning. Ordered easiest to hardest so the enum value alone can drive both
 * menu cursor navigation and the difficulty curve's steepness. */
typedef enum Difficulty {
    DIFFICULTY_BABY = 0,
    DIFFICULTY_EASY,
    DIFFICULTY_NORMAL,
    DIFFICULTY_HARD,
    DIFFICULTY_INSANE,
    DIFFICULTY_COUNT,
} Difficulty;

/* Chosen on the ship-select screen (see update_ship_select in
 * usecases/game_logic.c) reached right after confirming a difficulty, and
 * kept for the whole run - see GameState.selected_ship and usecases/ship.c
 * for how each ship's Speed/Strength ratings translate into real gameplay
 * multipliers. SHIP_B20, SHIP_C24, SHIP_MOTHERSHIP, SHIP_SHINE and
 * SHIP_CRUZADER and SHIP_TWINS are implemented; the ship-select grid has
 * room for more (adapters/sdl_renderer.c) but every slot past SHIP_COUNT
 * renders as a locked placeholder, not a real Ship value. SHIP_MOTHERSHIP
 * doesn't fire projectiles of its own at all - its two ShootModes
 * (SHOOT_MODE_SWARM_WANDER/SHOOT_MODE_SWARM_FORMATION below) dispatch
 * CPU-flown ChildShip escorts instead (see GameState.children and
 * update_mothership_dispatch/update_children in usecases/game_logic.c). A
 * ChildShip's own `kind` is always SHIP_B20 or SHIP_C24 - SHIP_MOTHERSHIP
 * itself never appears there. SHIP_TWINS is the one exception to "one
 * Player, one hitbox" - it's rendered and collided as two independent
 * bodies sharing a single Player (see the twins_* fields below and
 * player_hitboxes in usecases/game_logic.c). */
typedef enum Ship {
    SHIP_B20 = 0,
    SHIP_C24,
    SHIP_MOTHERSHIP,
    SHIP_SHINE,
    SHIP_CRUZADER,
    SHIP_TWINS,
    /* Antartica is the other exception to "one Player, one hitbox" besides
     * SHIP_TWINS - but unlike the Twins, who share a single input-driven
     * control point, Antartica and her sidekick Frosty are TWO fully
     * independently controlled bodies at once: arrow keys fly Antartica
     * herself (Player.x/y, same as every non-Twins ship), WASD flies Frosty
     * (Player.frosty_x/frosty_y) - see InputCommand's own arrow_* / wasd_*
     * fields and update_player's own SHIP_ANTARTICA branch in
     * usecases/game_logic.c. Frosty fires its own passive snowball weapon
     * automatically (update_frosty_fire) regardless of Antartica's own
     * shoot_mode/fire key, and each body tracks its own life/alive flag
     * independently (antartica_alive/antartica_life,
     * frosty_alive/frosty_life) - the same "one can die while the other
     * keeps going" rule SHIP_TWINS already has (see kill_antartica_body/
     * kill_frosty), just with two different weapons instead of one shared
     * one. */
    SHIP_ANTARTICA,
    /* Buckler is the one ship whose fire key isn't the spacebar at all - see
     * SHOOT_MODE_BUCKLER_CANNON below and update_buckler_cannon_fire in
     * usecases/game_logic.c. The spacebar instead triggers her own
     * protective orb (Player.buckler_orb_timer/buckler_orb_cooldown_timer) -
     * the same duration/cooldown pairing and incoming-fire-blocking behavior
     * as Cruzader's own deflector orb (SHOOT_MODE_CRUZADER_ORB), just never
     * reflecting anything back at the enemies the way Cruzader's does (see
     * trigger_buckler_orb's own doc comment). */
    SHIP_BUCKLER,
    /* Samurai's own 3-mode kit (see usecases/ship.c) is the one other ship
     * besides Buckler whose keys 2/3 are real, persistent Player.shoot_mode
     * values that auto-revert to mode 1 only once their own active window
     * ends (not immediately on keypress, the "trigger + immediate revert"
     * pattern every other special mode above uses) - see
     * SHOOT_MODE_SAMURAI_OMNI/SHOOT_MODE_SAMURAI_STEALTH's own doc comments
     * below. */
    SHIP_SAMURAI,
    SHIP_COUNT,
} Ship;

/* Every shooting pattern any ship can fire, switched with the 1-5 number
 * keys (see InputCommand). Which of these a given ship actually has, and
 * which key selects which, is per-ship - not every ship offers every mode,
 * and the same key can mean a different mode on a different ship (see
 * ship_shoot_mode_for_slot/ship_shoot_mode_slot_count in usecases/ship.h,
 * and update_shoot_mode_switch/reset_run in usecases/game_logic.c, which
 * are the only places a ShootMode value gets assigned to Player.shoot_mode).
 * The HUD's mode indicator (adapters/sdl_renderer.c) draws
 * ship_shoot_mode_slot_count(selected_ship) dots, not SHOOT_MODE_COUNT - a
 * new mode only needs a slot in some ship's own table (usecases/ship.c) to
 * show up there. */
typedef enum ShootMode {
    SHOOT_MODE_NORMAL = 0,
    SHOOT_MODE_RAPID,
    SHOOT_MODE_POWER,
    SHOOT_MODE_DOUBLE,
    SHOOT_MODE_SIDE,
    /* 8 shots fired at once in every direction, evenly spaced like the
     * points of an octagon - the same pattern ENEMY_SHOOT_OMNI uses (see
     * EnemyShootStyle below), just from the player's own position. Not part
     * of B-20's own moveset - only ships whose own slot table includes it
     * (currently just C-24) can ever reach it. */
    SHOOT_MODE_OMNI,
    /* The Mothership's own two modes (see ship_shoot_mode_for_slot(SHIP_MOTHERSHIP, ...)
     * in usecases/ship.c) - she never fires a projectile of her own under
     * either one. Both dispatch a new ChildShip escort exactly the same way
     * on fire (see update_mothership_dispatch in usecases/game_logic.c);
     * the only difference is which movement AI every currently-alive child
     * follows, read fresh every frame from GameState.player.shoot_mode (see
     * update_children) - not fixed at the child's own spawn time, so
     * switching mid-flight redirects the whole squad immediately. */
    SHOOT_MODE_SWARM_WANDER,   /* children roam independently */
    SHOOT_MODE_SWARM_FORMATION, /* children hold a triangular escort formation */
    /* Shine's own 3-mode kit (see usecases/ship.c) - none of these are
     * reachable by any other ship. */
    SHOOT_MODE_SHINE_SHARDS,  /* mode 1 (default): twin close-set crystal shards, straight ahead */
    /* Mode 2: not a persistent mode at all, despite having its own slot -
     * pressing key 2 directly fires a 12-way burst of the same shards (see
     * trigger_shine_omni_burst in usecases/game_logic.c) and immediately
     * puts shoot_mode back to SHOOT_MODE_SHINE_SHARDS, gated on
     * Player.shine_omni_cooldown_timer. Player.shoot_mode itself is never
     * actually set to this value - update_shoot_mode_switch intercepts slot
     * 1 on SHIP_SHINE before the normal switch-and-stay assignment runs. */
    SHOOT_MODE_SHINE_OMNI,
    SHOOT_MODE_SHINE_SPIRAL, /* mode 3: one longer shard, visually spinning as it flies */
    /* Cruzader's own 3-mode kit (see usecases/ship.c) - none of these are
     * reachable by any other ship. */
    SHOOT_MODE_CRUZADER_TWIN,     /* mode 1 (default): green/blue twin wingtip bolts */
    /* Mode 2: like SHOOT_MODE_SHINE_OMNI, never actually persists as
     * Player.shoot_mode - pressing key 2 triggers the deflector orb (see
     * trigger_cruzader_orb in usecases/game_logic.c) and immediately puts
     * shoot_mode back to SHOOT_MODE_CRUZADER_TWIN. Unlike Shine's omni
     * (a single instant burst), the orb also starts a
     * Player.cruzader_orb_timer active window (reflecting every incoming
     * enemy shot in range for its duration) before the following
     * Player.cruzader_orb_cooldown_timer lockout begins - both timers tick
     * unconditionally in update_player_firing, independent of shoot_mode,
     * since the orb stays active while shoot_mode has already reverted to
     * mode 1. */
    SHOOT_MODE_CRUZADER_ORB,
    SHOOT_MODE_CRUZADER_ROCKETS,  /* mode 3: slow, homing, explosive rockets */
    /* The Twins' own 2-mode kit (see usecases/ship.c) - none of these are
     * reachable by any other ship. Both fire identically (see
     * update_twins_alternating_fire in usecases/game_logic.c) - the only
     * difference between them is which flight behavior update_player's own
     * SHIP_TWINS branch applies that frame, read fresh from
     * Player.shoot_mode every frame same as Mothership's own two modes
     * above. */
    SHOOT_MODE_TWINS_ALTERNATE, /* mode 1 (default): rigid formation flight */
    SHOOT_MODE_TWINS_MIRROR,    /* mode 2: right twin free-flies, left mirrors it */
    /* Antartica's own 3-mode kit (see usecases/ship.c) - none of these are
     * reachable by any other ship. Frosty's own passive snowball fire
     * (update_frosty_fire) is completely separate from all three - it fires
     * on its own timer regardless of which of these is active. */
    SHOOT_MODE_ANTARTICA_SHARDS, /* mode 1 (default): twin ice shards, straight ahead - Shine's own mode 1, recolored */
    /* Mode 2: like SHOOT_MODE_SHINE_OMNI, never actually persists as
     * Player.shoot_mode - pressing key 2 directly fires a 16-shard fan
     * across Antartica's own frontal 180 degrees (see
     * trigger_antartica_ice_storm in usecases/game_logic.c) and immediately
     * puts shoot_mode back to mode 1, gated on
     * Player.antartica_ice_storm_cooldown_timer. */
    SHOOT_MODE_ANTARTICA_ICE_STORM,
    /* Mode 3: like SHOOT_MODE_CRUZADER_ORB, never actually persists as
     * Player.shoot_mode - pressing key 3 starts a 5s active window
     * (Player.antartica_freeze_beam_timer) during which both Antartica and
     * Frosty fire a continuous beam (see update_antartica_freezing_beam),
     * then immediately puts shoot_mode back to mode 1. Unlike the power
     * orb's own super beam, this neither heals nor grants invincibility -
     * see the timer's own doc comment on Player. */
    SHOOT_MODE_ANTARTICA_FREEZE_BEAM,
    /* Buckler's own (only) mode - see update_buckler_cannon_fire in
     * usecases/game_logic.c. Unlike every ship above, Buckler's own moveset
     * table (usecases/ship.c) lists this as its single slot purely so
     * ship_shoot_mode_slot_count/for_slot stay consistent for the ship-select
     * screen's own stat readout - the number keys never actually select a
     * *mode* on Buckler at all (update_shoot_mode_switch skips SHIP_BUCKLER
     * entirely). Instead each of keys 1-5 fires this exact same mode's
     * projectile from a different cannon/direction, gated on
     * Player.buckler_active_cannon (see InputCommand's own held key state and
     * BucklerCannon in usecases/game_logic.c) rather than on shoot_mode
     * switching or fire_held the way every other ship's own fire key does. */
    SHOOT_MODE_BUCKLER_CANNON,
    /* Samurai's own 3-mode kit (see usecases/ship.c) - none of these are
     * reachable by any other ship. */
    SHOOT_MODE_SAMURAI_SHURIKEN, /* mode 1 (default): a burst of 3 shuriken, straight ahead */
    /* Mode 2: unlike every "trigger + immediate revert" mode above
     * (SHOOT_MODE_SHINE_OMNI, SHOOT_MODE_CRUZADER_ORB, etc.), this DOES
     * persist as Player.shoot_mode for its whole active window - pressing
     * key 2 (see update_shoot_mode_switch in usecases/game_logic.c) starts
     * a 1s sweep (Player.samurai_omni_burst_timer) that fires one shuriken
     * every SAMURAI_OMNI_SHOT_INTERVAL seconds (Player.samurai_omni_
     * next_shot_index/samurai_omni_shot_timer track progress), spanning her
     * own frontal 180 degrees in SAMURAI_OMNI_STEP_DEG increments starting
     * from the west - only once the whole sweep finishes does shoot_mode
     * actually revert to mode 1, at the same moment
     * Player.samurai_omni_cooldown_timer's own 20s lockout begins (see
     * update_samurai_omni_fire). Mode-switching is locked out entirely
     * while the sweep is in progress (same rule update_shoot_mode_switch
     * already gives a B-20-style rapid-fire burst), and re-selecting mode 2
     * itself stays unavailable for the rest of the cooldown once it ends. */
    SHOOT_MODE_SAMURAI_OMNI,
    /* Mode 3: stealth - same "persists until its own window ends, not
     * intercepted on keypress" shape as mode 2 above, just no firing at all
     * during the 3s active window (Player.samurai_stealth_timer): the ship
     * turns 50% transparent, immune to every attack/collision (enemy shots
     * pass through, ordinary enemies pass through with neither side dying,
     * the boss ring does nothing - see check_collisions' own SHIP_SAMURAI
     * branches), and moves at SAMURAI_STEALTH_SPEED_MULTIPLIER the normal
     * speed (see update_player). Reverts to mode 1 and starts
     * Player.samurai_stealth_cooldown_timer's own 20s lockout the instant
     * the window ends (see update_samurai_stealth), same auto-revert timing
     * as mode 2. */
    SHOOT_MODE_SAMURAI_STEALTH,
    SHOOT_MODE_COUNT,
} ShootMode;

typedef struct Player {
    float x, y;
    bool alive;
    float fire_cooldown;
    Color laser_color;
    float super_beam_timer; /* seconds remaining; 0 = inactive */
    bool god_mode; /* toggled by Ctrl+G; ship turns gold and cannot die */
    float life; /* percentage, [0, PLAYER_LIFE_MAX]; hitting 0 kills the player */

    ShootMode shoot_mode;
    /* Rapid fire's own two-phase timer (see update_rapid_fire in
     * usecases/game_logic.c): rapid_burst_timer counts down the 3s of
     * automatic fire once triggered, then rapid_cooldown_timer counts down
     * the following 20s lockout, during which shoot_mode is auto-switched
     * away to slot 0 and mode 2 (SHOOT_MODE_RAPID) alone is unselectable
     * (see update_shoot_mode_switch) - every other mode is free to switch
     * into during the lockout. Both 0 means idle - free to fire normally
     * or switch modes, mode 2 included. Only one of the two is ever
     * nonzero at a time. */
    float rapid_burst_timer;
    float rapid_cooldown_timer;

    /* Shine's own mode 2 (SHOOT_MODE_SHINE_OMNI) cooldown - counts down
     * SHINE_OMNI_COOLDOWN after each burst (see trigger_shine_omni_burst in
     * usecases/game_logic.c). Unlike rapid_burst_timer/rapid_cooldown_timer
     * above, there's no "locked out of switching" phase to pair with it -
     * mode 2 is never actually a persistent shoot_mode value to switch into
     * or out of in the first place (see SHOOT_MODE_SHINE_OMNI's own doc
     * comment) - this timer purely gates whether pressing key 2 again does
     * anything. Unused by every other ship. */
    float shine_omni_cooldown_timer;

    /* Cruzader's own mode 2 (SHOOT_MODE_CRUZADER_ORB) two-phase timer - the
     * same duration/cooldown pairing rapid_burst_timer/rapid_cooldown_timer
     * above use, just not gated on shoot_mode (the orb stays active while
     * shoot_mode has already reverted to mode 1 - see trigger_cruzader_orb
     * in usecases/game_logic.c). cruzader_orb_timer counts down the 5s
     * active window (during which check_collisions reflects every enemy
     * shot within CRUZADER_ORB_RADIUS and blocks boss-ring contact
     * entirely); the instant it expires, cruzader_orb_cooldown_timer starts
     * its own 20s countdown, during which pressing key 2 again is a no-op.
     * Both tick unconditionally in update_player_firing. Only one is ever
     * nonzero at a time. Unused by every other ship. */
    float cruzader_orb_timer;
    float cruzader_orb_cooldown_timer;

    /* Counts down to the next engine trail particle emission (see
     * update_player_trail) - purely cosmetic, unrelated to fire_cooldown. */
    float trail_emit_timer;

    /* The Twins' own dual-body state - unused by every other ship. x/y
     * above remain the single input-driven control point (the right
     * twin's own x directly in SHOOT_MODE_TWINS_MIRROR, or the shared
     * formation center in SHOOT_MODE_TWINS_ALTERNATE - see update_player);
     * twins_right_x/twins_left_x are each twin's own actual on-screen x
     * every frame - directly derived from that control point in mirror
     * mode, eased toward it (not snapped) in formation mode (see
     * TWINS_FORMATION_REJOIN_SPEED), or driven directly once only one twin
     * remains (see kill_twin's own control-transfer step, which snaps x
     * onto the survivor). y is always shared - both twins move vertically
     * together, no separate y needed. twins_mirror_center_x is the
     * reflection axis for mirror mode - NOT always screen-center: it's
     * re-anchored to the twins' own current midpoint the instant mode 2
     * activates (see update_shoot_mode_switch), so the left twin always
     * starts mirroring from wherever it actually already was, never
     * teleporting to reconcile with a stale/unrelated axis. Re-anchored
     * the same way (to the twins' own current midpoint) when switching
     * back to mode 1, so formation's own target center picks up from
     * their current spread instead of some historical position - that's
     * what makes them visibly fly toward each other, not teleport
     * together. twins_next_shot_is_right alternates which twin's own x the
     * next shot in update_twins_alternating_fire spawns from. */
    float twins_right_x, twins_left_x;
    float twins_mirror_center_x;
    float twins_right_life, twins_left_life;
    bool twins_right_alive, twins_left_alive;
    bool twins_next_shot_is_right;

    /* Antartica's own dual-body state - unused by every other ship. Unlike
     * The Twins above, x/y remain Antartica's own actual on-screen
     * position, driven directly by arrow keys alone (see InputCommand's own
     * arrow_* fields and update_player's SHIP_ANTARTICA branch) exactly
     * like every non-Twins ship's x/y already are - frosty_x/frosty_y are
     * Frosty's own actual position, driven independently by WASD alone
     * (InputCommand's own wasd_* fields). antartica_life/frosty_life and
     * antartica_alive/frosty_alive are each body's own independent life
     * pool/status - the same "one can die while the other keeps going" rule
     * twins_right_life/twins_left_life etc. already establish (see
     * kill_antartica_body/kill_frosty in usecases/game_logic.c) - Player.life
     * itself stays unused by SHIP_ANTARTICA, same carve-out as SHIP_TWINS. */
    float frosty_x, frosty_y;
    float antartica_life, frosty_life;
    bool antartica_alive, frosty_alive;

    /* Frosty's own passive weapon (update_frosty_fire) - fires
     * automatically at FROSTY_SNOWBALL_FIRE_COOLDOWN's flat rate whenever
     * frosty_alive, completely independent of Antartica's own fire key or
     * shoot_mode (see update_player_firing). Unused by every other ship. */
    float frosty_fire_cooldown;

    /* Antartica's own mode 2 (SHOOT_MODE_ANTARTICA_ICE_STORM) cooldown -
     * same "gates re-trigger only, no lockout phase" role as
     * shine_omni_cooldown_timer above (see trigger_antartica_ice_storm in
     * usecases/game_logic.c). Unused by every other ship. */
    float antartica_ice_storm_cooldown_timer;

    /* Antartica's own mode 3 (SHOOT_MODE_ANTARTICA_FREEZE_BEAM) two-phase
     * timer - the same duration/cooldown pairing cruzader_orb_timer/
     * cruzader_orb_cooldown_timer above use: antartica_freeze_beam_timer
     * counts down the 5s active window (during which
     * update_antartica_freezing_beam sweeps a column from each of
     * Antartica/Frosty still alive - see trigger_antartica_freeze_beam),
     * then antartica_freeze_beam_cooldown_timer starts its own 30s
     * countdown the instant the window ends. Deliberately never checked by
     * kill_player/damage_player/kill_antartica_body/damage_antartica_body/
     * kill_frosty/damage_frosty's own immunity guards - unlike the power
     * orb's own super_beam_timer, this grants neither invincibility nor a
     * heal, only the beam sweep itself. antartica_freeze_beam_boss_hit_timer
     * paces repeat boss damage while in contact, the same role
     * Boss.beam_contact_timer plays for the power orb's own super beam, kept
     * as its own separate field so the two beams' boss-contact pacing can
     * never interfere with each other if both were ever active at once.
     * Unused by every other ship. */
    float antartica_freeze_beam_timer;
    float antartica_freeze_beam_cooldown_timer;
    float antartica_freeze_beam_boss_hit_timer;

    /* Buckler's own directional cannon (SHOOT_MODE_BUCKLER_CANNON) - unused
     * by every other ship. buckler_active_cannon is 0 while no cannon is
     * firing, or 1-5 (matching InputCommand's own shoot_mode_N_held slot
     * numbering) for whichever single cannon is currently latched - see
     * update_buckler_cannon_fire in usecases/game_logic.c for the
     * "first pressed wins, ties broken by release" rule this implements. */
    int buckler_active_cannon;

    /* Buckler's own spacebar power - the protective orb. Same two-phase
     * timer pairing as cruzader_orb_timer/cruzader_orb_cooldown_timer above
     * (same CRUZADER_ORB_DURATION/CRUZADER_ORB_COOLDOWN constants, see
     * trigger_buckler_orb in usecases/game_logic.c), just gated on
     * Player.fire_held (the spacebar) instead of a shoot-mode key, and
     * blocking incoming fire outright instead of reflecting it back (see
     * check_collisions' own SHIP_BUCKLER branch) - no passive chance either,
     * unlike Cruzader's own CRUZADER_PASSIVE_REFLECT_CHANCE. Unused by every
     * other ship. */
    float buckler_orb_timer;
    float buckler_orb_cooldown_timer;

    /* Samurai's own mode 1 (SHOOT_MODE_SAMURAI_SHURIKEN) burst state -
     * unused by every other ship. Same "burst_shots_remaining/
     * burst_shot_timer" shape as Enemy's own ENEMY_SHOOT_TRIBURST fields
     * (domain/types.h's own Enemy struct) - samurai_burst_shots_remaining
     * counts down shots left in the in-progress burst (0 = idle, waiting on
     * fire_held+fire_cooldown like every other mode 1), samurai_burst_shot_
     * timer paces the SAMURAI_SHURIKEN_SHOT_INTERVAL gap between each shot
     * within a burst - see update_samurai_shuriken in usecases/game_logic.c.
     * fire_cooldown itself (shared with every other ship) gates only when
     * the NEXT burst is allowed to start, set to SAMURAI_SHURIKEN_BURST_
     * COOLDOWN the instant the 3rd shot of the current burst fires. */
    int samurai_burst_shots_remaining;
    float samurai_burst_shot_timer;

    /* Samurai's own mode 2 (SHOOT_MODE_SAMURAI_OMNI) sweep state - unused
     * by every other ship. samurai_omni_burst_timer counts down the 1s
     * active window (see update_samurai_omni_fire in usecases/game_logic.c);
     * samurai_omni_next_shot_index (0-7) is how many of the 8 sweep shots
     * have fired so far, and samurai_omni_shot_timer counts down to the
     * next one (SAMURAI_OMNI_SHOT_INTERVAL apart). The instant
     * samurai_omni_burst_timer hits 0, samurai_omni_cooldown_timer starts
     * its own 20s countdown - same mutually-exclusive two-phase pairing as
     * cruzader_orb_timer/cruzader_orb_cooldown_timer, just gating
     * re-selection of the mode rather than a passive power. */
    float samurai_omni_burst_timer;
    int samurai_omni_next_shot_index;
    float samurai_omni_shot_timer;
    float samurai_omni_cooldown_timer;

    /* Samurai's own mode 3 (SHOOT_MODE_SAMURAI_STEALTH) two-phase timer -
     * same duration/cooldown pairing as samurai_omni_burst_timer/
     * samurai_omni_cooldown_timer above. samurai_stealth_timer counts down
     * the 3s active window (during which update_player applies
     * SAMURAI_STEALTH_SPEED_MULTIPLIER and check_collisions' own
     * SHIP_SAMURAI branches skip every attack/collision entirely - see
     * update_samurai_stealth); the instant it expires,
     * samurai_stealth_cooldown_timer starts its own 20s countdown. Unused
     * by every other ship. */
    float samurai_stealth_timer;
    float samurai_stealth_cooldown_timer;
} Player;

/* A CPU-flown escort dispatched by The Mothership (see
 * update_mothership_dispatch/update_children in usecases/game_logic.c) -
 * up to MOTHERSHIP_MAX_CHILDREN of these live in GameState.children.
 * Always a B-20 or C-24 "lookalike" (kind), rendered/sized/collided at the
 * stock PLAYER_WIDTH/PLAYER_HEIGHT (never SHIP_MOTHERSHIP's own +25%) and
 * firing its own kind's real moveset, at MOTHERSHIP_CHILD_LIFE_MAX life
 * (50% of what PLAYER_LIFE_MAX would be if it were the player). Movement
 * and weapons are both fully CPU-controlled - there's no InputCommand
 * involved anywhere in a child's own update. */
typedef struct ChildShip {
    bool alive;
    float x, y;
    float vx, vy;
    Ship kind; /* always SHIP_B20 or SHIP_C24, rolled 50/50 at dispatch */
    float life; /* [0, MOTHERSHIP_CHILD_LIFE_MAX]; hitting 0 kills this child */

    /* Rolled at dispatch from kind's own ship_shoot_mode_for_slot table -
     * mode #1 (slot 0) the overwhelming majority of the time, only a
     * MOTHERSHIP_CHILD_RANDOM_MODE_CHANCE sliver landing on anything else
     * (see update_mothership_dispatch). A CPU escort never switches modes
     * on its own initiative the way the player does (none of
     * update_shoot_mode_switch's mode-switch/lockout machinery applies
     * here), with exactly one built-in exception: a SHOOT_MODE_RAPID child
     * permanently falls back to mode #1 the instant its one burst ends
     * (see update_child_firing) and never returns to mode #2. fire_cooldown/
     * rapid_burst_timer mirror Player's own same-named fields, just driving
     * this child's own fire routine instead. */
    ShootMode shoot_mode;
    float fire_cooldown;
    float rapid_burst_timer;

    /* Counts down the brief post-dispatch launch kick (see
     * update_mothership_dispatch/update_children): while positive, this
     * child just coasts on its spawn-time vx/vy (a random left/right shove
     * out from underneath the Mothership) and ignores shoot_mode's AI
     * entirely. Once it hits 0, AI movement (wander or formation, per
     * GameState.player.shoot_mode) takes over. */
    float launch_timer;

    /* SHOOT_MODE_SWARM_WANDER's own state: the point this child is
     * currently steering toward, and how much longer until it rolls a new
     * one (see update_children) - both unused/stale while formation mode
     * is active, harmless since formation mode never reads them. */
    float wander_target_x, wander_target_y;
    float wander_retarget_timer;
} ChildShip;

/* Which of the 5 shooting patterns an enemy design fires - see
 * kEnemyKindShootStyle in usecases/spawner.c for which of the 16 designs
 * (Enemy.kind) uses which, and the per-style constants in domain/constants.h.
 * Purely a difference in what the player has to dodge, not how hard it
 * hits: every style still deals PLAYER_LIFE_LOSS_PER_HIT per contact. */
typedef enum EnemyShootStyle {
    ENEMY_SHOOT_THIN_BEAM = 0, /* a slim beam, like the player's own but thinner */
    ENEMY_SHOOT_LONG_BEAM,     /* the same beam, stretched much longer */
    ENEMY_SHOOT_TRIBURST,      /* 3 small round shots fired back-to-back */
    ENEMY_SHOOT_TRISHOT,       /* 3 beams per trigger: forward + both diagonals */
    ENEMY_SHOOT_OMNI,          /* 8 small round shots fired at once, all directions */
} EnemyShootStyle;

/* How a given enemy moves down the screen - rolled once at spawn time (see
 * spawn_one_enemy in usecases/spawner.c), based on how many bosses have
 * been defeated so far this run (GameState.bosses_defeated) -
 * ERRATIC_ENEMY_CHANCE_PER_BOSS_DEFEAT of ordinary enemies get one of the
 * three non-NORMAL styles below per defeat, uniformly picked among them;
 * the rest keep flying NORMAL forever, same as before this existed. Every
 * non-NORMAL style still nets a steady downward drift underneath its own
 * pattern (see update_enemies in usecases/game_logic.c) so it still
 * eventually clears the bottom of the screen the same way a NORMAL enemy
 * does - none of this needs its own despawn logic. */
typedef enum EnemyMovementStyle {
    ENEMY_MOVEMENT_NORMAL = 0, /* the original straight fall + small sideways wobble */
    ENEMY_MOVEMENT_CIRCLE,     /* loops a fixed-radius circle around a slowly-descending center */
    ENEMY_MOVEMENT_SPIRAL,     /* same as CIRCLE, but the radius grows over time up to a cap */
    ENEMY_MOVEMENT_SINE,       /* a wide horizontal sine wave while falling straight down */
    ENEMY_MOVEMENT_RANDOM,     /* re-rolls a fresh heading (with a guaranteed downward
                                 * component) every fraction of a second */
} EnemyMovementStyle;

typedef struct Enemy {
    bool alive;
    float x, y;
    float vx, vy;
    float size;
    Color color; /* a random color rolled at spawn time (see spawner.c); tints this enemy's projectiles - the sprite itself carries its own fixed colors */
    int kind; /* index into adapters/enemy_sprites' kEnemySprites, [0, ENEMY_KIND_COUNT); also picks this enemy's EnemyShootStyle, see kEnemyKindShootStyle */
    float fire_timer;
    EnemyMovementStyle movement_style;
    /* NORMAL's own small sideways wobble phase; repurposed by every
     * non-NORMAL EnemyMovementStyle for its own single per-frame driver -
     * CIRCLE/SPIRAL's orbital angle (radians), SINE's wave phase, RANDOM's
     * countdown to its next re-roll - so a style switch never needs its
     * own dedicated timer field. */
    float wobble_phase;

    /* CIRCLE/SPIRAL/SINE only: the slowly-descending point the pattern is
     * actually drawn around - drifts by the enemy's own vx/vy exactly like
     * a NORMAL enemy's raw position would, while e.x/e.y (the real,
     * collidable position) orbits/waves around it. Unused by
     * NORMAL/RANDOM, which write straight into x/y themselves. */
    float orbit_center_x, orbit_center_y;
    /* CIRCLE/SPIRAL's orbit radius (SPIRAL's own grows over time, up to
     * ERRATIC_ENEMY_SPIRAL_RADIUS_MAX) or SINE's wave amplitude - a single
     * shared "how far from center" field since no enemy ever needs more
     * than one of these at once. Unused by NORMAL/RANDOM. */
    float erratic_radius;

    /* ENEMY_SHOOT_TRIBURST's own state: burst_shots_remaining counts down
     * shots left in the in-progress burst (0 = idle, waiting on
     * fire_timer like every other style); burst_shot_timer paces the short
     * gap between each shot within a burst. Unused by every other style. */
    int burst_shots_remaining;
    float burst_shot_timer;

    /* Set when a shot (not captured) power orb schedules this enemy to
     * detonate; orb_kill_timer counts down the random per-enemy delay
     * (see ORB_SHOT_EXPLOSION_WINDOW) before it actually happens. */
    bool orb_kill_pending;
    float orb_kill_timer;

    /* Counts down to this enemy's next engine trail particle emission -
     * see update_enemy_and_boss_trails, the enemy/boss counterpart to the
     * player's own update_player_trail. Purely cosmetic. */
    float trail_emit_timer;

    /* True from the instant a boss dispatches this enemy (see
     * spawner_dispatch_enemy_from_boss) until it reaches
     * boss_dispatch_target_x/y, during which update_enemy_movement flies it
     * in a straight line at BOSS_DISPATCH_ENEMY_FLIGHT_SPEED instead of
     * running its (not yet rolled) movement_style - false for every
     * ordinarily-spawned enemy, the default/rest state once a dispatched
     * one lands (see spawner_land_boss_dispatched_enemy, which is what
     * finally rolls a real movement_style, same as an ordinary spawn
     * would've at spawn time). */
    bool boss_dispatch_flying;
    float boss_dispatch_target_x, boss_dispatch_target_y;
} Enemy;

/* Drives the player shot's rendering (adapters/sdl_renderer.c) and, for
 * PROJECTILE_KIND_POWER, its explode-on-contact behavior in check_collisions.
 * Unused (left NORMAL) by enemy shots. */
typedef enum ProjectileKind {
    PROJECTILE_KIND_NORMAL = 0,
    PROJECTILE_KIND_RAPID,
    PROJECTILE_KIND_POWER,
    /* Shine's own mode 3 shot only (SHOOT_MODE_SHINE_SPIRAL) - a single
     * longer crystal shard than her own modes 1/2 (still PROJECTILE_KIND_NORMAL)
     * use, and the only kind that spins in place (see draw_shine_shard in
     * adapters/sdl_renderer.c) while flying straight ahead. */
    PROJECTILE_KIND_SHINE_SPIRAL,
    /* Cruzader's own mode 3 shot (SHOOT_MODE_CRUZADER_ROCKETS) - homes
     * toward the closest alive Enemy every frame (see
     * update_cruzader_rocket_homing in usecases/game_logic.c) and, like
     * PROJECTILE_KIND_POWER, explodes in a radius sweep on contact (see
     * trigger_power_cannon_explosion) rather than only harming whatever it
     * directly touched. */
    PROJECTILE_KIND_CRUZADER_ROCKET,
    /* Frosty's own passive weapon (see update_frosty_fire in
     * usecases/game_logic.c) - the only thing that distinguishes its shots
     * from Antartica's own ice shards despite both sharing
     * Projectile.style_ship == SHIP_ANTARTICA (see draw_frosty_snowball vs
     * draw_antartica_shard in adapters/sdl_renderer.c, and
     * player_shot_half_extents' own SHIP_ANTARTICA branch). */
    PROJECTILE_KIND_FROSTY_SNOWBALL,
    /* Buckler's own mode (SHOOT_MODE_BUCKLER_CANNON) - a round neon-green
     * ball with yellowish accents (see draw_buckler_cannon_ball in
     * adapters/sdl_renderer.c), fired from whichever of her 5 cannons is
     * currently active (Player.buckler_active_cannon) toward that cannon's
     * own fixed direction rather than always straight up. Round like
     * PROJECTILE_KIND_SHINE_SPIRAL/a C-24 sphere shot, not elongated like a
     * beam - see player_shot_half_extents' own SHIP_BUCKLER branch. */
    PROJECTILE_KIND_BUCKLER_ORB,
    /* Every one of Samurai's own shots (modes 1/2 alike) - a spinning
     * 4-point shuriken star (see draw_samurai_shuriken in
     * adapters/sdl_renderer.c). Round like PROJECTILE_KIND_BUCKLER_ORB for
     * hit-testing purposes (player_shot_half_extents) despite the pointed
     * silhouette - same "simple fixed-radius sphere, no travel-direction
     * math needed" convention every other round player shot already uses. */
    PROJECTILE_KIND_SAMURAI_SHURIKEN,
} ProjectileKind;

/* Drives an enemy shot's rendering (adapters/sdl_renderer.c) and hitbox
 * (enemy_shot_half_extents in usecases/game_logic.c) - BEAM shots (styles
 * thin/long/trishot) are a slim bolt oriented along their own travel
 * direction, sized by Projectile.half_len/half_wid; ORB shots (styles
 * triburst/omni) are a glowing sphere, sized by half_len alone (its
 * radius; half_wid unused). Unused by player shots (see ProjectileKind). */
typedef enum EnemyProjectileKind {
    ENEMY_PROJECTILE_BEAM = 0,
    ENEMY_PROJECTILE_ORB,
} EnemyProjectileKind;

typedef struct Projectile {
    bool alive;
    float x, y;
    float vx, vy;
    Color color;
    ProjectileKind kind;
    /* How much of the boss's hit pool this shot consumes on contact (see
     * damage_boss) - BASE_PLAYER_DAMAGE for most modes, scaled by that
     * mode's own *_DAMAGE_MULTIPLIER constant otherwise. Every other
     * target dies to any hit regardless of this value. */
    float damage;
    /* True only for side-beam shots (ShootMode SHOOT_MODE_SIDE): the shot
     * travels sideways instead of upward, so its visual and hitbox are
     * elongated along x instead of y (see draw_projectile and
     * player_shot_half_extents). */
    bool horizontal;

    /* Set true only on enemy shots caught out when a boss arrives: they
     * keep drifting and visually fade (see inert_age) but can no longer
     * harm the player. Unused by player shots. */
    bool inert;
    float inert_age;

    /* Enemy shots only (see EnemyProjectileKind above and
     * enemy_shot_half_extents in usecases/game_logic.c): half_len is a
     * beam's half-length along its travel direction or an orb's radius;
     * half_wid is a beam's half-width across its travel direction (unused
     * by orbs). Already scaled by GameState.scale at spawn time, same
     * convention spawn_player_shot's vx/vy already follow. */
    EnemyProjectileKind enemy_kind;
    float half_len;
    float half_wid;

    /* Enemy shots only: set by Cruzader's passive/orb (see
     * reflect_enemy_shot in usecases/game_logic.c) when this shot gets
     * bounced back instead of hitting the player - vx/vy are negated in
     * place and every other field (color, enemy_kind, half_len/half_wid) is
     * left untouched, so a reflected shot keeps flying with its exact
     * original design, just backwards. check_collisions then tests
     * reflected shots against gs->enemies/gs->boss instead of the player.
     * Reset to false whenever a slot is reused (see spawn_enemy_shot).
     * Unused by player shots. */
    bool reflected;

    /* Counts down to this shot's next smoke-trail puff emission - see
     * spawn_projectile_trail_particle/update_projectile_trails in
     * usecases/game_logic.c, the projectile counterpart to
     * Player.trail_emit_timer/Enemy.trail_emit_timer. Shared by both
     * player_shots and enemy_shots, at the same PROJECTILE_TRAIL_MAX_ALPHA
     * visibility and spawn cadence for every shot regardless of source -
     * except Cruzader's own mode 3 rockets, the one deliberate exception
     * (see ProjectileTrailParticle's own doc comment and
     * CRUZADER_ROCKET_TRAIL_SPAWN_INTERVAL). Purely cosmetic. */
    float trail_emit_timer;
    /* Random per-shot phase seed, radians [0, 2*pi) - set at spawn (see
     * spawn_player_shot). Read by C-24's own sphere-shot rendering
     * (draw_c24_sphere_shot in adapters/sdl_renderer.c, reached whenever
     * style_ship below is SHIP_C24): offsets that shot's own hue-cycling
     * phase so simultaneous shots - a double-barrel pair, all 8 of an omni
     * burst - don't cycle color in lockstep. Also read by Shine's own
     * PROJECTILE_KIND_SHINE_SPIRAL shot (draw_shine_shard) for the same
     * reason, offsetting its spin phase instead of a hue. Unused by every
     * other shot. */
    float phase_seed;
    /* Player shots only: which ship's rendering/behavior style this shot
     * uses (SHIP_B20 or SHIP_C24 - never SHIP_MOTHERSHIP, she never fires a
     * shot of her own). Set at spawn (see spawn_player_shot_styled) to
     * whichever ship actually produced the shot - GameState.selected_ship
     * for the real player's own fire, or a ChildShip's own `kind` for an
     * escort's fire. Needed because a C-24-style ChildShip can exist while
     * selected_ship is SHIP_MOTHERSHIP, so C-24's own special rendering/
     * hit-sizing/explosion-radius bonus (player_shot_half_extents,
     * draw_projectile, trigger_power_cannon_explosion) can't just read
     * GameState.selected_ship directly anymore - it has to ask the shot
     * itself. Unused (left 0/SHIP_B20, harmless) by enemy shots. */
    Ship style_ship;
} Projectile;

typedef struct Explosion {
    bool alive;
    float x, y;
    float age;
    float max_age;
    float max_radius;
} Explosion;

/* One puff of the player ship's engine exhaust - a soft dot that starts
 * fire-colored and cools into smoke as it ages (see draw_trail_particle),
 * spawned continuously from the back of the ship (update_player_trail) and
 * released to drift for TRAIL_PARTICLE_LIFETIME seconds. Purely cosmetic:
 * never collides with anything, never affects gameplay. */
typedef struct TrailParticle {
    bool alive;
    float x, y;
    float vx, vy;
    float age;
    float max_age;
    float size; /* base radius at spawn, already scaled by GameState.scale */
} TrailParticle;

/* The same fire/smoke exhaust as TrailParticle above, applied to enemies
 * and the boss instead of the player - see spawn_enemy_trail_particle and
 * update_enemy_and_boss_trails in usecases/game_logic.c. A separate pool
 * (rather than sharing the player's trail_particles) so a screen full of
 * enemies can never starve the player's own trail of slots, and so the
 * player's existing trail code stays completely untouched by this.
 * alpha_cap bakes in each source's max visibility at spawn time (~5% for
 * enemies, ~15% for the boss, see draw_enemy_trail_particle) so the
 * renderer doesn't need to know which kind of ship a particle came from,
 * just how to draw one. */
typedef struct EnemyTrailParticle {
    bool alive;
    float x, y;
    float vx, vy;
    float age;
    float max_age;
    float size;
    unsigned char alpha_cap;
} EnemyTrailParticle;

/* The same smoke-puff mechanics as TrailParticle/EnemyTrailParticle above
 * (drift, drag, grow, fade - see draw_projectile_trail_particle), trailing
 * every projectile - player and enemy shots alike - instead of a ship; see
 * spawn_projectile_trail_particle and update_projectile_trails in
 * usecases/game_logic.c. Unlike the ship trails, which start fire-colored
 * and cool into gray smoke as they age, color is captured once at spawn -
 * normally the exact Projectile.color that emitted it, so the trail reads
 * as "this projectile's own color," but Cruzader's own mode 3 rockets
 * (PROJECTILE_KIND_CRUZADER_ROCKET) are the one deliberate exception,
 * spawning theirs blue regardless of pr->color - and never shifts
 * afterward; only alpha (fade) and size (growth) animate over the puff's
 * life. alpha_cap (same "per-source max visibility" convention as
 * EnemyTrailParticle's own field above) is PROJECTILE_TRAIL_MAX_ALPHA for
 * every normal shot, and CRUZADER_ROCKET_TRAIL_MAX_ALPHA (much higher) for
 * a Cruzader rocket's own puffs only - "increase the visibility of the
 * smoke... make it blue" per feedback, scoped to that one shooting mode on
 * that one ship, nothing else. A single pool shared by both player_shots
 * and enemy_shots (each Projectile carries its own trail_emit_timer) since
 * every other projectile of either side still gets identical treatment. */
typedef struct ProjectileTrailParticle {
    bool alive;
    float x, y;
    float vx, vy;
    float age;
    float max_age;
    float size;
    Color color;
    unsigned char alpha_cap;
} ProjectileTrailParticle;

typedef struct Star {
    float x, y;
    float speed;
    unsigned char brightness;
} Star;

/* One drifting source of influence behind the game's pixelated background
 * smoke effect - see draw_background_smoke in adapters/sdl_renderer.c,
 * which shades a coarse grid of blocky cells light or dark depending on
 * how much combined influence from every cloud reaches each cell, plus a
 * static dithered noise pattern per cell, for a chunky retro look rather
 * than a smooth gradient. Never rendered as a shape of its own - it's
 * purely an input to that grid, so several clouds overlapping never reads
 * as "circles," only as denser (darker) smoke where they do.
 *
 * Drifts straight down at its own speed, plus a side-to-side wobble
 * (sinf(time_elapsed * wobble_speed + wobble_seed) * wobble_amplitude,
 * computed in the renderer from time_elapsed rather than an incrementally
 * updated phase stored here - the same "derive the animation from a fixed
 * per-instance seed plus the global clock" convention Projectile.phase_seed
 * uses for C-24's own hue cycling) so the whole cloud field keeps visibly
 * reshaping over time as clouds drift past and through each other, never
 * settling into one static silhouette. Wraps back above the screen once
 * fully past the bottom, same convention as Star's own y wrap in
 * update_stars, re-rolling every field at that point for variety over a
 * long run. */
typedef struct BackgroundCloud {
    float x, y;
    float radius;
    float speed; /* downward drift */
    float wobble_seed; /* radians; random per-cloud phase offset */
    float wobble_speed;
    float wobble_amplitude;
} BackgroundCloud;

/* A rare falling power-up. Captured by the player's ship it grants the
 * super beam; shot by the player's laser it just detonates. */
typedef struct Orb {
    bool alive;
    float x, y;
    float size;
    float hue; /* degrees, 0-360; drives the cycling gradient color below */
    float wobble_phase;
    Color color;
} Orb;

/* A recurring heavyweight encounter: a normal enemy's look scaled way up,
 * that has to be shot down over many hits instead of one, and threatens
 * the player by ramming it rather than shooting at it. It relentlessly
 * seeks the player's exact position - a game of tag, not a stationary
 * turret - so it never idles even if the player stops moving. If its
 * visible danger ring ever reaches the player, both explode instantly:
 * there is no health bar for that, only avoidance. */
typedef struct Boss {
    bool alive;
    float x, y;
    float size;
    int kind; /* index into adapters/enemy_sprites' kEnemySprites, [0, ENEMY_KIND_COUNT) */

    /* A running total of Projectile.damage landed so far, not a literal
     * shot count - float because some modes deal fractional multiples of
     * BASE_PLAYER_DAMAGE (see damage_boss). hits_required stays a whole
     * number: the size of the pool, tuned in units of one normal-mode hit. */
    float hits_taken;
    int hits_required;

    /* The super beam can still whittle the boss down over sustained
     * contact (unlike the player, it isn't fatal to it); this timer
     * paces those repeat hits. Breaking contact resets it so the next
     * beam touch deals damage instantly again. */
    float beam_contact_timer;

    /* Counts down to the boss's next engine trail particle emission - see
     * update_enemy_and_boss_trails. Purely cosmetic. */
    float trail_emit_timer;

    /* Counts down to the boss's next enemy dispatch (see
     * update_boss_dispatch/spawner_dispatch_enemy_from_boss) - reset to
     * spawner_boss_dispatch_interval(gs->boss_count) both at spawn (see
     * spawn_boss) and every time it fires, so the interval only ever
     * changes between encounters, never mid-fight. */
    float dispatch_timer;
} Boss;

typedef struct GameState {
    GameStateId state;
    PauseSelection pause_selection;
    /* The cursor on the difficulty-select screen (see
     * update_difficulty_select in usecases/game_logic.c) and, once
     * confirmed, the difficulty the run in progress was started at - kept
     * across STATE_MENU/STATE_DIFFICULTY_SELECT round trips (reset_run
     * deliberately never touches it) so the last choice is remembered
     * instead of resetting every time. Defaults to DIFFICULTY_NORMAL at
     * startup (see game_init). */
    Difficulty selected_difficulty;

    /* The cursor on the ship-select screen (see update_ship_select in
     * usecases/game_logic.c), reached right after confirming a difficulty,
     * and once confirmed, the ship the run in progress was started with -
     * kept across STATE_MENU/STATE_DIFFICULTY_SELECT/STATE_SHIP_SELECT
     * round trips (reset_run deliberately never touches it), same
     * "selection is the state" pattern as selected_difficulty above.
     * Defaults to SHIP_B20 at startup (see game_init). */
    Ship selected_ship;

    /* Real playfield size in pixels (matches the physical screen exactly -
     * see domain/constants.h) and the uniform factor every design-baseline
     * size/speed constant is multiplied by so shapes scale without
     * distortion. */
    int screen_w, screen_h;
    float scale;

    Player player;
    /* Only ever populated while selected_ship is SHIP_MOTHERSHIP - every
     * other ship has no way to spawn one (see
     * update_mothership_dispatch/update_children in usecases/game_logic.c). */
    ChildShip children[MOTHERSHIP_MAX_CHILDREN];
    Enemy enemies[MAX_ENEMIES];
    Projectile player_shots[MAX_PLAYER_PROJECTILES];
    Projectile enemy_shots[MAX_ENEMY_PROJECTILES];
    Explosion explosions[MAX_EXPLOSIONS];
    TrailParticle trail_particles[MAX_TRAIL_PARTICLES];
    EnemyTrailParticle enemy_trail_particles[MAX_ENEMY_TRAIL_PARTICLES];
    ProjectileTrailParticle projectile_trails[MAX_PROJECTILE_TRAIL_PARTICLES];
    Star stars[MAX_STARS];
    BackgroundCloud background_clouds[MAX_BACKGROUND_CLOUDS];
    Orb orb;
    Boss boss;
    int boss_count; /* how many bosses have appeared so far this run */
    /* How many bosses have actually been *defeated* so far this run (see
     * end_boss_encounter in usecases/game_logic.c, the single place either
     * of the two ways a boss can go down - shot down, or its own ring
     * detonating - both end up) - distinct from boss_count above, which
     * counts appearances. Drives ERRATIC_ENEMY_CHANCE_PER_BOSS_DEFEAT: 0%
     * of ordinary enemies fly an erratic pattern before the first defeat,
     * +10 percentage points after each one since. */
    int bosses_defeated;
    /* Points earned with the arena clear since the last boss encounter
     * ended. Frozen while a boss is alive and zeroed the moment one leaves,
     * so the next arrival always costs a full fresh BOSS_SCORE_STEP. */
    int score_since_last_boss;

    /* True for the BOSS_WARNING_SCORE_GAP point stretch immediately before
     * a boss arrives, false the instant one actually does - score_since_
     * last_boss resets to 0 in the very same frame spawn_boss sets
     * boss.alive true, so this never overlaps with boss.alive - and false
     * again the instant the boss leaves. Recomputed every frame by
     * update_boss_warning (usecases/game_logic.c) from score_since_last_
     * boss/boss.alive rather than stored independently by whichever
     * mutation site changes those, so it can never drift out of sync with
     * them. Consumed by the adapters: draw_stars (sdl_renderer.c) fades
     * the star field red while this is true, and app.c ORs it with
     * boss.alive when driving audio->update's boss track. */
    bool boss_warning;

    int score;
    int last_game_score;
    float time_elapsed;
    float spawn_timer;
    float menu_blink_timer;

    bool quit_requested;
} GameState;

#endif
