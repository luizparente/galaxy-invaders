#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <string.h>

#include "adapters/sdl_renderer.h"
#include "adapters/graphics_primitives.h"
#include "adapters/pixel_font.h"
#include "adapters/ship_sprites.h"
#include "adapters/cruzader_rocket_sprite.h"
#include "adapters/enemy_sprites.h"
#include "adapters/menu_ship_sprite.h"
#include "adapters/menu_planet_sprites.h"
#include "domain/constants.h"
#include "usecases/ship.h"

typedef struct SdlRendererCtx {
    SDL_Window *window;
    SDL_Renderer *renderer;
    /* One GPU texture per enemy kind, uploaded once from the embedded RGBA
     * grids in adapters/enemy_sprites at renderer creation. Enemies and the
     * boss can appear dozens at a time (MAX_ENEMIES), so unlike the single
     * player ship they're drawn via a single textured blit each rather than
     * one gp_fill_rect per pixel - same exact embedded pixel data, just
     * blitted as a whole instead of cell by cell, to keep draw-call count
     * bounded under a screen full of enemies. */
    SDL_Texture *enemy_textures[ENEMY_KIND_COUNT];
    /* Same 16 designs at full native source resolution, used only for the
     * boss (see adapters/enemy_sprites.h) so its ~10x-larger size doesn't
     * stretch a small texture into visible blocks. */
    SDL_Texture *boss_textures[ENEMY_KIND_COUNT];
    /* The decorative hero ship on the main menu (adapters/menu_ship_sprite)
     * - a single large textured blit, same rationale as the enemy/boss
     * textures above: its grid is far too big to blit one gp_fill_rect per
     * pixel every frame. */
    SDL_Texture *menu_ship_texture;
    /* The 4 decorative menu planets (adapters/menu_planet_sprites), same
     * one-texture-per-design technique as enemy_textures/boss_textures. */
    SDL_Texture *planet_textures[MENU_PLANET_COUNT];
} SdlRendererCtx;

static const Color kBackground = {8, 8, 26, 255};
static const Color kYellow = {235, 220, 70, 255};
static const Color kWhite = {235, 235, 235, 255};
static const Color kDim = {120, 120, 140, 255};
static const Color kRed = {230, 60, 60, 255};
static const Color kGreen = {80, 220, 110, 255};
static const Color kGodModeTint = {255, 215, 40, 255};

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

/* A stable (never animated - see below) pseudo-random value in [0, 1) for
 * grid cell (cx, cy), used only to jitter draw_background_smoke's density
 * threshold so a cloud's edge comes out as scattered, dithered pixels
 * instead of a smooth curve - the classic retro/8-bit trick for shading a
 * gradient with a flat, hard-edged color palette (the same reason old
 * console skies and fog are stippled, not smoothly blended). A cheap
 * integer hash rather than a stored table: no per-cell state to own, and
 * it's the same handful of arithmetic ops either way. */
static float background_cell_dither(int cx, int cy) {
    unsigned int h = (unsigned int)(cx * 374761393 + cy * 668265263) ^ 2654435761u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFu) / 65536.0f;
}

/* The game's pixelated background smoke - a coarse grid of blocky,
 * flat-colored cells (BACKGROUND_SMOKE_CELL_SIZE, matching the game's own
 * pixel-art scale) shaded light or dark, both always darker than the flat
 * kBackground and never lighter, depending on how much combined influence
 * from the drifting BackgroundCloud sources reaches each cell, plus
 * background_cell_dither's static noise jittering the threshold so the
 * edges scatter into stippled pixels instead of a smooth curve -
 * deliberately chunky and retro rather than a soft gradient. Every cloud's
 * own wobble (see BackgroundCloud's own doc comment) is what keeps the
 * combined shape visibly reshaping over time instead of settling into one
 * static silhouette, "large clouds... moving around, changing shapes."
 * BACKGROUND_SMOKE_CONTRAST is the single contrast knob: how far
 * kSmokeLight/kSmokeDark depart from kBackground. */
static void draw_background_smoke(SdlRendererCtx *ctx, const GameState *gs) {
    static const Color kBlack = {0, 0, 0, 255};
    /* Both tiers lerp toward black, never toward anything lighter -
     * kSmokeDark just goes twice as far as kSmokeLight, so the denser tier
     * reads as noticeably deeper shadow rather than a different hue. */
    Color light = lerp_color(kBackground, kBlack, BACKGROUND_SMOKE_CONTRAST * 0.5f);
    Color dark = lerp_color(kBackground, kBlack, BACKGROUND_SMOKE_CONTRAST);

    float cell = BACKGROUND_SMOKE_CELL_SIZE * gs->scale;
    if (cell < 2.0f) cell = 2.0f;
    int cols = (int)ceilf((float)gs->screen_w / cell);
    int rows = (int)ceilf((float)gs->screen_h / cell);

    for (int gy = 0; gy < rows; gy++) {
        float py = ((float)gy + 0.5f) * cell;
        for (int gx = 0; gx < cols; gx++) {
            float px = ((float)gx + 0.5f) * cell;

            float density = 0.0f;
            for (int i = 0; i < MAX_BACKGROUND_CLOUDS; i++) {
                const BackgroundCloud *c = &gs->background_clouds[i];
                float wobble = sinf(gs->time_elapsed * c->wobble_speed + c->wobble_seed) * c->wobble_amplitude;
                float dx = px - (c->x + wobble);
                float dy = py - c->y;
                float t = 1.0f - sqrtf(dx * dx + dy * dy) / c->radius;
                if (t > 0.0f) density += t;
            }
            density += (background_cell_dither(gx, gy) - 0.5f) * 0.5f;

            if (density > BACKGROUND_SMOKE_DARK_THRESHOLD) {
                gp_fill_rect(ctx->renderer, (float)gx * cell, (float)gy * cell, cell, cell, dark);
            } else if (density > BACKGROUND_SMOKE_LIGHT_THRESHOLD) {
                gp_fill_rect(ctx->renderer, (float)gx * cell, (float)gy * cell, cell, cell, light);
            }
        }
    }
}

