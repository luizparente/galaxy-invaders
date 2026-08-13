#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "domain/constants.h"
#include "domain/events.h"
#include "domain/types.h"
#include "ports/input_port.h"
#include "usecases/collision.h"
#include "usecases/difficulty.h"
#include "usecases/ship.h"
#include "usecases/spawner.h"
#include "usecases/game_logic.h"

/* These tests exercise only the usecases layer (pure game rules) directly
 * against domain structs - no SDL, no window, no audio device. That is the
 * entire point of keeping usecases dependency-free: the rules are testable
 * in isolation from every adapter. */

static InputCommand no_input(void) {
    return (InputCommand){0};
}

/* Tests run at exactly the DESIGN_W x DESIGN_H baseline so gs->scale
 * comes out to 1.0 and every design-baseline constant (PLAYER_WIDTH, etc.)
 * can be asserted against directly without carrying a scale factor. Three
 * confirms: the main menu leads to the difficulty-select screen
 * (STATE_DIFFICULTY_SELECT), which leads to the ship-select screen
 * (STATE_SHIP_SELECT), before the run actually starts - the second and
 * third confirms accept whatever difficulty/ship game_init defaults to
 * (DIFFICULTY_NORMAL/SHIP_B20), matching this suite's pre-existing
 * behavior/tuning assumptions since normal was also the game's original
 * single-tier default before difficulty levels existed, and every ship
 * multiplier is 1.0 at the B-20 baseline. */
static void start_game(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(gs, &confirm, 0.016f, events);
    game_update(gs, &confirm, 0.016f, events);
    game_update(gs, &confirm, 0.016f, events);
}

/* Same as start_game, but navigates to C-24 on the ship-select screen
 * before confirming, for the C-24-specific weapon tests below. */
static void start_game_as_c24(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_SHIP_SELECT */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(gs, &right, 0.016f, events); /* B-20 -> C-24 */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_GAME */
    assert(gs->selected_ship == SHIP_C24);
}

/* Same as start_game_as_c24, but navigates one slot further right to
 * SHIP_MOTHERSHIP, for the Mothership/ChildShip tests below. */
static void start_game_as_mothership(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_SHIP_SELECT */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(gs, &right, 0.016f, events); /* B-20 -> C-24 */
    game_update(gs, &right, 0.016f, events); /* C-24 -> SHIP_MOTHERSHIP */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_GAME */
    assert(gs->selected_ship == SHIP_MOTHERSHIP);
}

/* Same as start_game_as_mothership, but navigates one slot further right
 * to SHIP_SHINE, for Shine's own weapon tests below. */
static void start_game_as_shine(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_SHIP_SELECT */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(gs, &right, 0.016f, events); /* B-20 -> C-24 */
    game_update(gs, &right, 0.016f, events); /* C-24 -> SHIP_MOTHERSHIP */
    game_update(gs, &right, 0.016f, events); /* SHIP_MOTHERSHIP -> SHIP_SHINE */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_GAME */
    assert(gs->selected_ship == SHIP_SHINE);
}

/* Same as start_game_as_shine, but navigates one slot further right to
 * SHIP_CRUZADER, for Cruzader's own weapon tests below. */
static void start_game_as_cruzader(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_SHIP_SELECT */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(gs, &right, 0.016f, events); /* B-20 -> C-24 */
    game_update(gs, &right, 0.016f, events); /* C-24 -> SHIP_MOTHERSHIP */
    game_update(gs, &right, 0.016f, events); /* SHIP_MOTHERSHIP -> SHIP_SHINE */
    game_update(gs, &right, 0.016f, events); /* SHIP_SHINE -> SHIP_CRUZADER */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_GAME */
    assert(gs->selected_ship == SHIP_CRUZADER);
}

/* Same as start_game_as_cruzader, but navigates one slot further right to
 * SHIP_TWINS, for The Twins' own weapon/life tests below. */
static void start_game_as_twins(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_SHIP_SELECT */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(gs, &right, 0.016f, events); /* B-20 -> C-24 */
    game_update(gs, &right, 0.016f, events); /* C-24 -> SHIP_MOTHERSHIP */
    game_update(gs, &right, 0.016f, events); /* SHIP_MOTHERSHIP -> SHIP_SHINE */
    game_update(gs, &right, 0.016f, events); /* SHIP_SHINE -> SHIP_CRUZADER */
    game_update(gs, &right, 0.016f, events); /* SHIP_CRUZADER -> SHIP_TWINS */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_GAME */
    assert(gs->selected_ship == SHIP_TWINS);
}

/* Same as start_game_as_twins, but navigates one slot further right to
 * SHIP_ANTARTICA, for Antartica/Frosty's own weapon/life tests below. */
static void start_game_as_antartica(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_SHIP_SELECT */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(gs, &right, 0.016f, events); /* B-20 -> C-24 */
    game_update(gs, &right, 0.016f, events); /* C-24 -> SHIP_MOTHERSHIP */
    game_update(gs, &right, 0.016f, events); /* SHIP_MOTHERSHIP -> SHIP_SHINE */
    game_update(gs, &right, 0.016f, events); /* SHIP_SHINE -> SHIP_CRUZADER */
    game_update(gs, &right, 0.016f, events); /* SHIP_CRUZADER -> SHIP_TWINS */
    game_update(gs, &right, 0.016f, events); /* SHIP_TWINS -> SHIP_ANTARTICA */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_GAME */
    assert(gs->selected_ship == SHIP_ANTARTICA);
}

/* Same as start_game_as_antartica, but navigates one slot further right to
 * SHIP_BUCKLER, for Buckler's own weapon/orb tests below. */
static void start_game_as_buckler(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_SHIP_SELECT */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(gs, &right, 0.016f, events); /* B-20 -> C-24 */
    game_update(gs, &right, 0.016f, events); /* C-24 -> SHIP_MOTHERSHIP */
    game_update(gs, &right, 0.016f, events); /* SHIP_MOTHERSHIP -> SHIP_SHINE */
    game_update(gs, &right, 0.016f, events); /* SHIP_SHINE -> SHIP_CRUZADER */
    game_update(gs, &right, 0.016f, events); /* SHIP_CRUZADER -> SHIP_TWINS */
    game_update(gs, &right, 0.016f, events); /* SHIP_TWINS -> SHIP_ANTARTICA */
    game_update(gs, &right, 0.016f, events); /* SHIP_ANTARTICA -> SHIP_BUCKLER */
    game_update(gs, &confirm, 0.016f, events); /* -> STATE_GAME */
    assert(gs->selected_ship == SHIP_BUCKLER);
}

static void test_collision(void) {
    assert(collision_aabb_overlap(0, 0, 5, 5, 8, 0, 5, 5));
    assert(!collision_aabb_overlap(0, 0, 5, 5, 20, 0, 5, 5));
    assert(collision_aabb_overlap(0, 0, 5, 5, 0, 9, 5, 5));
    assert(!collision_aabb_overlap(0, 0, 5, 5, 0, 11, 5, 5));
    printf("test_collision OK\n");
}

static void test_difficulty(void) {
    float normal_multiplier = difficulty_spawn_rate_multiplier(DIFFICULTY_NORMAL, 0.0f);
    assert(fabsf(difficulty_spawn_interval(0, DIFFICULTY_NORMAL, 0.0f) - BASE_SPAWN_INTERVAL * normal_multiplier) < 0.001f);
    assert(difficulty_spawn_interval(5000, DIFFICULTY_NORMAL, 0.0f) < difficulty_spawn_interval(0, DIFFICULTY_NORMAL, 0.0f));
    assert(difficulty_spawn_interval(1000000, DIFFICULTY_NORMAL, 0.0f) >= MIN_SPAWN_INTERVAL * normal_multiplier - 0.001f);

    /* Easier difficulties spawn slower (bigger interval) than harder ones,
     * and the multiplier ramps down (faster) over time - see
     * SPAWN_RATE_RAMP_INTERVAL/STEP in domain/constants.h. */
    assert(difficulty_spawn_rate_multiplier(DIFFICULTY_BABY, 0.0f) > difficulty_spawn_rate_multiplier(DIFFICULTY_INSANE, 0.0f));
    assert(difficulty_spawn_rate_multiplier(DIFFICULTY_NORMAL, 600.0f) < difficulty_spawn_rate_multiplier(DIFFICULTY_NORMAL, 0.0f));
    assert(difficulty_spawn_rate_multiplier(DIFFICULTY_NORMAL, 1000000.0f) >= SPAWN_RATE_MULTIPLIER_MIN - 0.001f);

    /* Harder difficulties fire more often than easier ones, and the fire
     * chance ramps up over time - see FIRE_CHANCE_RAMP_INTERVAL/STEP. */
    assert(difficulty_enemy_fire_chance_per_sec(DIFFICULTY_INSANE, 0.0f) > difficulty_enemy_fire_chance_per_sec(DIFFICULTY_BABY, 0.0f));
    assert(difficulty_enemy_fire_chance_per_sec(DIFFICULTY_NORMAL, 600.0f) > difficulty_enemy_fire_chance_per_sec(DIFFICULTY_NORMAL, 0.0f));
    assert(difficulty_enemy_fire_chance_per_sec(DIFFICULTY_NORMAL, 1000000.0f) <= ENEMY_FIRE_CHANCE_MAX + 0.001f);

    assert(fabsf(difficulty_enemy_speed(0) - ENEMY_BASE_SPEED) < 0.001f);
    assert(difficulty_enemy_speed(1000000) <= ENEMY_MAX_SPEED + 0.001f);

    assert(fabsf(difficulty_score_multiplier(0) - 1.0f) < 0.001f);
    assert(fabsf(difficulty_score_multiplier(500) - 1.1f) < 0.001f);
    assert(fabsf(difficulty_score_multiplier(1200) - 1.2f) < 0.001f);
    printf("test_difficulty OK\n");
}

static void test_menu_start_transition(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    assert(gs.state == STATE_GAME);
    assert(gs.player.alive);
    assert(fabsf(gs.player.x - DESIGN_W / 2.0f) < 0.5f);
    printf("test_menu_start_transition OK\n");
}

static void test_menu_confirm_leads_to_difficulty_select(void) {
    GameState gs;
    EventQueue events;
    game_init(&gs, DESIGN_W, DESIGN_H);
    assert(gs.state == STATE_MENU);
    assert(gs.selected_difficulty == DIFFICULTY_NORMAL);

    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_DIFFICULTY_SELECT);
    assert(gs.selected_difficulty == DIFFICULTY_NORMAL); /* untouched until navigated */
    printf("test_menu_confirm_leads_to_difficulty_select OK\n");
}

static void test_difficulty_select_navigation_clamps_at_ends(void) {
    GameState gs;
    EventQueue events;
    game_init(&gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_DIFFICULTY_SELECT);

    InputCommand up = no_input();
    up.nav_up_pressed = true;
    for (int i = 0; i < 10; i++) game_update(&gs, &up, 0.016f, &events);
    assert(gs.selected_difficulty == DIFFICULTY_BABY); /* clamped, doesn't wrap past the top */

    InputCommand down = no_input();
    down.nav_down_pressed = true;
    for (int i = 0; i < 10; i++) game_update(&gs, &down, 0.016f, &events);
    assert(gs.selected_difficulty == DIFFICULTY_INSANE); /* clamped, doesn't wrap past the bottom */
    printf("test_difficulty_select_navigation_clamps_at_ends OK\n");
}

static void test_difficulty_select_back_returns_to_menu_without_starting(void) {
    GameState gs;
    EventQueue events;
    game_init(&gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_DIFFICULTY_SELECT);

    InputCommand esc = no_input();
    esc.back_pressed = true;
    game_update(&gs, &esc, 0.016f, &events);
    assert(gs.state == STATE_MENU);
    printf("test_difficulty_select_back_returns_to_menu_without_starting OK\n");
}

static void test_difficulty_select_confirm_leads_to_ship_select(void) {
    GameState gs;
    EventQueue events;
    game_init(&gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_DIFFICULTY_SELECT);

    InputCommand down = no_input();
    down.nav_down_pressed = true;
    game_update(&gs, &down, 0.016f, &events); /* NORMAL -> HARD */
    assert(gs.selected_difficulty == DIFFICULTY_HARD);

    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_SHIP_SELECT); /* not started yet - ship still needs picking */
    assert(gs.selected_difficulty == DIFFICULTY_HARD); /* carried over, untouched */
    assert(gs.selected_ship == SHIP_B20); /* untouched until navigated */
    printf("test_difficulty_select_confirm_leads_to_ship_select OK\n");
}

static void test_ship_select_navigation_clamps_at_ends(void) {
    GameState gs;
    EventQueue events;
    game_init(&gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(&gs, &confirm, 0.016f, &events); /* -> STATE_SHIP_SELECT */
    assert(gs.state == STATE_SHIP_SELECT);

    InputCommand left = no_input();
    left.nav_left_pressed = true;
    for (int i = 0; i < 10; i++) game_update(&gs, &left, 0.016f, &events);
    assert(gs.selected_ship == SHIP_B20); /* clamped, doesn't go negative */

    InputCommand right = no_input();
    right.nav_right_pressed = true;
    for (int i = 0; i < 10; i++) game_update(&gs, &right, 0.016f, &events);
    assert(gs.selected_ship == SHIP_BUCKLER); /* clamped at the last implemented ship */
    printf("test_ship_select_navigation_clamps_at_ends OK\n");
}

static void test_ship_select_back_returns_to_difficulty_select(void) {
    GameState gs;
    EventQueue events;
    game_init(&gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events);
    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_SHIP_SELECT);

    InputCommand esc = no_input();
    esc.back_pressed = true;
    game_update(&gs, &esc, 0.016f, &events);
    assert(gs.state == STATE_DIFFICULTY_SELECT);
    printf("test_ship_select_back_returns_to_difficulty_select OK\n");
}

static void test_ship_select_confirm_starts_game_with_chosen_ship(void) {
    GameState gs;
    EventQueue events;
    game_init(&gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events);
    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_SHIP_SELECT);

    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(&gs, &right, 0.016f, &events); /* B-20 -> C-24 */
    assert(gs.selected_ship == SHIP_C24);

    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_GAME);
    assert(gs.selected_ship == SHIP_C24); /* the run keeps the chosen ship */
    printf("test_ship_select_confirm_starts_game_with_chosen_ship OK\n");
}

static void test_ship_ratings_and_multipliers(void) {
    assert(ship_speed_rating(SHIP_B20) == 7);
    assert(ship_strength_rating(SHIP_B20) == 5);
    assert(ship_attack_rating(SHIP_B20) == 8);
    assert(ship_speed_rating(SHIP_C24) == 5);
    assert(ship_strength_rating(SHIP_C24) == 7);
    assert(ship_attack_rating(SHIP_C24) == 7);

    /* B-20 is the tuning baseline: both its multipliers are always 1.0. */
    assert(fabsf(ship_speed_multiplier(SHIP_B20) - 1.0f) < 0.001f);
    assert(fabsf(ship_damage_taken_multiplier(SHIP_B20) - 1.0f) < 0.001f);

    /* C-24 is slower (lower Speed rating than B-20) but tougher (higher
     * Strength rating - takes proportionally less life loss per hit, so
     * it can absorb more hits before life reaches 0). */
    assert(ship_speed_multiplier(SHIP_C24) < 1.0f);
    assert(ship_damage_taken_multiplier(SHIP_C24) < 1.0f);
    assert(fabsf(ship_speed_multiplier(SHIP_C24) - 5.0f / 7.0f) < 0.001f);
    assert(fabsf(ship_damage_taken_multiplier(SHIP_C24) - 5.0f / 7.0f) < 0.001f);

    /* Pinned to the exact figures from the generalized formulas documented
     * in usecases/ship.h - Speed is a direct proportion of B-20's rating
     * (100% at Speed 7), Strength an inverse proportion of B-20's own
     * life-loss-per-hit (10% at Strength 5). Locking these exact numbers
     * down (not just "less than B-20") is what guarantees a tougher-rated
     * ship can never end up taking *more* damage per hit than a
     * weaker-rated one - the bug this test suite exists to catch. */
    assert(fabsf(ship_speed_percent(SHIP_B20) - 100.0f) < 0.001f);
    assert(fabsf(ship_life_loss_percent_per_hit(SHIP_B20) - 10.0f) < 0.001f);
    assert(fabsf(ship_speed_percent(SHIP_C24) - 500.0f / 7.0f) < 0.001f);
    assert(fabsf(ship_life_loss_percent_per_hit(SHIP_C24) - 50.0f / 7.0f) < 0.001f);
    assert(ship_life_loss_percent_per_hit(SHIP_C24) < ship_life_loss_percent_per_hit(SHIP_B20));
    printf("test_ship_ratings_and_multipliers OK\n");
}

/* Integration-level counterpart to test_ship_ratings_and_multipliers: picking
 * C-24 instead of the default B-20 must actually move update_player and
 * damage_player, not just change what usecases/ship.c reports in isolation. */
static void test_selected_ship_affects_speed_and_damage_taken(void) {
    GameState gs_b20, gs_c24;
    EventQueue events;
    start_game(&gs_b20, &events); /* defaults to SHIP_B20 */

    game_init(&gs_c24, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs_c24, &confirm, 0.016f, &events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(&gs_c24, &confirm, 0.016f, &events); /* -> STATE_SHIP_SELECT */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(&gs_c24, &right, 0.016f, &events); /* B-20 -> C-24 */
    game_update(&gs_c24, &confirm, 0.016f, &events); /* -> STATE_GAME */
    assert(gs_c24.selected_ship == SHIP_C24);

    float start_x_b20 = gs_b20.player.x, start_x_c24 = gs_c24.player.x;
    InputCommand move_right = no_input();
    move_right.move_right = true;
    game_update(&gs_b20, &move_right, 0.1f, &events);
    game_update(&gs_c24, &move_right, 0.1f, &events);
    float moved_b20 = gs_b20.player.x - start_x_b20;
    float moved_c24 = gs_c24.player.x - start_x_c24;
    assert(moved_b20 > 0.0f && moved_c24 > 0.0f);
    assert(moved_c24 < moved_b20); /* C-24's lower Speed rating flies slower */

    float life_b20 = gs_b20.player.life, life_c24 = gs_c24.player.life;
    gs_b20.enemy_shots[0] = (Projectile){.alive = true, .x = gs_b20.player.x, .y = gs_b20.player.y};
    gs_c24.enemy_shots[0] = (Projectile){.alive = true, .x = gs_c24.player.x, .y = gs_c24.player.y};
    InputCommand idle = no_input();
    game_update(&gs_b20, &idle, 0.001f, &events);
    game_update(&gs_c24, &idle, 0.001f, &events);
    float lost_b20 = life_b20 - gs_b20.player.life;
    float lost_c24 = life_c24 - gs_c24.player.life;
    assert(lost_b20 > 0.0f && lost_c24 > 0.0f);
    assert(lost_c24 < lost_b20); /* C-24's higher Strength rating absorbs more hits */
    /* Pinned to the exact formula in usecases/ship.h, through the real
     * damage_player code path (not just the pure ship_* functions in
     * isolation) - B-20 loses exactly PLAYER_LIFE_LOSS_PER_HIT, C-24 loses
     * exactly 5/7 of it. */
    assert(fabsf(lost_b20 - PLAYER_LIFE_LOSS_PER_HIT) < 0.01f);
    assert(fabsf(lost_c24 - PLAYER_LIFE_LOSS_PER_HIT * 5.0f / 7.0f) < 0.01f);
    printf("test_selected_ship_affects_speed_and_damage_taken OK\n");
}

static void test_player_movement_clamped(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    InputCommand left = no_input();
    left.move_left = true;
    for (int i = 0; i < 1000; i++) game_update(&gs, &left, 0.1f, &events);

    assert(fabsf(gs.player.x - PLAYER_WIDTH / 2.0f) < 0.01f);
    printf("test_player_movement_clamped OK\n");
}

static void test_player_can_reach_top_of_screen(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    InputCommand up = no_input();
    up.move_up = true;
    for (int i = 0; i < 1000; i++) {
        game_update(&gs, &up, 0.1f, &events);
        /* Isolate the movement clamp from the spawner: flying straight up
         * through the whole vertical band for 100 simulated seconds will
         * otherwise run into a spawned enemy sooner or later and end the
         * run before the player ever reaches the top. */
        for (int j = 0; j < MAX_ENEMIES; j++) gs.enemies[j].alive = false;
        for (int j = 0; j < MAX_ENEMY_PROJECTILES; j++) gs.enemy_shots[j].alive = false;
    }

    /* The player must be able to fly all the way to the top edge, not
     * just up to the lower ~55% of the screen. */
    assert(fabsf(gs.player.y - PLAYER_HEIGHT / 2.0f) < 0.01f);
    printf("test_player_can_reach_top_of_screen OK\n");
}

static void test_super_beam_increases_player_speed(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    InputCommand right = no_input();
    right.move_right = true;

    float x0 = gs.player.x;
    game_update(&gs, &right, 0.05f, &events);
    float normal_delta = gs.player.x - x0;

    gs.player.x = x0;
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;
    game_update(&gs, &right, 0.05f, &events);
    float boosted_delta = gs.player.x - x0;

    assert(boosted_delta > normal_delta);
    assert(fabsf(boosted_delta / normal_delta - SUPER_BEAM_SPEED_MULTIPLIER) < 0.01f);
    printf("test_super_beam_increases_player_speed OK\n");
}

static void test_player_fire_cooldown(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    int alive_count = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) alive_count++;
    }
    assert(alive_count == 1);

    game_update(&gs, &fire, 0.016f, &events); /* still inside the fire cooldown */
    alive_count = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) alive_count++;
    }
    assert(alive_count == 1);
    printf("test_player_fire_cooldown OK\n");
}

