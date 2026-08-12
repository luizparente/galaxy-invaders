#ifndef GALAXY_INVADERS_PORTS_AUDIO_PORT_H
#define GALAXY_INVADERS_PORTS_AUDIO_PORT_H

#include <stdbool.h>
#include "domain/events.h"

/* Output boundary for sound. difficulty01 is a normalized 0..1 knob the
 * background music generator uses to nudge its intensity as the game ramps up.
 * boss_active swaps the looping background track to the boss theme from the
 * moment the boss-warning window opens (see GameState.boss_warning) through
 * the whole time the boss is actually on screen, and back to the regular
 * soundtrack once it's gone - callers should pass gs.boss.alive ||
 * gs.boss_warning, not gs.boss.alive alone. */
typedef struct AudioPort {
    void *self;
    void (*update)(void *self, float dt, bool paused, float difficulty01, bool boss_active);
    void (*play_sfx)(void *self, SfxId sfx);
    void (*destroy)(void *self);
} AudioPort;

#endif