static void draw_stars(SdlRendererCtx *ctx, const GameState *gs) {
    bool danger = gs->boss.alive;
    for (int i = 0; i < MAX_STARS; i++) {
        const Star *s = &gs->stars[i];
        unsigned char b = s->brightness;
        /* Size scales with brightness too, not just color - at a plain
         * 2x2px dot the color-only difference between a dim and a bright
         * star is too subtle to read at a glance; a dim star being
         * visibly smaller, not just a bit darker, is what actually sells
         * "some are faded, some are bright" from normal viewing distance. */
        float dot = (1.2f + (float)b / 255.0f * 2.6f) * gs->scale;
        /* Same brightness-driven twinkle either way - only the primary
         * color channel changes, from white to a dark red, while a boss
         * is on screen. */
        Color c = danger ? (Color){b, 0, 0, 255} : (Color){b, b, b, 255};
        gp_fill_rect(ctx->renderer, s->x, s->y, dot, dot, c);
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

/* Blits an embedded square RGBA pixel grid (adapters/ship_sprites) cell by
 * cell, centered at (cx, cy) and scaled to (w, h) - the only bitmap sprites
 * in an otherwise procedural renderer, same technique for every ship the
 * player can fly (draw_player) and for that ship's icon/preview on the
 * ship-select screen (draw_ship_select_screen). god_tint, when non-NULL,
 * blends toward it (see kGodModeTint) - only the live in-game ship ever
 * passes one. */
static void draw_ship_sprite(SdlRendererCtx *ctx, const ShipSpriteSheet *sheet, float cx, float cy,
                              float w, float h, const Color *god_tint) {
    float cell_w = w / (float)sheet->size;
    float cell_h = h / (float)sheet->size;
    float left = cx - w / 2.0f;
    float top = cy - h / 2.0f;

    for (int gy = 0; gy < sheet->size; gy++) {
        for (int gx = 0; gx < sheet->size; gx++) {
            uint32_t packed = sheet->pixels[gy * sheet->size + gx];
            unsigned char a = (unsigned char)(packed & 0xFFu);
            if (a == 0) continue;

            Color c = {
                (unsigned char)((packed >> 24) & 0xFFu),
                (unsigned char)((packed >> 16) & 0xFFu),
                (unsigned char)((packed >> 8) & 0xFFu),
                a,
            };
            if (god_tint) c = lerp_color(c, *god_tint, 0.6f);

            gp_fill_rect(ctx->renderer, left + (float)gx * cell_w, top + (float)gy * cell_h,
                         cell_w + 0.5f, cell_h + 0.5f, c);
        }
    }
}

static void draw_player(SdlRendererCtx *ctx, const GameState *gs) {
    const Player *p = &gs->player;
    if (!p->alive) return;

    float size_mult = ship_size_multiplier(gs->selected_ship);
    float w = PLAYER_WIDTH * gs->scale * size_mult;
    float h = PLAYER_HEIGHT * gs->scale * size_mult;
    const Color *tint = p->god_mode ? &kGodModeTint : NULL;

    if (gs->selected_ship == SHIP_TWINS) {
        /* Two independent blits of the same single-twin sheet - see
         * ship_twins_sprite's own doc comment - each skipped once that
         * twin's own life is spent (see the Player struct's own doc
         * comment for twins_right_x/twins_left_x). */
        if (p->twins_right_alive) {
            draw_ship_sprite(ctx, &kShipSprites[SHIP_TWINS], p->twins_right_x, p->y, w, h, tint);
        }
        if (p->twins_left_alive) {
            draw_ship_sprite(ctx, &kShipSprites[SHIP_TWINS], p->twins_left_x, p->y, w, h, tint);
        }
        return;
    }

    draw_ship_sprite(ctx, &kShipSprites[gs->selected_ship], p->x, p->y, w, h, tint);
}

/* Cruzader's own mode 2 deflector orb (see check_collisions and
 * trigger_cruzader_orb in usecases/game_logic.c) - a translucent silver/
 * gold/blue shield sphere at CRUZADER_ORB_RADIUS, matching the ship's own
 * palette, drawn just before the ship so Cruzader reads as standing inside
 * it. Alpha eases down over the last portion of the active window as a
 * visual cue the orb is about to drop, rather than popping off abruptly. */
static void draw_cruzader_orb(SdlRendererCtx *ctx, const GameState *gs) {
    const Player *p = &gs->player;
    if (gs->selected_ship != SHIP_CRUZADER || p->cruzader_orb_timer <= 0.0f) return;

    float life = p->cruzader_orb_timer / CRUZADER_ORB_DURATION;
    float fade = life < 0.25f ? life / 0.25f : 1.0f;
    float r = CRUZADER_ORB_RADIUS * gs->scale;

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    /* Soft outer glow, a gold rim (a slightly smaller near-black glass fill
     * drawn on top leaves a visible ring of it showing), then a translucent
     * blue "glass" interior - reads as a shield sphere, not a solid ball. */
    Color glow = (Color){140, 190, 255, (unsigned char)(50.0f * fade)};
    gp_fill_circle(ctx->renderer, p->x, p->y, r * 1.08f, glow);

    Color rim = (Color){212, 175, 90, (unsigned char)(200.0f * fade)};
    gp_fill_circle(ctx->renderer, p->x, p->y, r, rim);

    Color glass = (Color){80, 140, 220, (unsigned char)(60.0f * fade)};
    gp_fill_circle(ctx->renderer, p->x, p->y, r * 0.9f, glass);
}

/* Children always render at the stock size regardless of which ship
 * dispatched them (see ship_size_multiplier's own doc comment) - same blit
 * as draw_player, just never scaled up and never god-mode-tinted (that
 * toggle is player-only). */
static void draw_child(SdlRendererCtx *ctx, const GameState *gs, const ChildShip *c) {
    if (!c->alive) return;

    float w = PLAYER_WIDTH * gs->scale;
    float h = PLAYER_HEIGHT * gs->scale;
    draw_ship_sprite(ctx, &kShipSprites[c->kind], c->x, c->y, w, h, NULL);
}

/* Shared by draw_enemy and draw_boss: the boss presents as "a randomly
 * picked enemy" at a much larger size, so it's the same sprite blit as a
 * regular enemy just given a bigger bounding size and, since it renders
 * far larger, the full-native-resolution texture (see draw_boss) instead
 * of the small one regular enemies use. Pixel-perfect reproduction of the
 * 16 reference designs, embedded as RGBA grids (see adapters/enemy_sprites)
 * and uploaded once to a GPU texture per kind - up to MAX_ENEMIES of these
 * can be on screen at once, so each is one textured blit rather than one
 * gp_fill_rect per pixel (SDL_HINT_RENDER_SCALE_QUALITY is set to nearest
 * globally at renderer creation, so scaling still lands on crisp pixel
 * blocks, not a blurred stretch). */
static void draw_sprite(SdlRendererCtx *ctx, SDL_Texture *tex, const EnemySpriteSheet *sheet,
                         float x, float y, float size) {
    int long_side = sheet->grid_w > sheet->grid_h ? sheet->grid_w : sheet->grid_h;
    float w = size * (float)sheet->grid_w / (float)long_side;
    float h = size * (float)sheet->grid_h / (float)long_side;

    SDL_FRect dst = {x - w / 2.0f, y - h / 2.0f, w, h};
    SDL_RenderCopyF(ctx->renderer, tex, NULL, &dst);
}

static void draw_enemy(SdlRendererCtx *ctx, const Enemy *e) {
    if (!e->alive) return;
    draw_sprite(ctx, ctx->enemy_textures[e->kind], &kEnemySprites[e->kind], e->x, e->y, e->size);
}

static void draw_boss(SdlRendererCtx *ctx, const Boss *b) {
    if (!b->alive) return;
    draw_sprite(ctx, ctx->boss_textures[b->kind], &kBossSprites[b->kind], b->x, b->y, b->size);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    /* This ring IS the fatal contact boundary (see check_collisions in
     * usecases/game_logic.c, which reads the exact same
     * BOSS_MENACE_RING_RATIO) - drawn boldly since touching it is
     * instant death for both the boss and the player. */
    Color menace = (Color){255, 30, 30, 200};
    float ring_radius = b->size * BOSS_MENACE_RING_RATIO;
    gp_draw_circle_outline(ctx->renderer, b->x, b->y, ring_radius, menace);
    gp_draw_circle_outline(ctx->renderer, b->x, b->y, ring_radius - 1.0f, menace);
}

/* A rounded capsule (a straight-sided rectangular body with a circular cap
 * at each end - a "stadium" shape) centered at (cx, cy) and oriented along
 * (dx, dy) (a unit vector) instead of always straight down - the enemy
 * beam styles' counterpart to draw_player_bolt_vertical's ellipse,
 * generalized to any direction since gp_fill_ellipse has no rotation of
 * its own. Unlike a tapered/pointed bolt (which reads as a plain triangle
 * at this game's small scale), the body never narrows, so this stays a
 * visibly rounded, modern beam at any width. (px, py) is the perpendicular
 * of (dx, dy), passed in rather than derived here since every caller
 * already has it. */
static void capsule_bolt(SDL_Renderer *r, float cx, float cy, float dx, float dy, float px, float py,
                          float half_len, float half_wid, Color c) {
    float bx = dx * half_len, by = dy * half_len;
    float wx = px * half_wid, wy = py * half_wid;
    gp_fill_quad(r, cx + bx + wx, cy + by + wy, cx - bx + wx, cy - by + wy,
                 cx - bx - wx, cy - by - wy, cx + bx - wx, cy + by - wy, c);
    gp_fill_circle(r, cx + bx, cy + by, half_wid, c);
    gp_fill_circle(r, cx - bx, cy - by, half_wid, c);
}

/* Modes 1/4/5's shot (SHOOT_MODE_NORMAL, DOUBLE, SIDE): a layered glow (dim,
 * wide, translucent) around a saturated core around a near-white hot
 * streak, the same construction Star Wars-style blaster bolts use - built
 * from nested primitives since there's no blur to fake it with. pr->color
 * (the cycling player laser hue) drives every layer's hue; only alpha and
 * the lerp-toward-white amount vary between layers. Oriented vertically
 * (nose-forward): the horizontal counterpart below is the same
 * construction with x/y swapped for side beams. */
static void draw_player_bolt_vertical(SdlRendererCtx *ctx, const Projectile *pr, float scale) {
    float core_rx = PLAYER_PROJECTILE_W * 0.9f * scale;
    float core_ry = PLAYER_PROJECTILE_H * 0.5f * scale;
    float cx = pr->x, cy = pr->y;

    Color glow = pr->color;
    glow.a = 55;
    gp_fill_ellipse(ctx->renderer, cx, cy, core_rx * 2.6f, core_ry * 1.2f, glow);
    glow.a = 120;
    gp_fill_ellipse(ctx->renderer, cx, cy, core_rx * 1.7f, core_ry * 1.1f, glow);

    gp_fill_ellipse(ctx->renderer, cx, cy, core_rx, core_ry, pr->color);

    Color hot = lerp_color(pr->color, kWhite, 0.85f);
    gp_fill_ellipse(ctx->renderer, cx, cy, core_rx * 0.4f, core_ry * 0.85f, hot);

    Color glint = kWhite;
    glint.a = 200;
    gp_fill_circle(ctx->renderer, cx, cy - core_ry * 0.55f, core_rx * 0.45f, glint);
}

/* Mode 5's shot (SHOOT_MODE_SIDE): the same layered bolt as
 * draw_player_bolt_vertical, rotated 90 degrees - travel direction (vx's
 * sign) is the long axis instead of always "up", so the glint sits at
 * whichever end (left or right) is actually the leading tip. */
static void draw_player_bolt_horizontal(SdlRendererCtx *ctx, const Projectile *pr, float scale) {
    float core_ry = PLAYER_PROJECTILE_W * 0.9f * scale;
    float core_rx = PLAYER_PROJECTILE_H * 0.5f * scale;
    float cx = pr->x, cy = pr->y;
    float dir = pr->vx >= 0.0f ? 1.0f : -1.0f;

    Color glow = pr->color;
    glow.a = 55;
    gp_fill_ellipse(ctx->renderer, cx, cy, core_rx * 1.2f, core_ry * 2.6f, glow);
    glow.a = 120;
    gp_fill_ellipse(ctx->renderer, cx, cy, core_rx * 1.1f, core_ry * 1.7f, glow);

    gp_fill_ellipse(ctx->renderer, cx, cy, core_rx, core_ry, pr->color);

    Color hot = lerp_color(pr->color, kWhite, 0.85f);
    gp_fill_ellipse(ctx->renderer, cx, cy, core_rx * 0.85f, core_ry * 0.4f, hot);

    Color glint = kWhite;
    glint.a = 200;
    gp_fill_circle(ctx->renderer, cx + dir * core_rx * 0.55f, cy, core_ry * 0.45f, glint);
}

/* Mode 2's shot (SHOOT_MODE_RAPID): a small glowing sphere in the cycling
 * laser color - same layered glow/core/hot/glint construction as the bolt
 * above and draw_orb, just built from concentric circles since a rapid-fire
 * shot has no "forward tip" worth emphasizing. */
static void draw_rapid_shot(SdlRendererCtx *ctx, const Projectile *pr, float scale) {
    float r = RAPID_FIRE_PROJECTILE_RADIUS * scale;
    float cx = pr->x, cy = pr->y;

    Color glow = pr->color;
    glow.a = 70;
    gp_fill_circle(ctx->renderer, cx, cy, r * 2.4f, glow);
    glow.a = 130;
    gp_fill_circle(ctx->renderer, cx, cy, r * 1.5f, glow);

    gp_fill_circle(ctx->renderer, cx, cy, r, pr->color);

    Color hot = lerp_color(pr->color, kWhite, 0.6f);
    gp_fill_circle(ctx->renderer, cx, cy, r * 0.4f, hot);

    Color glint = kWhite;
    glint.a = 220;
    gp_fill_circle(ctx->renderer, cx - r * 0.35f, cy - r * 0.35f, r * 0.2f, glint);
}

/* Mode 3's shot (SHOOT_MODE_POWER): a bigger sphere, always white per the
 * ability's spec (unlike every other mode it ignores the cycling laser
 * color) with a crisp rim outline so its bigger explode-on-contact hitbox
 * reads clearly against the smaller normal/rapid shots. */
static void draw_power_shot(SdlRendererCtx *ctx, const Projectile *pr, float scale) {
    float r = POWER_CANNON_PROJECTILE_RADIUS * scale;
    float cx = pr->x, cy = pr->y;
    static const Color kPowerWhite = {245, 248, 255, 255};
    static const Color kPowerRim = {190, 215, 255, 255};

    Color glow = kPowerWhite;
    glow.a = 60;
    gp_fill_circle(ctx->renderer, cx, cy, r * 2.2f, glow);
    glow.a = 120;
    gp_fill_circle(ctx->renderer, cx, cy, r * 1.5f, glow);

    gp_fill_circle(ctx->renderer, cx, cy, r, kPowerWhite);
    gp_draw_circle_outline(ctx->renderer, cx, cy, r, kPowerRim);

    Color glint = kWhite;
    glint.a = 235;
    gp_fill_circle(ctx->renderer, cx - r * 0.3f, cy - r * 0.3f, r * 0.25f, glint);
}

/* Enemy shooting styles thin beam/long beam/trishot's shot
 * (EnemyProjectileKind ENEMY_PROJECTILE_BEAM): the same layered glow/core/
 * hot/glint construction and proportions as draw_player_bolt_vertical
 * ("like the player's, but slimmer"), built from capsule_bolt instead of
 * gp_fill_ellipse so it can point along its own travel direction rather
 * than always straight down - trishot's two diagonal beams need that to
 * visibly aim where they're actually going. fade scales every layer's
 * alpha (see draw_projectile) for an inert shot's fade-out. pr->half_len/
 * half_wid (already scaled at spawn) are this shot's own proportions, so
 * one function serves every beam style. */
static void draw_enemy_beam(SdlRendererCtx *ctx, const Projectile *pr, float fade) {
    float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
    float dx = speed > 0.0f ? pr->vx / speed : 0.0f;
    float dy = speed > 0.0f ? pr->vy / speed : 1.0f;
    float px = -dy, py = dx;
    float cx = pr->x, cy = pr->y;
    float half_len = pr->half_len, half_wid = pr->half_wid;

    Color glow = pr->color;
    glow.a = (unsigned char)(55.0f * fade);
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 1.2f, half_wid * 2.6f, glow);
    glow.a = (unsigned char)(120.0f * fade);
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 1.1f, half_wid * 1.7f, glow);

    Color core = pr->color;
    core.a = (unsigned char)(255.0f * fade);
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len, half_wid, core);

    Color hot = lerp_color(pr->color, kWhite, 0.85f);
    hot.a = (unsigned char)(230.0f * fade);
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 0.85f, half_wid * 0.4f, hot);

    Color glint = kWhite;
    glint.a = (unsigned char)(200.0f * fade);
    gp_fill_circle(ctx->renderer, cx + dx * half_len * 0.55f, cy + dy * half_len * 0.55f,
                   half_wid * 0.45f, glint);
}

