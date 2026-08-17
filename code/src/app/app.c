#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "app/app.h"
#include "domain/types.h"
#include "domain/events.h"
#include "domain/constants.h"
#include "usecases/game_logic.h"
#include "usecases/difficulty.h"
#include "ports/renderer_port.h"
#include "ports/audio_port.h"
#include "ports/input_port.h"
#include "adapters/sdl_renderer.h"
#include "adapters/sdl_audio.h"
#include "adapters/sdl_input.h"

struct App {
    RendererPort *renderer;
    AudioPort *audio;
    InputPort *input;
    int screen_w;
    int screen_h;
};

App *app_create(void) {
    App *app = calloc(1, sizeof(App));
    if (!app) return NULL;

    app->renderer = sdl_renderer_create("GALAXY INVADERS", DESIGN_W, DESIGN_H, &app->screen_w, &app->screen_h);
    app->audio = sdl_audio_create();
    app->input = sdl_input_create();

    if (!app->renderer || !app->audio || !app->input) {
        fprintf(stderr, "Failed to initialize one or more subsystems.\n");
        app_destroy(app);
        return NULL;
    }
    return app;
}

static double seconds_between(struct timespec a, struct timespec b) {
    return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

void app_run(App *app) {
    GameState gs;
    game_init(&gs, app->screen_w, app->screen_h);

    EventQueue events;
    InputCommand input;

    struct timespec prev;
    clock_gettime(CLOCK_MONOTONIC, &prev);

    while (!gs.quit_requested) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double dt = seconds_between(prev, now);
        prev = now;
        if (dt > 0.05) dt = 0.05; /* clamp so a hitch (e.g. window drag) can't cause a physics blow-up */

        app->input->poll(app->input->self, &input);
        game_update(&gs, &input, (float)dt, &events);

        float difficulty01 = difficulty_normalized(gs.time_elapsed);
        /* boss.alive OR boss_warning: the boss track starts as soon as
         * the warning window opens (gs.boss_warning), not only once the
         * boss actually appears, and keeps playing uninterrupted straight
         * through boss.alive since the two flags never overlap (see
         * boss_warning's own comment in domain/types.h). */
        bool boss_track_active = gs.boss.alive || gs.boss_warning;
        app->audio->update(app->audio->self, (float)dt, gs.state == STATE_PAUSE, difficulty01, boss_track_active,
                            gs.state == STATE_GAME_OVER);
        for (int i = 0; i < events.count; i++) {
            if (events.items[i].type == EVENT_PLAY_SFX) {
                app->audio->play_sfx(app->audio->self, events.items[i].sfx);
            }
        }

        app->renderer->render(app->renderer->self, &gs);
    }
}

void app_destroy(App *app) {
    if (!app) return;
    if (app->input) {
        app->input->destroy(app->input->self);
        free(app->input);
    }
    if (app->audio) {
        app->audio->destroy(app->audio->self);
        free(app->audio);
    }
    if (app->renderer) {
        app->renderer->destroy(app->renderer->self);
        free(app->renderer);
    }
    free(app);
}
