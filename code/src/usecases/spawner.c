#include <stdlib.h>
#include "usecases/spawner.h"
#include "usecases/difficulty.h"
#include "domain/constants.h"

static float frand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

/* A random fully-saturated hue for one enemy's projectiles, rolled once at
 * spawn time (see spawn_one_enemy) and kept for that enemy's whole
 * lifetime - every shot it fires (including every shot of a multi-shot
 * burst/spread style) reads as one consistent color, while different
 * enemies - and so different volleys over time - vary. Mirrors
 * random_vivid_color in usecases/game_logic.c (kept file-local rather than
 * shared, same as frand01 above). */
static Color random_vivid_projectile_color(void) {
    float h = frand01() * 360.0f;
    float s = 0.75f + frand01() * 0.25f;
    float v = 0.9f + frand01() * 0.1f;
    return color_from_hsv(h, s, v);
}

/* One shooting style per enemy kind (adapters/enemy_sprites) - purely a
 * design choice, not tied to the sprite's own colors. Ordered to match
 * kEnemySprites: phoenix, green spider, gold saucer, purple interceptor,
 * cyan interceptor, magenta interceptor, orange turret, slate stealth,
 * black/red stealth, hornet, heavy gunship, purple elite, teal spider,
 * fire/ice wyrm, bat bomber, abyssal kraken. */
static const EnemyShootStyle kEnemyKindShootStyle[ENEMY_KIND_COUNT] = {
    ENEMY_SHOOT_THIN_BEAM,  /* phoenix */
    ENEMY_SHOOT_TRISHOT,    /* green spider */
    ENEMY_SHOOT_OMNI,       /* gold saucer */
    ENEMY_SHOOT_THIN_BEAM,  /* purple interceptor */
    ENEMY_SHOOT_THIN_BEAM,  /* cyan interceptor */
    ENEMY_SHOOT_LONG_BEAM,  /* magenta interceptor */
    ENEMY_SHOOT_TRIBURST,   /* orange turret */
    ENEMY_SHOOT_THIN_BEAM,  /* slate stealth */
    ENEMY_SHOOT_LONG_BEAM,  /* black/red stealth */
    ENEMY_SHOOT_TRIBURST,   /* hornet */
    ENEMY_SHOOT_OMNI,       /* heavy gunship */
    ENEMY_SHOOT_TRISHOT,    /* purple elite */
    ENEMY_SHOOT_TRISHOT,    /* teal spider */
    ENEMY_SHOOT_LONG_BEAM,  /* fire/ice wyrm */
    ENEMY_SHOOT_OMNI,       /* bat bomber */
    ENEMY_SHOOT_OMNI,       /* abyssal kraken */
};

int spawner_random_enemy_kind(void) {
    return rand() % ENEMY_KIND_COUNT;
}

EnemyShootStyle spawner_enemy_kind_shoot_style(int kind) {
    return kEnemyKindShootStyle[kind];
}

/* Kinds with a dedicated richer boss-scale redesign in kBossSprites (see
 * adapters/enemy_sprites.c) - phoenix, green spider, purple interceptor,
 * cyan interceptor, magenta interceptor, orange turret, slate stealth,
 * purple elite, teal spider, bat bomber, abyssal kraken. The rest still
 * spawn as ordinary enemies, just never get picked for the boss. */
static const int kBossCapableKinds[] = { 0, 1, 3, 4, 5, 6, 7, 11, 12, 14, 15 };
#define BOSS_CAPABLE_KIND_COUNT (int)(sizeof(kBossCapableKinds) / sizeof(kBossCapableKinds[0]))

int spawner_random_boss_kind(void) {
    return kBossCapableKinds[rand() % BOSS_CAPABLE_KIND_COUNT];
}

static void spawn_one_enemy(GameState *gs) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (e->alive) continue;

        float min_size = ENEMY_MIN_SIZE * gs->scale;
        float max_size = ENEMY_MAX_SIZE * gs->scale;
        float size = min_size + frand01() * (max_size - min_size);
        e->alive = true;
        e->size = size;
        e->x = size * 0.5f + frand01() * ((float)gs->screen_w - size);
        e->y = -size;
        e->vx = (frand01() - 0.5f) * 20.0f * gs->scale;
        e->vy = difficulty_enemy_speed(gs->score) * gs->scale * (0.85f + frand01() * 0.3f);
        e->kind = spawner_random_enemy_kind();
        e->color = random_vivid_projectile_color();
        e->fire_timer = 0.5f + frand01() * 1.5f;
        e->burst_shots_remaining = 0; /* slot may have been left mid-burst by a past triburst enemy */
        e->burst_shot_timer = 0.0f;
        e->wobble_phase = frand01() * 6.2831853f;
        e->orb_kill_pending = false; /* slot may have been left pending by a past orb detonation */
        e->orb_kill_timer = 0.0f;
        /* Staggered like wobble_phase above, so a whole wave spawned at
         * once doesn't all puff its first trail particle on the exact same
         * frame. */
        e->trail_emit_timer = frand01() * ENEMY_TRAIL_SPAWN_INTERVAL;
        return;
    }
}

void spawner_update(GameState *gs, float dt) {
    gs->spawn_timer -= dt;
    if (gs->spawn_timer > 0.0f) return;

    gs->spawn_timer = difficulty_spawn_interval(gs->score, gs->selected_difficulty, gs->time_elapsed);

    int burst = 1 + rand() % 3; /* 1-3 enemies per spawn event */
    for (int i = 0; i < burst; i++) {
        spawn_one_enemy(gs);
    }
}
