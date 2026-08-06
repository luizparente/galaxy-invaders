#ifndef GALAXY_INVADERS_PORTS_INPUT_PORT_H
#define GALAXY_INVADERS_PORTS_INPUT_PORT_H

#include <stdbool.h>

/* Semantic input the use-case layer understands. Adapters translate
 * whatever raw input API they use (SDL scancodes, events, ...) into this
 * shape so game rules never mention a key code. The *_pressed fields are
 * edge-triggered (true only on the frame the action was initiated); the
 * move_* fields are level-triggered (true for as long as held). */
typedef struct InputCommand {
    bool move_left;
    bool move_right;
    bool move_up;
    bool move_down;
    bool fire_held;

    bool confirm_pressed;
    bool back_pressed;
    bool nav_up_pressed;
    bool nav_down_pressed;
    bool god_mode_toggle_pressed;

    /* Number keys 1-5 select a shooting mode (ShootMode in domain/types.h)
     * directly, edge-triggered like every other *_pressed field. */
    bool shoot_mode_1_pressed;
    bool shoot_mode_2_pressed;
    bool shoot_mode_3_pressed;
    bool shoot_mode_4_pressed;
    bool shoot_mode_5_pressed;

    bool quit_requested;
} InputCommand;

typedef struct InputPort {
    void *self;
    void (*poll)(void *self, InputCommand *out);
    void (*destroy)(void *self);
} InputPort;

#endif
