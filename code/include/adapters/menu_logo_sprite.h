#ifndef GALAXY_INVADERS_ADAPTERS_MENU_LOGO_SPRITE_H
#define GALAXY_INVADERS_ADAPTERS_MENU_LOGO_SPRITE_H

#include <stdint.h>

/* Pixel data for the "GALAXY INVADERS" neon logo shown at the top of the
 * main menu (see draw_menu_logo in adapters/sdl_renderer.c), replacing the
 * two lines of procedurally-drawn neon pixel-font text this menu used to
 * show. Unlike every other embedded sprite here, this one is a glowing
 * bloom effect on a black backdrop, not flat-shaded pixel art - a flood-
 * filled background cutout would leave a hard edge straight through the
 * glow's own soft falloff, so alpha is instead derived directly from each
 * pixel's own brightness (its own max RGB channel): background black
 * already reads as fully transparent, and the glow itself fades through
 * partial alpha exactly the way it already fades to black in the source,
 * so it blends into the menu's own dark backdrop with no visible seam.
 * Premultiplied by construction. Lanczos-downsampled from the source
 * raster, preserving its own aspect ratio. A small stray star artifact in
 * the source's own bottom-right corner was painted out before conversion.
 * Row-major, top-left origin. Each entry is a packed 0xRRGGBBAA color; a
 * fully zero entry (alpha 0) is background and should not be drawn. */

#define MENU_LOGO_SPRITE_W 1000
#define MENU_LOGO_SPRITE_H 563

extern const uint32_t kMenuLogoSpritePixels[MENU_LOGO_SPRITE_W * MENU_LOGO_SPRITE_H];

#endif
