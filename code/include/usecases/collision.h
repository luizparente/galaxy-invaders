#ifndef GALAXY_INVADERS_USECASES_COLLISION_H_
#define GALAXY_INVADERS_USECASES_COLLISION_H_

#include <stdbool.h>

/* Axis-aligned bounding box overlap test. Every entity's (x, y) is its
 * center, so each box is described by its center plus half-width/height.
 * Pure and dependency-free: safe to unit test without SDL or GameState. */
bool collision_aabb_overlap(float ax, float ay, float a_half_w, float a_half_h,
                             float bx, float by, float b_half_w, float b_half_h);

#endif
