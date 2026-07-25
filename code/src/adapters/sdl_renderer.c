#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "adapters/sdl_renderer.h"
#include "adapters/graphics_primitives.h"
#include "adapters/pixel_font.h"
#include "domain/constants.h"

typedef struct SdlRendererCtx {
    SDL_Window *window;
    SDL_Renderer *renderer;
} SdlRendererCtx;

static const Color kBackground = {8, 8, 26, 255};
static const Color kCyan = {70, 230, 230, 255};
static const Color kYellow = {235, 220, 70, 255};
static const Color kWhite = {235, 235, 235, 255};
static const Color kDim = {120, 120, 140, 255};
static const Color kRed = {230, 60, 60, 255};
static const Color kHull = {225, 225, 230, 255};
static const Color kHullShadow = {170, 170, 180, 255};
static const Color kCockpit = {40, 40, 50, 255};
static const Color kEngineGlow = {255, 160, 40, 255};

static Color lerp_color(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){
        (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t),
        (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
        (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t),
        (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * t),
    };
}

static void draw_stars(SdlRendererCtx *ctx, const GameState *gs) {
    for (int i = 0; i < MAX_STARS; i++) {
        const Star *s = &gs->stars[i];
        unsigned char b = s->brightness;
        gp_fill_rect(ctx->renderer, s->x, s->y, 2.0f, 2.0f, (Color){b, b, b, 255});
    }
}

static void draw_scanlines(SdlRendererCtx *ctx) {
    Color line = {0, 0, 0, 40};
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < SCREEN_H; y += 3) {
        gp_fill_rect(ctx->renderer, 0, (float)y, (float)SCREEN_W, 1.0f, line);
    }
}

static void draw_player(SdlRendererCtx *ctx, const Player *p) {
    if (!p->alive) return;

    float half_w = PLAYER_WIDTH / 2.0f;
    float half_h = PLAYER_HEIGHT / 2.0f;

    /* Millennium-Falcon-esque top-down silhouette: a broad flattened hull
     * disc, twin forward mandibles with an off-center gap, an off-center
     * cockpit bump, a couple of panel-line details, and a rear engine
     * glow - all computed geometry, no sprite art. */
    gp_fill_ellipse(ctx->renderer, p->x, p->y, half_w, half_h, kHull);

    float mandible_w = half_w * 0.30f;
    float mandible_h = 9.0f;
    gp_fill_rect(ctx->renderer, p->x - half_w * 0.70f, p->y - half_h - mandible_h + 3.0f,
                 mandible_w, mandible_h, kHull);
    gp_fill_rect(ctx->renderer, p->x + half_w * 0.15f, p->y - half_h - mandible_h + 3.0f,
                 mandible_w, mandible_h, kHull);

    gp_draw_line(ctx->renderer, p->x - half_w * 0.6f, p->y, p->x - 2.0f, p->y - half_h * 0.6f, kHullShadow);
    gp_draw_line(ctx->renderer, p->x + half_w * 0.6f, p->y, p->x + 2.0f, p->y - half_h * 0.5f, kHullShadow);

    gp_fill_circle(ctx->renderer, p->x + half_w * 0.32f, p->y - half_h * 0.05f, 3.2f, kCockpit);
    gp_draw_circle_outline(ctx->renderer, p->x + half_w * 0.32f, p->y - half_h * 0.05f, 3.2f, kHullShadow);

    gp_fill_circle(ctx->renderer, p->x, p->y + half_h - 1.0f, 3.0f, kEngineGlow);
}

