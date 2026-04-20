/*
Last updated 2026-12-26.
*/

#include <math.h>

#include "au_core.h"
#include "au_math.h"

Vector2 normalize_v2(Vector2 v)
{
    float d = sqrtf(v.x*v.x + v.y*v.y);
    return (Vector2) { v.x / d, v.y / d };
}

Vector2 add_v2(Vector2 v, Vector2 w)
{
    return (Vector2) { v.x + w.x, v.y + w.y };
}

Vector2 mult_cv2(float c, Vector2 v)
{
    return (Vector2) { c*v.x, c*v.y };
}

