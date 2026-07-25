#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>

#include "app/app.h"

int main(void) {
    srand((unsigned int)time(NULL));

    if (SDL_Init(0) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    App *app = app_create();
    if (!app) {
        SDL_Quit();
        return EXIT_FAILURE;
    }

    app_run(app);
    app_destroy(app);
    SDL_Quit();
    return EXIT_SUCCESS;
}
