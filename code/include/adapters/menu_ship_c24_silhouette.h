#ifndef GALAXY_INVADERS_ADAPTERS_MENU_SHIP_C24_SILHOUETTE_H
#define GALAXY_INVADERS_ADAPTERS_MENU_SHIP_C24_SILHOUETTE_H

#include <stdint.h>

/* Solid black silhouette for the decorative C-24 hero ship shown
 * on the main menu (see draw_menu_ship_with_silhouette in
 * adapters/sdl_renderer.c) - a fully filled silhouette of the ship's own
 * outer contour, not just wherever the ship's own art happens to be
 * opaque: the raw alpha mask is morphologically closed (bridges the thin
 * gaps the pixel art's own black outline strokes punch between adjacent
 * parts - wingtips, gun barrels, flame - during background removal, since
 * those strokes are themselves near-black and would otherwise get flood-
 * filled away as background too), then any remaining fully-enclosed holes
 * are filled, then dilated outward a bit further so the silhouette extends
 * past the ship's own edge as a discrete outline/halo. Same W/H as the
 * ship's own full-color texture, so it can be drawn at the exact same
 * destination rect with zero extra scaling math. Row-major, top-left
 * origin. Each entry is a packed 0x000000AA color (RGB always 0); a fully
 * zero entry (alpha 0) is outside the silhouette and should not be drawn. */

#define MENU_SHIP_C24_SILHOUETTE_W 700
#define MENU_SHIP_C24_SILHOUETTE_H 584

extern const uint32_t kMenuShipC24SilhouettePixels[MENU_SHIP_C24_SILHOUETTE_W * MENU_SHIP_C24_SILHOUETTE_H];

#endif
