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
    float dot = 2.0f * gs->scale;
    for (int i = 0; i < MAX_STARS; i++) {
        const Star *s = &gs->stars[i];
        unsigned char b = s->brightness;
        gp_fill_rect(ctx->renderer, s->x, s->y, dot, dot, (Color){b, b, b, 255});
    }
}

static void draw_scanlines(SdlRendererCtx *ctx, const GameState *gs) {
    Color line = {0, 0, 0, 40};
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    float step = 3.0f * gs->scale;
    if (step < 1.0f) step = 1.0f;
    for (float y = 0.0f; y < (float)gs->screen_h; y += step) {
        gp_fill_rect(ctx->renderer, 0, y, (float)gs->screen_w, 1.0f, line);
    }
}

static void draw_player(SdlRendererCtx *ctx, const Player *p, float scale) {
    if (!p->alive) return;

    float half_w = PLAYER_WIDTH * scale / 2.0f;
    float half_h = PLAYER_HEIGHT * scale / 2.0f;

    /* Millennium-Falcon-esque top-down silhouette: a broad flattened hull
     * disc, twin forward mandibles with an off-center gap, an off-center
     * cockpit bump, a couple of panel-line details, and a rear engine
     * glow - all computed geometry, no sprite art. */
    gp_fill_ellipse(ctx->renderer, p->x, p->y, half_w, half_h, kHull);

    float mandible_w = half_w * 0.30f;
    float mandible_h = 9.0f * scale;
    gp_fill_rect(ctx->renderer, p->x - half_w * 0.70f, p->y - half_h - mandible_h + 3.0f * scale,
                 mandible_w, mandible_h, kHull);
    gp_fill_rect(ctx->renderer, p->x + half_w * 0.15f, p->y - half_h - mandible_h + 3.0f * scale,
                 mandible_w, mandible_h, kHull);

    gp_draw_line(ctx->renderer, p->x - half_w * 0.6f, p->y, p->x - 2.0f * scale, p->y - half_h * 0.6f, kHullShadow);
    gp_draw_line(ctx->renderer, p->x + half_w * 0.6f, p->y, p->x + 2.0f * scale, p->y - half_h * 0.5f, kHullShadow);

    float cockpit_r = 3.2f * scale;
    gp_fill_circle(ctx->renderer, p->x + half_w * 0.32f, p->y - half_h * 0.05f, cockpit_r, kCockpit);
    gp_draw_circle_outline(ctx->renderer, p->x + half_w * 0.32f, p->y - half_h * 0.05f, cockpit_r, kHullShadow);

    gp_fill_circle(ctx->renderer, p->x, p->y + half_h - 1.0f * scale, 3.0f * scale, kEngineGlow);
}

static void draw_enemy(SdlRendererCtx *ctx, const Enemy *e) {
    if (!e->alive) return;
    float half = e->size / 2.0f;
    Color dark = lerp_color(e->color, (Color){0, 0, 0, 255}, 0.55f);

    if (e->shape == ENEMY_SHAPE_INVADER) {
        gp_fill_triangle(ctx->renderer, e->x, e->y - half, e->x - half, e->y + half * 0.25f,
                          e->x + half, e->y + half * 0.25f, e->color);
        for (int k = -1; k <= 1; k++) {
            gp_fill_rect(ctx->renderer, e->x + (float)k * half * 0.5f - half * 0.06f, e->y + half * 0.25f,
                         half * 0.12f, half * 0.7f, e->color);
        }
        gp_fill_circle(ctx->renderer, e->x - half * 0.3f, e->y - half * 0.1f, half * 0.16f, dark);
        gp_fill_circle(ctx->renderer, e->x + half * 0.3f, e->y - half * 0.1f, half * 0.16f, dark);
    } else {
        gp_fill_ellipse(ctx->renderer, e->x, e->y, half, half * 0.55f, e->color);
        gp_fill_circle(ctx->renderer, e->x, e->y - half * 0.25f, half * 0.45f,
                        lerp_color(e->color, kWhite, 0.35f));
        gp_fill_circle(ctx->renderer, e->x - half * 0.35f, e->y + half * 0.05f, half * 0.14f, dark);
        gp_fill_circle(ctx->renderer, e->x + half * 0.35f, e->y + half * 0.05f, half * 0.14f, dark);
    }
}