static void test_pause_toggle(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    assert(gs.state == STATE_GAME);

    InputCommand esc = no_input();
    esc.back_pressed = true;
    game_update(&gs, &esc, 0.016f, &events);
    assert(gs.state == STATE_PAUSE);

    game_update(&gs, &esc, 0.016f, &events); /* ESC again resumes */
    assert(gs.state == STATE_GAME);
    printf("test_pause_toggle OK\n");
}

static void test_pause_menu_exit_to_menu(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    InputCommand esc = no_input();
    esc.back_pressed = true;
    game_update(&gs, &esc, 0.016f, &events);
    assert(gs.state == STATE_PAUSE);
    assert(gs.pause_selection == PAUSE_RESUME);

    InputCommand nav = no_input();
    nav.nav_down_pressed = true;
    game_update(&gs, &nav, 0.016f, &events);
    assert(gs.pause_selection == PAUSE_EXIT);

    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events);
    assert(gs.state == STATE_MENU);
    printf("test_pause_menu_exit_to_menu OK\n");
}

static void test_enemy_kill_scores(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y - PLAYER_HEIGHT / 2.0f - PLAYER_PROJECTILE_H / 2.0f;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    gs.player_shots[0].alive = true;
    gs.player_shots[0].x = gs.enemies[0].x;
    gs.player_shots[0].y = gs.enemies[0].y;
    gs.player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.enemies[0].alive);
    assert(gs.score == SCORE_PER_KILL);
    printf("test_enemy_kill_scores OK\n");
}

static void test_player_enemy_collision_ends_game(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.player.alive);
    assert(gs.state == STATE_GAME_OVER);
    printf("test_player_enemy_collision_ends_game OK\n");
}

static void kill_one_enemy(GameState *gs, EventQueue *events) {
    gs->enemies[0].alive = true;
    gs->enemies[0].x = gs->player.x;
    gs->enemies[0].y = gs->player.y - PLAYER_HEIGHT / 2.0f - PLAYER_PROJECTILE_H / 2.0f;
    gs->enemies[0].size = 20.0f;
    gs->enemies[0].fire_timer = 999.0f;

    gs->player_shots[0].alive = true;
    gs->player_shots[0].x = gs->enemies[0].x;
    gs->player_shots[0].y = gs->enemies[0].y;
    gs->player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;

    InputCommand none = no_input();
    game_update(gs, &none, 0.001f, events);
}

static bool colors_equal(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

/* Regression coverage for a real bug: the laser color used to reroll to a
 * random hue every 200 points (see the old LASER_COLOR_SCORE_STEP), which
 * read as "the projectile suddenly changed" mid-game with no obvious
 * trigger - especially disorienting right as a Super Beam's kill spree
 * often crossed that threshold. The player's laser must now stay exactly
 * the same color for the whole run, no matter how much score is earned. */
static void test_laser_color_never_changes_regardless_of_score(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    Color initial = gs.player.laser_color;

    /* Rack up several thousand points - comfortably past where the old
     * reroll-every-200 behavior would have changed color more than a
     * dozen times - and confirm the color never once moves. */
    for (int kill = 1; kill <= 40; kill++) {
        kill_one_enemy(&gs, &events);
        assert(colors_equal(gs.player.laser_color, initial));
    }
    assert(gs.score >= 400);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) assert(colors_equal(gs.player_shots[i].color, initial));
    }

    printf("test_laser_color_never_changes_regardless_of_score OK\n");
}

static void test_orb_capture_grants_super_beam(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    gs.orb.alive = true;
    gs.orb.x = gs.player.x;
    gs.orb.y = gs.player.y;
    gs.orb.size = 20.0f;

    assert(gs.player.super_beam_timer <= 0.0f);

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.orb.alive);
    assert(fabsf(gs.player.super_beam_timer - SUPER_BEAM_DURATION) < 0.01f);
    printf("test_orb_capture_grants_super_beam OK\n");
}

static void test_player_invincible_during_super_beam(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;

    /* Direct physical contact with an enemy would normally be fatal. */
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    assert(!gs.enemies[0].alive); /* still destroyed on contact, just harmless to the ship */

    /* An enemy projectile would normally be fatal too. */
    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = gs.player.x;
    gs.enemy_shots[0].y = gs.player.y;
    gs.enemy_shots[0].vy = 0.0f;

    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    assert(!gs.enemy_shots[0].alive);

    printf("test_player_invincible_during_super_beam OK\n");
}

static void test_god_mode_toggles_on_and_off(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    assert(!gs.player.god_mode);

    /* god_mode_toggle_pressed is edge-triggered by contract (the input
     * adapter debounces it, see adapters/sdl_input.c); game_logic just
     * flips state whenever it sees the flag true, so each call here
     * represents one distinct key-press edge, not a held key. */
    InputCommand toggle = no_input();
    toggle.god_mode_toggle_pressed = true;
    game_update(&gs, &toggle, 0.016f, &events);
    assert(gs.player.god_mode);

    InputCommand none = no_input();
    game_update(&gs, &none, 0.016f, &events);
    assert(gs.player.god_mode); /* stays on with no further presses */

    game_update(&gs, &toggle, 0.016f, &events);
    assert(!gs.player.god_mode); /* second press edge turns it back off */

    printf("test_god_mode_toggles_on_and_off OK\n");
}

static void test_god_mode_prevents_death(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    InputCommand toggle = no_input();
    toggle.god_mode_toggle_pressed = true;
    game_update(&gs, &toggle, 0.016f, &events);
    assert(gs.player.god_mode);

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    assert(!gs.enemies[0].alive);

    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = gs.player.x;
    gs.enemy_shots[0].y = gs.player.y;
    gs.enemy_shots[0].vy = 0.0f;

    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    assert(!gs.enemy_shots[0].alive);

    printf("test_god_mode_prevents_death OK\n");
}

static void test_player_starts_at_full_life(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    assert(fabsf(gs.player.life - PLAYER_LIFE_MAX) < 0.01f);
    printf("test_player_starts_at_full_life OK\n");
}

static void shoot_player_once(GameState *gs, EventQueue *events) {
    gs->enemy_shots[0] = (Projectile){0};
    gs->enemy_shots[0].alive = true;
    gs->enemy_shots[0].x = gs->player.x;
    gs->enemy_shots[0].y = gs->player.y;
    gs->enemy_shots[0].vy = 0.0f;

    InputCommand none = no_input();
    game_update(gs, &none, 0.001f, events);
}

static void test_enemy_projectile_hit_drains_life_without_killing(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    shoot_player_once(&gs, &events);

    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    assert(!gs.enemy_shots[0].alive);
    assert(fabsf(gs.player.life - (PLAYER_LIFE_MAX - PLAYER_LIFE_LOSS_PER_HIT)) < 0.01f);
    printf("test_enemy_projectile_hit_drains_life_without_killing OK\n");
}

static void test_enemy_projectile_hits_exhaust_life_and_kill_player(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    int hits_to_kill = (int)(PLAYER_LIFE_MAX / PLAYER_LIFE_LOSS_PER_HIT);
    for (int hit = 1; hit <= hits_to_kill; hit++) {
        shoot_player_once(&gs, &events);
        if (hit < hits_to_kill) {
            assert(gs.player.alive);
            assert(gs.state == STATE_GAME);
        }
    }

    assert(!gs.player.alive);
    assert(gs.player.life <= 0.0f);
    assert(gs.state == STATE_GAME_OVER);
    printf("test_enemy_projectile_hits_exhaust_life_and_kill_player OK\n");
}

static void test_enemy_ship_contact_kills_player_regardless_of_life(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    /* Life is still nearly full - contact must be instantly fatal anyway,
     * unlike a projectile hit which only drains it. */
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.player.alive);
    assert(gs.player.life <= 0.0f);
    assert(gs.state == STATE_GAME_OVER);
    printf("test_enemy_ship_contact_kills_player_regardless_of_life OK\n");
}

static void test_orb_capture_refills_life_to_full(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    shoot_player_once(&gs, &events);
    assert(gs.player.life < PLAYER_LIFE_MAX);

    gs.orb.alive = true;
    gs.orb.x = gs.player.x;
    gs.orb.y = gs.player.y;
    gs.orb.size = 20.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.orb.alive);
    assert(fabsf(gs.player.life - PLAYER_LIFE_MAX) < 0.01f);
    printf("test_orb_capture_refills_life_to_full OK\n");
}

static void test_god_mode_and_super_beam_prevent_life_loss(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;

    shoot_player_once(&gs, &events);
    assert(fabsf(gs.player.life - PLAYER_LIFE_MAX) < 0.01f);

    gs.player.super_beam_timer = 0.0f;
    InputCommand toggle = no_input();
    toggle.god_mode_toggle_pressed = true;
    game_update(&gs, &toggle, 0.016f, &events);
    assert(gs.player.god_mode);

    shoot_player_once(&gs, &events);
    assert(fabsf(gs.player.life - PLAYER_LIFE_MAX) < 0.01f);

    printf("test_god_mode_and_super_beam_prevent_life_loss OK\n");
}

static void test_super_beam_neutralizes_without_normal_fire(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y - 100.0f;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = gs.player.x;
    gs.enemy_shots[0].y = gs.player.y - 50.0f;
    gs.enemy_shots[0].vy = 0.0f;

    InputCommand fire = no_input();
    fire.fire_held = true;
    int score_before = gs.score;
    game_update(&gs, &fire, 0.016f, &events);

    assert(!gs.enemies[0].alive);
    assert(!gs.enemy_shots[0].alive);
    assert(gs.score > score_before);
    assert(gs.player.super_beam_timer > 0.0f && gs.player.super_beam_timer < SUPER_BEAM_DURATION);

    int alive_player_shots = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) alive_player_shots++;
    }
    assert(alive_player_shots == 0);
    printf("test_super_beam_neutralizes_without_normal_fire OK\n");
}

static void test_shooting_orb_schedules_enemies_without_granting_beam(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    gs.orb.alive = true;
    gs.orb.x = gs.player.x;
    gs.orb.y = gs.player.y - PLAYER_HEIGHT / 2.0f - PLAYER_PROJECTILE_H / 2.0f;
    gs.orb.size = 20.0f;

    gs.player_shots[0].alive = true;
    gs.player_shots[0].x = gs.orb.x;
    gs.player_shots[0].y = gs.orb.y;
    gs.player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;

    /* Scattered across the screen - distance from the orb no longer
     * matters, every enemy alive at the instant of the shot should be
     * scheduled, not just ones "nearby". */
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.orb.x + 40.0f;
    gs.enemies[0].y = gs.orb.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    gs.enemies[1].alive = true;
    gs.enemies[1].x = 30.0f;
    gs.enemies[1].y = 30.0f;
    gs.enemies[1].size = 20.0f;
    gs.enemies[1].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.orb.alive);
    assert(gs.player.super_beam_timer <= 0.0f);
    /* Both are scheduled immediately; whether either has already
     * detonated this same frame is down to its own random delay, so only
     * the "pending" bookkeeping is safe to assert here. */
    assert(gs.enemies[0].orb_kill_pending);
    assert(gs.enemies[1].orb_kill_pending);

    /* An enemy that shows up only after the shot must not be swept up by
     * the pending detonation - it wasn't on screen at the time. */
    gs.enemies[2].alive = true;
    gs.enemies[2].x = 100.0f;
    gs.enemies[2].y = 100.0f;
    gs.enemies[2].size = 20.0f;
    gs.enemies[2].fire_timer = 999.0f;

    /* By the end of the window every enemy that was scheduled must have
     * exploded. */
    game_update(&gs, &none, ORB_SHOT_EXPLOSION_WINDOW + 0.1f, &events);

    assert(!gs.enemies[0].alive);
    assert(!gs.enemies[1].alive);
    assert(gs.enemies[2].alive);
    printf("test_shooting_orb_schedules_enemies_without_granting_beam OK\n");
}

static void test_shooting_orb_assigns_random_delay_within_window(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    gs.orb.alive = true;
    gs.orb.x = gs.player.x;
    gs.orb.y = gs.player.y - PLAYER_HEIGHT / 2.0f - PLAYER_PROJECTILE_H / 2.0f;
    gs.orb.size = 20.0f;

    gs.player_shots[0].alive = true;
    gs.player_shots[0].x = gs.orb.x;
    gs.player_shots[0].y = gs.orb.y;
    gs.player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;

    for (int i = 0; i < 5; i++) {
        gs.enemies[i].alive = true;
        gs.enemies[i].x = 20.0f + (float)i * 30.0f;
        gs.enemies[i].y = 20.0f + (float)i * 30.0f;
        gs.enemies[i].size = 20.0f;
        gs.enemies[i].fire_timer = 999.0f;
    }

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    for (int i = 0; i < 5; i++) {
        assert(gs.enemies[i].orb_kill_pending);
        assert(gs.enemies[i].orb_kill_timer >= 0.0f);
        assert(gs.enemies[i].orb_kill_timer <= ORB_SHOT_EXPLOSION_WINDOW);
    }

    printf("test_shooting_orb_assigns_random_delay_within_window OK\n");
}

static void test_orb_falls_off_screen_when_uncaptured(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    gs.orb.alive = true;
    gs.orb.x = (float)gs.screen_w / 2.0f;
    gs.orb.y = 100.0f;
    gs.orb.size = 20.0f;
    gs.orb.hue = 0.0f;
    gs.orb.wobble_phase = 0.0f;

    /* Keep the player well clear so it can't accidentally capture the orb
     * while it drifts and falls. */
    gs.player.x = 10.0f;

    InputCommand none = no_input();
    for (int i = 0; i < 400 && gs.orb.alive; i++) {
        game_update(&gs, &none, 0.1f, &events);
        /* This test isolates the orb's own fall-off-bottom behavior; clear
         * any enemies the spawner produced over this simulated 40s so a
         * stray one can't end the run (STATE_GAME_OVER would freeze the
         * orb update entirely, which is a different behavior to test). */
        for (int j = 0; j < MAX_ENEMIES; j++) gs.enemies[j].alive = false;
        for (int j = 0; j < MAX_ENEMY_PROJECTILES; j++) gs.enemy_shots[j].alive = false;
    }

    assert(!gs.orb.alive);
    assert(gs.player.super_beam_timer <= 0.0f);
    printf("test_orb_falls_off_screen_when_uncaptured OK\n");
}

static void test_orb_spawn_chance_is_not_always_or_never(void) {
    int spawned = 0, not_spawned = 0;
    for (int trial = 0; trial < 30; trial++) {
        GameState gs;
        EventQueue events;
        start_game(&gs, &events);
        for (int kill = 0; kill < 20; kill++) kill_one_enemy(&gs, &events); /* 20 * 10 = exactly 200 */
        if (gs.orb.alive) spawned++;
        else not_spawned++;
    }
    /* With p=0.5 over 30 independent trials, getting all-same is
     * astronomically unlikely (~2 * 0.5^30) - this is a real assertion
     * on the branch, not a coin flip in the test itself. */
    assert(spawned > 0);
    assert(not_spawned > 0);
    printf("test_orb_spawn_chance_is_not_always_or_never OK\n");
}

static void shoot_boss_with_damage(GameState *gs, EventQueue *events, float damage) {
    /* The boss slides in gradually from off-screen (y = -size) when it
     * spawns, same as it would in a real run - a shot fired at it while
     * it's still off-screen would get culled by the ordinary "projectile
     * left the screen" cleanup before ever reaching the boss, just like
     * it would for any other off-screen target. Move it into view first
     * so this represents the player actually engaging a visible boss. */
    if (gs->boss.y < 0.0f) gs->boss.y = (float)gs->screen_h * 0.3f;

    gs->player_shots[0].alive = true;
    gs->player_shots[0].x = gs->boss.x;
    gs->player_shots[0].y = gs->boss.y;
    gs->player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;
    gs->player_shots[0].damage = damage;

    InputCommand none = no_input();
    game_update(gs, &none, 0.001f, events);
}

static void shoot_boss_once(GameState *gs, EventQueue *events) {
    shoot_boss_with_damage(gs, events, BASE_PLAYER_DAMAGE);
}

/* Fast-forwards straight to "one hit away from dead" and lands the final
 * blow, so multi-boss progression tests don't need 50-150 real shots. */
static void defeat_current_boss(GameState *gs, EventQueue *events) {
    assert(gs->boss.alive);
    gs->boss.hits_taken = (float)gs->boss.hits_required - 1.0f;
    shoot_boss_once(gs, events);
    assert(!gs->boss.alive);
}

static void test_boss_spawns_at_500_points_with_correct_hits_required(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    assert(!gs.boss.alive);
    assert(gs.boss_count == 0);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);

    assert(gs.score == 500);
    assert(gs.boss.alive);
    assert(gs.boss_count == 1);
    assert(gs.boss.hits_required == BOSS_HITS_INCREMENT);
    assert(gs.boss.hits_taken == 0);
    printf("test_boss_spawns_at_500_points_with_correct_hits_required OK\n");
}

/* Kills enemies until the next boss threshold is crossed. Doesn't assert
 * an exact score: above 500 points the existing score-multiplier system
 * (usecases/difficulty.c) makes kills worth more than SCORE_PER_KILL, so
 * points no longer land on round numbers - only the boss's own arrival
 * and hit-requirement scaling are this test's concern. */
static void kill_enemies_until_boss_spawns(GameState *gs, EventQueue *events) {
    for (int i = 0; i < 300 && !gs->boss.alive; i++) {
        kill_one_enemy(gs, events);
    }
    assert(gs->boss.alive);
}

/* gs->boss_warning drives the red star fade (draw_stars) and the early
 * boss-track start (app.c) for the BOSS_WARNING_SCORE_GAP-point stretch
 * right before a boss arrives. During this first encounter the score
 * multiplier is still exactly 1.0x (score stays under SCORE_MULTIPLIER_STEP
 * the whole way), so each kill is worth exactly SCORE_PER_KILL and the
 * 450-point gap boundary lands on the 45th kill precisely. */
static void test_boss_warning_activates_50_points_before_boss(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    assert(!gs.boss_warning);
    assert(!gs.boss.alive);

    for (int kill = 0; kill < 44; kill++) kill_one_enemy(&gs, &events);
    assert(gs.score_since_last_boss == 440);
    assert(!gs.boss_warning); /* one kill short of the gap */

    kill_one_enemy(&gs, &events); /* 45th kill: crosses into the last 50 points */
    assert(gs.score_since_last_boss == 450);
    assert(gs.boss_warning);
    assert(!gs.boss.alive); /* warning, not arrival, at exactly 450 */

    printf("test_boss_warning_activates_50_points_before_boss OK\n");
}

/* The warning must hold steady for the whole 450-499 stretch (no
 * flickering back off mid-gap) and must be false the instant boss.alive
 * flips true - the two flags never overlap, since score_since_last_boss
 * resets to 0 in the very same spawn_boss call that sets boss.alive (see
 * update_boss_warning in usecases/game_logic.c). */
static void test_boss_warning_holds_through_the_gap_then_hands_off_to_boss_alive(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 45; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss_warning && !gs.boss.alive);

    for (int kill = 45; kill < 49; kill++) {
        kill_one_enemy(&gs, &events);
        assert(gs.boss_warning);
        assert(!gs.boss.alive);
    }
    assert(gs.score_since_last_boss == 490);

    kill_one_enemy(&gs, &events); /* 50th kill: the boss actually arrives */
    assert(gs.boss.alive);
    assert(!gs.boss_warning);
    assert(gs.score_since_last_boss == 0);

    printf("test_boss_warning_holds_through_the_gap_then_hands_off_to_boss_alive OK\n");
}

/* Both ways a boss can go down (shot dead here, or ring-detonated in
 * test_boss_warning_stays_clear_after_ring_detonation below) end through
 * the single end_boss_encounter - confirm boss_warning comes back false,
 * not true, the instant it fires: a freshly zeroed score_since_last_boss
 * is nowhere near the next warning gap. */
