#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "domain/constants.h"
#include "domain/events.h"
#include "domain/types.h"
#include "ports/input_port.h"
#include "usecases/collision.h"
#include "usecases/difficulty.h"
#include "usecases/game_logic.h"

/* These tests exercise only the usecases layer (pure game rules) directly
 * against domain structs - no SDL, no window, no audio device. That is the
 * entire point of keeping usecases dependency-free: the rules are testable
 * in isolation from every adapter. */

static InputCommand no_input(void) {
    return (InputCommand){0};
}

static void start_game(GameState *gs, EventQueue *events) {
    game_init(gs);
    InputCommand start = no_input();
    start.confirm_pressed = true;
    game_update(gs, &start, 0.016f, events);
}

static void test_collision(void) {
    assert(collision_aabb_overlap(0, 0, 5, 5, 8, 0, 5, 5));
    assert(!collision_aabb_overlap(0, 0, 5, 5, 20, 0, 5, 5));
    assert(collision_aabb_overlap(0, 0, 5, 5, 0, 9, 5, 5));
    assert(!collision_aabb_overlap(0, 0, 5, 5, 0, 11, 5, 5));
    printf("test_collision OK\n");
}

static void test_difficulty(void) {
    assert(fabsf(difficulty_spawn_interval(0) - BASE_SPAWN_INTERVAL) < 0.001f);
    assert(difficulty_spawn_interval(5000) < difficulty_spawn_interval(0));
    assert(difficulty_spawn_interval(1000000) >= MIN_SPAWN_INTERVAL - 0.001f);

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
    assert(fabsf(gs.player.x - SCREEN_W / 2.0f) < 0.5f);
    printf("test_menu_start_transition OK\n");
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
    test_player_movement_clamped();
    test_player_fire_cooldown();
    test_pause_toggle();
    test_pause_menu_exit_to_menu();
    test_enemy_kill_scores();
    test_player_enemy_collision_ends_game();
    test_spawner_eventually_spawns();
    printf("\nAll tests passed.\n");
    return 0;
}
