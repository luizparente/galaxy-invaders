#ifndef GALAXY_INVADERS_ADAPTERS_MENU_PLANET_SPRITES_H
#define GALAXY_INVADERS_ADAPTERS_MENU_PLANET_SPRITES_H

#include <stdint.h>

/* Pixel-art data for the 4 decorative planets on the main menu, reproduced
 * from reference artwork the same way as adapters/menu_ship_sprite:
 * background removed via flood fill (plus, for the rocky/mooned reference,
 * a hue-based exclusion to strip a non-black nebula haze the brightness
 * threshold alone couldn't catch), any resulting seam patched with a
 * solid black silhouette backing, then cropped to content. Native source
 * resolution - none of these needed downsampling. Row-major, top-left
 * origin. Each entry is a packed 0xRRGGBBAA color; a fully zero entry
 * (alpha 0) is background and should not be drawn. */

typedef struct MenuPlanetSprite {
    const uint32_t *pixels;
    int grid_w;
    int grid_h;
} MenuPlanetSprite;

typedef enum MenuPlanetId {
    MENU_PLANET_OCEAN,
    MENU_PLANET_RINGED,
    MENU_PLANET_ROCKY,
    MENU_PLANET_GALAXY,
    MENU_PLANET_COUNT,
} MenuPlanetId;

extern const MenuPlanetSprite kMenuPlanetSprites[MENU_PLANET_COUNT];

#endif