static void draw_projectile(SdlRendererCtx *ctx, const Projectile *pr, bool is_player, float scale) {
    if (!pr->alive) return;
    if (is_player) {
        float w = PLAYER_PROJECTILE_W * scale;
        float h = PLAYER_PROJECTILE_H * scale;
        gp_fill_rect(ctx->renderer, pr->x - w / 2.0f, pr->y - h / 2.0f, w, h, pr->color);
    } else {
        float w = ENEMY_PROJECTILE_W * scale;
        float h = ENEMY_PROJECTILE_H * scale;
        float half = w / 2.0f;
        gp_fill_triangle(ctx->renderer, pr->x, pr->y + h / 2.0f,
                          pr->x - half, pr->y - h / 2.0f,
                          pr->x + half, pr->y - h / 2.0f, pr->color);
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

static void draw_orb(SdlRendererCtx *ctx, const Orb *o) {
    if (!o->alive) return;
    float r = o->size / 2.0f;

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    Color glow = o->color;
    glow.a = 90;
    gp_fill_circle(ctx->renderer, o->x, o->y, r * 1.6f, glow);

    gp_fill_circle(ctx->renderer, o->x, o->y, r, o->color);
    gp_draw_circle_outline(ctx->renderer, o->x, o->y, r, lerp_color(o->color, kWhite, 0.5f));

    Color highlight = lerp_color(o->color, kWhite, 0.75f);
    gp_fill_circle(ctx->renderer, o->x - r * 0.3f, o->y - r * 0.3f, r * 0.35f, highlight);
}

static void draw_super_beam(SdlRendererCtx *ctx, const GameState *gs) {
    const Player *p = &gs->player;
    if (p->super_beam_timer <= 0.0f || !p->alive) return;

    float beam_w = PLAYER_PROJECTILE_W * gs->scale * SUPER_BEAM_WIDTH_MULTIPLIER;
    float top = p->y - PLAYER_HEIGHT * gs->scale / 2.0f;
    float pulse = 0.75f + 0.25f * sinf(gs->time_elapsed * 18.0f);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    Color glow = lerp_color(p->laser_color, kWhite, 0.2f);
    glow.a = (unsigned char)(70.0f * pulse);
    gp_fill_rect(ctx->renderer, p->x - beam_w * 1.2f, 0.0f, beam_w * 2.4f, top, glow);

    Color core = lerp_color(p->laser_color, kWhite, 0.6f);
    core.a = (unsigned char)(235.0f * pulse);
    gp_fill_rect(ctx->renderer, p->x - beam_w / 2.0f, 0.0f, beam_w, top, core);
}

static void draw_gameplay(SdlRendererCtx *ctx, const GameState *gs) {
    for (int i = 0; i < MAX_ENEMIES; i++) draw_enemy(ctx, &gs->enemies[i]);
    draw_orb(ctx, &gs->orb);
    for (int i = 0; i < MAX_EXPLOSIONS; i++) draw_explosion(ctx, &gs->explosions[i]);
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) draw_projectile(ctx, &gs->player_shots[i], true, gs->scale);
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) draw_projectile(ctx, &gs->enemy_shots[i], false, gs->scale);
    draw_super_beam(ctx, gs);
    draw_player(ctx, &gs->player, gs->scale);
}

static void draw_hud(SdlRendererCtx *ctx, const GameState *gs) {
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE:%d", gs->score);
    float size = 3.0f * gs->scale;
    float margin = 12.0f * gs->scale;
    float w = pf_text_width(buf, size);
    pf_draw_text(ctx->renderer, (float)gs->screen_w - w - margin, margin, size, kWhite, buf);

    if (gs->player.super_beam_timer > 0.0f) {
        char beam_buf[24];
        snprintf(beam_buf, sizeof(beam_buf), "SUPER BEAM %.1fs", (double)gs->player.super_beam_timer);
        float beam_size = 2.2f * gs->scale;
        float beam_w = pf_text_width(beam_buf, beam_size);
        float pulse = 0.7f + 0.3f * sinf(gs->time_elapsed * 12.0f);
        Color c = lerp_color(gs->player.laser_color, kWhite, pulse);
        float score_line_height = 7.0f * size; /* the pixel font is 7 dots tall */
        pf_draw_text(ctx->renderer, (float)gs->screen_w - beam_w - margin,
                     margin + score_line_height + margin * 0.4f, beam_size, c, beam_buf);
    }
}

