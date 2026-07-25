#ifndef GALAXY_INVADERS_DOMAIN_EVENTS_H
#define GALAXY_INVADERS_DOMAIN_EVENTS_H

#include "domain/constants.h"

/* Output-boundary events raised by the use-case layer while it updates the
 * GameState. Outer layers (the app composition root) drain this queue and
 * translate each entry into a concrete side effect (e.g. playing a sound
 * through the AudioPort). This is what lets game rules stay ignorant of
 * SDL: usecases never call an adapter directly, they only record that
 * something worth reacting to happened. */

typedef enum SfxId {
    SFX_PLAYER_SHOOT,
    SFX_ENEMY_DESTROYED,
    SFX_PLAYER_DESTROYED,
    SFX_MENU_SELECT,
    SFX_ORB_CAPTURED,
    SFX_ORB_DESTROYED,
} SfxId;

typedef enum GameEventType {
    EVENT_PLAY_SFX,
} GameEventType;

typedef struct GameEvent {
    GameEventType type;
    SfxId sfx;
} GameEvent;

typedef struct EventQueue {
    GameEvent items[MAX_EVENTS];
    int count;
} EventQueue;

static inline void event_queue_clear(EventQueue *q) {
    q->count = 0;
}

static inline void event_queue_push_sfx(EventQueue *q, SfxId sfx) {
    if (q->count >= MAX_EVENTS) return;
    q->items[q->count++] = (GameEvent){.type = EVENT_PLAY_SFX, .sfx = sfx};
}

#endif
