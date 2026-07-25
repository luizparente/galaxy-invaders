#ifndef GALAXY_INVADERS_APP_APP_H
#define GALAXY_INVADERS_APP_APP_H

/* The composition root: the one place that knows both the abstract Ports
 * the use-case layer programs against AND the concrete SDL adapters that
 * implement them. main.c stays a two-line shell around this. */

typedef struct App App;

App *app_create(void);
void app_run(App *app);
void app_destroy(App *app);

#endif
