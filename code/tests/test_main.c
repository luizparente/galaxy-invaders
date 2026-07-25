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

/* Tests run at exactly the DESIGN_W x DESIGN_H baseline so gs->scale
 * comes out to 1.0 and every design-baseline constant (PLAYER_WIDTH, etc.)
 * can be asserted against directly without carrying a scale factor. */
static void start_game(GameState *gs, EventQueue *events) {
    game_init(gs, DESIGN_W, DESIGN_H);
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
    assert(fabsf(gs.player.x - DESIGN_W / 2.0f) < 0.5f);
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

static void test_laser_color_changes_every_100_points(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    Color initial = gs.player.laser_color;

    /* Each kill is worth exactly SCORE_PER_KILL (10) below the first score
     * multiplier step (500), so the 10th kill lands score on exactly 100
     * and should be the one that rerolls the laser color - not sooner. */
    for (int kill = 1; kill <= 10; kill++) {
        Color before = gs.player.laser_color;
        kill_one_enemy(&gs, &events);
        assert(gs.score == kill * SCORE_PER_KILL);

        if (kill < 10) {
            assert(colors_equal(gs.player.laser_color, before));
        } else {
            assert(!colors_equal(gs.player.laser_color, before));
        }
    }

    assert(!colors_equal(gs.player.laser_color, initial));

    /* The new color actually gets used by the next shot fired. */
    InputCommand fire = no_input();
    fire.fire_held = true;
    game_update(&gs, &fire, 0.016f, &events);
    bool found_new_color_shot = false;
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) {
        if (gs.player_shots[i].alive && colors_equal(gs.player_shots[i].color, gs.player.laser_color)) {
            found_new_color_shot = true;
        }
    }
    assert(found_new_color_shot);

    printf("test_laser_color_changes_every_100_points OK\n");
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

static void test_shooting_orb_explodes_without_granting_beam(void) {
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

    /* Close enough to be inside the neutralize radius but far enough that
     * the shot itself doesn't also directly hit it. */
    gs.enemies[0].alive = true;
    gs.enemies[0].x = gs.orb.x + 40.0f;
    gs.enemies[0].y = gs.orb.y;
    gs.enemies[0].size = 20.0f;
    gs.enemies[0].fire_timer = 999.0f;

    /* Outside the blast radius entirely. */
    gs.enemies[1].alive = true;
    gs.enemies[1].x = 30.0f;
    gs.enemies[1].y = gs.orb.y;
    gs.enemies[1].size = 20.0f;
    gs.enemies[1].fire_timer = 999.0f;

    InputCommand none = no_input();
    game_update(&gs, &none, 0.001f, &events);

    assert(!gs.orb.alive);
    assert(!gs.enemies[0].alive);
    assert(gs.enemies[1].alive);
    assert(gs.player.super_beam_timer <= 0.0f);
    printf("test_shooting_orb_explodes_without_granting_beam OK\n");
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
    test_laser_color_changes_every_100_points();
    test_orb_capture_grants_super_beam();
    test_super_beam_neutralizes_without_normal_fire();
    test_shooting_orb_explodes_without_granting_beam();
    test_orb_falls_off_screen_when_uncaptured();
    test_orb_spawn_chance_is_not_always_or_never();
    test_spawner_eventually_spawns();
    printf("\nAll tests passed.\n");
    return 0;
}
