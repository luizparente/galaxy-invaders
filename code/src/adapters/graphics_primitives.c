#include <math.h>
#include "adapters/graphics_primitives.h"

static SDL_Color to_sdl_color(Color c) {
    return (SDL_Color){c.r, c.g, c.b, c.a};
}

void gp_fill_rect(SDL_Renderer *r, float x, float y, float w, float h, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRectF(r, &rect);
}

void gp_draw_line(SDL_Renderer *r, float x0, float y0, float x1, float y1, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLineF(r, x0, y0, x1, y1);
}

void gp_fill_triangle(SDL_Renderer *r, float x0, float y0, float x1, float y1,
                       float x2, float y2, Color c) {
    SDL_Color sc = to_sdl_color(c);
    SDL_Vertex verts[3] = {
        {{x0, y0}, sc, {0, 0}},
        {{x1, y1}, sc, {0, 0}},
        {{x2, y2}, sc, {0, 0}},
    };
    SDL_RenderGeometry(r, NULL, verts, 3, NULL, 0);
}

void gp_fill_quad(SDL_Renderer *r, float x0, float y0, float x1, float y1,
                   float x2, float y2, float x3, float y3, Color c) {
    gp_fill_triangle(r, x0, y0, x1, y1, x2, y2, c);
    gp_fill_triangle(r, x0, y0, x2, y2, x3, y3, c);
}

void gp_fill_circle(SDL_Renderer *r, float cx, float cy, float radius, Color c) {
    if (radius <= 0.0f) return;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    int ir = (int)ceilf(radius);
    for (int dy = -ir; dy <= ir; dy++) {
        float span = radius * radius - (float)(dy * dy);
        if (span < 0.0f) continue;
        float dx = sqrtf(span);
        SDL_RenderDrawLineF(r, cx - dx, cy + (float)dy, cx + dx, cy + (float)dy);
    }
}

void gp_fill_ellipse(SDL_Renderer *r, float cx, float cy, float rx, float ry, Color c) {
    if (rx <= 0.0f || ry <= 0.0f) return;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    int iry = (int)ceilf(ry);
    for (int dy = -iry; dy <= iry; dy++) {
        float t = (float)dy / ry;
        float span = 1.0f - t * t;
        if (span < 0.0f) continue;
        float dx = rx * sqrtf(span);
        SDL_RenderDrawLineF(r, cx - dx, cy + (float)dy, cx + dx, cy + (float)dy);
    }
}

void gp_draw_circle_outline(SDL_Renderer *r, float cx, float cy, float radius, Color c) {
    if (radius <= 0.0f) return;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    const int segments = 24;
    float prevx = cx + radius, prevy = cy;
    for (int i = 1; i <= segments; i++) {
        float ang = (float)i / (float)segments * 2.0f * (float)M_PI;
        float x = cx + radius * cosf(ang);
        float y = cy + radius * sinf(ang);
        SDL_RenderDrawLineF(r, prevx, prevy, x, y);
        prevx = x;
        prevy = y;
    }
}
