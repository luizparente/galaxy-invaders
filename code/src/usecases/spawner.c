#include <stdlib.h>
#include "usecases/spawner.h"
#include "usecases/difficulty.h"
#include "domain/constants.h"

static float frand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static const Color kEnemyPalette[] = {
    {220, 40, 40, 255},   /* red */
    {40, 220, 220, 255},  /* cyan */
    {220, 40, 220, 255},  /* magenta */
    {230, 220, 40, 255},  /* yellow */
    {60, 220, 80, 255},   /* green */
};
#define ENEMY_PALETTE_SIZE (int)(sizeof(kEnemyPalette) / sizeof(kEnemyPalette[0]))

static void spawn_one_enemy(GameState *gs) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (e->alive) continue;

        float size = ENEMY_MIN_SIZE + frand01() * (ENEMY_MAX_SIZE - ENEMY_MIN_SIZE);
        e->alive = true;
        e->size = size;
        e->x = size * 0.5f + frand01() * (SCREEN_W - size);
        e->y = -size;
        e->vx = (frand01() - 0.5f) * 20.0f;
        e->vy = difficulty_enemy_speed(gs->score) * (0.85f + frand01() * 0.3f);
        e->color = kEnemyPalette[rand() % ENEMY_PALETTE_SIZE];
        e->shape = (rand() % 2 == 0) ? ENEMY_SHAPE_INVADER : ENEMY_SHAPE_SAUCER;
        e->fire_timer = 0.5f + frand01() * 1.5f;
        e->wobble_phase = frand01() * 6.2831853f;
        return;
    }
}

void spawner_update(GameState *gs, float dt) {
    gs->spawn_timer -= dt;
    if (gs->spawn_timer > 0.0f) return;

    gs->spawn_timer = difficulty_spawn_interval(gs->score);

    int burst = 1 + rand() % 3; /* 1-3 enemies per spawn event */
    for (int i = 0; i < burst; i++) {
        spawn_one_enemy(gs);
    }
}