/* Enemy shooting styles triburst/omni-shot's shot (EnemyProjectileKind
 * ENEMY_PROJECTILE_ORB): the same layered glow/core/hot/glint sphere as
 * draw_rapid_shot, just driven by pr->color (varies enemy to enemy - see
 * Enemy.color in domain/types.h) and pr->half_len as its radius (already
 * scaled at spawn) instead of a single fixed player constant, plus fade
 * support for an inert shot's fade-out. */
static void draw_enemy_orb(SdlRendererCtx *ctx, const Projectile *pr, float fade) {
    float r = pr->half_len;
    float cx = pr->x, cy = pr->y;

    Color glow = pr->color;
    glow.a = (unsigned char)(70.0f * fade);
    gp_fill_circle(ctx->renderer, cx, cy, r * 2.2f, glow);
    glow.a = (unsigned char)(130.0f * fade);
    gp_fill_circle(ctx->renderer, cx, cy, r * 1.4f, glow);

    Color core = pr->color;
    core.a = (unsigned char)(255.0f * fade);
    gp_fill_circle(ctx->renderer, cx, cy, r, core);

    Color hot = lerp_color(pr->color, kWhite, 0.6f);
    hot.a = (unsigned char)(230.0f * fade);
    gp_fill_circle(ctx->renderer, cx, cy, r * 0.4f, hot);

    Color glint = kWhite;
    glint.a = (unsigned char)(220.0f * fade);
    gp_fill_circle(ctx->renderer, cx - r * 0.35f, cy - r * 0.35f, r * 0.2f, glint);
}

/* C-24's own signature look - the same layered glow/core/hot/glint sphere
 * construction as draw_enemy_orb above (the octagonal enemy shot style's
 * own look, whose size and glow read well), but continuously cycling hue
 * the way the power orb's own color does (draw_orb/update_orb) rather than
 * a fixed hue - "that's the effect I'm going for." Not a copy-paste of the
 * orb's own mechanism though: the orb mutates one shared, stored Orb.hue a
 * little every frame (update_orb); here the hue is instead computed fresh
 * every frame straight from (time, phase_seed) with no stored-and-mutated
 * state of its own, offset per shot by phase_seed (see spawn_player_shot)
 * so simultaneous shots - a double-barrel pair, all 8 of an omni burst -
 * don't cycle in lockstep. Mode 2 (still PROJECTILE_KIND_POWER - see
 * player_shot_half_extents) renders 8x bigger than modes 1/3, matching its
 * own bigger hitbox. */
static void draw_c24_sphere_shot(SdlRendererCtx *ctx, const Projectile *pr, float scale, float time) {
    float radius_design = pr->kind == PROJECTILE_KIND_POWER ? SHIP_C24_POWER_MODE_RADIUS : SHIP_C24_PROJECTILE_RADIUS;
    float r = radius_design * scale;
    float cx = pr->x, cy = pr->y;

    float phase_seed_deg = pr->phase_seed * (180.0f / (float)M_PI);
    float hue = fmodf(time * SHIP_C24_PROJECTILE_HUE_CYCLE_SPEED + phase_seed_deg, 360.0f);
    if (hue < 0.0f) hue += 360.0f;
    Color base = color_from_hsv(hue, 0.85f, 1.0f);

    Color glow = base;
    glow.a = 70;
    gp_fill_circle(ctx->renderer, cx, cy, r * 2.2f, glow);
    glow.a = 130;
    gp_fill_circle(ctx->renderer, cx, cy, r * 1.4f, glow);

    gp_fill_circle(ctx->renderer, cx, cy, r, base);

    Color hot = lerp_color(base, kWhite, 0.55f);
    gp_fill_circle(ctx->renderer, cx, cy, r * 0.4f, hot);

    Color glint = kWhite;
    glint.a = 220;
    gp_fill_circle(ctx->renderer, cx - r * 0.35f, cy - r * 0.35f, r * 0.2f, glint);
}

/* One layer of a Shine shard's icicle silhouette: a pointed front tip and
 * a blunter, shorter tapered tail, oriented along (dx, dy) with (px, py)
 * its perpendicular - the same direction-vector construction capsule_bolt
 * above uses for enemy beams, just built from a single 4-point kite
 * (gp_fill_quad) tapered to points instead of a rounded-cap capsule, for
 * the crystalline look. Called several times at different sizes/colors by
 * draw_shine_shard for the layered glow/body/core look every other
 * player bolt already uses. */
static void shine_shard_layer(SDL_Renderer *r, float cx, float cy, float dx, float dy, float px, float py,
                               float front_len, float back_len, float half_wid, Color c) {
    float fx = cx + dx * front_len, fy = cy + dy * front_len;
    float bx = cx - dx * back_len, by = cy - dy * back_len;
    float lx = cx + px * half_wid, ly = cy + py * half_wid;
    float rx = cx - px * half_wid, ry = cy - py * half_wid;
    gp_fill_quad(r, fx, fy, lx, ly, bx, by, rx, ry, c);
}

/* Every one of Shine's own shots (modes 1/2/3 alike - see
 * Projectile.style_ship) - "crystal shards... white with light-grey-ish
 * accents": a wide translucent accent-colored glow, a solid accent body,
 * and a bright white core, same layered construction as every other
 * player bolt just built from shine_shard_layer's pointed kite instead of
 * an ellipse/capsule. Modes 1/2 (PROJECTILE_KIND_NORMAL) orient along the
 * shot's own actual travel direction - straight up for mode 1, radiating
 * outward per-shard for mode 2's omni burst. Mode 3's longer
 * PROJECTILE_KIND_SHINE_SPIRAL shot spins in place instead: its drawn
 * orientation comes from GameState.time_elapsed and the shot's own
 * phase_seed (the same "per-instance seed plus the global clock"
 * convention C-24's hue-cycling already uses), completely independent of
 * its (still straight-up) travel direction. */
static void draw_shine_shard(SdlRendererCtx *ctx, const Projectile *pr, float scale, float time) {
    static const Color kShineWhite = {250, 251, 255, 255};
    static const Color kShineAccent = {195, 205, 225, 255};

    bool spiral = pr->kind == PROJECTILE_KIND_SHINE_SPIRAL;
    float half_len = (spiral ? SHINE_SPIRAL_SHARD_LENGTH : SHINE_SHARD_LENGTH) * 0.5f * scale;
    float half_wid = (spiral ? SHINE_SPIRAL_SHARD_WIDTH : SHINE_SHARD_WIDTH) * 0.5f * scale;

    float dx, dy;
    if (spiral) {
        float phase_deg = pr->phase_seed * (180.0f / (float)M_PI);
        float angle_deg = fmodf(time * SHINE_SPIRAL_SPIN_SPEED + phase_deg, 360.0f);
        float angle = angle_deg * ((float)M_PI / 180.0f);
        dx = sinf(angle);
        dy = -cosf(angle);
    } else {
        float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
        dx = speed > 0.0f ? pr->vx / speed : 0.0f;
        dy = speed > 0.0f ? pr->vy / speed : -1.0f;
    }
    float px = -dy, py = dx;
    float cx = pr->x, cy = pr->y;

    Color glow = kShineAccent;
    glow.a = 70;
    shine_shard_layer(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 1.3f, half_len * 0.9f, half_wid * 2.2f, glow);

    shine_shard_layer(ctx->renderer, cx, cy, dx, dy, px, py, half_len, half_len * 0.6f, half_wid, kShineAccent);

    shine_shard_layer(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 0.85f, half_len * 0.4f, half_wid * 0.55f,
                       kShineWhite);

    Color glint = kWhite;
    glint.a = 220;
    gp_fill_circle(ctx->renderer, cx + dx * half_len * 0.5f, cy + dy * half_len * 0.5f, half_wid * 0.35f, glint);
}

/* Cruzader's own mode 1 (twin wingtip bolts) - "green with blue accents"
 * per spec: the same layered glow/core/hot/glint capsule_bolt construction
 * draw_enemy_beam uses, oriented along the shot's own (always-straight-up)
 * travel direction, just recolored - green core, blue glow/hot layers
 * instead of a single per-shot hue. Reflected shots (see reflect_enemy_shot
 * in usecases/game_logic.c) never reach this function - they stay in
 * gs->enemy_shots and keep rendering with their original enemy design
 * (draw_enemy_beam/draw_enemy_orb), per feedback that a reflected
 * projectile must not change appearance. */
static void draw_cruzader_bolt(SdlRendererCtx *ctx, const Projectile *pr, float scale) {
    static const Color kCruzaderGreen = {70, 210, 100, 255};
    static const Color kCruzaderBlue = {90, 180, 255, 255};

    float half_len = CRUZADER_BOLT_LENGTH * 0.5f * scale;
    float half_wid = CRUZADER_BOLT_WIDTH * 0.5f * scale;
    float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
    float dx = speed > 0.0f ? pr->vx / speed : 0.0f;
    float dy = speed > 0.0f ? pr->vy / speed : -1.0f;
    float px = -dy, py = dx;
    float cx = pr->x, cy = pr->y;

    Color glow = kCruzaderBlue;
    glow.a = 70;
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 1.3f, half_wid * 2.4f, glow);
    glow.a = 130;
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 1.15f, half_wid * 1.6f, glow);

    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len, half_wid, kCruzaderGreen);

    Color hot = lerp_color(kCruzaderBlue, kWhite, 0.5f);
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 0.8f, half_wid * 0.4f, hot);

    Color glint = kWhite;
    glint.a = 220;
    gp_fill_circle(ctx->renderer, cx + dx * half_len * 0.5f, cy + dy * half_len * 0.5f, half_wid * 0.35f, glint);
}

/* The Twins' own bolt (SHOOT_MODE_TWINS_ALTERNATE/_MIRROR, see
 * update_twins_alternating_fire) - same capsule_bolt glow/core/hot/glint
 * layering as draw_cruzader_bolt above, recolored silver/blue to match the
 * reference rocket art instead of Cruzader's green/blue. */