static void test_boss_warning_stays_clear_after_boss_defeat(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);
    assert(!gs.boss_warning);

    defeat_current_boss(&gs, &events);

    assert(!gs.boss.alive);
    assert(!gs.boss_warning);
    assert(gs.score_since_last_boss == 0);
    printf("test_boss_warning_stays_clear_after_boss_defeat OK\n");
}

/* Same check as above, but through the other defeat route: the boss's own
 * menace ring touching the player (see check_collisions), which detonates
 * the boss unconditionally even when god mode keeps the player alive. */
static void test_boss_warning_stays_clear_after_ring_detonation(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);
    gs.player.god_mode = true; /* survive the ring touch so the run keeps going */
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.boss.alive);
    assert(!gs.boss_warning);
    assert(gs.score_since_last_boss == 0);
    printf("test_boss_warning_stays_clear_after_ring_detonation OK\n");
}

/* Confirms the warning re-arms cleanly for a second encounter rather than
 * only working the first time through a run - deliberately doesn't assume
 * round-number scoring (the defeat bonus can push gs.score past
 * SCORE_MULTIPLIER_STEP, making later kills worth more than SCORE_PER_KILL),
 * so it just drives kills until one of the two flags goes true. */
static void test_boss_warning_re_arms_for_the_second_encounter(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    defeat_current_boss(&gs, &events);
    assert(!gs.boss.alive);
    assert(!gs.boss_warning);
    assert(gs.score_since_last_boss == 0);

    for (int i = 0; i < 300 && !gs.boss_warning && !gs.boss.alive; i++) {
        kill_one_enemy(&gs, &events);
    }

    assert(gs.boss_warning);
    assert(!gs.boss.alive);
    printf("test_boss_warning_re_arms_for_the_second_encounter OK\n");
}

/* Pins spawner_boss_dispatch_interval's exact formula down directly, same
 * "deterministic formula, not sampled" style as
 * test_erratic_enemy_chance_scales_with_bosses_defeated does for its own
 * pure function: 3.0s for the very first encounter, 0.5s shorter per
 * encounter since, floored at 1.0s. */
static void test_boss_dispatch_interval_formula(void) {
    assert(fabsf(spawner_boss_dispatch_interval(1) - 3.0f) < 0.001f);
    assert(fabsf(spawner_boss_dispatch_interval(2) - 2.5f) < 0.001f);
    assert(fabsf(spawner_boss_dispatch_interval(3) - 2.0f) < 0.001f);
    assert(fabsf(spawner_boss_dispatch_interval(4) - 1.5f) < 0.001f);
    assert(fabsf(spawner_boss_dispatch_interval(5) - 1.0f) < 0.001f);
    assert(fabsf(spawner_boss_dispatch_interval(6) - 1.0f) < 0.001f); /* floor holds */
    assert(fabsf(spawner_boss_dispatch_interval(20) - 1.0f) < 0.001f);
    printf("test_boss_dispatch_interval_formula OK\n");
}

/* Confirms Boss.dispatch_timer is actually seeded from
 * spawner_boss_dispatch_interval(gs->boss_count) at spawn, and that the
 * interval gets shorter each encounter (down to the floor), across a full
 * run of encounters rather than just checking the pure function in
 * isolation above. */
static void test_boss_dispatch_interval_shortens_across_encounters(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    float expected[] = {3.0f, 2.5f, 2.0f, 1.5f, 1.0f, 1.0f};
    for (int n = 1; n <= 6; n++) {
        kill_enemies_until_boss_spawns(&gs, &events);
        assert(gs.boss.alive);
        assert(gs.boss_count == n);
        assert(fabsf(gs.boss.dispatch_timer - expected[n - 1]) < 0.001f);
        defeat_current_boss(&gs, &events);
    }
    printf("test_boss_dispatch_interval_shortens_across_encounters OK\n");
}

/* Once dispatch_timer elapses, spawner_dispatch_enemy_from_boss must put a
 * brand new alive Enemy directly beneath the boss (its own x, and y at the
 * boss's own bottom edge), in flight (boss_dispatch_flying) toward a target
 * point that's actually on screen. */
static void test_boss_dispatches_enemy_from_beneath_itself_after_interval(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    float interval = spawner_boss_dispatch_interval(gs.boss_count);
    assert(fabsf(gs.boss.dispatch_timer - interval) < 0.001f);

    /* Clear the arena so the boss's own dispatch is the unambiguous only
     * possible source of a new alive enemy - ordinary spawns are already
     * suspended while a boss is alive, but fleeing survivors from before
     * the boss arrived may not have cleared the bottom of the screen yet. */
    for (int i = 0; i < MAX_ENEMIES; i++) gs.enemies[i].alive = false;

    InputCommand none = no_input();
    float dt = 0.05f;
    float elapsed = 0.0f;
    while (elapsed + dt < interval) {
        game_update(&gs, &none, dt, &events);
        elapsed += dt;
    }
    for (int i = 0; i < MAX_ENEMIES; i++) assert(!gs.enemies[i].alive); /* not yet */

    game_update(&gs, &none, dt, &events); /* crosses the interval */

    int idx = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (gs.enemies[i].alive) {
            assert(idx < 0 && "expected exactly one dispatched enemy");
            idx = i;
        }
    }
    assert(idx >= 0);
    Enemy *e = &gs.enemies[idx];
    assert(e->boss_dispatch_flying);
    assert(fabsf(e->x - gs.boss.x) < 0.01f);
    assert(fabsf(e->y - (gs.boss.y + gs.boss.size * 0.5f)) < 0.01f);
    assert(e->boss_dispatch_target_x >= 0.0f && e->boss_dispatch_target_x <= (float)gs.screen_w);
    assert(e->boss_dispatch_target_y >= 0.0f && e->boss_dispatch_target_y <= (float)gs.screen_h);
    /* dispatch_timer immediately re-armed for the next one, same interval
     * since boss_count hasn't changed. */
    assert(fabsf(gs.boss.dispatch_timer - interval) < 0.001f);
    printf("test_boss_dispatches_enemy_from_beneath_itself_after_interval OK\n");
}

/* Once a dispatched enemy reaches its landing point, it must stop flying
 * and pick up completely ordinary behavior from there - a real vy, a real
 * fire_timer, and a movement_style actually rolled (not left at whatever
 * placeholder spawner_dispatch_enemy_from_boss set). */
static void test_boss_dispatched_enemy_lands_and_becomes_normal(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    for (int i = 0; i < MAX_ENEMIES; i++) gs.enemies[i].alive = false;
    gs.boss.dispatch_timer = 0.001f; /* fire on the very next update */

    InputCommand none = no_input();
    game_update(&gs, &none, 0.01f, &events);

    int idx = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (gs.enemies[i].alive) idx = i;
    }
    assert(idx >= 0);
    assert(gs.enemies[idx].boss_dispatch_flying);

    /* However far the randomly-rolled target is, BOSS_DISPATCH_ENEMY_FLIGHT_
     * SPEED tightly bounds how long the flight can possibly take - the
     * target is never farther than one full screen diagonal away. */
    float max_flight_time = (float)(gs.screen_w + gs.screen_h) / BOSS_DISPATCH_ENEMY_FLIGHT_SPEED + 1.0f;
    float elapsed = 0.0f;
    while (elapsed < max_flight_time && gs.enemies[idx].boss_dispatch_flying) {
        game_update(&gs, &none, 0.05f, &events);
        elapsed += 0.05f;
    }

    assert(!gs.enemies[idx].boss_dispatch_flying);
    assert(gs.enemies[idx].alive);
    assert(gs.enemies[idx].vy > 0.0f); /* falls like a regular enemy from here on */
    assert(gs.enemies[idx].fire_timer > 0.0f);
    assert(gs.enemies[idx].movement_style >= ENEMY_MOVEMENT_NORMAL &&
           gs.enemies[idx].movement_style <= ENEMY_MOVEMENT_RANDOM);
    printf("test_boss_dispatched_enemy_lands_and_becomes_normal OK\n");
}

/* A boss-dispatched enemy is a completely ordinary Enemy entry as far as
 * combat is concerned - shooting it down (even mid-flight, before it's
 * "really" landed) must award score exactly like any other kill. */
static void test_boss_dispatched_enemy_can_be_killed_for_score(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    for (int i = 0; i < MAX_ENEMIES; i++) gs.enemies[i].alive = false;
    gs.boss.dispatch_timer = 0.001f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.01f, &events);

    int idx = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (gs.enemies[i].alive) idx = i;
    }
    assert(idx >= 0);

    /* The boss (and so the enemy it just dispatched) is still off-screen
     * above y=0 this early into the encounter - move it into clear view
     * before testing the shot, so the projectile isn't simply culled by
     * update_projectiles' own off-screen cleanup before check_collisions
     * ever runs, independent of anything this test actually cares about. */
    gs.enemies[idx].y = (float)gs.screen_h * 0.5f;

    int score_before = gs.score;
    gs.player_shots[0].alive = true;
    gs.player_shots[0].x = gs.enemies[idx].x;
    gs.player_shots[0].y = gs.enemies[idx].y;
    gs.player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.enemies[idx].alive);
    assert(gs.score > score_before);
    printf("test_boss_dispatched_enemy_can_be_killed_for_score OK\n");
}

/* Outside a boss fight, orbs drop via the ORB_SCORE_STEP/ORB_SPAWN_CHANCE
 * coin-flip (see test_orb_spawn_chance_is_not_always_or_never); during one,
 * that whole mechanic is skipped and replaced by a flat
 * BOSS_FIGHT_ORB_SPAWN_CHANCE rolled on every kill instead (see
 * destroy_enemy_for_score). Sampling-tolerance style matches
 * test_erratic_enemies_start_appearing_after_first_boss_defeat. */
static void test_boss_fight_orb_spawn_uses_flat_chance_per_kill(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);

    int trials = 400;
    int spawned = 0;
    for (int i = 0; i < trials; i++) {
        gs.orb.alive = false; /* isolate each kill's own roll */
        kill_one_enemy(&gs, &events);
        if (gs.orb.alive) spawned++;
    }

    float rate = (float)spawned / (float)trials;
    assert(rate > 0.01f && rate < 0.12f); /* true rate is 5% */
    printf("test_boss_fight_orb_spawn_uses_flat_chance_per_kill OK\n");
}

/* The score-step mechanic must be fully, deterministically disabled while
 * a boss is alive - not just unlikely to fire. The boss's own defeat bonus
 * (BOSS_HITS_INCREMENT * BOSS_KILL_SCORE_MULTIPLIER = 200 points for the
 * first boss) is awarded via apply_score_delta while boss.alive is still
 * true (see damage_boss), crossing at least one ORB_SCORE_STEP (200)
 * multiple - if maybe_trigger_orb_spawn's boss.alive guard were missing,
 * this would have a 50% chance of spawning an orb; with it, it must never
 * spawn one, on every single run. */
static void test_boss_fight_never_spawns_orb_via_score_step(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    gs.orb.alive = false;
    int score_before = gs.score;

    defeat_current_boss(&gs, &events);

    assert(gs.score - score_before >= ORB_SCORE_STEP);
    assert(!gs.orb.alive);
    printf("test_boss_fight_never_spawns_orb_via_score_step OK\n");
}

/* Shooting the power orb (as opposed to capturing it) must never touch the
 * boss, however close together they happen to be - check_collisions' own
 * orb-shot sweep only ever iterates gs->enemies (see destroy_enemy_for_score
 * unaffected, but more directly: the orb-shot block itself), which the boss
 * simply isn't part of. Positioned away from the boss so the boss's own
 * separate shot-vs-boss hit test (earlier in check_collisions) can't eat
 * the same projectile first and produce a false pass. */
static void test_shooting_power_orb_during_boss_fight_never_damages_boss(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    float hits_before = gs.boss.hits_taken;

    gs.orb.alive = true;
    gs.orb.x = 30.0f;
    gs.orb.y = 30.0f;
    gs.orb.size = 20.0f;

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.orb.x;
    gs.enemies[0].y = gs.orb.y + 40.0f;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    gs.player_shots[0].alive = true;
    gs.player_shots[0].x = gs.orb.x;
    gs.player_shots[0].y = gs.orb.y;
    gs.player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.orb.alive);                  /* detonated */
    assert(gs.enemies[0].orb_kill_pending); /* still works normally otherwise */
    assert(gs.boss.alive);                  /* boss completely unaffected */
    assert(gs.boss.hits_taken == hits_before);
    printf("test_shooting_power_orb_during_boss_fight_never_damages_boss OK\n");
}

/* super_beam_shields_player's whole point: the immunity a captured orb
 * grants must not extend to a boss fight for ANY hazard, not just the
 * boss's own ring (covered separately by
 * test_boss_ring_contact_kills_boss_and_ignores_super_beam_but_not_god_mode) -
 * plain enemy contact must be just as fatal. Outside a boss fight this is
 * completely unchanged, still covered by test_player_invincible_during_
 * super_beam. */
static void test_super_beam_does_not_shield_against_ordinary_hazards_during_boss_fight(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.player.alive); /* the beam no longer protects mid-fight */
    assert(gs.state == STATE_GAME_OVER);
    printf("test_super_beam_does_not_shield_against_ordinary_hazards_during_boss_fight OK\n");
}

/* Fires once in the given mode against nothing (so the shot(s) survive to
 * be inspected) and returns a single shot's damage - the first alive one
 * found, which for the two-shot modes (double barrel, side beams) is
 * either wingtip shot since both carry identical damage. Deliberately used
 * below to compare modes against a freshly-fired mode 1 baseline instead of
 * against the *_DAMAGE_MULTIPLIER constants themselves, so these tests
 * actually fail if a multiplier is ever accidentally changed, rather than
 * trivially agreeing with whatever value it happens to hold. */
static float fired_shot_damage(ShootMode mode) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    gs.player.shoot_mode = mode;

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) return gs.player_shots[i].damage;
    }
    assert(false && "expected a shot to spawn");
    return 0.0f;
}

/* Regression coverage for the per-mode damage scaling this was built for:
 * mode 4 (double barrel) at half of mode 1's damage, mode 3 (power cannon)
 * at triple. Every other target in the game dies to a single hit
 * regardless of damage (see kill_one_enemy passing untouched by any of
 * this), so the boss - the only entity with an actual hit-point pool - is
 * the only place any of it is observable. */
static void test_double_barrel_deals_half_the_damage_of_normal_mode(void) {
    float normal = fired_shot_damage(SHOOT_MODE_NORMAL);
    float doubled = fired_shot_damage(SHOOT_MODE_DOUBLE);
    assert(normal > 0.0f);
    assert(fabsf(doubled - normal * 0.5f) < 0.001f);
    printf("test_double_barrel_deals_half_the_damage_of_normal_mode OK\n");
}

static void test_power_cannon_deals_triple_the_damage_of_normal_mode(void) {
    float normal = fired_shot_damage(SHOOT_MODE_NORMAL);
    float power = fired_shot_damage(SHOOT_MODE_POWER);
    assert(fabsf(power - normal * 3.0f) < 0.001f);
    printf("test_power_cannon_deals_triple_the_damage_of_normal_mode OK\n");
}

/* The other half of the feature: it's not enough for the spawned shots to
 * carry the scaled damage value (covered above) - damage_boss must actually
 * consume it correctly (accumulating a fractional/scaled amount into
 * hits_taken, not silently falling back to a flat one hit per shot like it
 * did before Projectile.damage existed). */
static void test_boss_hit_pool_drops_by_the_exact_damage_landed(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    assert(gs.boss.hits_taken == 0.0f);

    float double_barrel_damage = fired_shot_damage(SHOOT_MODE_DOUBLE);
    shoot_boss_with_damage(&gs, &events, double_barrel_damage);

    assert(fabsf(gs.boss.hits_taken - double_barrel_damage) < 0.001f);
    assert(gs.boss.alive); /* half a hit is nowhere near enough to defeat it */
    printf("test_boss_hit_pool_drops_by_the_exact_damage_landed OK\n");
}

/* Double barrel fires two half-damage shots per trigger pull instead of
 * mode 1's one full-damage shot - both landing on the boss must cost the
 * same total as a single normal hit, not double it. */
static void test_double_barrel_pair_together_cost_one_normal_hit(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);

    float normal = fired_shot_damage(SHOOT_MODE_NORMAL);
    float per_wing = fired_shot_damage(SHOOT_MODE_DOUBLE);

    shoot_boss_with_damage(&gs, &events, per_wing);
    shoot_boss_with_damage(&gs, &events, per_wing);

    assert(fabsf(gs.boss.hits_taken - normal) < 0.001f);
    printf("test_double_barrel_pair_together_cost_one_normal_hit OK\n");
}

/* C-24's own 3-slot moveset, pinned exactly: B-20's double barrel and power
 * cannon reused under different keys, plus its own exclusive omni burst -
 * see ship_shoot_mode_for_slot/ship_shoot_mode_slot_count in usecases/ship.h. */
static void test_c24_shoot_mode_slots(void) {
    assert(ship_shoot_mode_slot_count(SHIP_B20) == 5);
    assert(ship_shoot_mode_for_slot(SHIP_B20, 0) == SHOOT_MODE_NORMAL);
    assert(ship_shoot_mode_for_slot(SHIP_B20, 1) == SHOOT_MODE_RAPID);
    assert(ship_shoot_mode_for_slot(SHIP_B20, 2) == SHOOT_MODE_POWER);
    assert(ship_shoot_mode_for_slot(SHIP_B20, 3) == SHOOT_MODE_DOUBLE);
    assert(ship_shoot_mode_for_slot(SHIP_B20, 4) == SHOOT_MODE_SIDE);

    assert(ship_shoot_mode_slot_count(SHIP_C24) == 3);
    assert(ship_shoot_mode_for_slot(SHIP_C24, 0) == SHOOT_MODE_DOUBLE);
    assert(ship_shoot_mode_for_slot(SHIP_C24, 1) == SHOOT_MODE_POWER);
    assert(ship_shoot_mode_for_slot(SHIP_C24, 2) == SHOOT_MODE_OMNI);

    assert(ship_shoot_mode_slot_index(SHIP_C24, SHOOT_MODE_DOUBLE) == 0);
    assert(ship_shoot_mode_slot_index(SHIP_C24, SHOOT_MODE_POWER) == 1);
    assert(ship_shoot_mode_slot_index(SHIP_C24, SHOOT_MODE_OMNI) == 2);
    assert(ship_shoot_mode_slot_index(SHIP_C24, SHOOT_MODE_NORMAL) == -1); /* not in C-24's table at all */
    printf("test_c24_shoot_mode_slots OK\n");
}

/* reset_run must pick each ship's own slot-0 mode as the default, not a
 * hardcoded SHOOT_MODE_NORMAL that C-24 doesn't even have. */
static void test_default_shoot_mode_is_per_ship(void) {
    GameState gs_b20, gs_c24;
    EventQueue events;
    start_game(&gs_b20, &events);
    assert(gs_b20.player.shoot_mode == SHOOT_MODE_NORMAL);

    start_game_as_c24(&gs_c24, &events);
    assert(gs_c24.player.shoot_mode == SHOOT_MODE_DOUBLE);
    printf("test_default_shoot_mode_is_per_ship OK\n");
}

/* C-24 only has 3 keys that do anything - pressing 4 or 5 must be a no-op,
 * not silently fall back to some other mode. */
static void test_c24_ignores_keys_beyond_its_own_mode_count(void) {
    GameState gs;
    EventQueue events;
    start_game_as_c24(&gs, &events);
    ShootMode before = gs.player.shoot_mode;

    InputCommand key4 = no_input();
    key4.shoot_mode_4_pressed = true;
    game_update(&gs, &key4, 0.016f, &events);
    assert(gs.player.shoot_mode == before);

    InputCommand key5 = no_input();
    key5.shoot_mode_5_pressed = true;
    game_update(&gs, &key5, 0.016f, &events);
    assert(gs.player.shoot_mode == before);

    /* Key 3 is C-24's own slot 2 (OMNI) and must still work. */
    InputCommand key3 = no_input();
    key3.shoot_mode_3_pressed = true;
    game_update(&gs, &key3, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_OMNI);
    printf("test_c24_ignores_keys_beyond_its_own_mode_count OK\n");
}

/* C-24's own mode 3: fires all 8 pellets of the omni burst in one frame,
 * each at BASE_PLAYER_DAMAGE, and locks out immediate refire behind
 * SHIP_C24_OMNI_FIRE_COOLDOWN ("2 shots per second"). */
