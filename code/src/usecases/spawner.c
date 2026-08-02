#include <stdlib.h>
#include "usecases/spawner.h"
#include "usecases/difficulty.h"
#include "domain/constants.h"

static float frand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

/* One accent color per enemy kind (adapters/enemy_sprites), used only to
 * tint that enemy's own projectiles - the sprite art itself carries its
 * own fixed, exact colors and doesn't use this palette. Ordered to match
 * kEnemySprites: phoenix, green spider, gold saucer, purple interceptor,
 * cyan interceptor, magenta interceptor, orange turret, slate stealth,
 * black/red stealth, hornet, heavy gunship, purple elite, teal spider,
 * fire/ice wyrm, bat bomber, abyssal kraken. */
static const Color kEnemyKindAccentColor[ENEMY_KIND_COUNT] = {
    {230, 80, 40, 255},
    {60, 200, 90, 255},
    {230, 190, 40, 255},
    {170, 80, 220, 255},
    {60, 190, 230, 255},
    {220, 60, 190, 255},
    {230, 120, 40, 255},
    {90, 130, 150, 255},
    {200, 60, 60, 255},
    {220, 190, 40, 255},
    {190, 150, 90, 255},
    {150, 90, 210, 255},
    {70, 190, 140, 255},
    {230, 140, 40, 255},
    {200, 60, 60, 255},
    {60, 190, 190, 255},
};

int spawner_random_enemy_kind(void) {
    return rand() % ENEMY_KIND_COUNT;
}

Color spawner_enemy_kind_accent_color(int kind) {
    return kEnemyKindAccentColor[kind];
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
        e->color = spawner_enemy_kind_accent_color(e->kind);
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
