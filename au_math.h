#ifndef AU_MATH_H
#define AU_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vector2 {
    float x;
    float y;
} Vector2;

typedef struct vector3 {
    float x;
    float y;
    float z;
} Vector3;

typedef struct vector4 {
    float x;
    float y;
    float z;
    float w;
} Vector4;

Vector2 normalize_v2(Vector2 v);
Vector2 add_v2(Vector2 v, Vector2 w);
Vector2 mult_cv2(float c, Vector2 v);

#ifdef __cplusplus
}
#endif

#endif