static void test_c24_omni_burst_fires_eight_shots_at_once(void) {
    GameState gs;
    EventQueue events;
    start_game_as_c24(&gs, &events);
    gs.player.shoot_mode = SHOOT_MODE_OMNI;

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.001f, &events);

    int alive_count = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive) continue;
        alive_count++;
        assert(fabsf(gs.player_shots[i].damage - BASE_PLAYER_DAMAGE) < 0.001f);
    }
    assert(alive_count == ENEMY_OMNI_SHOT_COUNT);
    /* "1 shot per second": the whole 8-pellet volley retriggers once a
     * second, not that each pellet fires individually. */
    assert(fabsf(gs.player.fire_cooldown - SHIP_C24_OMNI_FIRE_COOLDOWN) < 0.01f);
    assert(fabsf(SHIP_C24_OMNI_FIRE_COOLDOWN - 1.0f) < 0.001f);
    printf("test_c24_omni_burst_fires_eight_shots_at_once OK\n");
}

/* Trail visibility is uniform now - every player shot (any ship, any mode)
 * gets the same PROJECTILE_TRAIL_MAX_ALPHA trail enemy shots already get,
 * with no per-ship or per-mode suppression/override. Checked through the
 * real fire -> spawn -> emit path, not just by asserting a stored field. */
static void test_player_shots_spawn_trail_particles_same_as_enemy_shots(void) {
    GameState gs_b20, gs_c24;
    EventQueue events;
    start_game(&gs_b20, &events);
    start_game_as_c24(&gs_c24, &events);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs_b20, &fire, 0.001f, &events);
    game_update(&gs_c24, &fire, 0.001f, &events);

    /* Advance past PROJECTILE_TRAIL_SPAWN_INTERVAL so each fresh shot's own
     * trail_emit_timer expires and it puffs at least one particle. */
    InputCommand idle = no_input();
    for (int i = 0; i < 5; i++) {
        game_update(&gs_b20, &idle, 0.02f, &events);
        game_update(&gs_c24, &idle, 0.02f, &events);
    }

    bool b20_has_trail = false, c24_has_trail = false;
    for (int i = 0; i < MAX_PROJECTILE_TRAIL_PARTICLES; i++) {
        if (gs_b20.projectile_trails[i].alive) b20_has_trail = true;
        if (gs_c24.projectile_trails[i].alive) c24_has_trail = true;
    }
    assert(b20_has_trail);
    assert(c24_has_trail); /* no longer suppressed for any of C-24's modes */
    printf("test_player_shots_spawn_trail_particles_same_as_enemy_shots OK\n");
}

/* C-24's mode 2 (the power-cannon reuse) hit-tests 8x bigger than its other
 * two modes (SHIP_C24_POWER_MODE_RADIUS vs SHIP_C24_PROJECTILE_RADIUS - see
 * player_shot_half_extents in usecases/game_logic.c). Verified by placing
 * the boss just past the small radius's own reach but well within the
 * bigger one's, so only a PROJECTILE_KIND_POWER shot can land here. */
static void test_c24_mode2_projectile_hitbox_is_eight_times_bigger(void) {
    GameState gs;
    EventQueue events;
    start_game_as_c24(&gs, &events);
    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    if (gs.boss.y < 0.0f) gs.boss.y = (float)gs.screen_h * 0.3f;

    float boss_half = gs.boss.size / 2.0f;
    /* Comfortably beyond boss_half + SHIP_C24_PROJECTILE_RADIUS (a miss for
     * the small sphere), comfortably inside boss_half +
     * SHIP_C24_POWER_MODE_RADIUS (a hit for the 8x-bigger one). */
    float offset = boss_half + (SHIP_C24_PROJECTILE_RADIUS + SHIP_C24_POWER_MODE_RADIUS) / 2.0f;
    InputCommand none = no_input();

    gs.player_shots[0] = (Projectile){0};
    gs.player_shots[0].alive = true;
    gs.player_shots[0].x = gs.boss.x + offset;
    gs.player_shots[0].y = gs.boss.y;
    gs.player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;
    gs.player_shots[0].damage = BASE_PLAYER_DAMAGE;
    gs.player_shots[0].kind = PROJECTILE_KIND_NORMAL; /* C-24 modes 1/3 size */
    game_update(&gs, &none, 0.001f, &events);
    assert(gs.boss.hits_taken == 0.0f); /* too far for the small sphere */

    gs.player_shots[0] = (Projectile){0};
    gs.player_shots[0].alive = true;
    gs.player_shots[0].x = gs.boss.x + offset;
    gs.player_shots[0].y = gs.boss.y;
    gs.player_shots[0].vy = -PLAYER_PROJECTILE_SPEED;
    gs.player_shots[0].damage = BASE_PLAYER_DAMAGE * POWER_CANNON_DAMAGE_MULTIPLIER;
    gs.player_shots[0].kind = PROJECTILE_KIND_POWER; /* C-24 mode 2's own 8x size */
    game_update(&gs, &none, 0.001f, &events);
    assert(gs.boss.hits_taken > 0.0f); /* well within the bigger sphere */
    printf("test_c24_mode2_projectile_hitbox_is_eight_times_bigger OK\n");
}

static void test_boss_hits_required_increases_each_appearance(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss_count == 1);
    assert(gs.boss.hits_required == 50);
    defeat_current_boss(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss_count == 2);
    assert(gs.boss.hits_required == 100);
    defeat_current_boss(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss_count == 3);
    assert(gs.boss.hits_required == 150);

    printf("test_boss_hits_required_increases_each_appearance OK\n");
}

/* Regression test: a boss's re-appearance must be gated on a fresh
 * BOSS_SCORE_STEP earned since ITS OWN defeat, not on absolute score
 * crossing the next multiple of BOSS_SCORE_STEP. Since defeating a boss
 * awards a bonus that rarely lands score on a round number, checking
 * against absolute multiples would let a second boss slip in almost
 * immediately (or, depending on the bonus size, skip the count checker
 * comparisons) rather than requiring a full new cycle of points. */
static void test_boss_reappearance_requires_fresh_points_since_defeat(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss_count == 1);
    assert(gs.score_since_last_boss == 0); /* reset the instant it appeared */

    defeat_current_boss(&gs, &events);
    assert(!gs.boss.alive);
    /* The defeat bonus is worth full score but contributes nothing to the
     * gap: the counter restarts from zero the instant the boss is cleared,
     * so the next one is always a full BOSS_SCORE_STEP of play away. */
    assert(gs.score_since_last_boss == 0);

    /* One more kill's worth of points is nowhere near BOSS_SCORE_STEP -
     * the second boss must NOT appear yet, even though the underlying
     * absolute score may already be well past its own next multiple of
     * BOSS_SCORE_STEP. */
    kill_one_enemy(&gs, &events);
    assert(!gs.boss.alive);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss_count == 2);
    assert(gs.score_since_last_boss == 0); /* reset again for the third */

    printf("test_boss_reappearance_requires_fresh_points_since_defeat OK\n");
}

/* Regression test for the long-standing "bosses arrive back to back" bug.
 * The defeat bonus is hits_required * BOSS_KILL_SCORE_MULTIPLIER, so it
 * grows with every boss (200, 400, 600, ...) and from the third one on
 * clears BOSS_SCORE_STEP all by itself. It used to be credited to the
 * gap counter after the boss had already been marked dead, which brought
 * the next boss in on the very frame the previous one died - no playtime
 * in between at all. Walks four full encounters so the two bosses whose
 * bonus alone exceeds the threshold are both covered. */
static void test_large_defeat_bonus_never_chains_bosses(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    bool saw_bonus_over_threshold = false;

    for (int n = 1; n <= 4; n++) {
        kill_enemies_until_boss_spawns(&gs, &events);
        assert(gs.boss_count == n);

        int bonus = gs.boss.hits_required * BOSS_KILL_SCORE_MULTIPLIER;
        if (bonus >= BOSS_SCORE_STEP) saw_bonus_over_threshold = true;

        defeat_current_boss(&gs, &events);

        /* However big the payout, the arena must be empty the instant this
         * boss dies and the gap must start over from zero. */
        assert(!gs.boss.alive);
        assert(gs.boss_count == n);
        assert(gs.score_since_last_boss == 0);
    }

    /* Guards the test itself: if the bonus curve is ever retuned so no
     * boss can single-handedly clear the threshold, this stops silently
     * testing nothing. */
    assert(saw_bonus_over_threshold);
    printf("test_large_defeat_bonus_never_chains_bosses OK\n");
}

/* The other half of the gap rule: points scored while a boss is on screen
 * (picking off enemies still fleeing the arena) are worth full score, but
 * must not count toward bringing the next boss in either. */
static void test_points_scored_during_boss_fight_do_not_shorten_gap(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss.alive);
    assert(gs.score_since_last_boss == 0);

    int score_before = gs.score;
    for (int i = 0; i < 10; i++) kill_one_enemy(&gs, &events);

    assert(gs.score > score_before);       /* the points are still awarded */
    assert(gs.score_since_last_boss == 0); /* they just don't close the gap */
    assert(gs.boss_count == 1);            /* and certainly bring in no second boss */
    printf("test_points_scored_during_boss_fight_do_not_shorten_gap OK\n");
}

static void test_boss_arrival_makes_enemies_flee_and_projectiles_harmless(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 49; kill++) kill_one_enemy(&gs, &events);
    assert(!gs.boss.alive);

    /* A decoy enemy and one of its shots, still in play right as the
     * 50th kill (below) brings the boss in. */
    gs.enemies[5].alive = true;
    gs.enemies[5].x = 100.0f;
    gs.enemies[5].y = 50.0f;
    gs.enemies[5].vy = 40.0f;
    gs.enemies[5].size = 20.0f;
    gs.enemies[5].fire_timer = 999.0f;

    gs.enemy_shots[5].alive = true;
    gs.enemy_shots[5].inert = false;
    gs.enemy_shots[5].x = 300.0f;
    gs.enemy_shots[5].y = 300.0f;
    gs.enemy_shots[5].vy = 50.0f;

    kill_one_enemy(&gs, &events); /* the 50th kill: boss arrives */

    assert(gs.boss.alive);
    assert(fabsf(gs.enemies[5].vy - 40.0f * ENEMY_FLEE_SPEED_MULTIPLIER) < 0.01f);
    assert(gs.enemy_shots[5].inert);

    /* Touching a now-inert shot does nothing - move the player right onto
     * it and confirm it neither hurts the player nor is consumed. */
    gs.player.x = gs.enemy_shots[5].x;
    gs.player.y = gs.enemy_shots[5].y;
    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    assert(gs.enemy_shots[5].alive);

    printf("test_boss_arrival_makes_enemies_flee_and_projectiles_harmless OK\n");
}

static void test_boss_defeat_awards_bonus_score(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.score == 500);
    int hits_required = gs.boss.hits_required;

    defeat_current_boss(&gs, &events);

    assert(!gs.boss.alive);
    assert(gs.score == 500 + hits_required * BOSS_KILL_SCORE_MULTIPLIER);
    printf("test_boss_defeat_awards_bonus_score OK\n");
}

static void test_boss_ring_contact_destroys_both_boss_and_player(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;
    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    /* First touch, no health pool: both explode immediately. */
    assert(!gs.boss.alive);
    assert(!gs.player.alive);
    assert(gs.state == STATE_GAME_OVER);
    printf("test_boss_ring_contact_destroys_both_boss_and_player OK\n");
}

/* The boss's detonation on ring contact is unconditional; the player's own
 * death still respects god mode's own unconditional immunity (kept
 * separate from this change - see super_beam_shields_player's own doc
 * comment), but NOT the super beam's: capturing the orb during a boss
 * fight still grants the full beam (offense/healing/speed - see
 * update_super_beam/the orb-capture block above), just never protection
 * from the boss itself, so a ring touch is fatal even with an active
 * beam - only god mode still blocks it. Gating the whole ring interaction
 * on the player being killable used to deadlock the encounter: an
 * invulnerable player would have the boss sit on top of them at zero
 * distance indefinitely with nothing resolving - so the boss's own
 * detonation stays unconditional regardless of which immunity (if any)
 * the player's own death respects. */
static void test_boss_ring_contact_kills_boss_and_ignores_super_beam_but_not_god_mode(void) {
    GameState gs;
    EventQueue events;

    /* god mode */
    start_game(&gs, &events);
    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);
    gs.player.god_mode = true;
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.boss.alive);  /* boss always detonates - no stalemate */
    assert(gs.player.alive); /* but god mode still protects the player */
    assert(gs.state == STATE_GAME);

    /* the super beam grants no such protection during a boss fight */
    start_game(&gs, &events);
    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;

    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.boss.alive);   /* boss still always detonates */
    assert(!gs.player.alive); /* but the beam does not save the player this time */
    assert(gs.state == STATE_GAME_OVER);

    printf("test_boss_ring_contact_kills_boss_and_ignores_super_beam_but_not_god_mode OK\n");
}

/* Confirms the fatal radius is genuinely tied to BOSS_MENACE_RING_RATIO -
 * not just "anywhere near the boss" - matching what's actually drawn. */
static void test_boss_ring_radius_matches_menace_ratio(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    float ring_radius = gs.boss.size * BOSS_MENACE_RING_RATIO;
    float player_radius = fmaxf(PLAYER_WIDTH, PLAYER_HEIGHT) / 2.0f;
    float safe_distance = ring_radius + player_radius + 5.0f;
    float deadly_distance = ring_radius + player_radius - 2.0f;

    InputCommand none = no_input();

    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y - safe_distance;
    game_update(&gs, &none, 0.001f, &events);
    assert(gs.boss.alive);
    assert(gs.player.alive);

    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y - deadly_distance;
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.boss.alive);
    assert(!gs.player.alive);

    printf("test_boss_ring_radius_matches_menace_ratio OK\n");
}

static void test_spawner_suspended_while_boss_alive(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    /* This test is scoped to spawner_update's own suspension specifically -
     * the boss's separate periodic dispatch mechanic (update_boss_dispatch/
     * spawner_dispatch_enemy_from_boss) is a deliberate, different spawn
     * path that's supposed to add enemies while a boss is alive, covered by
     * its own tests below. Push it out of reach so it can't fire during
     * this test's own 6-second wait and be mistaken for a regression. */
    gs.boss.dispatch_timer = 9999.0f;

    /* Give any fleeing survivors time to clear the bottom of the screen,
     * then confirm nothing new spawns to replace them while the boss is
     * still up. */
    InputCommand none = no_input();
    for (int i = 0; i < 60; i++) {
        game_update(&gs, &none, 0.1f, &events);
    }

    int alive_enemies = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (gs.enemies[i].alive) alive_enemies++;
    }
    assert(alive_enemies == 0);
    printf("test_spawner_suspended_while_boss_alive OK\n");
}

/* Regression test for a bug where an enemy projectile slot that went
 * inert (and fully faded) during a boss fight would silently stay broken
 * forever after: since update_enemies didn't reset inert/inert_age when
 * reusing the slot for a brand new shot, update_projectiles (which runs
 * later in the same frame) would immediately see the shot's still-stale
 * inert_age and kill it on the spot - so every enemy that happened to
 * reuse that slot would appear to never fire again. */
static void test_enemy_projectile_slot_reset_after_boss_fade(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    gs.enemy_shots[0].alive = false;
    gs.enemy_shots[0].inert = true;
    gs.enemy_shots[0].inert_age = 999.0f;

    for (int i = 0; i < MAX_ENEMIES; i++) gs.enemies[i].alive = false;
    gs.enemies[0].alive = true;
    gs.enemies[0].x = 100.0f;
    gs.enemies[0].y = 100.0f;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].vy = 10.0f;
    gs.enemies[0].fire_timer = 0.001f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.01f, &events);

    assert(gs.enemy_shots[0].alive);
    assert(!gs.enemy_shots[0].inert);
    printf("test_enemy_projectile_slot_reset_after_boss_fade OK\n");
}

static void test_boss_always_advances_toward_stationary_player(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    gs.boss.x = gs.player.x;
    gs.boss.y = 50.0f;

    InputCommand none = no_input(); /* the player never moves */
    float prev_dist = fabsf(gs.boss.y - gs.player.y);
    bool ever_failed_to_close_in = false;
    for (int i = 0; i < 20 && prev_dist > 1.0f; i++) {
        game_update(&gs, &none, 0.05f, &events);
        float dx = gs.player.x - gs.boss.x;
        float dy = gs.player.y - gs.boss.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist >= prev_dist - 0.001f) ever_failed_to_close_in = true;
        prev_dist = dist;
    }
    assert(!ever_failed_to_close_in);
    printf("test_boss_always_advances_toward_stationary_player OK\n");
}

static void test_boss_speed_matches_configured_multiplier(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y - 300.0f;

    InputCommand none = no_input();
    float y0 = gs.boss.y;
    game_update(&gs, &none, 0.01f, &events);
    float moved = gs.boss.y - y0;
    float expected = PLAYER_SPEED * BOSS_SPEED_MULTIPLIER * 0.01f;
    assert(fabsf(moved - expected) < 0.05f);
    printf("test_boss_speed_matches_configured_multiplier OK\n");
}

static void test_super_beam_damages_boss_periodically_while_sustained(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);
    int hits_before = gs.boss.hits_taken;

    gs.player.super_beam_timer = SUPER_BEAM_DURATION;
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y - 200.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);
    assert(gs.boss.hits_taken == hits_before + 1);

    /* Well under BEAM_BOSS_HIT_INTERVAL of continued overlap: no extra hit
     * yet - this isn't a per-frame tick. */
    for (int i = 0; i < 5; i++) {
        gs.boss.x = gs.player.x;
        game_update(&gs, &none, 0.001f, &events);
    }
    assert(gs.boss.hits_taken == hits_before + 1);

    /* Keep the beam trained on it past BEAM_BOSS_HIT_INTERVAL: it keeps
     * taking damage rather than just the initial single hit. */
    int extra_steps = (int)(BEAM_BOSS_HIT_INTERVAL / 0.05f) + 2;
    for (int i = 0; i < extra_steps; i++) {
        gs.boss.x = gs.player.x;
        game_update(&gs, &none, 0.05f, &events);
    }
    assert(gs.boss.hits_taken == hits_before + 2);

    printf("test_super_beam_damages_boss_periodically_while_sustained OK\n");
}

static void test_mothership_ratings_and_moveset(void) {
    assert(ship_speed_rating(SHIP_MOTHERSHIP) == 2);
    assert(ship_strength_rating(SHIP_MOTHERSHIP) == 10);
    assert(ship_attack_rating(SHIP_MOTHERSHIP) == 10);
    assert(fabsf(ship_speed_multiplier(SHIP_MOTHERSHIP) - 2.0f / 7.0f) < 0.001f);
    assert(fabsf(ship_damage_taken_multiplier(SHIP_MOTHERSHIP) - 0.5f) < 0.001f);

    assert(fabsf(ship_size_multiplier(SHIP_B20) - 1.0f) < 0.001f);
    assert(fabsf(ship_size_multiplier(SHIP_C24) - 1.0f) < 0.001f);
    assert(fabsf(ship_size_multiplier(SHIP_MOTHERSHIP) - 2.0f) < 0.001f);

    assert(ship_shoot_mode_slot_count(SHIP_MOTHERSHIP) == 2);
    assert(ship_shoot_mode_for_slot(SHIP_MOTHERSHIP, 0) == SHOOT_MODE_SWARM_WANDER);
    assert(ship_shoot_mode_for_slot(SHIP_MOTHERSHIP, 1) == SHOOT_MODE_SWARM_FORMATION);
    printf("test_mothership_ratings_and_moveset OK\n");
}

/* Dispatched children are fixed at their kind's own mode #1 the
 * overwhelming majority of the time - only MOTHERSHIP_CHILD_RANDOM_MODE_CHANCE
 * (5%) of dispatches should ever roll anything else. Sampled across many
 * independent single-dispatch trials since the live cap of
 * MOTHERSHIP_MAX_CHILDREN limits how many draws one run can make. */
static void test_mothership_child_mode_mostly_fixed_at_slot0(void) {
    const int total = 300;
    int non_default = 0;
    for (int trial = 0; trial < total; trial++) {
        GameState gs;
        EventQueue events;
        start_game_as_mothership(&gs, &events);

        InputCommand fire = no_input();
        fire.fire_held = true;
        game_update(&gs, &fire, 0.016f, &events);

        ChildShip *c = NULL;
        for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
            if (gs.children[i].alive) {
                c = &gs.children[i];
                break;
            }
        }
        assert(c != NULL);
        if (c->shoot_mode != ship_shoot_mode_for_slot(c->kind, 0)) non_default++;
    }
    /* Both bounds catch a regression: too many non-default draws means the
     * "fixed at #1" rule broke; zero across 300 trials (an astronomically
     * unlikely outcome at a genuine 5% chance) means the random-mode path
     * itself got dropped. */
    assert(non_default > 0);
    assert(non_default < total / 4);
    printf("test_mothership_child_mode_mostly_fixed_at_slot0 OK\n");
}