static void draw_centered(SdlRendererCtx *ctx, const GameState *gs, const char *text, float y, float size, Color c) {
    float w = pf_text_width(text, size);
    pf_draw_text(ctx->renderer, ((float)gs->screen_w - w) / 2.0f, y, size, c, text);
}

static void draw_menu_screen(SdlRendererCtx *ctx, const GameState *gs) {
    float title_size = 7.0f * gs->scale;
    draw_centered(ctx, gs, "GALAXY", (float)gs->screen_h * 0.16f, title_size, kCyan);
    draw_centered(ctx, gs, "INVADERS", (float)gs->screen_h * 0.16f + 60.0f * gs->scale, title_size, kCyan);

    bool blink_on = fmodf(gs->menu_blink_timer, 1.0f) < 0.5f;
    if (blink_on) {
        draw_centered(ctx, gs, "START GAME", (float)gs->screen_h * 0.6f, 4.0f * gs->scale, kYellow);
    }
    draw_centered(ctx, gs, "ARROWS-WASD MOVE  SPACE FIRE  ESC PAUSE", (float)gs->screen_h * 0.85f,
                  1.6f * gs->scale, kDim);
}

static void draw_pause_overlay(SdlRendererCtx *ctx, const GameState *gs) {
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_rect(ctx->renderer, 0, 0, (float)gs->screen_w, (float)gs->screen_h, (Color){5, 5, 15, 170});

    draw_centered(ctx, gs, "PAUSED", (float)gs->screen_h * 0.32f, 5.0f * gs->scale, kWhite);

    Color resume_c = gs->pause_selection == PAUSE_RESUME ? kYellow : kDim;
    Color exit_c = gs->pause_selection == PAUSE_EXIT ? kYellow : kDim;
    draw_centered(ctx, gs, "RESUME", (float)gs->screen_h * 0.48f, 3.5f * gs->scale, resume_c);
    draw_centered(ctx, gs, "EXIT TO MENU", (float)gs->screen_h * 0.56f, 3.5f * gs->scale, exit_c);
}

static void draw_game_over_screen(SdlRendererCtx *ctx, const GameState *gs) {
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_rect(ctx->renderer, 0, 0, (float)gs->screen_w, (float)gs->screen_h, (Color){5, 0, 0, 165});

    bool blink_on = fmodf(gs->menu_blink_timer, 1.0f) < 0.5f;
    if (blink_on) {
        draw_centered(ctx, gs, "GAME OVER", (float)gs->screen_h * 0.34f, 5.5f * gs->scale, kRed);
    }

    char buf[40];
    snprintf(buf, sizeof(buf), "FINAL SCORE:%d", gs->last_game_score);
    draw_centered(ctx, gs, buf, (float)gs->screen_h * 0.48f, 3.0f * gs->scale, kWhite);
    draw_centered(ctx, gs, "PRESS ENTER OR SPACE", (float)gs->screen_h * 0.6f, 2.2f * gs->scale, kYellow);
    draw_centered(ctx, gs, "TO RESTART", (float)gs->screen_h * 0.6f + 24.0f * gs->scale, 2.2f * gs->scale, kYellow);
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

    draw_scanlines(ctx, gs);
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

RendererPort *sdl_renderer_create(const char *title, int fallback_w, int fallback_h,
                                   int *out_screen_w, int *out_screen_h) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL video init failed: %s\n", SDL_GetError());
        return NULL;
    }

    SdlRendererCtx *ctx = calloc(1, sizeof(SdlRendererCtx));
    if (!ctx) return NULL;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    /* Borderless fullscreen at the desktop's own resolution - the window
     * itself is exactly as big as the screen, with no window-manager
     * decoration and no display-mode switch. SDL ignores the requested
     * width/height for this flag and sizes the window to the desktop. */
    ctx->window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                    fallback_w, fallback_h, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!ctx->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return NULL;
    }
    SDL_ShowCursor(SDL_DISABLE);

    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ctx->window);
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return NULL;
    }
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    /* Every draw call in this file works directly in real screen pixels -
     * the playfield's logical size *is* the physical screen, at whatever
     * aspect ratio it has, so it fills the display edge to edge with no
     * letterboxing and no non-uniform stretch (which would distort every
     * circle into an ellipse and every square into a rectangle). Callers
     * scale sizes uniformly via GameState.scale instead. */
    int real_w = fallback_w, real_h = fallback_h;
    SDL_GetRendererOutputSize(ctx->renderer, &real_w, &real_h);
    *out_screen_w = real_w;
    *out_screen_h = real_h;

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
