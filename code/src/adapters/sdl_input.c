#include <stdlib.h>
#include <SDL2/SDL.h>
#include "adapters/sdl_input.h"

typedef struct SdlInputCtx {
    bool prev_confirm;
    bool prev_back;
    bool prev_up;
    bool prev_down;
    bool prev_god_mode_combo;
    bool prev_shoot_mode_1;
    bool prev_shoot_mode_2;
    bool prev_shoot_mode_3;
    bool prev_shoot_mode_4;
    bool prev_shoot_mode_5;
} SdlInputCtx;

static void sdl_input_poll(void *self, InputCommand *out) {
    SdlInputCtx *ctx = self;

    bool quit = false;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) quit = true;
    }

    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    bool left = ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A];
    bool right = ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D];
    bool up = ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W];
    bool down = ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S];
    bool fire = ks[SDL_SCANCODE_SPACE];
    bool confirm = ks[SDL_SCANCODE_SPACE] || ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_KP_ENTER];
    bool back = ks[SDL_SCANCODE_ESCAPE];
    bool ctrl = ks[SDL_SCANCODE_LCTRL] || ks[SDL_SCANCODE_RCTRL];
    bool god_mode_combo = ctrl && ks[SDL_SCANCODE_G];

    bool shoot_mode_1 = ks[SDL_SCANCODE_1];
    bool shoot_mode_2 = ks[SDL_SCANCODE_2];
    bool shoot_mode_3 = ks[SDL_SCANCODE_3];
    bool shoot_mode_4 = ks[SDL_SCANCODE_4];
    bool shoot_mode_5 = ks[SDL_SCANCODE_5];

    out->move_left = left;
    out->move_right = right;
    out->move_up = up;
    out->move_down = down;
    out->fire_held = fire;

    out->confirm_pressed = confirm && !ctx->prev_confirm;
    out->back_pressed = back && !ctx->prev_back;
    out->nav_up_pressed = up && !ctx->prev_up;
    out->nav_down_pressed = down && !ctx->prev_down;
    out->god_mode_toggle_pressed = god_mode_combo && !ctx->prev_god_mode_combo;
    out->shoot_mode_1_pressed = shoot_mode_1 && !ctx->prev_shoot_mode_1;
    out->shoot_mode_2_pressed = shoot_mode_2 && !ctx->prev_shoot_mode_2;
    out->shoot_mode_3_pressed = shoot_mode_3 && !ctx->prev_shoot_mode_3;
    out->shoot_mode_4_pressed = shoot_mode_4 && !ctx->prev_shoot_mode_4;
    out->shoot_mode_5_pressed = shoot_mode_5 && !ctx->prev_shoot_mode_5;
    out->quit_requested = quit;

    ctx->prev_confirm = confirm;
    ctx->prev_back = back;
    ctx->prev_up = up;
    ctx->prev_down = down;
    ctx->prev_god_mode_combo = god_mode_combo;
    ctx->prev_shoot_mode_1 = shoot_mode_1;
    ctx->prev_shoot_mode_2 = shoot_mode_2;
    ctx->prev_shoot_mode_3 = shoot_mode_3;
    ctx->prev_shoot_mode_4 = shoot_mode_4;
    ctx->prev_shoot_mode_5 = shoot_mode_5;
}

static void sdl_input_destroy(void *self) {
    SdlInputCtx *ctx = self;
    free(ctx);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

InputPort *sdl_input_create(void) {
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        return NULL;
    }

    SdlInputCtx *ctx = calloc(1, sizeof(SdlInputCtx));
    if (!ctx) {
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
        return NULL;
    }

    InputPort *port = calloc(1, sizeof(InputPort));
    if (!port) {
        free(ctx);
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
        return NULL;
    }
    port->self = ctx;
    port->poll = sdl_input_poll;
    port->destroy = sdl_input_destroy;
    return port;
}