/* A SHOOT_MODE_RAPID child (the rare random-mode dispatch) fires exactly
 * one burst, then permanently falls back to mode #1 - unlike the player's
 * own mode 2, it must never come back to mode 2 even after waiting out what
 * would have been the player's own RAPID_FIRE_LOCKOUT_DURATION cooldown. */
static void test_mothership_rapid_child_permanently_falls_back_to_mode1(void) {
    GameState gs;
    EventQueue events;
    start_game_as_mothership(&gs, &events);

    gs.children[0] = (ChildShip){0};
    gs.children[0].alive = true;
    gs.children[0].kind = SHIP_B20;
    gs.children[0].shoot_mode = SHOOT_MODE_RAPID;
    gs.children[0].x = gs.player.x;
    gs.children[0].y = gs.player.y - 200.0f;
    gs.children[0].life = MOTHERSHIP_CHILD_LIFE_MAX;

    InputCommand idle = no_input();
    game_update(&gs, &idle, 0.016f, &events); /* kicks off the one burst */
    assert(gs.children[0].shoot_mode == SHOOT_MODE_RAPID);
    assert(gs.children[0].rapid_burst_timer > 0.0f);

    float elapsed = 0.0f;
    while (elapsed < RAPID_FIRE_BURST_DURATION + 0.2f) {
        game_update(&gs, &idle, 0.05f, &events);
        elapsed += 0.05f;
    }
    assert(gs.children[0].shoot_mode == ship_shoot_mode_for_slot(SHIP_B20, 0));
    assert(gs.children[0].rapid_burst_timer == 0.0f);

    elapsed = 0.0f;
    while (elapsed < RAPID_FIRE_LOCKOUT_DURATION + 1.0f) {
        game_update(&gs, &idle, 0.5f, &events);
        elapsed += 0.5f;
    }
    assert(gs.children[0].shoot_mode == ship_shoot_mode_for_slot(SHIP_B20, 0));
    printf("test_mothership_rapid_child_permanently_falls_back_to_mode1 OK\n");
}

static void test_mothership_dispatch_caps_and_resumes(void) {
    GameState gs;
    EventQueue events;
    start_game_as_mothership(&gs, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_SWARM_WANDER);

    InputCommand fire = no_input();
    fire.fire_held = true;

    int alive_count = 0;
    for (int tick = 0; tick < 500 && alive_count < MOTHERSHIP_MAX_CHILDREN; tick++) {
        game_update(&gs, &fire, 0.02f, &events);
        alive_count = 0;
        for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
            if (gs.children[i].alive) alive_count++;
        }
    }
    assert(alive_count == MOTHERSHIP_MAX_CHILDREN);

    /* At capacity: continuing to hold fire spawns nothing further. */
    for (int tick = 0; tick < 100; tick++) game_update(&gs, &fire, 0.02f, &events);
    int still_alive = 0;
    for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
        if (gs.children[i].alive) still_alive++;
    }
    assert(still_alive == MOTHERSHIP_MAX_CHILDREN);

    /* Free a slot and clear the cooldown directly, isolating "capacity
     * gates dispatch" from "cooldown gates dispatch" - dispatch must
     * resume on the very next held-fire frame once a slot is free. */
    gs.children[0].alive = false;
    gs.player.fire_cooldown = 0.0f;
    game_update(&gs, &fire, 0.02f, &events);
    int after_death_alive = 0;
    for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
        if (gs.children[i].alive) after_death_alive++;
    }
    assert(after_death_alive == MOTHERSHIP_MAX_CHILDREN);

    printf("test_mothership_dispatch_caps_and_resumes OK\n");
}

static void test_mothership_child_launch_then_ai_handoff(void) {
    GameState gs;
    EventQueue events;
    start_game_as_mothership(&gs, &events);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    ChildShip *c = NULL;
    for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
        if (gs.children[i].alive) {
            c = &gs.children[i];
            break;
        }
    }
    assert(c != NULL);
    assert(c->launch_timer > 0.0f);
    assert(c->vx != 0.0f); /* the random left/right launch kick */
    assert(c->vy == 0.0f);
    assert(c->y > gs.player.y); /* dispatched from underneath her, not in front/behind */

    float x_after_spawn = c->x;
    game_update(&gs, &fire, 0.05f, &events);
    /* Still coasting on the launch kick alone - no AI steering yet. */
    assert(fabsf(c->x - (x_after_spawn + c->vx * 0.05f)) < 0.01f);

    float elapsed = 0.0f;
    while (elapsed < MOTHERSHIP_CHILD_LAUNCH_DURATION + 0.2f && c->alive) {
        game_update(&gs, &fire, 0.05f, &events);
        elapsed += 0.05f;
    }
    assert(c->alive);
    assert(c->launch_timer <= 0.0f); /* AI has taken over */
    printf("test_mothership_child_launch_then_ai_handoff OK\n");
}

/* SHOOT_MODE_SWARM_FORMATION pulls an alive child toward its assigned
 * triangle slot (see mothership_formation_slot in usecases/game_logic.c) -
 * with only one child alive, that's always slot 0 (front, straight ahead
 * of her). */
static void test_mothership_formation_pulls_child_to_slot(void) {
    GameState gs;
    EventQueue events;
    start_game_as_mothership(&gs, &events);

    InputCommand mode2 = no_input();
    mode2.shoot_mode_2_pressed = true;
    game_update(&gs, &mode2, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_SWARM_FORMATION);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    ChildShip *c = NULL;
    for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
        if (gs.children[i].alive) {
            c = &gs.children[i];
            break;
        }
    }
    assert(c != NULL);

    /* Stop holding fire so this stays the only child alive, keeping the
     * slot assignment unambiguous throughout. */
    InputCommand idle = no_input();
    float elapsed = 0.0f;
    while (elapsed < MOTHERSHIP_CHILD_LAUNCH_DURATION + 3.0f && c->alive) {
        game_update(&gs, &idle, 0.05f, &events);
        elapsed += 0.05f;
    }
    assert(c->alive);

    float target_x = gs.player.x;
    float target_y = gs.player.y - MOTHERSHIP_CHILD_FORMATION_FRONT_OFFSET;
    float dist = sqrtf((c->x - target_x) * (c->x - target_x) + (c->y - target_y) * (c->y - target_y));
    assert(dist < 5.0f);
    printf("test_mothership_formation_pulls_child_to_slot OK\n");
}

static void test_mothership_child_takes_enemy_shot_damage_and_dies(void) {
    GameState gs;
    EventQueue events;
    start_game_as_mothership(&gs, &events);

    gs.children[0] = (ChildShip){0};
    gs.children[0].alive = true;
    gs.children[0].kind = SHIP_B20;
    gs.children[0].x = gs.player.x;
    gs.children[0].y = gs.player.y - 200.0f;
    gs.children[0].life = MOTHERSHIP_CHILD_LIFE_MAX;

    gs.enemy_shots[0] = (Projectile){0};
    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = gs.children[0].x;
    gs.enemy_shots[0].y = gs.children[0].y;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    float expected_loss = PLAYER_LIFE_LOSS_PER_HIT * ship_damage_taken_multiplier(SHIP_B20);
    assert(fabsf(gs.children[0].life - (MOTHERSHIP_CHILD_LIFE_MAX - expected_loss)) < 0.01f);
    assert(gs.children[0].alive);
    assert(!gs.enemy_shots[0].alive);

    int hits_needed = (int)(MOTHERSHIP_CHILD_LIFE_MAX / expected_loss) + 2;
    for (int i = 0; i < hits_needed && gs.children[0].alive; i++) {
        gs.enemy_shots[0] = (Projectile){0};
        gs.enemy_shots[0].alive = true;
        gs.enemy_shots[0].x = gs.children[0].x;
        gs.enemy_shots[0].y = gs.children[0].y;
        game_update(&gs, &none, 0.001f, &events);
    }
    assert(!gs.children[0].alive);
    assert(gs.children[0].life <= 0.0f);
    printf("test_mothership_child_takes_enemy_shot_damage_and_dies OK\n");
}

static void test_mothership_child_dies_on_enemy_contact_and_enemy_dies_too(void) {
    GameState gs;
    EventQueue events;
    start_game_as_mothership(&gs, &events);

    gs.children[0] = (ChildShip){0};
    gs.children[0].alive = true;
    gs.children[0].kind = SHIP_C24;
    gs.children[0].x = gs.player.x;
    gs.children[0].y = gs.player.y - 200.0f;
    gs.children[0].life = MOTHERSHIP_CHILD_LIFE_MAX;

    gs.enemies[0] = (Enemy){0};
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.children[0].x;
    gs.enemies[0].y = gs.children[0].y;
    gs.enemies[0].size = 20.0f;

    int score_before = gs.score;
    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.children[0].alive);
    assert(!gs.enemies[0].alive);
    assert(gs.score > score_before); /* destroy_enemy_for_score still awarded */
    printf("test_mothership_child_dies_on_enemy_contact_and_enemy_dies_too OK\n");
}

/* Unlike the player's own ring touch (test_boss_ring_contact_destroys_both_boss_and_player),
 * a ChildShip's death on the same ring must NOT end the boss encounter -
 * an escort is cheap to re-dispatch, so it can't be a free boss kill. */
static void test_mothership_child_dies_on_boss_ring_without_defeating_boss(void) {
    GameState gs;
    EventQueue events;
    start_game_as_mothership(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    /* Keep the player well clear so only the child's own ring touch is
     * exercised. */
    gs.player.x = 10.0f;
    gs.player.y = 10.0f;
    gs.boss.x = 300.0f;
    gs.boss.y = 300.0f;

    gs.children[0] = (ChildShip){0};
    gs.children[0].alive = true;
    gs.children[0].kind = SHIP_B20;
    gs.children[0].x = gs.boss.x;
    gs.children[0].y = gs.boss.y;
    gs.children[0].life = MOTHERSHIP_CHILD_LIFE_MAX;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.children[0].alive);
    assert(gs.boss.alive);
    printf("test_mothership_child_dies_on_boss_ring_without_defeating_boss OK\n");
}

/* Regression guard for the Projectile.style_ship refactor (see
 * spawn_player_shot_styled/trigger_power_cannon_explosion in
 * usecases/game_logic.c): a C-24-styled shot - as a C-24-kind ChildShip's
 * own mode 2 would fire - must still get the 50% bigger damage radius even
 * while gs->selected_ship is SHIP_MOTHERSHIP, and a B-20-styled shot must
 * not. */
static void test_mothership_c24_styled_shot_still_gets_bigger_explosion_radius(void) {
    GameState gs;
    EventQueue events;
    start_game_as_mothership(&gs, &events);

    float radius_b20 = POWER_CANNON_EXPLOSION_RADIUS_RATIO * fminf((float)gs.screen_w, (float)gs.screen_h);
    float radius_c24 = radius_b20 * SHIP_C24_POWER_MODE_EXPLOSION_RADIUS_MULTIPLIER;
    float probe_dist = radius_b20 + (radius_c24 - radius_b20) / 2.0f; /* strictly between the two */

    for (int trial = 0; trial < 2; trial++) {
        Ship style = (trial == 0) ? SHIP_B20 : SHIP_C24;

        memset(&gs.player_shots, 0, sizeof(gs.player_shots));
        memset(&gs.enemies, 0, sizeof(gs.enemies));

        gs.player_shots[0].alive = true;
        gs.player_shots[0].kind = PROJECTILE_KIND_POWER;
        gs.player_shots[0].style_ship = style;
        gs.player_shots[0].x = 0.0f;
        gs.player_shots[0].y = 0.0f;

        gs.enemies[0].alive = true; /* directly under the shot - guarantees the AABB hit */
        gs.enemies[0].x = 0.0f;
        gs.enemies[0].y = 0.0f;
        gs.enemies[0].size = 20.0f;

        gs.enemies[1].alive = true; /* strictly between the two ships' own radii */
        gs.enemies[1].x = probe_dist;
        gs.enemies[1].y = 0.0f;
        gs.enemies[1].size = 20.0f;

        InputCommand none = no_input();
        game_update(&gs, &none, 0.0001f, &events);

        bool probe_died = !gs.enemies[1].alive;
        assert(probe_died == (style == SHIP_C24));
    }
    printf("test_mothership_c24_styled_shot_still_gets_bigger_explosion_radius OK\n");
}

static void test_shine_ratings_and_moveset(void) {
    assert(ship_speed_rating(SHIP_SHINE) == 8);
    assert(ship_strength_rating(SHIP_SHINE) == 4);
    assert(ship_attack_rating(SHIP_SHINE) == 6);
    /* Faster than B-20 itself (8 > 7) - the fastest ship in the fleet. */
    assert(ship_speed_multiplier(SHIP_SHINE) > 1.0f);
    assert(fabsf(ship_speed_multiplier(SHIP_SHINE) - 8.0f / 7.0f) < 0.001f);
    /* Weaker than B-20 (4 < 5) - takes more damage per hit. */
    assert(ship_damage_taken_multiplier(SHIP_SHINE) > 1.0f);
    assert(fabsf(ship_damage_taken_multiplier(SHIP_SHINE) - 5.0f / 4.0f) < 0.001f);
    assert(fabsf(ship_size_multiplier(SHIP_SHINE) - 1.0f) < 0.001f); /* same size as B-20 */

    assert(ship_shoot_mode_slot_count(SHIP_SHINE) == 3);
    assert(ship_shoot_mode_for_slot(SHIP_SHINE, 0) == SHOOT_MODE_SHINE_SHARDS);
    assert(ship_shoot_mode_for_slot(SHIP_SHINE, 1) == SHOOT_MODE_SHINE_OMNI);
    assert(ship_shoot_mode_for_slot(SHIP_SHINE, 2) == SHOOT_MODE_SHINE_SPIRAL);
    printf("test_shine_ratings_and_moveset OK\n");
}

/* Mode 1 (default): twin shards close together - "separated by a distance
 * equal to the shard's width" - and each shard's damage halved so the pair
 * costs the same total as one full shot. */
static void test_shine_twin_shards_close_together_and_halved_damage(void) {
    GameState gs;
    EventQueue events;
    start_game_as_shine(&gs, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_SHINE_SHARDS);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    int found = 0;
    float xs[2];
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive) continue;
        assert(fabsf(gs.player_shots[i].damage - BASE_PLAYER_DAMAGE * SHINE_TWIN_SHARD_DAMAGE_MULTIPLIER) < 0.001f);
        xs[found++] = gs.player_shots[i].x;
    }
    assert(found == 2);

    float gap_between_centers = fabsf(xs[0] - xs[1]);
    /* Centers are 2x the per-side offset apart; the edge-to-edge gap
     * between the two shards is that minus one full shard width, which by
     * construction (SHINE_TWIN_SHARD_OFFSET_X == SHINE_SHARD_WIDTH) equals
     * exactly one shard-width. */
    float edge_gap = gap_between_centers - SHINE_SHARD_WIDTH;
    assert(fabsf(edge_gap - SHINE_SHARD_WIDTH) < 0.01f);
    printf("test_shine_twin_shards_close_together_and_halved_damage OK\n");
}

/* Mode 2: not a mode the player stays in - pressing key 2 fires a 12-way
 * burst at full damage and immediately puts shoot_mode back to mode 1,
 * regardless of which mode was active beforehand, then locks out further
 * presses until SHINE_OMNI_COOLDOWN passes. */
static void test_shine_omni_burst_fires_twelve_and_reverts_to_mode1(void) {
    GameState gs;
    EventQueue events;
    start_game_as_shine(&gs, &events);

    /* Start from mode 3, not the default mode 1, to prove the revert isn't
     * just "shoot_mode never changed in the first place". */
    InputCommand mode3 = no_input();
    mode3.shoot_mode_3_pressed = true;
    game_update(&gs, &mode3, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_SHINE_SPIRAL);

    InputCommand mode2 = no_input();
    mode2.shoot_mode_2_pressed = true;
    game_update(&gs, &mode2, 0.016f, &events);

    assert(gs.player.shoot_mode == SHOOT_MODE_SHINE_SHARDS);
    assert(gs.player.shine_omni_cooldown_timer > 0.0f);

    int alive_count = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive) continue;
        assert(fabsf(gs.player_shots[i].damage - BASE_PLAYER_DAMAGE) < 0.001f);
        alive_count++;
    }
    assert(alive_count == SHINE_OMNI_SHOT_COUNT);

    /* Pressing it again mid-cooldown does nothing further. */
    memset(&gs.player_shots, 0, sizeof(gs.player_shots));
    game_update(&gs, &mode2, 0.016f, &events);
    int after_retry = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) after_retry++;
    }
    assert(after_retry == 0);

    /* Once the cooldown fully elapses, it fires again. */
    gs.player.shine_omni_cooldown_timer = 0.0f;
    game_update(&gs, &mode2, 0.016f, &events);
    int after_cooldown = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) after_cooldown++;
    }
    assert(after_cooldown == SHINE_OMNI_SHOT_COUNT);
    printf("test_shine_omni_burst_fires_twelve_and_reverts_to_mode1 OK\n");
}

/* Mode 3: a single longer shard, "2 shots per second". */
static void test_shine_spiral_shot_is_longer_shard_at_two_per_second(void) {
    GameState gs;
    EventQueue events;
    start_game_as_shine(&gs, &events);

    InputCommand mode3 = no_input();
    mode3.shoot_mode_3_pressed = true;
    game_update(&gs, &mode3, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_SHINE_SPIRAL);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    int found = -1;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) {
            found = i;
            break;
        }
    }
    assert(found >= 0);
    assert(gs.player_shots[found].kind == PROJECTILE_KIND_SHINE_SPIRAL);
    assert(fabsf(gs.player_shots[found].damage - BASE_PLAYER_DAMAGE * SHINE_SPIRAL_DAMAGE_MULTIPLIER) < 0.001f);
    assert(fabsf(SHINE_SPIRAL_DAMAGE_MULTIPLIER - 3.0f) < 0.001f); /* triple damage */
    assert(fabsf(gs.player.fire_cooldown - SHINE_SPIRAL_FIRE_COOLDOWN) < 0.001f);
    assert(fabsf(SHINE_SPIRAL_FIRE_COOLDOWN - 0.5f) < 0.001f); /* exactly 2 shots/sec */
    assert(SHINE_SPIRAL_SHARD_LENGTH > SHINE_SHARD_LENGTH); /* longer... */
    assert(fabsf(SHINE_SPIRAL_SHARD_WIDTH - SHINE_SHARD_WIDTH) < 0.001f); /* ...not thicker */
    printf("test_shine_spiral_shot_is_longer_shard_at_two_per_second OK\n");
}

static void test_cruzader_ratings_and_moveset(void) {
    assert(ship_speed_rating(SHIP_CRUZADER) == 5);
    assert(ship_strength_rating(SHIP_CRUZADER) == 8);
    assert(ship_attack_rating(SHIP_CRUZADER) == 4);
    assert(fabsf(ship_size_multiplier(SHIP_CRUZADER) - 1.5f) < 0.001f); /* 50% bigger than B-20 */

    /* Slower than B-20 (5 < 7). */
    assert(ship_speed_multiplier(SHIP_CRUZADER) < 1.0f);
    assert(fabsf(ship_speed_multiplier(SHIP_CRUZADER) - 5.0f / 7.0f) < 0.001f);
    /* Tougher than B-20 (8 > 5) - takes less damage per hit. */
    assert(ship_damage_taken_multiplier(SHIP_CRUZADER) < 1.0f);
    assert(fabsf(ship_damage_taken_multiplier(SHIP_CRUZADER) - 5.0f / 8.0f) < 0.001f);

    assert(ship_shoot_mode_slot_count(SHIP_CRUZADER) == 3);
    assert(ship_shoot_mode_for_slot(SHIP_CRUZADER, 0) == SHOOT_MODE_CRUZADER_TWIN);
    assert(ship_shoot_mode_for_slot(SHIP_CRUZADER, 1) == SHOOT_MODE_CRUZADER_ORB);
    assert(ship_shoot_mode_for_slot(SHIP_CRUZADER, 2) == SHOOT_MODE_CRUZADER_ROCKETS);
    printf("test_cruzader_ratings_and_moveset OK\n");
}