static void draw_twins_bolt(SdlRendererCtx *ctx, const Projectile *pr, float scale) {
    static const Color kTwinsSilver = {200, 210, 220, 255};
    static const Color kTwinsBlue = {90, 180, 255, 255};

    float half_len = TWINS_BOLT_LENGTH * 0.5f * scale;
    float half_wid = TWINS_BOLT_WIDTH * 0.5f * scale;
    float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
    float dx = speed > 0.0f ? pr->vx / speed : 0.0f;
    float dy = speed > 0.0f ? pr->vy / speed : -1.0f;
    float px = -dy, py = dx;
    float cx = pr->x, cy = pr->y;

    Color glow = kTwinsBlue;
    glow.a = 70;
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 1.3f, half_wid * 2.4f, glow);
    glow.a = 130;
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 1.15f, half_wid * 1.6f, glow);

    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len, half_wid, kTwinsSilver);

    Color hot = lerp_color(kTwinsBlue, kWhite, 0.5f);
    capsule_bolt(ctx->renderer, cx, cy, dx, dy, px, py, half_len * 0.8f, half_wid * 0.4f, hot);

    Color glint = kWhite;
    glint.a = 220;
    gp_fill_circle(ctx->renderer, cx + dx * half_len * 0.5f, cy + dy * half_len * 0.5f, half_wid * 0.35f, glint);
}

/* Cruzader's own mode 3 rockets - "white with a red tip" per spec: the same
 * capsule_bolt body as draw_cruzader_bolt above, just white with a small
 * red circle at the nose for the tip instead of a green/blue palette; the
 * "increased smoke trail" ask is handled purely by a shorter
 * trail_emit_timer reset for this kind (see update_projectile_trails in
 * usecases/game_logic.c), not anything drawn here. */
/* Cruzader's own mode 3 rocket - the reference sprite (kCruzaderRocketSpritePixels,
 * see adapters/cruzader_rocket_sprite.h) rotated to match the shot's own
 * actual travel direction every frame, the same "keeps its exact original
 * design" bar as every other sprite in this game, just drawn per-texel
 * instead of axis-aligned like draw_ship_sprite (a rocket homes, so it can
 * point any way - a ship never rotates, so that function never needed
 * this). Each opaque texel becomes one small quad: `right`/`forward` are
 * the shot's own perpendicular/travel unit vectors (same construction
 * capsule_bolt's callers already use), and a texel's local offset from the
 * sprite's center - COLS wide, ROWS tall, row 0 = nose - is expressed in
 * that (right, forward) basis instead of plain (x, y) before being placed
 * in the world. Scaled to fill exactly CRUZADER_ROCKET_LENGTH x
 * CRUZADER_ROCKET_WIDTH (the same bounding box the old procedural design
 * used, and the one player_shot_half_extents already hit-tests against) -
 * "keep the size, change the design" per feedback. */
static void draw_cruzader_rocket(SdlRendererCtx *ctx, const Projectile *pr, float scale) {
    const int cols = CRUZADER_ROCKET_SPRITE_COLS;
    const int rows = CRUZADER_ROCKET_SPRITE_ROWS;
    float width = CRUZADER_ROCKET_WIDTH * scale;
    float height = CRUZADER_ROCKET_LENGTH * scale;
    float cell_w = width / (float)cols;
    float cell_h = height / (float)rows;
    float half_cell_w = cell_w / 2.0f;
    float half_cell_h = cell_h / 2.0f;

    float speed = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy);
    float forward_x = speed > 0.0f ? pr->vx / speed : 0.0f;
    float forward_y = speed > 0.0f ? pr->vy / speed : -1.0f;
    float right_x = -forward_y, right_y = forward_x;
    float cx = pr->x, cy = pr->y;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            uint32_t packed = kCruzaderRocketSpritePixels[row * cols + col];
            unsigned char a = (unsigned char)(packed & 0xFFu);
            if (a == 0) continue;
            Color c = {
                (unsigned char)((packed >> 24) & 0xFFu),
                (unsigned char)((packed >> 16) & 0xFFu),
                (unsigned char)((packed >> 8) & 0xFFu),
                a,
            };

            float local_right = ((float)col + 0.5f - (float)cols / 2.0f) * cell_w;
            float local_forward = ((float)rows / 2.0f - ((float)row + 0.5f)) * cell_h;

            float c0r = local_right - half_cell_w, c0f = local_forward + half_cell_h;
            float c1r = local_right + half_cell_w, c1f = local_forward + half_cell_h;
            float c2r = local_right + half_cell_w, c2f = local_forward - half_cell_h;
            float c3r = local_right - half_cell_w, c3f = local_forward - half_cell_h;

            gp_fill_quad(ctx->renderer,
                         cx + c0r * right_x + c0f * forward_x, cy + c0r * right_y + c0f * forward_y,
                         cx + c1r * right_x + c1f * forward_x, cy + c1r * right_y + c1f * forward_y,
                         cx + c2r * right_x + c2f * forward_x, cy + c2r * right_y + c2f * forward_y,
                         cx + c3r * right_x + c3f * forward_x, cy + c3r * right_y + c3f * forward_y,
                         c);
        }
    }
}

static void draw_projectile(SdlRendererCtx *ctx, const GameState *gs, const Projectile *pr, bool is_player) {
    if (!pr->alive) return;
    float scale = gs->scale;

    float fade = 1.0f;
    if (!is_player && pr->inert) {
        fade = 1.0f - pr->inert_age / ENEMY_SHOT_FADE_DURATION;
        if (fade < 0.0f) fade = 0.0f;
    }

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    if (is_player) {
        /* Keyed off the shot's own style_ship, not gs->selected_ship
         * directly, so a C-24-kind ChildShip's own shots still render as
         * C-24's sphere while selected_ship is SHIP_MOTHERSHIP. */
        if (pr->style_ship == SHIP_C24) {
            draw_c24_sphere_shot(ctx, pr, scale, gs->time_elapsed);
            return;
        }
        if (pr->style_ship == SHIP_SHINE) {
            draw_shine_shard(ctx, pr, scale, gs->time_elapsed);
            return;
        }
        if (pr->style_ship == SHIP_CRUZADER) {
            if (pr->kind == PROJECTILE_KIND_CRUZADER_ROCKET) {
                draw_cruzader_rocket(ctx, pr, scale);
            } else {
                draw_cruzader_bolt(ctx, pr, scale);
            }
            return;
        }
        if (pr->style_ship == SHIP_TWINS) {
            draw_twins_bolt(ctx, pr, scale);
            return;
        }
        switch (pr->kind) {
            case PROJECTILE_KIND_RAPID:
                draw_rapid_shot(ctx, pr, scale);
                break;
            case PROJECTILE_KIND_POWER:
                draw_power_shot(ctx, pr, scale);
                break;
            case PROJECTILE_KIND_NORMAL:
            default:
                if (pr->horizontal) {
                    draw_player_bolt_horizontal(ctx, pr, scale);
                } else {
                    draw_player_bolt_vertical(ctx, pr, scale);
                }
                break;
        }
    } else {
        switch (pr->enemy_kind) {
            case ENEMY_PROJECTILE_ORB:
                draw_enemy_orb(ctx, pr, fade);
                break;
            case ENEMY_PROJECTILE_BEAM:
            default:
                draw_enemy_beam(ctx, pr, fade);
                break;
        }
    }
}

/* Same layered glow/core/hot construction as the projectile bolts and the
 * orb - a soft wide halo, the saturated blast color, and a near-white hot
 * flash at the center - plus the existing radiating spark lines for the
 * shrapnel burst. core (pale yellow-white cooling to orange-red as the
 * explosion ages) still drives every layer's hue; only alpha and
 * lerp-toward-white amounts vary. */
static void draw_explosion(SdlRendererCtx *ctx, const Explosion *e) {
    if (!e->alive) return;
    float t = e->age / e->max_age;
    float radius = e->max_radius * (0.35f + 0.65f * t);

    Color core = lerp_color((Color){255, 255, 210, 255}, (Color){255, 120, 30, 255}, t);
    unsigned char alpha = (unsigned char)(255.0f * (1.0f - t));
    core.a = alpha;

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    Color glow = core;
    glow.a = (unsigned char)((float)alpha * 0.35f);
    gp_fill_circle(ctx->renderer, e->x, e->y, radius * 1.6f, glow);

    gp_fill_circle(ctx->renderer, e->x, e->y, radius, core);

    Color hot = lerp_color(core, kWhite, 0.6f);
    hot.a = alpha;
    gp_fill_circle(ctx->renderer, e->x, e->y, radius * 0.45f, hot);

    if (t > 0.1f && t < 0.75f) {
        Color spark = lerp_color((Color){255, 220, 120, 255}, (Color){180, 40, 20, 255}, t);
        spark.a = alpha;
        for (int k = 0; k < 8; k++) {
            float ang = (float)k * (float)M_PI / 4.0f + t * 1.5f;
            float len = radius * 1.6f;
            gp_draw_line(ctx->renderer, e->x, e->y, e->x + cosf(ang) * len, e->y + sinf(ang) * len, spark);
        }
    }
}

/* Same layered glow/core/hot/glint construction as draw_projectile's
 * bolts, just built from concentric circles instead of an elongated
 * ellipse - a soft wide aura, a saturated sphere, a near-white hot core,
 * the crisp retro rim outline, and a specular glint. o->color (driven by
 * the hue cycling in update_orb) still drives every layer's hue; only
 * alpha and lerp-toward-white amounts vary, so the color-phasing logic is
 * untouched. */
static void draw_orb(SdlRendererCtx *ctx, const Orb *o) {
    if (!o->alive) return;
    float r = o->size / 2.0f;

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    Color glow = o->color;
    glow.a = 55;
    gp_fill_circle(ctx->renderer, o->x, o->y, r * 2.2f, glow);
    glow.a = 110;
    gp_fill_circle(ctx->renderer, o->x, o->y, r * 1.5f, glow);

    gp_fill_circle(ctx->renderer, o->x, o->y, r, o->color);

    Color hot = lerp_color(o->color, kWhite, 0.55f);
    gp_fill_circle(ctx->renderer, o->x, o->y, r * 0.4f, hot);

    gp_draw_circle_outline(ctx->renderer, o->x, o->y, r, lerp_color(o->color, kWhite, 0.5f));

    Color glint = kWhite;
    glint.a = 230;
    gp_fill_circle(ctx->renderer, o->x - r * 0.4f, o->y - r * 0.4f, r * 0.18f, glint);
}

