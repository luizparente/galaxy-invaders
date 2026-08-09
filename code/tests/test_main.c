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
    assert(gs.selected_ship == SHIP_SHINE); /* clamped at the last implemented ship */
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

/* The boss's detonation on ring contact is unconditional; only the
 * player's death respects invulnerability. Gating the whole interaction
 * on the player being killable used to deadlock the encounter: an
 * invulnerable player would have the boss sit on top of them at zero
 * distance indefinitely with nothing resolving. */
static void test_boss_ring_contact_kills_boss_but_spares_invincible_player(void) {
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

    /* super beam grants the same protection, with the same resolution */
    start_game(&gs, &events);
    for (int kill = 0; kill < 50; kill++) kill_one_enemy(&gs, &events);
    assert(gs.boss.alive);
    gs.player.super_beam_timer = SUPER_BEAM_DURATION;
    gs.boss.x = gs.player.x;
    gs.boss.y = gs.player.y;

    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.boss.alive);
    assert(gs.player.alive);
    assert(gs.state == STATE_GAME);

    printf("test_boss_ring_contact_kills_boss_but_spares_invincible_player OK\n");
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
    test_boss_ring_contact_kills_boss_but_spares_invincible_player();
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
    test_spawner_eventually_spawns();
    printf("\nAll tests passed.\n");
    return 0;
}