/* Mode 1 (default): B-20's own DOUBLE pattern, recolored, at 1.5 shots/sec. */
static void test_cruzader_twin_bolts_recolored_and_correct_rate(void) {
    GameState gs;
    EventQueue events;
    start_game_as_cruzader(&gs, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_CRUZADER_TWIN);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    int found = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive) continue;
        assert(gs.player_shots[i].style_ship == SHIP_CRUZADER);
        assert(gs.player_shots[i].kind == PROJECTILE_KIND_NORMAL);
        assert(fabsf(gs.player_shots[i].damage - BASE_PLAYER_DAMAGE * DOUBLE_BARREL_DAMAGE_MULTIPLIER) < 0.001f);
        found++;
    }
    assert(found == 2);
    assert(fabsf(gs.player.fire_cooldown - CRUZADER_TWIN_FIRE_COOLDOWN) < 0.001f);
    assert(fabsf(CRUZADER_TWIN_FIRE_COOLDOWN - (1.0f / 1.5f)) < 0.001f); /* exactly 1.5 shots/sec */
    printf("test_cruzader_twin_bolts_recolored_and_correct_rate OK\n");
}

/* Mode 2: triggered-and-revert like Shine's own mode 2, plus a 5s active
 * window that reflects every enemy shot within CRUZADER_ORB_RADIUS at zero
 * player damage, then a 20s cooldown lockout. */
static void test_cruzader_orb_activates_reflects_and_reverts_with_cooldown(void) {
    GameState gs;
    EventQueue events;
    start_game_as_cruzader(&gs, &events);

    /* Start from mode 3, not the default mode 1, to prove the revert isn't
     * just "shoot_mode never changed in the first place". */
    InputCommand mode3 = no_input();
    mode3.shoot_mode_3_pressed = true;
    game_update(&gs, &mode3, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_CRUZADER_ROCKETS);

    InputCommand mode2 = no_input();
    mode2.shoot_mode_2_pressed = true;
    game_update(&gs, &mode2, 0.016f, &events);

    assert(gs.player.shoot_mode == SHOOT_MODE_CRUZADER_TWIN);
    assert(gs.player.cruzader_orb_timer > 0.0f);

    /* Pressing it again mid-activation does nothing further - the timer
     * only ticks down by dt, it's never reset back up to full. */
    float timer_before_retry = gs.player.cruzader_orb_timer;
    game_update(&gs, &mode2, 0.016f, &events);
    assert(gs.player.cruzader_orb_timer < timer_before_retry);

    /* An enemy shot inside the orb radius is bounced back in place - still
     * the exact same shot (never destroyed/respawned), flying the opposite
     * way, at zero player damage - and its original design (color, shape,
     * size) is completely untouched, per feedback that a reflected
     * projectile must not change appearance. */
    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = gs.player.x;
    /* Inside the orb radius but well outside the player's own contact
     * range, so placing an enemy at this same spot below tests only the
     * reflected shot's own collision, not the player physically touching
     * that enemy too. */
    gs.enemy_shots[0].y = gs.player.y - CRUZADER_ORB_RADIUS * 0.9f;
    gs.enemy_shots[0].vx = 0.0f;
    gs.enemy_shots[0].vy = 100.0f;
    gs.enemy_shots[0].enemy_kind = ENEMY_PROJECTILE_BEAM;
    gs.enemy_shots[0].color = (Color){200, 60, 90, 255};
    gs.enemy_shots[0].half_len = 12.0f;
    gs.enemy_shots[0].half_wid = 3.0f;
    float life_before = gs.player.life;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.016f, &events);

    assert(gs.enemy_shots[0].alive); /* reflected in place, not consumed */
    assert(gs.enemy_shots[0].reflected);
    assert(gs.enemy_shots[0].vy < 0.0f); /* now flying back the way it came */
    assert(fabsf(gs.enemy_shots[0].damage - CRUZADER_REFLECTED_SHOT_DAMAGE) < 0.001f);
    assert(gs.enemy_shots[0].enemy_kind == ENEMY_PROJECTILE_BEAM);
    assert(gs.enemy_shots[0].color.r == 200 && gs.enemy_shots[0].color.g == 60 && gs.enemy_shots[0].color.b == 90);
    assert(fabsf(gs.enemy_shots[0].half_len - 12.0f) < 0.001f);
    assert(fabsf(gs.enemy_shots[0].half_wid - 3.0f) < 0.001f);
    assert(fabsf(gs.player.life - life_before) < 0.001f);

    /* And it can still go on to hurt an enemy, still in that same
     * unmodified shape. */
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.enemy_shots[0].x;
    gs.enemies[0].y = gs.enemy_shots[0].y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.enemies[0].alive);
    assert(!gs.enemy_shots[0].alive); /* consumed on the enemy it hit */

    /* Once the active window fully elapses, the cooldown starts immediately
     * and mode 2 stays unselectable until it too elapses. */
    gs.player.cruzader_orb_timer = 0.016f;
    game_update(&gs, &none, 0.016f, &events);
    assert(gs.player.cruzader_orb_timer <= 0.0f);
    assert(gs.player.cruzader_orb_cooldown_timer > 0.0f);

    game_update(&gs, &mode2, 0.016f, &events);
    assert(gs.player.cruzader_orb_timer <= 0.0f); /* not reactivated during cooldown */

    gs.player.cruzader_orb_cooldown_timer = 0.0f;
    game_update(&gs, &mode2, 0.016f, &events);
    assert(gs.player.cruzader_orb_timer > 0.0f); /* free to trigger again once cooldown clears */

    printf("test_cruzader_orb_activates_reflects_and_reverts_with_cooldown OK\n");
}

/* Passive (always on): 50% chance to reflect an incoming enemy shot at half
 * damage taken instead of a full hit - run many trials since it's
 * probabilistic (no RNG seeding hook exists to force a specific outcome). */
static void test_cruzader_passive_reflect_or_full_damage(void) {
    GameState gs;
    EventQueue events;
    start_game_as_cruzader(&gs, &events);

    int reflect_count = 0, full_damage_count = 0;
    InputCommand none = no_input();
    float expected_full = PLAYER_LIFE_LOSS_PER_HIT * ship_damage_taken_multiplier(SHIP_CRUZADER);
    float expected_half = expected_full * CRUZADER_PASSIVE_REFLECT_DAMAGE_MULTIPLIER;
    Color original_color = (Color){10, 220, 30, 255};

    for (int t = 0; t < 200; t++) {
        memset(&gs.enemy_shots, 0, sizeof(gs.enemy_shots));
        memset(&gs.player_shots, 0, sizeof(gs.player_shots));
        gs.player.life = PLAYER_LIFE_MAX;

        gs.enemy_shots[0].alive = true;
        gs.enemy_shots[0].x = gs.player.x;
        gs.enemy_shots[0].y = gs.player.y;
        gs.enemy_shots[0].vx = 0.0f;
        gs.enemy_shots[0].vy = 10.0f;
        gs.enemy_shots[0].enemy_kind = ENEMY_PROJECTILE_ORB;
        gs.enemy_shots[0].color = original_color;
        gs.enemy_shots[0].half_len = 6.0f;

        game_update(&gs, &none, 0.001f, &events);

        float loss = PLAYER_LIFE_MAX - gs.player.life;
        if (fabsf(loss - expected_half) < 0.01f) {
            reflect_count++;
            /* Reflected in place - still that same shot, original design
             * (color, shape, size) completely untouched. */
            assert(gs.enemy_shots[0].alive);
            assert(gs.enemy_shots[0].reflected);
            assert(gs.enemy_shots[0].vy < 0.0f);
            assert(gs.enemy_shots[0].enemy_kind == ENEMY_PROJECTILE_ORB);
            assert(gs.enemy_shots[0].color.r == original_color.r && gs.enemy_shots[0].color.g == original_color.g &&
                   gs.enemy_shots[0].color.b == original_color.b);
            assert(fabsf(gs.enemy_shots[0].half_len - 6.0f) < 0.001f);
        } else {
            assert(fabsf(loss - expected_full) < 0.01f);
            assert(!gs.enemy_shots[0].alive); /* consumed on a normal hit */
            full_damage_count++;
        }
    }
    /* Over 200 trials at a true 50% chance, both outcomes are certain in
     * practice - this would only flake with astronomically bad luck. */
    assert(reflect_count > 0);
    assert(full_damage_count > 0);
    printf("test_cruzader_passive_reflect_or_full_damage OK\n");
}

/* Mode 3: slow rockets that home toward the closest alive enemy and, on
 * contact, explode with B-20's own Power Cannon radius (confirmed with the
 * user as what "B-20's #2" meant) rather than only harming what they
 * directly touched. */
static void test_cruzader_rockets_home_and_explode_with_power_cannon_radius(void) {
    GameState gs;
    EventQueue events;
    start_game_as_cruzader(&gs, &events);

    InputCommand mode3 = no_input();
    mode3.shoot_mode_3_pressed = true;
    game_update(&gs, &mode3, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_CRUZADER_ROCKETS);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    int idx = -1;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) {
            idx = i;
            break;
        }
    }
    assert(idx >= 0);
    assert(gs.player_shots[idx].kind == PROJECTILE_KIND_CRUZADER_ROCKET);
    assert(fabsf(gs.player_shots[idx].damage - CRUZADER_ROCKET_DAMAGE) < 0.001f);
    assert(fabsf(gs.player.fire_cooldown - CRUZADER_ROCKET_FIRE_COOLDOWN) < 0.001f);
    assert(fabsf(CRUZADER_ROCKET_FIRE_COOLDOWN - 2.0f) < 0.001f); /* 1 shot/2s */

    /* Plant an enemy off to one side - the rocket, fired straight up,
     * should bend its heading toward it within a single update tick. */
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player_shots[idx].x + 200.0f;
    gs.enemies[0].y = gs.player_shots[idx].y - 300.0f;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.016f, &events);

    assert(gs.player_shots[idx].alive);
    assert(gs.player_shots[idx].vx > 0.0f); /* now steering rightward, toward the enemy */

    /* Fast-forward it onto the enemy and plant a second one nearby - the
     * blast should catch both, not just what it directly touched. */
    gs.player_shots[idx].x = gs.enemies[0].x;
    gs.player_shots[idx].y = gs.enemies[0].y;

    float radius = POWER_CANNON_EXPLOSION_RADIUS_RATIO * fminf((float)gs.screen_w, (float)gs.screen_h);
    gs.enemies[1].alive = true;
    gs.enemies[1].x = gs.enemies[0].x + radius * 0.5f;
    gs.enemies[1].y = gs.enemies[0].y;
    gs.enemies[1].size = 20.0f;
    gs.enemies[1].fire_timer = 999.0f;

    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.player_shots[idx].alive);
    assert(!gs.enemies[0].alive);
    assert(!gs.enemies[1].alive); /* caught in the same blast, not directly touched */
    printf("test_cruzader_rockets_home_and_explode_with_power_cannon_radius OK\n");
}

/* Notes: Cruzader survives touching an ordinary enemy (the enemy still
 * dies) but takes a flat CRUZADER_ENEMY_CONTACT_LIFE_LOSS penalty instead of
 * exploding outright - unscaled by his own Strength multiplier. A boss's
 * danger ring is unaffected by any of this and stays fatal outside the orb. */
static void test_cruzader_survives_enemy_contact_for_flat_damage_but_not_boss_ring(void) {
    GameState gs;
    EventQueue events;
    start_game_as_cruzader(&gs, &events);

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;
    float life_before = gs.player.life;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    assert(!gs.enemies[0].alive); /* still destroyed on contact */
    assert(fabsf((life_before - gs.player.life) - CRUZADER_ENEMY_CONTACT_LIFE_LOSS) < 0.001f);
    assert(fabsf(CRUZADER_ENEMY_CONTACT_LIFE_LOSS - 10.0f) < 0.001f); /* exactly 10 points, unscaled */

    /* Repeated contact keeps draining that same flat amount, same as any
     * other life-loss source, and is still fatal once life runs out -
     * bounded well past PLAYER_LIFE_MAX / CRUZADER_ENEMY_CONTACT_LIFE_LOSS
     * so a regression here fails loudly instead of hanging the suite. */
    int contacts = 0;
    while (gs.player.alive && contacts < 50) {
        gs.enemies[0].alive = true;
        gs.enemies[0].x = gs.player.x;
        gs.enemies[0].y = gs.player.y;
        gs.enemies[0].size = 20.0f;
        gs.enemies[0].fire_timer = 999.0f;
        game_update(&gs, &none, 0.001f, &events);
        contacts++;
    }
    assert(!gs.player.alive);
    assert(gs.state == STATE_GAME_OVER);

    /* The boss's danger ring is a different story - still unconditionally
     * fatal, immunity or not. */
    start_game_as_cruzader(&gs, &events);
    gs.boss.alive = true;
    gs.boss.size = 100.0f;
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;

    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.player.alive);
    assert(gs.state == STATE_GAME_OVER);
    printf("test_cruzader_survives_enemy_contact_for_flat_damage_but_not_boss_ring OK\n");
}

/* While the deflector orb is active, a boss ring touch does nothing at all
 * - not fatal to Cruzader, but not a free boss kill either (confirmed with
 * the user). The exact same touch is fatal again once the orb drops. */
static void test_cruzader_orb_blocks_boss_ring_without_free_kill(void) {
    GameState gs;
    EventQueue events;
    start_game_as_cruzader(&gs, &events);

    InputCommand mode2 = no_input();
    mode2.shoot_mode_2_pressed = true;
    game_update(&gs, &mode2, 0.016f, &events);
    assert(gs.player.cruzader_orb_timer > 0.0f);

    gs.boss.alive = true;
    gs.boss.size = 100.0f;
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.alive);
    assert(gs.boss.alive);
    assert(gs.state == STATE_GAME);

    gs.player.cruzader_orb_timer = 0.0f;
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.player.alive);
    printf("test_cruzader_orb_blocks_boss_ring_without_free_kill OK\n");
}

static void test_twins_ratings_and_moveset(void) {
    assert(ship_speed_rating(SHIP_TWINS) == 10);
    assert(ship_strength_rating(SHIP_TWINS) == 5);
    assert(ship_attack_rating(SHIP_TWINS) == 3);
    assert(fabsf(ship_size_multiplier(SHIP_TWINS) - 1.25f) < 0.001f); /* 25% bigger than B-20 */

    /* The single fastest ship in the fleet (10 > B-20's own 7). */
    assert(ship_speed_multiplier(SHIP_TWINS) > 1.0f);
    assert(fabsf(ship_speed_multiplier(SHIP_TWINS) - 10.0f / 7.0f) < 0.001f);
    /* Same Strength as B-20 (5 == 5) - a full, standard hit per twin. */
    assert(fabsf(ship_damage_taken_multiplier(SHIP_TWINS) - 1.0f) < 0.001f);

    assert(ship_shoot_mode_slot_count(SHIP_TWINS) == 2);
    assert(ship_shoot_mode_for_slot(SHIP_TWINS, 0) == SHOOT_MODE_TWINS_ALTERNATE);
    assert(ship_shoot_mode_for_slot(SHIP_TWINS, 1) == SHOOT_MODE_TWINS_MIRROR);
    printf("test_twins_ratings_and_moveset OK\n");
}

/* Mode 1 (default): one shot per activation, alternating muzzle between the
 * two twins, combined 4 shots/sec (2/sec per twin). */
static void test_twins_alternating_fire(void) {
    GameState gs;
    EventQueue events;
    start_game_as_twins(&gs, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_TWINS_ALTERNATE);

    float right_x = gs.player.twins_right_x;
    float left_x = gs.player.twins_left_x;
    assert(right_x > left_x);

    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);

    int found = 0, first_idx = -1;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive) continue;
        found++;
        first_idx = i;
    }
    assert(found == 1); /* one shot per activation, not a simultaneous pair */
    assert(gs.player_shots[first_idx].style_ship == SHIP_TWINS);
    assert(gs.player_shots[first_idx].kind == PROJECTILE_KIND_NORMAL);
    assert(fabsf(gs.player_shots[first_idx].damage - BASE_PLAYER_DAMAGE) < 0.001f);
    assert(fabsf(gs.player_shots[first_idx].x - right_x) < 0.001f); /* first shot from the right twin */
    assert(fabsf(gs.player.fire_cooldown - TWINS_ALTERNATE_FIRE_COOLDOWN) < 0.001f);
    assert(fabsf(TWINS_ALTERNATE_FIRE_COOLDOWN - 0.25f) < 0.001f); /* 4 shots/sec combined */

    gs.player.fire_cooldown = 0.0f;
    game_update(&gs, &fire, 0.016f, &events);
    found = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive || i == first_idx) continue;
        found++;
        assert(fabsf(gs.player_shots[i].x - left_x) < 0.001f); /* second shot alternates to the left twin */
    }
    assert(found == 1);
    printf("test_twins_alternating_fire OK\n");
}

/* Life is tracked per twin: an enemy shot overlapping only one twin's own
 * hitbox damages only that twin, and killing one twin transfers control to
 * the survivor (control point snapped onto its own position) without
 * ending the run - only killing both does. */
static void test_twins_individual_damage_and_control_transfer(void) {
    GameState gs;
    EventQueue events;
    start_game_as_twins(&gs, &events);

    float right_x = gs.player.twins_right_x;
    float left_x = gs.player.twins_left_x;
    assert(fabsf(gs.player.twins_right_life - PLAYER_LIFE_MAX) < 0.001f);
    assert(fabsf(gs.player.twins_left_life - PLAYER_LIFE_MAX) < 0.001f);

    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = right_x;
    gs.enemy_shots[0].y = gs.player.y;
    gs.enemy_shots[0].vx = 0.0f;
    gs.enemy_shots[0].vy = 10.0f;
    gs.enemy_shots[0].enemy_kind = ENEMY_PROJECTILE_ORB;
    gs.enemy_shots[0].half_len = 6.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.twins_right_life < PLAYER_LIFE_MAX);
    assert(fabsf(gs.player.twins_left_life - PLAYER_LIFE_MAX) < 0.001f); /* untouched */
    assert(gs.player.twins_right_alive);
    assert(gs.player.twins_left_alive);
    assert(gs.player.alive);

    /* Drive the right twin's life low, then land one more hit to kill it
     * outright rather than simulating dozens of real hits. */
    gs.player.twins_right_life = 5.0f;
    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = gs.player.twins_right_x;
    gs.enemy_shots[0].y = gs.player.y;
    gs.enemy_shots[0].vx = 0.0f;
    gs.enemy_shots[0].vy = 10.0f;
    gs.enemy_shots[0].enemy_kind = ENEMY_PROJECTILE_ORB;
    gs.enemy_shots[0].half_len = 6.0f;
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.player.twins_right_alive);
    assert(gs.player.twins_left_alive);
    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    /* The dead twin's own life bar reads exactly 0. */
    assert(gs.player.twins_right_life == 0.0f);
    /* Control snapped onto the survivor - x now equals the left twin's own
     * position, not wherever the right twin was. */
    assert(fabsf(gs.player.x - left_x) < 1.0f);
    /* Losing a twin immediately forces mode 1, and mode 2 is now locked
     * out entirely - pressing key 2 does nothing for the rest of the run. */
    assert(gs.player.shoot_mode == SHOOT_MODE_TWINS_ALTERNATE);
    InputCommand mode2 = no_input();
    mode2.shoot_mode_2_pressed = true;
    float locked_left_x = gs.player.twins_left_x;
    game_update(&gs, &mode2, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_TWINS_ALTERNATE); /* still locked out */
    assert(fabsf(gs.player.twins_left_x - locked_left_x) < 1.0f); /* no jump from the rejected switch */

    /* From here on, every shot originates from the survivor. */
    InputCommand fire = no_input();
    fire.fire_held = true;
    gs.player.fire_cooldown = 0.0f;
    memset(&gs.player_shots, 0, sizeof(gs.player_shots));
    game_update(&gs, &fire, 0.016f, &events);
    int idx = -1;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive) {
            idx = i;
            break;
        }
    }
    assert(idx >= 0);
    assert(fabsf(gs.player_shots[idx].x - gs.player.twins_left_x) < 1.0f);

    /* Killing the second twin ends the run, same as any other ship. */
    gs.player.twins_left_life = 5.0f;
    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = gs.player.twins_left_x;
    gs.enemy_shots[0].y = gs.player.y;
    gs.enemy_shots[0].vx = 0.0f;
    gs.enemy_shots[0].vy = 10.0f;
    gs.enemy_shots[0].enemy_kind = ENEMY_PROJECTILE_ORB;
    gs.enemy_shots[0].half_len = 6.0f;
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.player.twins_left_alive);
    assert(!gs.player.alive);
    assert(gs.state == STATE_GAME_OVER);
    printf("test_twins_individual_damage_and_control_transfer OK\n");
}

/* Mode 1 (default): rigid formation - both twins move together, keeping a
 * fixed gap. Mode 2: the right twin free-flies under direct input, the left
 * twin mirrors its position around the twins' own current midpoint (not
 * always screen-center - see twins_mirror_center_x's own doc comment). */