static void draw_enemy(SdlRendererCtx *ctx, const Enemy *e) {
    if (!e->alive) return;
    float half = e->size / 2.0f;
    Color dark = lerp_color(e->color, (Color){0, 0, 0, 255}, 0.55f);

    if (e->shape == ENEMY_SHAPE_INVADER) {
        gp_fill_triangle(ctx->renderer, e->x, e->y - half, e->x - half, e->y + half * 0.25f,
                          e->x + half, e->y + half * 0.25f, e->color);
        for (int k = -1; k <= 1; k++) {
            gp_fill_rect(ctx->renderer, e->x + (float)k * half * 0.5f - 1.0f, e->y + half * 0.25f,
                         2.0f, half * 0.7f, e->color);
        }
        gp_fill_circle(ctx->renderer, e->x - half * 0.3f, e->y - half * 0.1f, 1.6f, dark);
        gp_fill_circle(ctx->renderer, e->x + half * 0.3f, e->y - half * 0.1f, 1.6f, dark);
    } else {
        gp_fill_ellipse(ctx->renderer, e->x, e->y, half, half * 0.55f, e->color);
        gp_fill_circle(ctx->renderer, e->x, e->y - half * 0.25f, half * 0.45f,
                        lerp_color(e->color, kWhite, 0.35f));
        gp_fill_circle(ctx->renderer, e->x - half * 0.35f, e->y + half * 0.05f, 1.4f, dark);
        gp_fill_circle(ctx->renderer, e->x + half * 0.35f, e->y + half * 0.05f, 1.4f, dark);
    }
}

static void draw_projectile(SdlRendererCtx *ctx, const Projectile *pr, bool is_player) {
    if (!pr->alive) return;
    if (is_player) {
        gp_fill_rect(ctx->renderer, pr->x - PLAYER_PROJECTILE_W / 2.0f, pr->y - PLAYER_PROJECTILE_H / 2.0f,
                     PLAYER_PROJECTILE_W, PLAYER_PROJECTILE_H, pr->color);
    } else {
        float half = ENEMY_PROJECTILE_W / 2.0f;
        gp_fill_triangle(ctx->renderer, pr->x, pr->y + ENEMY_PROJECTILE_H / 2.0f,
                          pr->x - half, pr->y - ENEMY_PROJECTILE_H / 2.0f,
                          pr->x + half, pr->y - ENEMY_PROJECTILE_H / 2.0f, pr->color);
    }
}

static void draw_explosion(SdlRendererCtx *ctx, const Explosion *e) {
    if (!e->alive) return;
    float t = e->age / e->max_age;
    float radius = e->max_radius * (0.35f + 0.65f * t);
    Color core = lerp_color((Color){255, 255, 210, 255}, (Color){255, 120, 30, 255}, t);
    core.a = (unsigned char)(255.0f * (1.0f - t));

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_circle(ctx->renderer, e->x, e->y, radius, core);

    if (t > 0.1f && t < 0.75f) {
        Color spark = lerp_color((Color){255, 220, 120, 255}, (Color){180, 40, 20, 255}, t);
        spark.a = core.a;
        for (int k = 0; k < 8; k++) {
            float ang = (float)k * (float)M_PI / 4.0f + t * 1.5f;
            float len = radius * 1.6f;
            gp_draw_line(ctx->renderer, e->x, e->y, e->x + cosf(ang) * len, e->y + sinf(ang) * len, spark);
        }
    }
}

static void draw_gameplay(SdlRendererCtx *ctx, const GameState *gs) {
    for (int i = 0; i < MAX_ENEMIES; i++) draw_enemy(ctx, &gs->enemies[i]);
    for (int i = 0; i < MAX_EXPLOSIONS; i++) draw_explosion(ctx, &gs->explosions[i]);
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) draw_projectile(ctx, &gs->player_shots[i], true);
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) draw_projectile(ctx, &gs->enemy_shots[i], false);
    draw_player(ctx, &gs->player);
}

static void draw_hud(SdlRendererCtx *ctx, const GameState *gs) {
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE:%d", gs->score);
    float size = 3.0f;
    float w = pf_text_width(buf, size);
    pf_draw_text(ctx->renderer, SCREEN_W - w - 12.0f, 12.0f, size, kWhite, buf);
}

static void draw_centered(SdlRendererCtx *ctx, const char *text, float y, float size, Color c) {
    float w = pf_text_width(text, size);
    pf_draw_text(ctx->renderer, (SCREEN_W - w) / 2.0f, y, size, c, text);
}

