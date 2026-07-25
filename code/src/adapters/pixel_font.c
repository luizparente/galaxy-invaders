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

float pf_text_width(const char *text, float pixel_size) {
    return (float)strlen(text) * glyph_advance(pixel_size) - pixel_size;
}
