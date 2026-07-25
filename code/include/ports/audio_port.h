#ifndef GALAXY_INVADERS_PORTS_AUDIO_PORT_H
#define GALAXY_INVADERS_PORTS_AUDIO_PORT_H

#include <stdbool.h>
#include "domain/events.h"

/* Output boundary for sound. difficulty01 is a normalized 0..1 knob the
 * background music generator uses to nudge its tempo as the game ramps up. */
typedef struct AudioPort {
    void *self;
    void (*update)(void *self, float dt, bool paused, float difficulty01);
    void (*play_sfx)(void *self, SfxId sfx);
    void (*destroy)(void *self);
} AudioPort;

#endif