static void draw_super_beam(SdlRendererCtx *ctx, const GameState *gs) {
    const Player *p = &gs->player;
    if (p->super_beam_timer <= 0.0f || !p->alive) return;

    /* Purely cosmetic: a fast width flicker and a fast-cycling hue. The
     * beam's actual neutralize width (usecases/game_logic.c) is fixed, so
     * this animation never changes what the beam actually hits. */
    float base_w = PLAYER_PROJECTILE_W * gs->scale * SUPER_BEAM_WIDTH_MULTIPLIER;
    float width_factor = 1.0f + SUPER_BEAM_WIDTH_PULSE_AMOUNT * sinf(gs->time_elapsed * SUPER_BEAM_WIDTH_PULSE_SPEED);
    float beam_w = base_w * width_factor;
    float top = p->y - PLAYER_HEIGHT * gs->scale / 2.0f;
    float alpha_pulse = 0.75f + 0.25f * sinf(gs->time_elapsed * 18.0f);

    float hue = fmodf(gs->time_elapsed * SUPER_BEAM_COLOR_CYCLE_SPEED, 360.0f);
    if (hue < 0.0f) hue += 360.0f;
    Color beam_hue = color_from_hsv(hue, 0.85f, 1.0f);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    Color glow = lerp_color(beam_hue, kWhite, 0.15f);
    glow.a = (unsigned char)(70.0f * alpha_pulse);
    Color core = lerp_color(beam_hue, kWhite, 0.6f);
    core.a = (unsigned char)(235.0f * alpha_pulse);

    /* One column per currently-alive twin (matches update_super_beam's own
     * player_beam_origin_xs in usecases/game_logic.c exactly, so the beam
     * always looks like it's hitting precisely what it actually hits) -
     * every other ship still draws its single column at p->x, unchanged. */
    float origins[2];
    int origin_count = 1;
    origins[0] = p->x;
    if (gs->selected_ship == SHIP_TWINS) {
        origin_count = 0;
        if (p->twins_right_alive) origins[origin_count++] = p->twins_right_x;
        if (p->twins_left_alive) origins[origin_count++] = p->twins_left_x;
    }

    for (int i = 0; i < origin_count; i++) {
        gp_fill_rect(ctx->renderer, origins[i] - beam_w * 1.2f, 0.0f, beam_w * 2.4f, top, glow);
        gp_fill_rect(ctx->renderer, origins[i] - beam_w / 2.0f, 0.0f, beam_w, top, core);
    }
}

/* Engine exhaust: fades in and out over its TRAIL_PARTICLE_LIFETIME-second
 * life (never popping in or vanishing abruptly), cooling from a fire color
 * to smoke-grey as it ages, expanding a little the way dispersing smoke
 * does. Capped at TRAIL_PARTICLE_MAX_ALPHA (~25%) even at its brightest, per
 * the "subtle, kinda faded" ask - deliberately a single soft blended circle
 * rather than the multi-layer glow/core/hot construction the projectiles
 * and explosions use, since a whole trail of those would be anything but
 * subtle. */
static void draw_trail_particle(SdlRendererCtx *ctx, const TrailParticle *t) {
    if (!t->alive) return;

    float life = t->age / t->max_age; /* 0 = just spawned, 1 = about to expire */

    static const Color kFireCore = {255, 140, 40, 255};
    static const Color kSmoke = {90, 90, 95, 255};
    float cool = life < 0.4f ? life / 0.4f : 1.0f; /* fire cools into smoke over the first 40% of its life */
    Color color = lerp_color(kFireCore, kSmoke, cool);

    float radius = t->size * (1.0f + (TRAIL_PARTICLE_SIZE_GROWTH - 1.0f) * life);

    float fade_in = life < 0.08f ? life / 0.08f : 1.0f; /* brief fade-in so it doesn't pop into view */
    float fade_out = 1.0f - life;
    color.a = (unsigned char)((float)TRAIL_PARTICLE_MAX_ALPHA * fade_in * fade_out);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_circle(ctx->renderer, t->x, t->y, radius, color);
}

/* Same fire-cooling-to-smoke construction as draw_trail_particle (the
 * player's own engine trail) above, reused for enemies and the boss -
 * t->alpha_cap (baked in at spawn, see spawn_enemy_trail_particle) is what
 * makes enemies read at ~5% visibility and the boss at ~15%, both fainter
 * than the player's own TRAIL_PARTICLE_MAX_ALPHA so neither ever competes
 * with it for attention. */
static void draw_enemy_trail_particle(SdlRendererCtx *ctx, const EnemyTrailParticle *t) {
    if (!t->alive) return;

    float life = t->age / t->max_age;

    static const Color kFireCore = {255, 140, 40, 255};
    static const Color kSmoke = {90, 90, 95, 255};
    float cool = life < 0.4f ? life / 0.4f : 1.0f;
    Color color = lerp_color(kFireCore, kSmoke, cool);

    float radius = t->size * (1.0f + (TRAIL_PARTICLE_SIZE_GROWTH - 1.0f) * life);

    float fade_in = life < 0.08f ? life / 0.08f : 1.0f;
    float fade_out = 1.0f - life;
    color.a = (unsigned char)((float)t->alpha_cap * fade_in * fade_out);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_circle(ctx->renderer, t->x, t->y, radius, color);
}

/* The projectile counterpart to draw_trail_particle/draw_enemy_trail_particle
 * above - unlike those (which cool from a fixed fire-orange into gray
 * smoke as they age), t->color is captured once at spawn from the exact
 * projectile that emitted it (see spawn_projectile_trail_particle) and
 * never shifts hue here; only alpha (fade) and radius (growth) still
 * follow the same smoke curve, so every trail reads as "this shot's own
 * color" trailing behind it. */
static void draw_projectile_trail_particle(SdlRendererCtx *ctx, const ProjectileTrailParticle *t) {
    if (!t->alive) return;

    float life = t->age / t->max_age;
    Color color = t->color;

    float radius = t->size * (1.0f + (PROJECTILE_TRAIL_SIZE_GROWTH - 1.0f) * life);

    float fade_in = life < 0.08f ? life / 0.08f : 1.0f;
    float fade_out = 1.0f - life;
    color.a = (unsigned char)((float)t->alpha_cap * fade_in * fade_out);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_circle(ctx->renderer, t->x, t->y, radius, color);
}

static void draw_gameplay(SdlRendererCtx *ctx, const GameState *gs) {
    for (int i = 0; i < MAX_ENEMY_TRAIL_PARTICLES; i++) draw_enemy_trail_particle(ctx, &gs->enemy_trail_particles[i]);
    for (int i = 0; i < MAX_ENEMIES; i++) draw_enemy(ctx, &gs->enemies[i]);
    draw_boss(ctx, &gs->boss);
    draw_orb(ctx, &gs->orb);
    for (int i = 0; i < MAX_EXPLOSIONS; i++) draw_explosion(ctx, &gs->explosions[i]);
    for (int i = 0; i < MAX_PROJECTILE_TRAIL_PARTICLES; i++) draw_projectile_trail_particle(ctx, &gs->projectile_trails[i]);
    for (int i = 0; i < MAX_PLAYER_PROJECTILES; i++) draw_projectile(ctx, gs, &gs->player_shots[i], true);
    for (int i = 0; i < MAX_ENEMY_PROJECTILES; i++) draw_projectile(ctx, gs, &gs->enemy_shots[i], false);
    for (int i = 0; i < MAX_TRAIL_PARTICLES; i++) draw_trail_particle(ctx, &gs->trail_particles[i]);
    draw_super_beam(ctx, gs);
    for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) draw_child(ctx, gs, &gs->children[i]);
    draw_cruzader_orb(ctx, gs);
    draw_player(ctx, gs);
}

/* One life bar's worth of drawing, shared by draw_life_bar (a single bar,
 * every ship but SHIP_TWINS) and draw_twins_life_bars (two, stacked) below:
 * a grey outline always shows the full-bar extent, and a yellow fill (red
 * once life drops to PLAYER_LIFE_LOW_THRESHOLD or below) shrinks from the
 * right edge to reflect life, with the percentage (optionally prefixed,
 * for Twins' own "R"/"L" markers) centered on top. */
static void draw_one_life_bar(SdlRendererCtx *ctx, float scale, float x, float y, float w, float h, float life,
                               const char *prefix) {
    float outline_t = 2.0f * scale;

    gp_fill_rect(ctx->renderer, x, y, w, h, kDim);

    float inner_x = x + outline_t;
    float inner_y = y + outline_t;
    float inner_w = w - outline_t * 2.0f;
    float inner_h = h - outline_t * 2.0f;
    gp_fill_rect(ctx->renderer, inner_x, inner_y, inner_w, inner_h, kBackground);

    if (life < 0.0f) life = 0.0f;
    if (life > PLAYER_LIFE_MAX) life = PLAYER_LIFE_MAX;
    float fill_w = inner_w * (life / PLAYER_LIFE_MAX);
    if (fill_w > 0.0f) {
        Color fill_color = (life <= PLAYER_LIFE_LOW_THRESHOLD) ? kRed : kYellow;
        gp_fill_rect(ctx->renderer, inner_x, inner_y, fill_w, inner_h, fill_color);
    }

    char buf[12];
    if (prefix) {
        snprintf(buf, sizeof(buf), "%s %d%%", prefix, (int)(life + 0.5f));
    } else {
        snprintf(buf, sizeof(buf), "%d%%", (int)(life + 0.5f));
    }
    float text_size = (h * 0.5f) / 7.0f; /* the pixel font is 7 dots tall */
    float text_w = pf_text_width(buf, text_size);
    float text_h = 7.0f * text_size;
    pf_draw_text_plain(ctx->renderer, x + (w - text_w) / 2.0f, y + (h - text_h) / 2.0f, text_size, kDim, buf);
}

static void draw_life_bar(SdlRendererCtx *ctx, const GameState *gs) {
    float margin = 12.0f * gs->scale;
    float w = 130.0f * gs->scale;
    float h = 16.0f * gs->scale;
    draw_one_life_bar(ctx, gs->scale, margin, margin, w, h, gs->player.life, NULL);
}

/* The Twins' own dual life bars (see the Player struct's own doc comment
 * for why they need two) - same top-left corner and per-bar visual
 * language as draw_life_bar above, just stacked vertically (right twin's
 * own bar on top, left twin's directly below), each with an "R"/"L" prefix
 * so they're distinguishable. A dead twin's own life is already clamped to
 * 0 by kill_twin, so its bar simply renders empty/red - no separate "dead"
 * state needed. */
static void draw_twins_life_bars(SdlRendererCtx *ctx, const GameState *gs) {
    float margin = 12.0f * gs->scale;
    float w = 130.0f * gs->scale;
    float h = 16.0f * gs->scale;
    float gap = 4.0f * gs->scale;
    draw_one_life_bar(ctx, gs->scale, margin, margin, w, h, gs->player.twins_right_life, "R");
    draw_one_life_bar(ctx, gs->scale, margin, margin + h + gap, w, h, gs->player.twins_left_life, "L");
}

/* Top-center life bar for the boss fight: same grey-outline/red-fill
 * language as the player's life bar (draw_life_bar), with a "BOSS" label
 * centered underneath so it doesn't get read as a second player bar. Kept
 * away from the player's own top-left bar so the two never overlap.
 * hits_taken/hits_required drives the fill exactly like gs->player.life
 * drives the player bar - full at 0 hits taken, empty (and the boss dead)
 * once hits_required is reached. */
