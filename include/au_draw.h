#ifndef AU_DRAW_H
#define AU_DRAW_H

#include "stb_truetype.h"

#include "au_core.h"
#include "au_math.h"
#include "au_window.h"

#ifdef __cplusplus
extern "C" {
#endif

/********* Generic API *************/

// 0xRRGGBBAA -> { r, g, b, a}
Vector4 hex2vec(u32 hex);

u32 vec2hex(Vector4 vector);

enum au_scene_type {
    AU_SCENE_TYPE_SCENE = -1,
    AU_SCENE_TYPE_WINDOW = 1,
};

// Todo: type -> flags. flags window type, y_up.
typedef struct scene {
    i32 type;
    i32 x, y, w, h;
    struct scene *parent;
    Window *window;
} Scene;

// Create a child scene of parent. To this is and all Scene functions, you can pass a
// Window * instead, cast to Scene *.
Scene *create_child_scene(Arena *arena, Scene *parent, float x, float y, float w, float h);

// Same as create_child_scene, but return the scene directly.
Scene make_child_scene(Scene *parent, float x, float y, float w, float h);

// Translate scene coords to window coords
Vector2 scene_coords_to_window(Scene *scene, float x, float y);

// The boundaries of the scene in window coordinates, returns { x=x1, y=y1, z=x2, w=y2 }
Vector4 scene_get_window_bounds(Scene *scene);

Vector2 window_coords_to_scene(Scene *scene, float x, float y);

i32 load_font(Scene *scene, const char *font_file);

i32 load_font_from_memory(Scene *scene, const void *font_data, u64 data_size);

// 'type' can be one of:
//     "png"
//     "jpg"
//     "svg", which rasterizes the svg
//     "svg_alpha", which rasterzies the svg, only keeping the alpha channel
//         -can be used with add_image_with_color
i32 load_image(Scene *scene, const char *path, const char *type);

float get_font_metric_descent(Scene *scene, i32 font_i);

i32 load_image_from_memory(Scene *scene, const void *data, u64 data_size, const char *type);

i32 load_image_at_size(Scene *scene, const char *path, const char *type, i32 w, i32 h);

i32 load_image_at_size_from_memory(Scene *scene, const void *data, u64 data_size, const char *type,
    i32 w, i32 h);

i32 load_bitmap(Scene *scene, const void *data, i32 width, i32 height, i32 bytes_per_row,
    i8 channels);

void unload_image(Scene *scene, i32 image_i);

void clear_background(Scene *scene, u32 Color);

void add_line(Scene *scene, float x1, float y1, float x2, float y2, u32 color);

void add_rectangle(Scene *scene, float x, float y, float w, float h, u32 color);

void add_rectangle_outline(Scene *scene, float x, float y, float w, float h, u32 color);

void add_triangle(Scene *scene, float x1, float y1, float x2, float y2, float x3, float y3, u32 color);

void add_triangle_outline(Scene *scene, float x1, float y1, float x2, float y2, float x3, float y3, u32 color);

void add_circle(Scene *scene, float x, float y, float r, u32 color);

void add_circle_outline(Scene *scene, float x, float y, float r, u32 color);

void add_character(Scene *scene, i32 font_i, i32 size, u32 c, float x, float y, u32 color,
    float *advance_x);

void add_text(Scene *scene, i32 font_i, i32 size, const char *text, float x, float y,
    u32 color);

void add_textf(Scene *scene, i32 font_i, i32 size, float x, float y, u32 color,
    const char *fmt, ...);

void add_text_utf32(Scene *scene, i32 font_i, i32 size, const u32 *text, float x, float y,
    u32 color);

float measure_text_width(Scene *scene, i32 font_i, i32 size, const char *text);

float measure_text_widthf(Scene *scene, i32 font_i, i32 size, const char *fmt, ...);

float measure_text_width_utf32(Scene *scene, i32 font_i, i32 size, const u32 *text);

void add_image(Scene *scene, i32 image_i, float x, float y);

void add_image_with_alpha(Scene *scene, i32 image_i, float x, float y, float alpha);

// image must be single channel, loaded with load_bitmap or load_image and type "svg_alpha"
void add_image_with_color(Scene *scene, i32 image_i, float x, float y, u32 color);

/********* "in_box" ***********/

// These convenience functions create a temporary scene with corners (x1, y1) and (x2, y2) and call
// the functions above. All coordinates are relative to the passed in scene.

void add_line_in_box(Scene *scene, float start_x, float start_y, float end_x, float end_y,
    u32 color, float x1, float y1, float x2, float y2);

void add_rectangle_in_box(Scene *scene, float x, float y, float w, float h, u32 color, float x1,
    float y1, float x2, float y2);

void add_rectangle_outline_in_box(Scene *scene, float x, float y, float w, float h, u32 color,
    float x1, float y1, float x2, float y2);

void add_triangle_in_box(Scene *scene, float x1, float y1, float x2, float y2, float x3, float y3,
    u32 color, float box_x1, float box_y1, float box_x2, float box_y2);

void add_triangle_outline_in_box(Scene *scene, float x1, float y1, float x2, float y2, float x3,
    float y3, u32 color);

void add_circle_in_box(Scene *scene, float x, float y, float r, u32 color, float x1,
    float y1, float x2, float y2);

void add_circle_outline_in_box(Scene *scene, float x, float y, float r, u32 color, float x1,
    float y1, float x2, float y2);

void add_character_in_box(Scene *scene, i32 font_i, i32 size, u32 c, float x, float y, u32 color,
    float *advance_x, float x1, float y1, float x2, float y2);

void add_text_in_box(Scene *scene, i32 font_i, i32 size, const char *text, float x, float y,
    u32 color, float x1, float y1, float x2, float y2);

void add_textf_in_box(Scene *scene, i32 font_i, i32 size, float x, float y, u32 color,
    float x1, float y1, float x2, float y2, const char *fmt, ...);

void add_text_utf32_in_box(Scene *scene, i32 font_i, i32 size, const u32 *text, float x, float y,
    u32 color, float x1, float y1, float x2, float y2);

void add_image_in_box(Scene *scene, i32 image_i, float x, float y, float x1, float y1, float x2,
    float y2);

void add_image_with_alpha_in_box(Scene *scene, i32 image_i, float x, float y, float alpha,
    float x1, float y1, float x2, float y2);

/********* CPU only *****************/

typedef struct font Font;

bool pixel(CPU_Window *win, i32 x, i32 y, u32 color);

bool mpixel(CPU_Window *win, i32 x, i32 y, u32 color);

void copy_bitmap(CPU_Window *win, i32 x, i32 y, u32 *bitmap, i32 w, i32 h, i32 stride);

void draw_rect(CPU_Window *win, i32 x, i32 y, i32 w, i32 h, u32 color);

void draw_rect_in_box(CPU_Window *win, i32 x, i32 y, i32 w, i32 h, u32 color, i32 x1,
    i32 y1, i32 x2, i32 y2);

void draw_circle(CPU_Window *win, i32 x, i32 y, i32 r, u32 color);

void draw_circle_in_box(CPU_Window *win, i32 x, i32 y, i32 r, u32 color, i32 x1,
    i32 y1, i32 x2, i32 y2);

void draw_filled_circle(CPU_Window *win, i32 x, i32 y, i32 r, u32 color);

void clipped_rect(CPU_Window *win, i32 x, i32 y, i32 w, i32 h, u32 color);

void rect_outline_in_box(CPU_Window *win, i32 x, i32 y, i32 w, i32 h, u32 color,
    i32 x1, i32 y1, i32 x2, i32 y2);

void rect_outline_clipped(CPU_Window *win, i32 x, i32 y, i32 w, i32 h, u32 color);

void fill_window(CPU_Window *win, u32 color);

bool init_font(Font *font, Arena *arena, const char *path);

// +
Font *create_font(Arena *arena, const char *path);

u32 blend_colors(u32 c1, u32 c2, float p);

/********* OpenGL only **************/

void add_rounded_quad(Scene *scene, Vector2 *corners, bool *rounded, float radius, u32 color);

void add_rounded_quad_outline(Scene *scene, Vector2 *corners, bool *rounded, float radius,
    u32 color);

void add_rounded_rectangle(Scene *scene, float x, float y, float w, float h, float radius,
    u32 color);

void add_rounded_rectangle_outline(Scene *scene, float x, float y, float w, float h, float radius,
    u32 color);

#ifdef __cplusplus
}
#endif

#endif