static void test_twins_mirrored_flight(void) {
    GameState gs;
    EventQueue events;
    start_game_as_twins(&gs, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_TWINS_ALTERNATE);

    float start_right = gs.player.twins_right_x;
    float start_left = gs.player.twins_left_x;
    assert(fabsf(gs.player.twins_right_x - gs.player.twins_left_x - TWINS_FORMATION_GAP) < 0.001f);

    InputCommand right_input = no_input();
    right_input.move_right = true;
    game_update(&gs, &right_input, 0.1f, &events);
    assert(gs.player.twins_right_x > start_right);
    assert(gs.player.twins_left_x > start_left);
    /* Gap preserved - both moved together, not independently. */
    assert(fabsf((gs.player.twins_right_x - gs.player.twins_left_x) -
                 (start_right - start_left)) < 0.001f);

    float pre_switch_right = gs.player.twins_right_x;
    float pre_switch_left = gs.player.twins_left_x;

    InputCommand mode2 = no_input();
    mode2.shoot_mode_2_pressed = true;
    game_update(&gs, &mode2, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_TWINS_MIRROR);
    /* Switching modes must never teleport either twin - each stays exactly
     * where it currently is the instant the mode changes. */
    assert(fabsf(gs.player.twins_right_x - pre_switch_right) < 0.001f);
    assert(fabsf(gs.player.twins_left_x - pre_switch_left) < 0.001f);

    float before_right = gs.player.twins_right_x;
    float before_left = gs.player.twins_left_x;
    float mirror_center = gs.player.twins_mirror_center_x;
    game_update(&gs, &right_input, 0.1f, &events);
    assert(gs.player.twins_right_x > before_right); /* right twin moves right, under direct input */
    assert(gs.player.twins_left_x < before_left); /* left twin mirrors - moves the opposite way */
    /* Mirrored around the twins' own current midpoint at the moment mode 2
     * activated, which - since they'd already moved together in formation
     * mode first - is not screen-center. */
    assert(fabsf(gs.player.twins_left_x - (2.0f * mirror_center - gs.player.twins_right_x)) < 0.001f);
    printf("test_twins_mirrored_flight OK\n");
}

/* Regression for a reported bug: switching modes must never snap either
 * twin's own actual position. Mode 2 re-anchors the mirror axis to the
 * twins' own current midpoint (so each twin starts mirroring from exactly
 * where it already is, not some stale screen-center-relative spot); mode 1
 * re-centers the formation target on their own current midpoint too, so
 * they visibly fly toward each other (eased at TWINS_FORMATION_REJOIN_SPEED)
 * instead of snapping into formation instantly, or - the reported bug -
 * springing back to wherever they were before the previous mode-1 switch
 * the next time mode 2 is re-selected. */
static void test_twins_mode_switch_reanchors_without_teleport(void) {
    GameState gs;
    EventQueue events;
    start_game_as_twins(&gs, &events);

    InputCommand mode2 = no_input();
    mode2.shoot_mode_2_pressed = true;
    game_update(&gs, &mode2, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_TWINS_MIRROR);

    /* Spread the twins far apart under mirrored control. */
    InputCommand right_input = no_input();
    right_input.move_right = true;
    for (int i = 0; i < 10; i++) game_update(&gs, &right_input, 0.1f, &events);
    float spread_right = gs.player.twins_right_x;
    float spread_left = gs.player.twins_left_x;
    assert(spread_right - spread_left > TWINS_FORMATION_GAP * 2.0f); /* meaningfully spread apart */

    /* Switching to mode 1 must NOT snap them into formation instantly -
     * their own actual positions are unchanged the very same frame the
     * mode switch happens. */
    InputCommand mode1 = no_input();
    mode1.shoot_mode_1_pressed = true;
    game_update(&gs, &mode1, 0.001f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_TWINS_ALTERNATE);
    assert(fabsf(gs.player.twins_right_x - spread_right) < 1.0f);
    assert(fabsf(gs.player.twins_left_x - spread_left) < 1.0f);
    assert(gs.player.twins_right_x - gs.player.twins_left_x > TWINS_FORMATION_GAP * 1.5f); /* still spread */

    /* Over subsequent frames they ease closer together - monotonically,
     * never overshooting outward - eventually converging on the standard
     * formation gap, same as flying toward each other under their own
     * power. */
    float prev_gap = gs.player.twins_right_x - gs.player.twins_left_x;
    InputCommand none = no_input();
    for (int i = 0; i < 200 && fabsf(prev_gap - TWINS_FORMATION_GAP) > 0.01f; i++) {
        game_update(&gs, &none, 0.1f, &events);
        float gap = gs.player.twins_right_x - gs.player.twins_left_x;
        assert(gap <= prev_gap + 0.001f);
        prev_gap = gap;
    }
    assert(fabsf(prev_gap - TWINS_FORMATION_GAP) < 0.01f); /* fully reformed */

    /* Switching back to mode 2 now must keep these exact reformed
     * positions as the new starting point - NOT revert to the spread-apart
     * positions from before the mode-1 switch (the reported bug). */
    float reformed_right = gs.player.twins_right_x;
    float reformed_left = gs.player.twins_left_x;
    game_update(&gs, &mode2, 0.001f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_TWINS_MIRROR);
    assert(fabsf(gs.player.twins_right_x - reformed_right) < 0.5f);
    assert(fabsf(gs.player.twins_left_x - reformed_left) < 0.5f);
    assert(fabsf(gs.player.twins_right_x - spread_right) > 1.0f); /* not the old spread-apart values */
    printf("test_twins_mode_switch_reanchors_without_teleport OK\n");
}

/* Enemy CONTACT (not a projectile hit) is an instant kill, bypassing
 * damage_twin's own gradual life-loss path entirely (see
 * kill_player_hitbox in usecases/game_logic.c) - kill_twin must still zero
 * that twin's own life field itself, so its life bar never reads a stale
 * nonzero value after an instant death. */
static void test_twins_enemy_contact_zeroes_dead_twins_life(void) {
    GameState gs;
    EventQueue events;
    start_game_as_twins(&gs, &events);

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.twins_right_x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.player.twins_right_alive);
    assert(gs.player.twins_right_life == 0.0f);
    assert(gs.player.twins_left_alive);
    assert(fabsf(gs.player.twins_left_life - PLAYER_LIFE_MAX) < 0.001f);
    assert(gs.player.alive);
    printf("test_twins_enemy_contact_zeroes_dead_twins_life OK\n");
}

/* The power orb heals whichever twin(s) are still alive back to full and
 * grants the super beam - same "full refill" every other ship's own orb
 * capture already does - but must never resurrect a twin that's already
 * dead: its own life bar stays at 0. */
static void test_twins_orb_capture_heals_survivor_not_dead_twin(void) {
    GameState gs;
    EventQueue events;
    start_game_as_twins(&gs, &events);

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.twins_right_x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;
    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.player.twins_right_alive);

    gs.player.twins_left_life = 40.0f;
    gs.orb.alive = true;
    gs.orb.x = gs.player.twins_left_x;
    gs.orb.y = gs.player.y;
    gs.orb.size = 20.0f;
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.orb.alive);
    assert(fabsf(gs.player.super_beam_timer - SUPER_BEAM_DURATION) < 0.01f);
    assert(fabsf(gs.player.twins_left_life - PLAYER_LIFE_MAX) < 0.01f); /* survivor healed */
    assert(gs.player.twins_right_life == 0.0f); /* dead twin NOT resurrected */
    assert(!gs.player.twins_right_alive);
    printf("test_twins_orb_capture_heals_survivor_not_dead_twin OK\n");
}

/* The super beam (granted by the orb, see the previous test) sweeps a
 * column from each twin still alive - both columns while both are
 * standing, only the survivor's own column once one has died - rather than
 * a single column from whatever p->x happens to mean for the ship's own
 * current flight mode. */
static void test_twins_super_beam_sweeps_both_twins_columns(void) {
    GameState gs;
    EventQueue events;
    start_game_as_twins(&gs, &events);
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;

    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.twins_right_x;
    gs.enemies[0].y = gs.player.y - 100.0f;
    gs.enemies[0].size = 10.0f;
    gs.enemies[0].fire_timer = 999.0f;

    gs.enemies[1].alive = true;
    gs.enemies[1].x = gs.player.twins_left_x;
    gs.enemies[1].y = gs.player.y - 100.0f;
    gs.enemies[1].size = 10.0f;
    gs.enemies[1].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    /* Both destroyed - one beam column per twin. */
    assert(!gs.enemies[0].alive);
    assert(!gs.enemies[1].alive);

    /* Once solo (killed here via plain contact, with no beam active yet -
     * the beam itself would otherwise grant immunity), only the
     * survivor's own column remains active - an enemy lined up with the
     * dead twin's old position no longer gets swept. */
    start_game_as_twins(&gs, &events);
    float dead_right_x = gs.player.twins_right_x;
    gs.enemies[0].alive = true;
    gs.enemies[0].x = dead_right_x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.player.twins_right_alive);

    gs.player.super_beam_timer = SUPER_BEAM_DURATION;
    gs.enemies[1].alive = true;
    gs.enemies[1].x = dead_right_x;
    gs.enemies[1].y = gs.player.y - 100.0f;
    gs.enemies[1].size = 10.0f;
    gs.enemies[1].fire_timer = 999.0f;
    game_update(&gs, &none, 0.001f, &events);
    assert(gs.enemies[1].alive); /* no column at the dead twin's old spot */
    printf("test_twins_super_beam_sweeps_both_twins_columns OK\n");
}

/* Regression coverage for a real bug: the boss's own menace ring used to
 * call kill_player directly on any touch, unconditionally killing the WHOLE
 * player - for SHIP_TWINS that meant touching the ring with just one twin
 * took the other, untouched twin down too. The ring's own consequence block
 * now routes through kill_player_hitbox (the same dispatcher every other
 * hazard in check_collisions already uses), so only the twin that actually
 * touched it dies - the boss's own detonation stays unconditional either
 * way (same "avoid an invincible-player stalemate" rationale as
 * test_boss_ring_contact_kills_boss_and_ignores_super_beam_but_not_god_mode
 * above). */
static void test_twins_boss_ring_kills_only_the_touched_twin(void) {
    GameState gs;
    EventQueue events;
    start_game_as_twins(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    gs.boss.x = gs.player.twins_right_x;
    gs.boss.y = gs.player.y;
    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    /* The boss still dies (unconditional detonation), but only the right
     * twin - the one actually touched - dies with it. */
    assert(!gs.boss.alive);
    assert(!gs.player.twins_right_alive);
    assert(gs.player.twins_left_alive);
    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    printf("test_twins_boss_ring_kills_only_the_touched_twin OK\n");
}

/* Regression coverage for a real bug: the power orb's own super beam swept
 * BOTH Antartica's and Frosty's columns using Antartica's own shared p->y
 * as the "enemy must be above this" cutoff for both, even though Frosty can
 * be at a very different height than Antartica (independent WASD/arrow
 * control - see update_player's own SHIP_ANTARTICA branch). An enemy level
 * with Frosty's own actual y, but below Antartica's own y, would wrongly
 * survive under both columns. update_super_beam now reads each origin's own
 * y (see player_beam_origins in usecases/game_logic.c) instead of a single
 * shared one - draw_super_beam (adapters/sdl_renderer.c) got the same fix,
 * unverified here since these tests never touch the renderer. */
static void test_antartica_super_beam_columns_use_each_bodys_own_y(void) {
    GameState gs;
    EventQueue events;
    start_game_as_antartica(&gs, &events);
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;

    /* Antartica moves well up the screen; Frosty stays much lower. */
    gs.player.frosty_x = gs.player.x;
    gs.player.frosty_y = gs.player.y - 50.0f;
    gs.player.y -= 200.0f;

    /* This enemy sits between the two: above Frosty's own y, but below
     * Antartica's - the old bug (a single shared p->y, Antartica's own)
     * would skip it under both columns. */
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.frosty_x;
    gs.enemies[0].y = gs.player.frosty_y - 30.0f;
    gs.enemies[0].size = 10.0f;
    gs.enemies[0].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.enemies[0].alive);
    printf("test_antartica_super_beam_columns_use_each_bodys_own_y OK\n");
}

/* Regression coverage for a real bug: once Antartica died (Frosty alone
 * surviving), gs.player.x/y stayed frozen at Antartica's own last position
 * forever - nothing fed Frosty's own independent position back into the
 * single shared x/y field every ship-agnostic single-position consumer
 * reads as "the player" (the boss's own chase target in update_boss, the
 * power orb's own super beam origin). That made the boss keep closing in on
 * the spot Antartica died at, never Frosty, even though Frosty was the one
 * actually still flying. update_player's own SHIP_ANTARTICA branch now
 * mirrors p->x/p->y onto Frosty's own position every frame once Antartica
 * is dead (same "keep the shared position live for whoever's actually
 * still relevant" idea as kill_twin's own control-transfer step) - this
 * asserts both that mirroring and that the boss actually closes in on
 * Frosty's real position, not Antartica's stale one, same proven pattern as
 * test_boss_always_advances_toward_stationary_player above. */
static void test_antartica_boss_chases_frosty_after_antartica_dies(void) {
    GameState gs;
    EventQueue events;
    start_game_as_antartica(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    float antartica_death_x = gs.player.x;
    float antartica_death_y = gs.player.y;

    /* Kill Antartica herself via plain enemy contact - Frosty survives,
     * same "one can die while the other keeps going" rule as The Twins. */
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.player.x;
    gs.enemies[0].y = gs.player.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;
    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.player.antartica_alive);
    assert(gs.player.frosty_alive);
    assert(gs.player.alive);

    /* Move Frosty well clear of where Antartica died, then let
     * update_player's own mirroring pick it up on the next frame. */
    gs.player.frosty_x = antartica_death_x + 150.0f;
    gs.player.frosty_y = antartica_death_y;
    game_update(&gs, &none, 0.001f, &events);
    assert(fabsf(gs.player.x - gs.player.frosty_x) < 0.001f);
    assert(fabsf(gs.player.y - gs.player.frosty_y) < 0.001f);

    /* Position the boss a safe distance from Frosty - well outside its own
     * menace ring (whose radius depends on the boss's own randomly-rolled
     * size, see BOSS_MENACE_RING_RATIO - spawn_boss doesn't fix it, so it
     * can't be hardcoded here the way test_boss_always_advances_toward_
     * stationary_player's own 100px gap does) plus comfortably more than
     * the boss could ever close over the 20 frames below
     * (BOSS_SPEED_MULTIPLIER * PLAYER_SPEED * 20 * 0.05, well under 80px) -
     * so ring contact (which would end the run and freeze both positions,
     * reading as "stopped closing in" below) can never happen here
     * regardless of the random roll. */
    float ring_radius = gs.boss.size * BOSS_MENACE_RING_RATIO;
    float frosty_radius = fmaxf(PLAYER_WIDTH, PLAYER_HEIGHT) * ANTARTICA_FROSTY_SIZE_MULTIPLIER / 2.0f;
    float start_distance = ring_radius + frosty_radius + 80.0f;
    gs.boss.x = gs.player.frosty_x;
    gs.boss.y = gs.player.frosty_y - start_distance;

    float prev_dist = start_distance;
    bool ever_failed_to_close_in = false;
    for (int i = 0; i < 20; i++) {
        game_update(&gs, &none, 0.05f, &events);
        float dx = gs.player.frosty_x - gs.boss.x;
        float dy = gs.player.frosty_y - gs.boss.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist >= prev_dist - 0.001f) ever_failed_to_close_in = true;
        prev_dist = dist;
    }
    assert(!ever_failed_to_close_in);
    printf("test_antartica_boss_chases_frosty_after_antartica_dies OK\n");
}

/* Regression coverage for the same real bug as
 * test_twins_boss_ring_kills_only_the_touched_twin above, just for
 * SHIP_ANTARTICA: the boss's own menace ring used to call kill_player
 * directly on any touch, so touching it with only Frosty took Antartica
 * down too, even though she was nowhere near it. */
static void test_antartica_boss_ring_kills_only_the_touched_body(void) {
    GameState gs;
    EventQueue events;
    start_game_as_antartica(&gs, &events);

    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);

    /* Move Frosty well clear of Antartica so only Frosty's own hitbox is
     * anywhere near the boss. */
    gs.player.frosty_x = gs.player.x + 200.0f;
    gs.player.frosty_y = gs.player.y;
    gs.boss.x = gs.player.frosty_x;
    gs.boss.y = gs.player.frosty_y;
    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    /* The boss still dies (unconditional detonation), but only Frosty - the
     * one actually touched - dies with it; Antartica, untouched, survives. */
    assert(!gs.boss.alive);
    assert(!gs.player.frosty_alive);
    assert(gs.player.antartica_alive);
    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);
    printf("test_antartica_boss_ring_kills_only_the_touched_body OK\n");
}

static void test_buckler_ratings_and_moveset(void) {
    assert(ship_speed_rating(SHIP_BUCKLER) == 6);
    assert(ship_strength_rating(SHIP_BUCKLER) == 8);
    assert(ship_attack_rating(SHIP_BUCKLER) == 6);
    assert(fabsf(ship_size_multiplier(SHIP_BUCKLER) - 1.0f) < 0.001f); /* same size as B-20 */

    assert(ship_shoot_mode_slot_count(SHIP_BUCKLER) == 1);
    assert(ship_shoot_mode_for_slot(SHIP_BUCKLER, 0) == SHOOT_MODE_BUCKLER_CANNON);
    printf("test_buckler_ratings_and_moveset OK\n");
}

/* Buckler's own (only) mode: keys 1-5 fire from 5 fixed-direction cannons
 * (west, north-west, north, north-east, east) instead of switching shoot
 * modes - update_shoot_mode_switch never touches Player.shoot_mode for
 * SHIP_BUCKLER, so it must stay pinned at SHOOT_MODE_BUCKLER_CANNON even
 * while every key is pressed. "Only one cannon at a time; if two are
 * pressed, the one pressed first fires" - holding key 1 then also pressing
 * key 3 must keep firing west, not switch to north, until key 1 releases. */
static void test_buckler_cannon_directions_and_first_pressed_wins(void) {
    GameState gs;
    EventQueue events;
    start_game_as_buckler(&gs, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_BUCKLER_CANNON);

    InputCommand west = no_input();
    west.shoot_mode_1_held = true;
    game_update(&gs, &west, 0.016f, &events);
    assert(gs.player.shoot_mode == SHOOT_MODE_BUCKLER_CANNON); /* never switches */
    assert(gs.player.buckler_active_cannon == 1);

    int found = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive) continue;
        found++;
        assert(gs.player_shots[i].style_ship == SHIP_BUCKLER);
        assert(gs.player_shots[i].kind == PROJECTILE_KIND_BUCKLER_ORB);
        assert(fabsf(gs.player_shots[i].damage - BASE_PLAYER_DAMAGE) < 0.001f);
        assert(gs.player_shots[i].vx < 0.0f); /* west */
        assert(fabsf(gs.player_shots[i].vy) < 0.001f);
    }
    assert(found == 1);
    assert(fabsf(gs.player.fire_cooldown - BUCKLER_CANNON_FIRE_COOLDOWN) < 0.001f);
    assert(fabsf(BUCKLER_CANNON_FIRE_COOLDOWN - 0.5f) < 0.001f); /* 2 shots/sec */

    /* Key 3 (north) is now held too, but key 1 (west) was pressed first and
     * is still held - it must keep firing exclusively, no switch to north. */
    memset(&gs.player_shots, 0, sizeof(gs.player_shots));
    InputCommand west_and_north = no_input();
    west_and_north.shoot_mode_1_held = true;
    west_and_north.shoot_mode_3_held = true;
    gs.player.fire_cooldown = 0.0f;
    game_update(&gs, &west_and_north, 0.016f, &events);
    assert(gs.player.buckler_active_cannon == 1);
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive) continue;
        assert(gs.player_shots[i].vx < 0.0f); /* still west, not north */
        assert(fabsf(gs.player_shots[i].vy) < 0.001f);
    }

    /* Releasing key 1 hands off to key 3 (still held) - north from here on. */
    memset(&gs.player_shots, 0, sizeof(gs.player_shots));
    InputCommand north_only = no_input();
    north_only.shoot_mode_3_held = true;
    gs.player.fire_cooldown = 0.0f;
    game_update(&gs, &north_only, 0.016f, &events);
    assert(gs.player.buckler_active_cannon == 3);
    found = 0;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (!gs.player_shots[i].alive) continue;
        found++;
        assert(fabsf(gs.player_shots[i].vx) < 0.001f);
        assert(gs.player_shots[i].vy < 0.0f); /* north */
    }
    assert(found == 1);

    /* No key held at all - no shot, no active cannon. */
    memset(&gs.player_shots, 0, sizeof(gs.player_shots));
    InputCommand none = no_input();
    gs.player.fire_cooldown = 0.0f;
    game_update(&gs, &none, 0.016f, &events);
    assert(gs.player.buckler_active_cannon == 0);
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) assert(!gs.player_shots[i].alive);

    printf("test_buckler_cannon_directions_and_first_pressed_wins OK\n");
}