static void draw_boss_bar(SdlRendererCtx *ctx, const GameState *gs) {
    const Boss *b = &gs->boss;
    if (!b->alive) return;

    float bar_w = 130.0f * gs->scale;
    float bar_h = 16.0f * gs->scale;
    float outline_t = 2.0f * gs->scale;
    float margin = 12.0f * gs->scale;

    float bar_x = ((float)gs->screen_w - bar_w) / 2.0f;
    float y = margin;

    gp_fill_rect(ctx->renderer, bar_x, y, bar_w, bar_h, kDim);

    float inner_x = bar_x + outline_t;
    float inner_y = y + outline_t;
    float inner_w = bar_w - outline_t * 2.0f;
    float inner_h = bar_h - outline_t * 2.0f;
    gp_fill_rect(ctx->renderer, inner_x, inner_y, inner_w, inner_h, kBackground);

    float life = 0.0f;
    if (b->hits_required > 0) {
        life = 1.0f - (float)b->hits_taken / (float)b->hits_required;
    }
    if (life < 0.0f) life = 0.0f;
    if (life > 1.0f) life = 1.0f;
    float fill_w = inner_w * life;
    if (fill_w > 0.0f) {
        gp_fill_rect(ctx->renderer, inner_x, inner_y, fill_w, inner_h, kRed);
    }

    char pct_buf[8];
    snprintf(pct_buf, sizeof(pct_buf), "%d%%", (int)(life * 100.0f + 0.5f));
    float pct_size = (bar_h * 0.5f) / 7.0f;
    float pct_w = pf_text_width(pct_buf, pct_size);
    float pct_h = 7.0f * pct_size;
    pf_draw_text_plain(ctx->renderer, bar_x + (bar_w - pct_w) / 2.0f, y + (bar_h - pct_h) / 2.0f, pct_size, kDim,
                        pct_buf);

    const char *label = "BOSS";
    float label_size = 1.3f * gs->scale; /* half the old 2.6f label size */
    float label_w = pf_text_width(label, label_size);
    float label_gap = 4.0f * gs->scale;
    pf_draw_text(ctx->renderer, bar_x + (bar_w - label_w) / 2.0f, y + bar_h + label_gap, label_size, kRed, label);
}

static const char *kShootModeNames[SHOOT_MODE_COUNT] = {
    "NORMAL", "RAPID", "POWER", "DOUBLE", "SIDE", "OMNI", "WANDER", "FORMATION",
    "SHARDS", "OMNI", "SPIRAL", "TWIN", "ORB", "ROCKETS", "ALTERNATE", "MIRROR",
};

/* Bottom-left indicator (the one HUD corner draw_life_bar/draw_boss_bar/the
 * score don't already use): one numbered box per slot in the current ship's
 * own moveset (see ship_shoot_mode_slot_count/ship_shoot_mode_for_slot in
 * usecases/ship.h - B-20 draws 5, C-24 only 3), with the mode's name above.
 * Three states per box, same language as the life bar's "red means can't
 * act right now": green for whichever mode is currently selected (every
 * ship, every mode); red while locked out on cooldown - B-20's mode 2
 * (rapid_cooldown_timer) or Shine's mode 2 (shine_omni_cooldown_timer,
 * which - unlike B-20's - can never also be the selected/green slot at the
 * same time, since Shine's mode 2 is never a persistent shoot_mode value
 * at all, see SHOOT_MODE_SHINE_OMNI's own doc comment); yellow for every
 * other unselected, available mode. */
static void draw_shoot_mode_indicator(SdlRendererCtx *ctx, const GameState *gs) {
    const Player *p = &gs->player;
    int slot_count = ship_shoot_mode_slot_count(gs->selected_ship);

    float margin = 12.0f * gs->scale;
    float box = 18.0f * gs->scale;
    float gap = 4.0f * gs->scale;
    float outline_t = 2.0f * gs->scale;

    float x0 = margin;
    float y0 = (float)gs->screen_h - margin - box;

    for (int i = 0; i < slot_count; i++) {
        float x = x0 + (float)i * (box + gap);
        ShootMode slot_mode = ship_shoot_mode_for_slot(gs->selected_ship, i);
        bool active = (slot_mode == p->shoot_mode);
        /* SHOOT_MODE_TWINS_MIRROR reads as permanently on cooldown (red)
         * the instant one twin dies - there's nothing left to mirror, and
         * update_shoot_mode_switch locks the mode out for the rest of the
         * run to match (see kill_twin's own doc comment). */
        bool cooling_down = (slot_mode == SHOOT_MODE_RAPID && p->rapid_cooldown_timer > 0.0f) ||
                            (slot_mode == SHOOT_MODE_SHINE_OMNI && p->shine_omni_cooldown_timer > 0.0f) ||
                            (slot_mode == SHOOT_MODE_CRUZADER_ORB && p->cruzader_orb_cooldown_timer > 0.0f) ||
                            (slot_mode == SHOOT_MODE_TWINS_MIRROR &&
                             !(p->twins_right_alive && p->twins_left_alive));

        Color outline = active ? kGreen : (cooling_down ? kRed : kYellow);
        gp_fill_rect(ctx->renderer, x, y0, box, box, outline);

        float inner_x = x + outline_t;
        float inner_y = y0 + outline_t;
        float inner_w = box - outline_t * 2.0f;
        float inner_h = box - outline_t * 2.0f;
        gp_fill_rect(ctx->renderer, inner_x, inner_y, inner_w, inner_h, kBackground);

        char buf[2] = {(char)('1' + i), '\0'};
        float text_size = (box * 0.5f) / 7.0f;
        float text_w = pf_text_width(buf, text_size);
        float text_h = 7.0f * text_size;
        pf_draw_text(ctx->renderer, x + (box - text_w) / 2.0f, y0 + (box - text_h) / 2.0f,
                     text_size, active ? kWhite : kDim, buf);
    }

    const char *label = kShootModeNames[p->shoot_mode];
    /* Always describes the currently selected mode, which - per the loop
     * above - is always green: mode 2 can never be both selected and
     * cooling down at once. */
    Color label_color = kGreen;
    float label_size = 1.3f * gs->scale;
    float label_h = 7.0f * label_size;
    pf_draw_text(ctx->renderer, x0, y0 - label_h - gap, label_size, label_color, label);

    /* The Mothership only: her cap on concurrent escorts (see
     * MOTHERSHIP_MAX_CHILDREN) is otherwise invisible - "why isn't holding
     * fire doing anything" - so surface it right above the mode label. */
    if (gs->selected_ship == SHIP_MOTHERSHIP) {
        int alive_children = 0;
        for (int i = 0; i < MOTHERSHIP_MAX_CHILDREN; i++) {
            if (gs->children[i].alive) alive_children++;
        }
        char escort_buf[24];
        snprintf(escort_buf, sizeof(escort_buf), "ESCORTS: %d/%d", alive_children, MOTHERSHIP_MAX_CHILDREN);
        float escort_size = 1.3f * gs->scale;
        float escort_h = 7.0f * escort_size;
        pf_draw_text(ctx->renderer, x0, y0 - label_h - gap - escort_h - gap, escort_size,
                     alive_children >= MOTHERSHIP_MAX_CHILDREN ? kRed : kDim, escort_buf);
    }
}

static void draw_hud(SdlRendererCtx *ctx, const GameState *gs) {
    if (gs->selected_ship == SHIP_TWINS) {
        draw_twins_life_bars(ctx, gs);
    } else {
        draw_life_bar(ctx, gs);
    }
    draw_boss_bar(ctx, gs);
    draw_shoot_mode_indicator(ctx, gs);

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
        float hue = fmodf(gs->time_elapsed * SUPER_BEAM_COLOR_CYCLE_SPEED, 360.0f);
        if (hue < 0.0f) hue += 360.0f;
        Color c = color_from_hsv(hue, 0.85f, 1.0f);
        float score_line_height = 7.0f * size; /* the pixel font is 7 dots tall */
        pf_draw_text(ctx->renderer, (float)gs->screen_w - beam_w - margin,
                     margin + score_line_height + margin * 0.4f, beam_size, c, beam_buf);
    }

}

static void draw_centered(SdlRendererCtx *ctx, const GameState *gs, const char *text, float y, float size, Color c) {
    float w = pf_text_width(text, size);
    pf_draw_text(ctx->renderer, ((float)gs->screen_w - w) / 2.0f, y, size, c, text);
}

/* One of the 4 decorative menu planets (adapters/menu_planet_sprites) -
 * same textured-blit technique as draw_menu_ship: dst_w is the same
 * "overall visual footprint" each planet used to occupy back when it was
 * drawn procedurally (for the ringed planet, that includes the ring's own
 * reach, since it's baked into this sprite too), with dst_h derived from
 * the source image's own aspect ratio so nothing stretches. */
static void draw_menu_planet(SdlRendererCtx *ctx, MenuPlanetId id, float x, float y, float dst_w) {
    const MenuPlanetSprite *sprite = &kMenuPlanetSprites[id];
    float dst_h = dst_w * (float)sprite->grid_h / (float)sprite->grid_w;

    SDL_FRect dst = {x - dst_w / 2.0f, y - dst_h / 2.0f, dst_w, dst_h};
    SDL_RenderCopyF(ctx->renderer, ctx->planet_textures[id], NULL, &dst);
}

/* A 4-point twinkle: two crossed diamonds. */
static void draw_sparkle(SdlRendererCtx *ctx, float x, float y, float size, Color c) {
    float spread = size * 0.3f;
    gp_fill_triangle(ctx->renderer, x, y - size, x - spread, y, x + spread, y, c);
    gp_fill_triangle(ctx->renderer, x, y + size, x - spread, y, x + spread, y, c);
    gp_fill_triangle(ctx->renderer, x - size, y, x, y - spread, x, y + spread, c);
    gp_fill_triangle(ctx->renderer, x + size, y, x, y - spread, x, y + spread, c);
}

/* The decorative hero ship (adapters/menu_ship_sprite), anchored to the
 * bottom-right corner - a single large textured blit, the same technique
 * draw_sprite uses for enemies/the boss, just from its own dedicated
 * texture instead of a shared per-kind array since there's only one. */
static void draw_menu_ship(SdlRendererCtx *ctx, const GameState *gs) {
    float w = (float)gs->screen_w, h = (float)gs->screen_h, s = gs->scale;
    float margin = 14.0f * s;

    float dst_w = w * 0.46f;
    float dst_h = dst_w * (float)MENU_SHIP_SPRITE_H / (float)MENU_SHIP_SPRITE_W;

    SDL_FRect dst = {w - margin - dst_w, h - margin - dst_h, dst_w, dst_h};
    SDL_RenderCopyF(ctx->renderer, ctx->menu_ship_texture, NULL, &dst);
}

