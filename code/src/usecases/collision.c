#include "usecases/collision.h"
#include <math.h>

bool collision_aabb_overlap(float ax, float ay, float a_half_w, float a_half_h,
                             float bx, float by, float b_half_w, float b_half_h) {
    return fabsf(ax - bx) < (a_half_w + b_half_w) &&
           fabsf(ay - by) < (a_half_h + b_half_h);
}
