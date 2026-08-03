#include <string.h>
#include <ctype.h>
#include "adapters/pixel_font.h"
#include "adapters/graphics_primitives.h"

#define GLYPH_COLS 5
#define GLYPH_ROWS 7

typedef struct Glyph {
    char ch;
    const char *rows[GLYPH_ROWS];
} Glyph;

/* Each row is 5 characters: 'X' = lit dot, '.' = empty. Authored as a
 * literal grid (rather than packed bit masks) so every letter can be
 * proofread by eye directly in this table. */
static const Glyph kGlyphs[] = {
    {'A', {".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"}},
    {'B', {"XXXX.", "X...X", "X...X", "XXXX.", "X...X", "X...X", "XXXX."}},
    {'C', {".XXXX", "X....", "X....", "X....", "X....", "X....", ".XXXX"}},
    {'D', {"XXXX.", "X...X", "X...X", "X...X", "X...X", "X...X", "XXXX."}},
    {'E', {"XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "XXXXX"}},
    {'F', {"XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "X...."}},
    {'G', {".XXXX", "X....", "X....", "X.XXX", "X...X", "X...X", ".XXXX"}},
    {'H', {"X...X", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"}},
    {'I', {"XXXXX", "..X..", "..X..", "..X..", "..X..", "..X..", "XXXXX"}},
    {'J', {"..XXX", "...X.", "...X.", "...X.", "...X.", "X..X.", ".XX.."}},
    {'K', {"X...X", "X..X.", "X.X..", "XX...", "X.X..", "X..X.", "X...X"}},
    {'L', {"X....", "X....", "X....", "X....", "X....", "X....", "XXXXX"}},
    {'M', {"X...X", "XX.XX", "X.X.X", "X...X", "X...X", "X...X", "X...X"}},
    {'N', {"X...X", "XX..X", "X.X.X", "X.X.X", "X..XX", "X...X", "X...X"}},
    {'O', {".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX."}},
    {'P', {"XXXX.", "X...X", "X...X", "XXXX.", "X....", "X....", "X...."}},
    {'Q', {".XXX.", "X...X", "X...X", "X...X", "X.X.X", "X..X.", ".XX.X"}},
    {'R', {"XXXX.", "X...X", "X...X", "XXXX.", "X.X..", "X..X.", "X...X"}},
    {'S', {".XXXX", "X....", "X....", ".XXX.", "....X", "....X", "XXXX."}},
    {'T', {"XXXXX", "..X..", "..X..", "..X..", "..X..", "..X..", "..X.."}},
    {'U', {"X...X", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX."}},
    {'V', {"X...X", "X...X", "X...X", "X...X", "X...X", ".X.X.", "..X.."}},
    {'W', {"X...X", "X...X", "X...X", "X...X", "X.X.X", "XX.XX", "X...X"}},
    {'X', {"X...X", "X...X", ".X.X.", "..X..", ".X.X.", "X...X", "X...X"}},
    {'Y', {"X...X", "X...X", ".X.X.", "..X..", "..X..", "..X..", "..X.."}},
    {'Z', {"XXXXX", "....X", "...X.", "..X..", ".X...", "X....", "XXXXX"}},
    {'0', {".XXX.", "X...X", "X..XX", "X.X.X", "XX..X", "X...X", ".XXX."}},
    {'1', {"..X..", ".XX..", "..X..", "..X..", "..X..", "..X..", ".XXX."}},
    {'2', {".XXX.", "X...X", "....X", "...X.", "..X..", ".X...", "XXXXX"}},
    {'3', {".XXX.", "X...X", "....X", "..XX.", "....X", "X...X", ".XXX."}},
    {'4', {"...X.", "..XX.", ".X.X.", "X..X.", "XXXXX", "...X.", "...X."}},
    {'5', {"XXXXX", "X....", "XXXX.", "....X", "....X", "X...X", ".XXX."}},
    {'6', {"..XX.", ".X...", "X....", "XXXX.", "X...X", "X...X", ".XXX."}},
    {'7', {"XXXXX", "....X", "...X.", "..X..", ".X...", ".X...", ".X..."}},
    {'8', {".XXX.", "X...X", "X...X", ".XXX.", "X...X", "X...X", ".XXX."}},
    {'9', {".XXX.", "X...X", "X...X", ".XXXX", "....X", "...X.", ".XX.."}},
    {':', {".....", "..X..", ".....", ".....", ".....", "..X..", "....."}},
    {'!', {"..X..", "..X..", "..X..", "..X..", "..X..", ".....", "..X.."}},
    {'.', {".....", ".....", ".....", ".....", ".....", ".....", "..X.."}},
    {',', {".....", ".....", ".....", ".....", ".....", "..X..", ".X..."}},
    {'-', {".....", ".....", ".....", "XXXXX", ".....", ".....", "....."}},
    {'\'', {"..X..", "..X..", ".....", ".....", ".....", ".....", "....."}},
    {'/', {"....X", "...X.", "...X.", "..X..", ".X...", ".X...", "X...."}},
};
#define GLYPH_COUNT (int)(sizeof(kGlyphs) / sizeof(kGlyphs[0]))

static const Glyph *find_glyph(char c) {
    char up = (char)toupper((unsigned char)c);
    for (int i = 0; i < GLYPH_COUNT; i++) {
        if (kGlyphs[i].ch == up) return &kGlyphs[i];
    }
    return NULL; /* space and any unsupported character render as blank */
}

static float glyph_advance(float pixel_size) {
    return (GLYPH_COLS + 1) * pixel_size;
}

void pf_draw_text(SDL_Renderer *r, float x, float y, float pixel_size, Color c, const char *text) {
    float cursor_x = x;
    for (const char *p = text; *p; p++) {
        const Glyph *g = find_glyph(*p);
        if (g) {
            for (int row = 0; row < GLYPH_ROWS; row++) {
                for (int col = 0; col < GLYPH_COLS; col++) {
                    if (g->rows[row][col] == 'X') {
                        gp_fill_rect(r, cursor_x + (float)col * pixel_size,
                                     y + (float)row * pixel_size,
                                     pixel_size, pixel_size, c);
                    }
                }
            }
        }
        cursor_x += glyph_advance(pixel_size);
    }
}

void pf_draw_text_neon(SDL_Renderer *r, float x, float y, float pixel_size,
                        Color glow, Color edge, Color fill, Color shadow, const char *text) {
    /* Three full passes over the whole string, rather than looping
     * glow+shadow+fill per glyph, so that e.g. the next letter's glow
     * (pass 1) can never paint over this letter's drop shadow (pass 2) -
     * every shadow needs to end up on top of every glow, and every fill
     * on top of every shadow, string-wide. */

    /* Pass 1 - soft outer glow: a few progressively smaller, more opaque
     * enlarged copies of each lit cell, faking a blur without running one. */
    static const float glow_scales[] = {2.8f, 2.0f, 1.5f};
    static const unsigned char glow_alphas[] = {35, 55, 75};
    float cursor_x = x;
    for (const char *p = text; *p; p++) {
        const Glyph *g = find_glyph(*p);
        if (g) {
            for (int layer = 0; layer < 3; layer++) {
                float size = pixel_size * glow_scales[layer];
                Color gc = glow;
                gc.a = glow_alphas[layer];
                for (int row = 0; row < GLYPH_ROWS; row++) {
                    for (int col = 0; col < GLYPH_COLS; col++) {
                        if (g->rows[row][col] != 'X') continue;
                        float cx = cursor_x + (float)col * pixel_size + pixel_size / 2.0f;
                        float cy = y + (float)row * pixel_size + pixel_size / 2.0f;
                        gp_fill_rect(r, cx - size / 2.0f, cy - size / 2.0f, size, size, gc);
                    }
                }
            }
        }
        cursor_x += glyph_advance(pixel_size);
    }

    /* Pass 2 - drop shadow: solid silhouette offset well down-right of
     * each letter, for the chunky extruded/3D look retro arcade logos
     * use - needs a real offset (not a faint blur) to read at a glance. */
    float shadow_off = pixel_size * 2.0f;
    cursor_x = x;
    for (const char *p = text; *p; p++) {
        const Glyph *g = find_glyph(*p);
        if (g) {
            for (int row = 0; row < GLYPH_ROWS; row++) {
                for (int col = 0; col < GLYPH_COLS; col++) {
                    if (g->rows[row][col] != 'X') continue;
                    gp_fill_rect(r, cursor_x + (float)col * pixel_size + shadow_off,
                                 y + (float)row * pixel_size + shadow_off,
                                 pixel_size, pixel_size, shadow);
                }
            }
        }
        cursor_x += glyph_advance(pixel_size);
    }

    /* Pass 3 - dark contact outline: a slightly enlarged black copy of
     * just the boundary cells, drawn under the crisp letter, so there's a
     * hard-contrast rim separating "letter" from "glow" instead of the
     * glow's own color fading straight into the fill - without this the
     * eye can't tell where the glow ends and the actual glyph begins. */
    cursor_x = x;
    float outline_margin = pixel_size * 0.4f;
    for (const char *p = text; *p; p++) {
        const Glyph *g = find_glyph(*p);
        if (g) {
            for (int row = 0; row < GLYPH_ROWS; row++) {
                for (int col = 0; col < GLYPH_COLS; col++) {
                    if (g->rows[row][col] != 'X') continue;
                    bool boundary =
                        row == 0 || row == GLYPH_ROWS - 1 ||
                        col == 0 || col == GLYPH_COLS - 1 ||
                        g->rows[row - 1][col] != 'X' || g->rows[row + 1][col] != 'X' ||
                        g->rows[row][col - 1] != 'X' || g->rows[row][col + 1] != 'X';
                    if (!boundary) continue;
                    gp_fill_rect(r, cursor_x + (float)col * pixel_size - outline_margin,
                                 y + (float)row * pixel_size - outline_margin,
                                 pixel_size + outline_margin * 2.0f, pixel_size + outline_margin * 2.0f,
                                 (Color){0, 0, 0, 255});
                }
            }
        }
        cursor_x += glyph_advance(pixel_size);
    }

    /* Pass 4 - neon tube: cells touching the background (or the glyph's
     * own edge) get the bright outline color; interior cells get a
     * dimmer fill - the same hollow-tube look every neon sign uses. */
    cursor_x = x;
    for (const char *p = text; *p; p++) {
        const Glyph *g = find_glyph(*p);
        if (g) {
            for (int row = 0; row < GLYPH_ROWS; row++) {
                for (int col = 0; col < GLYPH_COLS; col++) {
                    if (g->rows[row][col] != 'X') continue;
                    bool boundary =
                        row == 0 || row == GLYPH_ROWS - 1 ||
                        col == 0 || col == GLYPH_COLS - 1 ||
                        g->rows[row - 1][col] != 'X' || g->rows[row + 1][col] != 'X' ||
                        g->rows[row][col - 1] != 'X' || g->rows[row][col + 1] != 'X';
                    gp_fill_rect(r, cursor_x + (float)col * pixel_size,
                                 y + (float)row * pixel_size,
                                 pixel_size, pixel_size, boundary ? edge : fill);
                }
            }
        }
        cursor_x += glyph_advance(pixel_size);
    }
}

float pf_text_width(const char *text, float pixel_size) {
    return (float)strlen(text) * glyph_advance(pixel_size) - pixel_size;
}