/* Spacebar: the protective orb - same defensive behavior/duration/cooldown
 * as Cruzader's own deflector orb, just destroying enemy shots in range
 * outright (no player damage) instead of reflecting them back, and
 * triggered by the spacebar's own edge instead of a shoot-mode key. */
static void test_buckler_orb_blocks_shots_and_reverts_with_cooldown(void) {
    GameState gs;
    EventQueue events;
    start_game_as_buckler(&gs, &events);

    InputCommand fire_press = no_input();
    fire_press.fire_held = true;
    fire_press.fire_pressed = true;
    game_update(&gs, &fire_press, 0.016f, &events);

    assert(gs.player.shoot_mode == SHOOT_MODE_BUCKLER_CANNON); /* never touched */
    assert(gs.player.buckler_orb_timer > 0.0f);

    /* Holding the spacebar down doesn't re-trigger every frame - only the
     * edge does, same as Cruzader's own key-2 orb trigger. */
    float timer_before_retry = gs.player.buckler_orb_timer;
    InputCommand fire_hold_only = no_input();
    fire_hold_only.fire_held = true;
    game_update(&gs, &fire_hold_only, 0.016f, &events);
    assert(gs.player.buckler_orb_timer < timer_before_retry);

    /* An enemy shot inside the orb radius is destroyed outright - no player
     * damage, and (unlike Cruzader's own orb) never marked reflected or
     * left flying back at the enemies. */
    gs.enemy_shots[0].alive = true;
    gs.enemy_shots[0].x = gs.player.x;
    gs.enemy_shots[0].y = gs.player.y - BUCKLER_ORB_RADIUS * 0.9f;
    gs.enemy_shots[0].vx = 0.0f;
    gs.enemy_shots[0].vy = 100.0f;
    gs.enemy_shots[0].enemy_kind = ENEMY_PROJECTILE_BEAM;
    gs.enemy_shots[0].half_len = 12.0f;
    gs.enemy_shots[0].half_wid = 3.0f;
    float life_before = gs.player.life;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.016f, &events);

    assert(!gs.enemy_shots[0].alive); /* destroyed, not reflected */
    assert(fabsf(gs.player.life - life_before) < 0.001f);

    /* Once the active window fully elapses, the cooldown starts immediately
     * and pressing the spacebar again does nothing until it too elapses. */
    gs.player.buckler_orb_timer = 0.016f;
    game_update(&gs, &none, 0.016f, &events);
    assert(gs.player.buckler_orb_timer <= 0.0f);
    assert(gs.player.buckler_orb_cooldown_timer > 0.0f);

    game_update(&gs, &fire_press, 0.016f, &events);
    assert(gs.player.buckler_orb_timer <= 0.0f); /* not reactivated during cooldown */

    gs.player.buckler_orb_cooldown_timer = 0.0f;
    game_update(&gs, &fire_press, 0.016f, &events);
    assert(gs.player.buckler_orb_timer > 0.0f); /* free to trigger again once cooldown clears */

    printf("test_buckler_orb_blocks_shots_and_reverts_with_cooldown OK\n");
}

static void test_buckler_orb_blocks_boss_ring_without_free_kill(void) {
    GameState gs;
    EventQueue events;
    start_game_as_buckler(&gs, &events);

    InputCommand fire_press = no_input();
    fire_press.fire_held = true;
    fire_press.fire_pressed = true;
    game_update(&gs, &fire_press, 0.016f, &events);
    assert(gs.player.buckler_orb_timer > 0.0f);

    gs.boss.alive = true;
    gs.boss.size = 100.0f;
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(gs.player.alive);
    assert(gs.boss.alive);
    assert(gs.state == STATE_GAME);

    gs.player.buckler_orb_timer = 0.0f;
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.player.alive);
    printf("test_buckler_orb_blocks_boss_ring_without_free_kill OK\n");
}

/* All 4 arrows navigate the ship-select grid, not just left/right: up/down
 * step by a full SHIP_SELECT_GRID_COLS-wide row, clamped at the last real
 * ship (SHIP_COUNT - 1) rather than wrapping into one of the locked
 * placeholder slots that fill out the rest of the grid. Deliberately placed
 * here, after every RNG-sensitive test earlier in this suite (Mothership's
 * random child-mode rolls, spawner timing, etc.) rather than back up near
 * test_ship_select_navigation_clamps_at_ends - this suite runs on a single
 * unseeded rand() stream shared across every test in one process, and an
 * earlier test's own game_init/game_update calls consume from that same
 * stream, so inserting a new test earlier than an RNG-sensitive one can
 * shift its draws enough to change outcomes it hardcodes assertions
 * against. */
static void test_ship_select_up_down_navigate_grid_rows(void) {
    GameState gs;
    EventQueue events;
    game_init(&gs, DESIGN_W, DESIGN_H);
    InputCommand confirm = no_input();
    confirm.confirm_pressed = true;
    game_update(&gs, &confirm, 0.016f, &events); /* -> STATE_DIFFICULTY_SELECT */
    game_update(&gs, &confirm, 0.016f, &events); /* -> STATE_SHIP_SELECT */
    assert(gs.selected_ship == SHIP_B20);

    InputCommand down = no_input();
    down.nav_down_pressed = true;
    game_update(&gs, &down, 0.016f, &events);
    assert(gs.selected_ship == SHIP_CRUZADER); /* row 0 col 0 -> row 1 col 0, the only slot 4 further down */

    /* Cruzader's row has no slot 4 further down (that would land past
     * SHIP_COUNT, in locked-placeholder territory) - down is a no-op here. */
    game_update(&gs, &down, 0.016f, &events);
    assert(gs.selected_ship == SHIP_CRUZADER);

    InputCommand up = no_input();
    up.nav_up_pressed = true;
    game_update(&gs, &up, 0.016f, &events);
    assert(gs.selected_ship == SHIP_B20); /* back up to row 0 col 0 */

    /* No further row above row 0 - up is a no-op, doesn't go negative. */
    game_update(&gs, &up, 0.016f, &events);
    assert(gs.selected_ship == SHIP_B20);

    /* From C-24 (row 0 col 1), down now lands on The Twins (row 1 col 1) -
     * a real ship now that The Twins joined the fleet, no longer one of
     * the locked placeholder slots that used to fill out the rest of
     * row 1. */
    InputCommand right = no_input();
    right.nav_right_pressed = true;
    game_update(&gs, &right, 0.016f, &events);
    assert(gs.selected_ship == SHIP_C24);
    game_update(&gs, &down, 0.016f, &events);
    assert(gs.selected_ship == SHIP_TWINS);

    /* Twins' own row (row 1) has no slot further down either. */
    game_update(&gs, &down, 0.016f, &events);
    assert(gs.selected_ship == SHIP_TWINS);

    /* Left/right within row 1 moves directly between Cruzader and Twins. */
    InputCommand left = no_input();
    left.nav_left_pressed = true;
    game_update(&gs, &left, 0.016f, &events);
    assert(gs.selected_ship == SHIP_CRUZADER);

    printf("test_ship_select_up_down_navigate_grid_rows OK\n");
}

static void test_erratic_enemy_chance_scales_with_bosses_defeated(void) {
    assert(fabsf(spawner_erratic_enemy_chance(0) - 0.0f) < 0.001f);
    assert(fabsf(spawner_erratic_enemy_chance(1) - 0.10f) < 0.001f);
    assert(fabsf(spawner_erratic_enemy_chance(2) - 0.20f) < 0.001f);
    assert(fabsf(spawner_erratic_enemy_chance(3) - 0.30f) < 0.001f);
    /* Capped at 100%, never overshoots past a fistful of defeats. */
    assert(fabsf(spawner_erratic_enemy_chance(10) - 1.0f) < 0.001f);
    assert(fabsf(spawner_erratic_enemy_chance(50) - 1.0f) < 0.001f);
    printf("test_erratic_enemy_chance_scales_with_bosses_defeated OK\n");
}

/* bosses_defeated tracks actual defeats, distinct from boss_count (which
 * counts appearances - see test_boss_spawns_at_500_points_with_correct_hits_required) -
 * both the direct-shot-down path (defeat_current_boss) and the ring-
 * detonation path must advance it. */
static void test_boss_defeat_increments_bosses_defeated(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    assert(gs.bosses_defeated == 0);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss_count == 1);
    defeat_current_boss(&gs, &events);
    assert(gs.bosses_defeated == 1);

    kill_enemies_until_boss_spawns(&gs, &events);
    assert(gs.boss_count == 2);
    /* This time, defeat it via the ring-detonation path instead of
     * shooting it down. */
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;
    gs.player.god_mode = true; /* survive the ring touch to inspect state after */
    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);
    assert(!gs.boss.alive);
    assert(gs.bosses_defeated == 2);
    printf("test_boss_defeat_increments_bosses_defeated OK\n");
}

/* No enemy flies erratically before the first boss defeat; once
 * bosses_defeated is 1, roughly ERRATIC_ENEMY_CHANCE_PER_BOSS_DEFEAT of
 * newly-spawned enemies should. Sampled across many independent spawns. */
static void test_erratic_enemies_start_appearing_after_first_boss_defeat(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);
    assert(gs.bosses_defeated == 0);

    for (int i = 0; i < 100; i++) {
        memset(&gs.enemies, 0, sizeof(gs.enemies));
        gs.spawn_timer = 0.0f;
        InputCommand none = no_input();
        game_update(&gs, &none, 0.001f, &events);
        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (gs.enemies[j].alive) assert(gs.enemies[j].movement_style == ENEMY_MOVEMENT_NORMAL);
        }
    }

    gs.bosses_defeated = 1;
    int erratic_count = 0, total = 0;
    for (int i = 0; i < 300; i++) {
        memset(&gs.enemies, 0, sizeof(gs.enemies));
        gs.spawn_timer = 0.0f;
        InputCommand none = no_input();
        game_update(&gs, &none, 0.001f, &events);
        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (!gs.enemies[j].alive) continue;
            total++;
            if (gs.enemies[j].movement_style != ENEMY_MOVEMENT_NORMAL) erratic_count++;
        }
    }
    assert(total > 0);
    float observed = (float)erratic_count / (float)total;
    /* Generous tolerance around the true 10% - this is sampling noise, not
     * an exact formula check (that's test_erratic_enemy_chance_scales_with_bosses_defeated
     * above). */
    assert(observed > 0.03f);
    assert(observed < 0.22f);
    printf("test_erratic_enemies_start_appearing_after_first_boss_defeat OK\n");
}

/* Pins down CIRCLE's exact orbit formula: a fixed-radius loop around a
 * center that drifts by the enemy's own vx/vy, so a regression here (e.g.
 * swapping sin/cos, or forgetting to drift the center) actually fails
 * instead of silently changing the shape. */
static void test_circle_enemy_orbits_a_drifting_center(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    Enemy *e = &gs.enemies[0];
    *e = (Enemy){0};
    e->alive = true;
    e->size = 20.0f;
    e->movement_style = ENEMY_MOVEMENT_CIRCLE;
    e->orbit_center_x = 100.0f;
    e->orbit_center_y = 100.0f;
    e->erratic_radius = 40.0f;
    e->wobble_phase = 0.0f; /* starts at angle 0: (center_x + radius, center_y) */
    e->vx = 5.0f;
    e->vy = 30.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.1f, &events);

    float expected_center_x = 100.0f + 5.0f * 0.1f;
    float expected_center_y = 100.0f + 30.0f * 0.1f;
    float expected_angle = ERRATIC_ENEMY_CIRCLE_ANGULAR_SPEED * 0.017453293f * 0.1f;
    float expected_x = expected_center_x + cosf(expected_angle) * 40.0f;
    float expected_y = expected_center_y + sinf(expected_angle) * 40.0f;

    assert(fabsf(e->orbit_center_x - expected_center_x) < 0.01f);
    assert(fabsf(e->orbit_center_y - expected_center_y) < 0.01f);
    assert(fabsf(e->x - expected_x) < 0.5f);
    assert(fabsf(e->y - expected_y) < 0.5f);
    /* Genuinely off the plain "straight fall" path a NORMAL enemy would take. */
    assert(fabsf(e->x - expected_center_x) > 1.0f);
    printf("test_circle_enemy_orbits_a_drifting_center OK\n");
}

/* RANDOM re-rolls its heading periodically but always keeps a downward
 * component, so - despite the lumpy path - it still nets real progress
 * toward the bottom of the screen over time, same as every other style. */
static void test_random_enemy_still_nets_downward_progress(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    Enemy *e = &gs.enemies[0];
    *e = (Enemy){0};
    e->alive = true;
    e->size = 20.0f;
    e->movement_style = ENEMY_MOVEMENT_RANDOM;
    e->x = 200.0f;
    e->y = 100.0f;
    e->wobble_phase = 0.05f; /* about to retarget on the very first tick */

    InputCommand none = no_input();
    float start_y = e->y;
    for (int i = 0; i < 120 && e->alive; i++) {
        game_update(&gs, &none, 0.05f, &events);
    }
    if (e->alive) assert(e->y > start_y);
    printf("test_random_enemy_still_nets_downward_progress OK\n");
}

static void test_spawner_eventually_spawns(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    InputCommand none = no_input();
    bool any_alive = false;
    for (int i = 0; i < 200 && !any_alive; i++) {
        game_update(&gs, &none, 0.05f, &events);
        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (gs.enemies[j].alive) {
                any_alive = true;
                break;
            }
        }
    }
    assert(any_alive);
    printf("test_spawner_eventually_spawns OK\n");
}

int main(void) {
    test_collision();
    test_difficulty();
    test_menu_start_transition();
    test_menu_confirm_leads_to_difficulty_select();
    test_difficulty_select_navigation_clamps_at_ends();
    test_difficulty_select_back_returns_to_menu_without_starting();
    test_difficulty_select_confirm_leads_to_ship_select();
    test_ship_select_navigation_clamps_at_ends();
    test_ship_select_back_returns_to_difficulty_select();
    test_ship_select_confirm_starts_game_with_chosen_ship();
    test_ship_ratings_and_multipliers();
    test_selected_ship_affects_speed_and_damage_taken();
    test_player_movement_clamped();
    test_player_can_reach_top_of_screen();
    test_super_beam_increases_player_speed();
    test_player_fire_cooldown();
    test_pause_toggle();
    test_pause_menu_exit_to_menu();
    test_enemy_kill_scores();
    test_player_enemy_collision_ends_game();
    test_laser_color_never_changes_regardless_of_score();
    test_orb_capture_grants_super_beam();
    test_player_invincible_during_super_beam();
    test_god_mode_toggles_on_and_off();
    test_god_mode_prevents_death();
    test_player_starts_at_full_life();
    test_enemy_projectile_hit_drains_life_without_killing();
    test_enemy_projectile_hits_exhaust_life_and_kill_player();
    test_enemy_ship_contact_kills_player_regardless_of_life();
    test_orb_capture_refills_life_to_full();
    test_god_mode_and_super_beam_prevent_life_loss();
    test_super_beam_neutralizes_without_normal_fire();
    test_shooting_orb_schedules_enemies_without_granting_beam();
    test_shooting_orb_assigns_random_delay_within_window();
    test_orb_falls_off_screen_when_uncaptured();
    test_orb_spawn_chance_is_not_always_or_never();
    test_boss_spawns_at_500_points_with_correct_hits_required();
    test_boss_warning_activates_50_points_before_boss();
    test_boss_warning_holds_through_the_gap_then_hands_off_to_boss_alive();
    test_boss_warning_stays_clear_after_boss_defeat();
    test_boss_warning_stays_clear_after_ring_detonation();
    test_boss_warning_re_arms_for_the_second_encounter();
    test_boss_dispatch_interval_formula();
    test_boss_dispatch_interval_shortens_across_encounters();
    test_boss_dispatches_enemy_from_beneath_itself_after_interval();
    test_boss_dispatched_enemy_lands_and_becomes_normal();
    test_boss_dispatched_enemy_can_be_killed_for_score();
    test_boss_fight_orb_spawn_uses_flat_chance_per_kill();
    test_boss_fight_never_spawns_orb_via_score_step();
    test_shooting_power_orb_during_boss_fight_never_damages_boss();
    test_super_beam_does_not_shield_against_ordinary_hazards_during_boss_fight();
    test_double_barrel_deals_half_the_damage_of_normal_mode();
    test_power_cannon_deals_triple_the_damage_of_normal_mode();
    test_boss_hit_pool_drops_by_the_exact_damage_landed();
    test_double_barrel_pair_together_cost_one_normal_hit();
    test_c24_shoot_mode_slots();
    test_default_shoot_mode_is_per_ship();
    test_c24_ignores_keys_beyond_its_own_mode_count();
    test_c24_omni_burst_fires_eight_shots_at_once();
    test_player_shots_spawn_trail_particles_same_as_enemy_shots();
    test_c24_mode2_projectile_hitbox_is_eight_times_bigger();
    test_boss_hits_required_increases_each_appearance();
    test_boss_reappearance_requires_fresh_points_since_defeat();
    test_large_defeat_bonus_never_chains_bosses();
    test_points_scored_during_boss_fight_do_not_shorten_gap();
    test_boss_arrival_makes_enemies_flee_and_projectiles_harmless();
    test_boss_defeat_awards_bonus_score();
    test_boss_ring_contact_destroys_both_boss_and_player();
    test_boss_ring_contact_kills_boss_and_ignores_super_beam_but_not_god_mode();
    test_boss_ring_radius_matches_menace_ratio();
    test_spawner_suspended_while_boss_alive();
    test_enemy_projectile_slot_reset_after_boss_fade();
    test_boss_always_advances_toward_stationary_player();
    test_boss_speed_matches_configured_multiplier();
    test_super_beam_damages_boss_periodically_while_sustained();
    test_mothership_ratings_and_moveset();
    test_mothership_child_mode_mostly_fixed_at_slot0();
    test_mothership_rapid_child_permanently_falls_back_to_mode1();
    test_mothership_dispatch_caps_and_resumes();
    test_mothership_child_launch_then_ai_handoff();
    test_mothership_formation_pulls_child_to_slot();
    test_mothership_child_takes_enemy_shot_damage_and_dies();
    test_mothership_child_dies_on_enemy_contact_and_enemy_dies_too();
    test_mothership_child_dies_on_boss_ring_without_defeating_boss();
    test_mothership_c24_styled_shot_still_gets_bigger_explosion_radius();
    test_shine_ratings_and_moveset();
    test_shine_twin_shards_close_together_and_halved_damage();
    test_shine_omni_burst_fires_twelve_and_reverts_to_mode1();
    test_shine_spiral_shot_is_longer_shard_at_two_per_second();
    test_cruzader_ratings_and_moveset();
    test_cruzader_twin_bolts_recolored_and_correct_rate();
    test_cruzader_orb_activates_reflects_and_reverts_with_cooldown();
    test_cruzader_passive_reflect_or_full_damage();
    test_cruzader_rockets_home_and_explode_with_power_cannon_radius();
    test_cruzader_survives_enemy_contact_for_flat_damage_but_not_boss_ring();
    test_cruzader_orb_blocks_boss_ring_without_free_kill();
    test_twins_ratings_and_moveset();
    test_twins_alternating_fire();
    test_twins_individual_damage_and_control_transfer();
    test_twins_mirrored_flight();
    test_twins_mode_switch_reanchors_without_teleport();
    test_twins_enemy_contact_zeroes_dead_twins_life();
    test_twins_orb_capture_heals_survivor_not_dead_twin();
    test_twins_super_beam_sweeps_both_twins_columns();
    test_twins_boss_ring_kills_only_the_touched_twin();
    test_antartica_super_beam_columns_use_each_bodys_own_y();
    test_antartica_boss_chases_frosty_after_antartica_dies();
    test_antartica_boss_ring_kills_only_the_touched_body();
    test_buckler_ratings_and_moveset();
    test_buckler_cannon_directions_and_first_pressed_wins();
    test_buckler_orb_blocks_shots_and_reverts_with_cooldown();
    test_buckler_orb_blocks_boss_ring_without_free_kill();
    test_ship_select_up_down_navigate_grid_rows();
    test_erratic_enemy_chance_scales_with_bosses_defeated();
    test_boss_defeat_increments_bosses_defeated();
    test_erratic_enemies_start_appearing_after_first_boss_defeat();
    test_circle_enemy_orbits_a_drifting_center();
    test_random_enemy_still_nets_downward_progress();
    test_spawner_eventually_spawns();
    printf("\nAll tests passed.\n");
    return 0;
}
