#include <stdio.h>

#include "au_core.h"
#include "au_math.h"
#include "au_draw.h"

Scene *create_child_scene(Arena *arena, Scene *parent, float x, float y, float w, float h)
{
    Scene *scene = aalloc(arena, sizeof(Scene));
    *scene = (Scene) { AU_SCENE_TYPE_SCENE, x, y, w, h, parent, parent->window };
    return scene;
	return (Scene *) scene;
}

Scene make_child_scene(Scene *parent, float x, float y, float w, float h)
{
    Scene ret = { AU_SCENE_TYPE_SCENE, x, y, w, h, parent, parent->window };
    return ret;
}

Vector2 scene_coords_to_window(Scene *scene, float x, float y)
{
	float x_cur = x;
	float y_cur = y;
	Scene *cur = scene;
	while (cur->parent) {
		x_cur = cur->x + x_cur;
		y_cur = cur->y + y_cur;
		cur = cur->parent;
	}
	return (Vector2) { x_cur, y_cur };
}

Vector4 scene_get_window_bounds(Scene *scene)
{
	float x1_cur = 0;
	float y1_cur = 0;
	float x2_cur = scene->w;
	float y2_cur = scene->h;
	Scene *cur = scene;
	while (cur->parent) {
		x1_cur = cur->x + x1_cur;
		y1_cur = cur->y + y1_cur;
		x2_cur = cur->x + x2_cur;
		y2_cur = cur->y + y2_cur;
		cur = cur->parent;
	}
	return (Vector4) { x1_cur, y1_cur, x2_cur, y2_cur };
}

Vector2 window_coords_to_scene(Scene *scene, float x, float y)
{
	Vector2 origin = scene_coords_to_window(scene, 0, 0);
	return (Vector2) { x - origin.x, y - origin.y };
}

void add_textf(Scene *scene, i32 font_i, i32 size, float x, float y, u32 color,
	const char *fmt, ...)
{
	char text[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, 1024, fmt, ap);
	va_end(ap);
	add_text(scene, font_i, size, text, x, y, color);
}

float measure_text_widthf(Scene *scene, i32 font_i, i32 size, const char *fmt, ...)
{
	char text[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, 1024, fmt, ap);
	va_end(ap);
	return measure_text_width(scene, font_i, size, text);
}

void add_textf_in_box(Scene *scene, i32 font_i, i32 size, float x, float y, u32 color,
	float x1, float y1, float x2, float y2, const char *fmt, ...)
{
	char text[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, 1024, fmt, ap);
	va_end(ap);
	add_text_in_box(scene, font_i, size, text, x, y, color, x1, y1, x2, y2);
}

void add_line_in_box(Scene *scene, float start_x, float start_y, float end_x, float end_y,
    u32 color, float x1, float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_line(&child, start_x - x1, start_y - y1, end_x - x1, end_y - y1, color);
}

void add_rectangle_in_box(Scene *scene, float x, float y, float w, float h, u32 color, float x1,
    float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_rectangle(&child, x - x1, y - y1, w, h, color);
}

void add_rectangle_outline_in_box(Scene *scene, float x, float y, float w, float h, u32 color,
    float x1, float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_rectangle_outline(&child, x - x1, y - y1, w, h, color);
}

void add_circle_in_box(Scene *scene, float x, float y, float r, u32 color, float x1,
    float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_circle(&child, x - x1, y - y1, r, color);
}

void add_circle_outline_in_box(Scene *scene, float x, float y, float r, u32 color, float x1,
    float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_circle_outline(&child, x - x1, y - y1, r, color);
}

void add_character_in_box(Scene *scene, i32 font_i, i32 size, u32 c, float x, float y, u32 color,
    float *advance_x, float x1, float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_character(&child, font_i, size, c, x - x1, y - y1, color, advance_x);
}

void add_text_in_box(Scene *scene, i32 font_i, i32 size, const char *text, float x, float y,
    u32 color, float x1, float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_text(&child, font_i, size, text, x - x1, y - y1, color);
}

void add_text_utf32_in_box(Scene *scene, i32 font_i, i32 size, const u32 *text, float x, float y,
    u32 color, float x1, float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_text_utf32(&child, font_i, size, text, x - x1, y - y1, color);
}

void add_image_in_box(Scene *scene, i32 image_i, float x, float y, float x1, float y1, float x2,
    float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_image(&child, image_i, x - x1, y - y1);
}

void add_image_with_alpha_in_box(Scene *scene, i32 image_i, float x, float y, float alpha,
    float x1, float y1, float x2, float y2)
{
	Scene child = { AU_SCENE_TYPE_SCENE, scene->x + x1, scene->y + y1, x2 - x1, y2 - y1, scene,
		scene->window };
	add_image_with_alpha(&child, image_i, x - x1, y - y1, alpha);
}