static void draw_menu_decorations(SdlRendererCtx *ctx, const GameState *gs) {
    float w = (float)gs->screen_w, h = (float)gs->screen_h, s = gs->scale;

    /* Two big "hero" planets anchored just past opposite corners so most
     * of their now much larger bulk bleeds off-canvas - a bigger planet
     * reads as closer/grander rather than just cluttering the frame if it
     * shares the frame's edge instead of floating free inside it. Two
     * smaller ones stay fully on-screen for scale contrast (depth cue).
     * Unlike the ship, these sprites' native resolution is modest (they're
     * cropped straight from reference photos, not authored at a huge
     * size), so display width is capped well under their old vector-drawn
     * footprint - past that point real screens just magnify the same
     * source pixels into visible blocks instead of showing more detail. */
    draw_menu_planet(ctx, MENU_PLANET_OCEAN, w * 0.06f, h * 0.02f, 140.0f * s);
    draw_menu_planet(ctx, MENU_PLANET_RINGED, w * 0.08f, h * 0.90f, 310.0f * s);
    draw_menu_planet(ctx, MENU_PLANET_ROCKY, w * 0.86f, h * 0.10f, 68.0f * s);
    draw_menu_planet(ctx, MENU_PLANET_GALAXY, w * 0.85f, h * 0.66f, 48.0f * s);

    static const Color kSparkleGold = {255, 210, 120, 255};
    static const Color kSparklePink = {255, 150, 220, 255};
    draw_sparkle(ctx, w * 0.20f, h * 0.20f, 6.0f * s, kWhite);
    draw_sparkle(ctx, w * 0.78f, h * 0.14f, 5.0f * s, kSparkleGold);
    draw_sparkle(ctx, w * 0.85f, h * 0.55f, 7.0f * s, kSparklePink);
    draw_sparkle(ctx, w * 0.15f, h * 0.62f, 5.0f * s, kSparkleGold);
    draw_sparkle(ctx, w * 0.55f, h * 0.90f, 6.0f * s, kWhite);
    draw_sparkle(ctx, w * 0.93f, h * 0.85f, 5.0f * s, kSparklePink);

    draw_menu_ship(ctx, gs);
}

static void draw_menu_screen(SdlRendererCtx *ctx, const GameState *gs) {
    draw_menu_decorations(ctx, gs);

    float title_size = 9.0f * gs->scale;
    float title_w1 = pf_text_width("GALAXY", title_size);
    float title_w2 = pf_text_width("INVADERS", title_size);
    float y1 = (float)gs->screen_h * 0.12f;
    float y2 = y1 + 78.0f * gs->scale;

    static const Color kMagentaGlow = {255, 40, 190, 255};
    static const Color kMagentaEdge = {255, 140, 235, 255};
    static const Color kMagentaFill = {110, 15, 85, 255};
    static const Color kMagentaShadow = {70, 8, 55, 255};
    static const Color kGreenGlow = {60, 255, 90, 255};
    static const Color kGreenEdge = {180, 255, 190, 255};
    static const Color kGreenFill = {15, 95, 35, 255};
    static const Color kGreenShadow = {8, 55, 18, 255};

    pf_draw_text_neon(ctx->renderer, ((float)gs->screen_w - title_w1) / 2.0f, y1, title_size,
                       kMagentaGlow, kMagentaEdge, kMagentaFill, kMagentaShadow, "GALAXY");
    pf_draw_text_neon(ctx->renderer, ((float)gs->screen_w - title_w2) / 2.0f, y2, title_size,
                       kGreenGlow, kGreenEdge, kGreenFill, kGreenShadow, "INVADERS");

    bool blink_on = fmodf(gs->menu_blink_timer, 1.0f) < 0.5f;
    if (blink_on) {
        draw_centered(ctx, gs, "START GAME", (float)gs->screen_h * 0.6f, 4.0f * gs->scale, kYellow);
    }

    /* Unlike the title, this line has no outline of its own to stay
     * legible over whatever decoration (a planet, a bright sparkle) ends
     * up behind it, so give it a translucent backing bar instead of
     * fighting to keep every decoration's exact position clear of it. */
    const char *instructions = "ARROWS-WASD MOVE  SPACE FIRE  ESC PAUSE";
    float instr_size = 1.6f * gs->scale;
    float instr_y = (float)gs->screen_h * 0.85f;
    float instr_w = pf_text_width(instructions, instr_size);
    float instr_h = 7.0f * instr_size;
    float pad_x = 8.0f * gs->scale, pad_y = 4.0f * gs->scale;
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_rect(ctx->renderer, ((float)gs->screen_w - instr_w) / 2.0f - pad_x, instr_y - pad_y,
                 instr_w + pad_x * 2.0f, instr_h + pad_y * 2.0f, (Color){0, 0, 0, 130});
    draw_centered(ctx, gs, instructions, instr_y, instr_size, kDim);
}

static const char *const kDifficultyLabels[DIFFICULTY_COUNT] = {
    "BABY", "EASY", "NORMAL", "HARD", "INSANE",
};

/* Reached right after confirming START GAME on the main menu (see
 * update_difficulty_select in usecases/game_logic.c) - same decorative
 * backdrop as draw_menu_screen (this is still part of the same "getting
 * into a game" flow, not gameplay), dimmed with a translucent overlay the
 * same way draw_pause_overlay dims the game field, so 5 stacked plain-text
 * options stay legible over the bright planets/sparkles/ship behind them. */
static void draw_difficulty_select_screen(SdlRendererCtx *ctx, const GameState *gs) {
    draw_menu_decorations(ctx, gs);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_rect(ctx->renderer, 0, 0, (float)gs->screen_w, (float)gs->screen_h, (Color){5, 5, 15, 150});

    draw_centered(ctx, gs, "SELECT DIFFICULTY", (float)gs->screen_h * 0.24f, 4.0f * gs->scale, kWhite);

    float first_y = (float)gs->screen_h * 0.40f;
    float step_y = 42.0f * gs->scale;
    for (int i = 0; i < DIFFICULTY_COUNT; i++) {
        Color c = ((int)gs->selected_difficulty == i) ? kYellow : kDim;
        draw_centered(ctx, gs, kDifficultyLabels[i], first_y + step_y * (float)i, 3.5f * gs->scale, c);
    }

    const char *instructions = "ARROWS CHOOSE  ENTER/SPACE NEXT  ESC BACK";
    float instr_size = 1.6f * gs->scale;
    draw_centered(ctx, gs, instructions, (float)gs->screen_h * 0.88f, instr_size, kDim);
}

/* Left-aligned counterpart to draw_centered, for the ship-select screen's
 * right-hand panel (name, attribute labels, description) where every line
 * shares a left edge instead of being individually centered. */
static void draw_left(SdlRendererCtx *ctx, float x, float y, const char *text, float size, Color c) {
    pf_draw_text(ctx->renderer, x, y, size, c, text);
}

/* Greedily wraps `text` (plain spaces between words) into lines no wider
 * than max_w at the given font size, writing up to max_lines results into
 * `out` (each up to out_line_cap - 1 chars) and returning how many lines it
 * used. Used only for each ship's description on the ship-select screen -
 * pf_draw_text itself has no wrapping of its own, it just draws whatever
 * single line it's given. */
static int wrap_text_lines(const char *text, float size, float max_w, char out[][96], int max_lines,
                            int out_line_cap) {
    char buf[512];
    size_t len = strlen(text);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, text, len);
    buf[len] = '\0';

    int line_count = 0;
    char current[128] = "";
    char *save = NULL;
    for (char *word = strtok_r(buf, " ", &save); word; word = strtok_r(NULL, " ", &save)) {
        char trial[128];
        if (current[0]) snprintf(trial, sizeof(trial), "%s %s", current, word);
        else snprintf(trial, sizeof(trial), "%s", word);

        if (current[0] && pf_text_width(trial, size) > max_w) {
            if (line_count >= max_lines) break;
            snprintf(out[line_count++], (size_t)out_line_cap, "%s", current);
            snprintf(current, sizeof(current), "%s", word);
        } else {
            snprintf(current, sizeof(current), "%s", trial);
        }
    }
    if (current[0] && line_count < max_lines) snprintf(out[line_count++], (size_t)out_line_cap, "%s", current);
    return line_count;
}

static const char *const kShipNames[SHIP_COUNT] = {"B-20", "C-24", "THE MOTHERSHIP", "SHINE", "CRUZADER",
                                                     "THE TWINS"};

/* Ad copy for the ship-select screen's description panel - written from
 * the same capsule descriptions each ship was specced with ("versatile
 * and fast... built for the most skilled pilots" / "resilient and strong,
 * piloted only by the bravest" / "the matriarch who fights against evil,
 * dispatching brave warriors to fight by her side" / "leverages the cosmic
 * powers of the universe to materialize crystals for offense and defense" /
 * "strength, honor and virtue... fights for true justice, and must have no
 * pity for evil"), expanded to fill the panel. All caps: the pixel font
 * (adapters/pixel_font) only has uppercase glyphs, same convention every
 * other in-game string here already follows. */
static const char *const kShipDescriptions[SHIP_COUNT] = {
    "A VERSATILE, FAST SPACESHIP BUILT FOR THE MOST SKILLED PILOTS. "
    "QUICK ON THE STICK AND SHARP IN A DOGFIGHT, THE B-20 REWARDS "
    "PRECISION AND REFLEX OVER BRUTE FORCE.",
    "RESILIENT AND STRONG, PILOTED ONLY BY THE BRAVEST. THE C-24 "
    "TRADES RAW SPEED FOR HEAVY PLATING THAT SHRUGS OFF PUNISHMENT, "
    "LETTING ITS PILOT STAND AND FIGHT WHEN OTHERS WOULD FLEE.",
    "THE MATRIARCH WHO FIGHTS AGAINST EVIL, AND THE STRONGEST "
    "SPACESHIP IN THE FLEET. SHE DOES NOT FIGHT ALONE - SHE DISPATCHES "
    "BRAVE, COURAGEOUS WARRIORS TO FIGHT BY HER SIDE.",
    "SHINE LEVERAGES THE COSMIC POWERS OF THE UNIVERSE TO MATERIALIZE "
    "POWERFUL CRYSTALS, USED FOR BOTH OFFENSE AND DEFENSE AGAINST THE "
    "MOST NEFARIOUS ELEMENTS OF THE GALAXY.",
    "STRENGTH, HONOR AND VIRTUE ARE THE THREE QUALIFYING CRITERIA FOR "
    "PILOTING THE CRUZADER. HE FIGHTS FOR TRUE JUSTICE, AND HAS NO PITY "
    "FOR EVIL.",
    "INSEPARABLE BROTHERS IN ARMS, THE TWINS ARE TRUE SPACE GLADIATORS. "
    "THEIR FIGHT NEVER STOPS, AS THE EVIL NEVER SLEEPS.",
};

static const char *const kShipAttackAttributeLabels[3] = {"SPEED", "STRENGTH", "ATTACK"};

#define SHIP_SELECT_GRID_SLOTS (SHIP_SELECT_GRID_COLS * SHIP_SELECT_GRID_ROWS)

/* A 0-10 rating drawn as 10 small blocks, filled left to right - shared by
 * every attribute row on the ship-select screen's right-hand panel. */
static void draw_rating_bar(SdlRendererCtx *ctx, float x, float y, float w, float h, int rating, Color fill_c) {
    static const int kSegments = 10;
    float seg_gap = w * 0.015f;
    float seg_w = (w - seg_gap * (float)(kSegments - 1)) / (float)kSegments;
    for (int i = 0; i < kSegments; i++) {
        Color c = (i < rating) ? fill_c : kDim;
        gp_fill_rect(ctx->renderer, x + (float)i * (seg_w + seg_gap), y, seg_w, h, c);
    }
}

