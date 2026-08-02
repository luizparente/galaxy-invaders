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
    assert(fabsf(difficulty_spawn_interval(0) - BASE_SPAWN_INTERVAL * SPAWN_RATE_MULTIPLIER) < 0.001f);
    assert(difficulty_spawn_interval(5000) < difficulty_spawn_interval(0));
    assert(difficulty_spawn_interval(1000000) >= MIN_SPAWN_INTERVAL * SPAWN_RATE_MULTIPLIER - 0.001f);

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

static void test_laser_color_changes_every_laser_color_score_step(void) {
    GameState gs;
    EventQueue events;
    start_game(&gs, &events);

    Color initial = gs.player.laser_color;

    /* Each kill is worth exactly SCORE_PER_KILL below the first score
     * multiplier step (500), so this many kills lands score on exactly
     * LASER_COLOR_SCORE_STEP and should be the one that rerolls the
     * laser color - not sooner. */
    int kills_to_threshold = LASER_COLOR_SCORE_STEP / SCORE_PER_KILL;
    for (int kill = 1; kill <= kills_to_threshold; kill++) {
        Color before = gs.player.laser_color;
        kill_one_enemy(&gs, &events);
        assert(gs.score == kill * SCORE_PER_KILL);

        if (kill < kills_to_threshold) {
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

    printf("test_laser_color_changes_every_laser_color_score_step OK\n");
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

static void shoot_boss_once(GameState *gs, EventQueue *events) {
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

    InputCommand none = no_input();
    game_update(gs, &none, 0.001f, events);
}

/* Fast-forwards straight to "one hit away from dead" and lands the final
 * blow, so multi-boss progression tests don't need 50-150 real shots. */
static void defeat_current_boss(GameState *gs, EventQueue *events) {
    assert(gs->boss.alive);
    gs->boss.hits_taken = gs->boss.hits_required - 1;
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

    int expected_bonus = gs.boss.hits_required * BOSS_KILL_SCORE_MULTIPLIER;
    defeat_current_boss(&gs, &events);
    assert(!gs.boss.alive);
    /* The defeat bonus is the first contribution to the fresh count. */
    assert(gs.score_since_last_boss == expected_bonus);

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
    test_player_can_reach_top_of_screen();
    test_super_beam_increases_player_speed();
    test_player_fire_cooldown();
    test_pause_toggle();
    test_pause_menu_exit_to_menu();
    test_enemy_kill_scores();
    test_player_enemy_collision_ends_game();
    test_laser_color_changes_every_laser_color_score_step();
    test_orb_capture_grants_super_beam();
    test_player_invincible_during_super_beam();
    test_god_mode_toggles_on_and_off();
    test_god_mode_prevents_death();
    test_super_beam_neutralizes_without_normal_fire();
    test_shooting_orb_explodes_without_granting_beam();
    test_orb_falls_off_screen_when_uncaptured();
    test_orb_spawn_chance_is_not_always_or_never();
    test_boss_spawns_at_500_points_with_correct_hits_required();
    test_boss_hits_required_increases_each_appearance();
    test_boss_reappearance_requires_fresh_points_since_defeat();
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
    test_spawner_eventually_spawns();
    printf("\nAll tests passed.\n");
    return 0;
}