static void draw_menu_screen(SdlRendererCtx *ctx, const GameState *gs) {
    draw_centered(ctx, "GALAXY", SCREEN_H * 0.16f, 7.0f, kCyan);
    draw_centered(ctx, "INVADERS", SCREEN_H * 0.16f + 60.0f, 7.0f, kCyan);

    bool blink_on = fmodf(gs->menu_blink_timer, 1.0f) < 0.5f;
    if (blink_on) {
        draw_centered(ctx, "START GAME", SCREEN_H * 0.6f, 4.0f, kYellow);
    }
    draw_centered(ctx, "ARROWS-WASD MOVE  SPACE FIRE  ESC PAUSE", SCREEN_H * 0.85f, 1.6f, kDim);
}

static void draw_pause_overlay(SdlRendererCtx *ctx, const GameState *gs) {
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_rect(ctx->renderer, 0, 0, SCREEN_W, SCREEN_H, (Color){5, 5, 15, 170});

    draw_centered(ctx, "PAUSED", SCREEN_H * 0.32f, 5.0f, kWhite);

    Color resume_c = gs->pause_selection == PAUSE_RESUME ? kYellow : kDim;
    Color exit_c = gs->pause_selection == PAUSE_EXIT ? kYellow : kDim;
    draw_centered(ctx, "RESUME", SCREEN_H * 0.48f, 3.5f, resume_c);
    draw_centered(ctx, "EXIT TO MENU", SCREEN_H * 0.56f, 3.5f, exit_c);
}

static void draw_game_over_screen(SdlRendererCtx *ctx, const GameState *gs) {
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_rect(ctx->renderer, 0, 0, SCREEN_W, SCREEN_H, (Color){5, 0, 0, 165});

    bool blink_on = fmodf(gs->menu_blink_timer, 1.0f) < 0.5f;
    if (blink_on) {
        draw_centered(ctx, "GAME OVER", SCREEN_H * 0.34f, 5.5f, kRed);
    }

    char buf[40];
    snprintf(buf, sizeof(buf), "FINAL SCORE:%d", gs->last_game_score);
    draw_centered(ctx, buf, SCREEN_H * 0.48f, 3.0f, kWhite);
    draw_centered(ctx, "PRESS ENTER OR SPACE", SCREEN_H * 0.6f, 2.2f, kYellow);
    draw_centered(ctx, "TO RESTART", SCREEN_H * 0.6f + 24.0f, 2.2f, kYellow);
}

static void sdl_render_frame(void *self, const GameState *gs) {
    SdlRendererCtx *ctx = self;
    SDL_SetRenderDrawColor(ctx->renderer, kBackground.r, kBackground.g, kBackground.b, 255);
    SDL_RenderClear(ctx->renderer);

    draw_stars(ctx, gs);

    switch (gs->state) {
        case STATE_MENU:
            draw_menu_screen(ctx, gs);
            break;
        case STATE_GAME:
            draw_gameplay(ctx, gs);
            draw_hud(ctx, gs);
            break;
        case STATE_PAUSE:
            draw_gameplay(ctx, gs);
            draw_hud(ctx, gs);
            draw_pause_overlay(ctx, gs);
            break;
        case STATE_GAME_OVER:
            draw_gameplay(ctx, gs);
            draw_game_over_screen(ctx, gs);
            break;
    }

    draw_scanlines(ctx);
    SDL_RenderPresent(ctx->renderer);
}

static void sdl_render_destroy(void *self) {
    SdlRendererCtx *ctx = self;
    if (!ctx) return;
    if (ctx->renderer) SDL_DestroyRenderer(ctx->renderer);
    if (ctx->window) SDL_DestroyWindow(ctx->window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(ctx);
}

RendererPort *sdl_renderer_create(const char *title, int logical_w, int logical_h) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL video init failed: %s\n", SDL_GetError());
        return NULL;
    }

    SdlRendererCtx *ctx = calloc(1, sizeof(SdlRendererCtx));
    if (!ctx) return NULL;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    ctx->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    logical_w, logical_h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!ctx->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return NULL;
    }

    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ctx->window);
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return NULL;
    }

    SDL_RenderSetLogicalSize(ctx->renderer, logical_w, logical_h);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    RendererPort *port = calloc(1, sizeof(RendererPort));
    if (!port) {
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return NULL;
    }
    port->self = ctx;
    port->render = sdl_render_frame;
    port->destroy = sdl_render_destroy;
    return port;
}