/* The dark-grey "not yet implemented" placeholder shown in every ship-select
 * grid slot past SHIP_COUNT - three layered, progressively larger and more
 * transparent squares faking a soft blur, the same "enlarged copies at
 * decreasing alpha" trick pf_draw_text_neon already uses for its glow
 * (adapters/pixel_font.c), rather than an actual box-blur render pass. */
static void draw_locked_ship_slot(SdlRendererCtx *ctx, float cx, float cy, float size) {
    static const float kBlurScales[] = {1.3f, 1.12f, 1.0f};
    static const unsigned char kBlurAlphas[] = {35, 70, 120};
    for (int layer = 0; layer < 3; layer++) {
        float s = size * kBlurScales[layer];
        Color c = {60, 60, 68, kBlurAlphas[layer]};
        gp_fill_rect(ctx->renderer, cx - s / 2.0f, cy - s / 2.0f, s, s, c);
    }
}

/* Reached right after confirming a difficulty (see update_ship_select in
 * usecases/game_logic.c) - same decorative, dimmed backdrop as
 * draw_difficulty_select_screen. The screen splits in half: a 4x4 grid of
 * ship slots on the left (only SHIP_COUNT of the 16 are unlocked - B-20 and
 * C-24, top-left in reading order - every other slot is an inert locked
 * placeholder, see draw_locked_ship_slot), and the hovered ship's
 * Speed/Strength/Attack ratings plus its description on the right - both
 * driven by gs->selected_ship, which doubles as the grid cursor exactly the
 * way gs->selected_difficulty drives the difficulty list. */
static void draw_ship_select_screen(SdlRendererCtx *ctx, const GameState *gs) {
    draw_menu_decorations(ctx, gs);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    gp_fill_rect(ctx->renderer, 0, 0, (float)gs->screen_w, (float)gs->screen_h, (Color){5, 5, 15, 150});

    draw_centered(ctx, gs, "SELECT YOUR SHIP", (float)gs->screen_h * 0.08f, 3.6f * gs->scale, kWhite);

    float margin = 20.0f * gs->scale;
    float half_x = (float)gs->screen_w / 2.0f;

    /* --- Left half: the 4x4 grid --- */
    float grid_x0 = margin, grid_x1 = half_x - margin * 0.5f;
    float grid_y0 = (float)gs->screen_h * 0.16f, grid_y1 = (float)gs->screen_h * 0.84f;
    float cell_gap = 10.0f * gs->scale;
    float avail_w = grid_x1 - grid_x0, avail_h = grid_y1 - grid_y0;
    float cell_w = (avail_w - cell_gap * (float)(SHIP_SELECT_GRID_COLS - 1)) / (float)SHIP_SELECT_GRID_COLS;
    float cell_h = (avail_h - cell_gap * (float)(SHIP_SELECT_GRID_ROWS - 1)) / (float)SHIP_SELECT_GRID_ROWS;
    float cell_size = fminf(cell_w, cell_h);
    float grid_w = cell_size * (float)SHIP_SELECT_GRID_COLS + cell_gap * (float)(SHIP_SELECT_GRID_COLS - 1);
    float grid_h = cell_size * (float)SHIP_SELECT_GRID_ROWS + cell_gap * (float)(SHIP_SELECT_GRID_ROWS - 1);
    float grid_ox = grid_x0 + (avail_w - grid_w) / 2.0f;
    float grid_oy = grid_y0 + (avail_h - grid_h) / 2.0f;

    static const Color kPanelBg = {20, 20, 45, 255};
    float border_t = 3.0f * gs->scale;

    for (int idx = 0; idx < SHIP_SELECT_GRID_SLOTS; idx++) {
        int row = idx / SHIP_SELECT_GRID_COLS, col = idx % SHIP_SELECT_GRID_COLS;
        float cx0 = grid_ox + (float)col * (cell_size + cell_gap);
        float cy0 = grid_oy + (float)row * (cell_size + cell_gap);
        float ccx = cx0 + cell_size / 2.0f, ccy = cy0 + cell_size / 2.0f;

        if (idx >= SHIP_COUNT) {
            draw_locked_ship_slot(ctx, ccx, ccy, cell_size * 0.85f);
            continue;
        }

        bool hovered = (int)gs->selected_ship == idx;
        Color border_c = hovered ? kYellow : kDim;
        gp_fill_rect(ctx->renderer, cx0, cy0, cell_size, cell_size, border_c);
        gp_fill_rect(ctx->renderer, cx0 + border_t, cy0 + border_t, cell_size - border_t * 2.0f,
                     cell_size - border_t * 2.0f, kPanelBg);

        draw_ship_sprite(ctx, &kShipSprites[idx], ccx, ccy, cell_size * 0.72f, cell_size * 0.72f, NULL);
    }

    /* --- Right half: attributes + description for the hovered ship --- */
    float right_x0 = half_x + margin * 0.5f;
    float right_w = (float)gs->screen_w - margin - right_x0;

    Ship ship = gs->selected_ship;
    float name_y = (float)gs->screen_h * 0.16f;
    /* Shrunk to fit right_w when a name is too long to fit at the intended
     * size (as-is for B-20/C-24's short 4-char codes, but "THE MOTHERSHIP"
     * needs it) - kept on one line rather than wrapped, since this header
     * reads as a single title everywhere else. */
    float name_size = 4.0f * gs->scale;
    float name_w = pf_text_width(kShipNames[ship], name_size);
    if (name_w > right_w) name_size *= right_w / name_w;
    draw_left(ctx, right_x0, name_y, kShipNames[ship], name_size, kYellow);

    float attr_y = name_y + 56.0f * gs->scale;
    float attr_step = 44.0f * gs->scale;
    float label_size = 2.0f * gs->scale;
    float bar_h = 10.0f * gs->scale;
    int ratings[3] = {ship_speed_rating(ship), ship_strength_rating(ship), ship_attack_rating(ship)};
    for (int i = 0; i < 3; i++) {
        float y = attr_y + attr_step * (float)i;
        draw_left(ctx, right_x0, y, kShipAttackAttributeLabels[i], label_size, kWhite);
        draw_rating_bar(ctx, right_x0, y + 7.0f * label_size + 4.0f * gs->scale, right_w, bar_h, ratings[i],
                         kYellow);
    }

    float desc_y = attr_y + attr_step * 3.0f + 20.0f * gs->scale;
    float desc_size = 1.7f * gs->scale;
    float desc_line_h = 9.0f * desc_size;
    char lines[12][96];
    int line_count = wrap_text_lines(kShipDescriptions[ship], desc_size, right_w, lines, 12, 96);
    for (int i = 0; i < line_count; i++) {
        pf_draw_text_strong_shadow(ctx->renderer, right_x0, desc_y + desc_line_h * (float)i, desc_size, kDim,
                                    lines[i]);
    }

    const char *instructions = "ARROWS CHOOSE  ENTER/SPACE CONFIRM  ESC BACK";
    float instr_size = 1.6f * gs->scale;
    draw_centered(ctx, gs, instructions, (float)gs->screen_h * 0.92f, instr_size, kDim);
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

    draw_background_smoke(ctx, gs);
    draw_stars(ctx, gs);

    switch (gs->state) {
        case STATE_MENU:
            draw_menu_screen(ctx, gs);
            break;
        case STATE_DIFFICULTY_SELECT:
            draw_difficulty_select_screen(ctx, gs);
            break;
        case STATE_SHIP_SELECT:
            draw_ship_select_screen(ctx, gs);
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
    for (int i = 0; i < ENEMY_KIND_COUNT; i++) {
        if (ctx->enemy_textures[i]) SDL_DestroyTexture(ctx->enemy_textures[i]);
        if (ctx->boss_textures[i]) SDL_DestroyTexture(ctx->boss_textures[i]);
    }
    if (ctx->menu_ship_texture) SDL_DestroyTexture(ctx->menu_ship_texture);
    for (int i = 0; i < MENU_PLANET_COUNT; i++) {
        if (ctx->planet_textures[i]) SDL_DestroyTexture(ctx->planet_textures[i]);
    }
    if (ctx->renderer) SDL_DestroyRenderer(ctx->renderer);
    if (ctx->window) SDL_DestroyWindow(ctx->window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(ctx);
}

/* Uploads one EnemySpriteSheet array (adapters/enemy_sprites) to a GPU
 * texture per kind, once, at startup - see draw_sprite for why enemies
 * and the boss are blitted as whole textures instead of one gp_fill_rect
 * per pixel like the single player ship. */
static bool sdl_load_sprite_textures(SDL_Renderer *renderer, const EnemySpriteSheet *sheets,
                                      SDL_Texture **out_textures) {
    for (int i = 0; i < ENEMY_KIND_COUNT; i++) {
        const EnemySpriteSheet *sheet = &sheets[i];
        if (!sheet->pixels) continue; /* not every kind has boss-scale art (see kBossSprites) */
        /* RGBA8888, not the RGBA32 alias: RGBA8888 is a fixed bit layout
         * (R in the most significant byte of the 32-bit value, A in the
         * least), which is exactly how enemy_sprites.c packs each pixel
         * ((r<<24)|(g<<16)|(b<<8)|a). RGBA32 instead means "R is byte 0 in
         * memory," which resolves to a *different* bit layout on a
         * little-endian machine and silently scrambles every channel. */
        SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                              SDL_TEXTUREACCESS_STATIC, sheet->grid_w, sheet->grid_h);
        if (!tex) {
            fprintf(stderr, "SDL_CreateTexture failed for sprite kind %d: %s\n", i, SDL_GetError());
            return false;
        }
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(tex, NULL, sheet->pixels, sheet->grid_w * (int)sizeof(uint32_t));
        out_textures[i] = tex;
    }
    return true;
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

    if (!sdl_load_sprite_textures(ctx->renderer, kEnemySprites, ctx->enemy_textures) ||
        !sdl_load_sprite_textures(ctx->renderer, kBossSprites, ctx->boss_textures)) {
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return NULL;
    }

    ctx->menu_ship_texture = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC,
                                                MENU_SHIP_SPRITE_W, MENU_SHIP_SPRITE_H);
    if (!ctx->menu_ship_texture) {
        fprintf(stderr, "SDL_CreateTexture failed for menu ship: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return NULL;
    }
    SDL_SetTextureBlendMode(ctx->menu_ship_texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(ctx->menu_ship_texture, NULL, kMenuShipSpritePixels, MENU_SHIP_SPRITE_W * (int)sizeof(uint32_t));

    for (int i = 0; i < MENU_PLANET_COUNT; i++) {
        const MenuPlanetSprite *sprite = &kMenuPlanetSprites[i];
        SDL_Texture *tex = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC,
                                              sprite->grid_w, sprite->grid_h);
        if (!tex) {
            fprintf(stderr, "SDL_CreateTexture failed for menu planet %d: %s\n", i, SDL_GetError());
            SDL_DestroyRenderer(ctx->renderer);
            SDL_DestroyWindow(ctx->window);
            free(ctx);
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return NULL;
        }
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(tex, NULL, sprite->pixels, sprite->grid_w * (int)sizeof(uint32_t));
        ctx->planet_textures[i] = tex;
    }

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
