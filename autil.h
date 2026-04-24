#ifndef AUTIL_H
#define AUTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#ifndef M_PI
#define M_PI 3.1415926535897932385
#endif
#define F_PI 3.1415926535897932385f

#undef MIN
#undef MAX
#undef ABS
#define MIN(x, y) ((x)<(y) ? (x) : (y))
#define MAX(x, y) ((x)>(y) ? (x) : (y))
#define ABS(x) ((x)<0 ? -(x) : (x))
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#if defined(_WIN32) && defined(AUTIL_IS_DLL)
	#ifdef AUTIL_IMPLEMENTATION
		#define DLL_LINK __declspec(dllexport)
	#elif AUTIL_IN_DLL
		#define DLL_LINK
	#else
		#define DLL_LINK __declspec(dllimport)
	#endif
#else
	#define DLL_LINK
#endif

#ifdef __cplusplus
extern "C" {
#endif

void errexit(char *format, ...);

void assertf(bool condition, char *format, ...);

#define dev_assertf assertf

/************************************** Memory management *****************************************/

#define DYNARRAY_MIN_SLOT 5 // 32 bytes
#define DYNARRAY_MAX_SLOT 34 // 16G, inclusive

#define AU_EXPAND_POLICY_ALWAYS_CRASH 1
#define AU_EXPAND_POLICY_EXTEND_OR_CRASH 2
#define AU_EXPAND_POLICY_EXTEND_OR_MOVE 3

typedef struct arena {
	i32 type;
	i32 expand_policy;
	u64 expand_by;
	u64 dyn_expand_by;
	u8 *start;
	u8 *next;
	u8 *end;
	u8 *dyn_data;
	u8 *dyn_end;
} Arena;

DLL_LINK extern Arena app_arena;

void arena_init(Arena *arena, u64 size, u64 dyn_size);

void arena_reset(Arena *arena);

bool arena_init_at(Arena *arena, void *static_start, u64 static_size, void *dyn_start,
	u64 dyn_size);

void arena_align(Arena *arena, i16 align);

Arena arena_copy(Arena *arena);

#ifdef AU_DEBUG_MEM

#define aalloc(a, n) aalloc_debug_mem((a), (n), __FILE__, __LINE__)

#define adalloc(a,n) adalloc_debug_mem((a), (n), __FILE__, __LINE__)

#define arealloc(a,n) arealloc_debug_mem((a), (n), __FILE__, __LINE__)

#define afree(a, n) afree_debug_mem((a), (n), __FILE__, __LINE__)

#else

// Allocate in the static area; cannot be freed.
void *aalloc(Arena *arena, u64 size);

// Allocate in the dynamic area; can be freed and realloc'ed.
void *adalloc(Arena *arena, u64 size);

void *arealloc(Arena *arena, const void *ptr, u64 size);

void afree(Arena *arena, const void *ptr);

#endif

void *aalloc_debug_mem(Arena *arena, u64 size, char *file, i32 line);

void *adalloc_debug_mem(Arena *arena, u64 size, char *file, i32 line);

void *arealloc_debug_mem(Arena *arena, const void *ptr, u64 size, char *file, i32 line);

void afree_debug_mem(Arena *arena, const void *ptr, char *file, i32 line);

void arena_print_usage(Arena *arena);

// Print allocations logged with a _debug_mem function.
void arena_print_logged_allocations(Arena *arena);

/************************************** Containers ************************************************/

/********* Dynarray ***********/
typedef struct dynarray {
	u8 *d;
	u64 length;
	u64 capacity;
	u64 item_size;
	/* impl */
	Arena *arena;
} Dynarray;

#define GEN_DYNARRAY_TYPE(name, item_type) struct name {\
	item_type *d;\
	u64 length;\
	u64 capacity;\
	u64 item_size;\
	Arena *arena;\
};

#define GEN_DYNARRAY_TYPEDEF(name, item_type) typedef struct {\
	item_type *d;\
	u64 length;\
	u64 capacity;\
	u64 item_size;\
	Arena *arena;\
} name;

/* NOTE: typeof is a C23 feature that requires gcc, clang, or cl with /std:clatest. */
#define dynarray_add_value(dynarray, item) {\
	typeof(item) x = item;\
	dynarray_add(dynarray, &x);\
}

struct dynarray *dynarray_from_data(Arena *arena, void *src, u64 item_size, u64 length);

struct dynarray *new_dynarray(Arena *arena, u64 item_size);

// arr must be a pointer to a struct dynarray or an equivalent created by GEN_DYNARRAY_TYPE, hence
// void * to avoid having to cast.
void dynarray_add(void *arr, void *item);

void dynarray_insert(void *arrp, void *item, u64 i);

void dynarray_remove(void *arr, u64 i);

void dynarray_expand_to(void *dynarray, u64 new_capacity);

void dynarray_expand_by(void *dynarray, u64 added_capacity);

void *dynarray_get(void *arr, u64 i);

/********* Hash Table *********/

typedef struct hash_entry {
	void *key;
	u64 len;
	void *value;
	bool alive;
} Hash_Entry;

typedef struct hash_table {
	Hash_Entry *d;
	u64 capacity;
	u64 n_entries;
	float expand_threshold;
	Arena *arena; // needed to make copies of the keys and to reallocate if expand_threshold is reached.
	bool copy_keys;
} Hash_Table;

u64 fnv1a_64(void *data, u64 len);

// Todo: return pointer
Hash_Table create_hash_table(Arena *arena, u64 capacity);

// Most hash tables will want to copy keys, in case the key is modified or deleted between insertion
// and lookup. But if you have static keys, you can turn that off by using this constructor.
Hash_Table create_nocopy_hash_table(Arena *arena, u64 capacity);

// TODO: destructors
// void destroy_hash_table(Hash_Table *table);

bool hash_table_set(Hash_Table *table, void *key, u64 key_len, void *value);

void *hash_table_get(Hash_Table *table, void *key, u64 key_len);

bool hash_table_delete(Hash_Table *table, void *key, u64 key_len);

/*************************************** Strings **************************************************/

typedef struct string {
	char *d;
	u64 length;
} String;

#define NULLSTRING ((String) { 0, 0 })

String string_from(char *from);

String string_copy_from(Arena *arena, char *from);

String string_ncopy_from(Arena *arena, char *from, u64 n);

String string_ncopy_from_string(Arena *arena, String from, u64 n);

String string_vprintf(char *fmt, va_list args);

String string_printf(char *fmt, ...);

String string_append(Arena *arena, String s1, String s2);

bool string_endswith(String s, String suffix);

bool string_startswith(String s, String prefix);

String string_array_join(Arena *arena, String *strings, u64 length, String delim);

u32 *decode_utf8(Arena *arena, const char *s, u64 *out_len);

// String String replace(Arena *area, String *s1, String *replace_this, String *with_this);

/*************************************** printa ***************************************************/

int vsnprinta(char *s, size_t n, const char *fmt, va_list ap);

int vsprinta(char *s, const char *fmt, va_list ap);

int sprinta(char *s, const char *fmt, ...);

int snprinta(char *s, size_t n, const char *fmt, ...);

// vfprinta and fprinta require FILE *, but we avoid including <stdio.h> here
// by using void *
int vfprinta(void *stream, const char *fmt, va_list ap);

int fprinta(void *stream, const char *fmt, ...);

int vprinta(const char *fmt, va_list ap);

int printa(const char *fmt, ...);

/************************************** Math ************************************************/

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

/************************************** Platform abstractions *************************************/

/* Basic threading */

typedef void (*au_thread_func)(void *arg);

struct au_thread {
	au_thread_func f;
	void *arg;
	bool running;
	// implementation specific fields...
};

struct au_thread *au_create_thread(Arena *arena, au_thread_func f, void *arg);

bool au_start_thread(struct au_thread *thread);

void au_join_thread(struct au_thread *thread, i32 timeout_ms);

struct au_mutex;

struct au_mutex *au_create_mutex(Arena *arena);

bool au_lock_mutex(struct au_mutex *mutex);

bool au_unlock_mutex(struct au_mutex *mutex);

/* Basic timing */

// time in nanoseconds since arbitrary start time
u64 get_os_time();

// time in seconds
double get_os_time_s();

void au_sleep(i32 millis);

/* Building, dynamic library loading */

u64 au_get_modification_time(char **filenames, int n);

void au_run_command_in_dir(char *command, char *dir);

void *au_load_library(char *path);

void au_unload_library(void *library);

void *au_get_function(void *library, char *func_name);

/************************************** Window and input ******************************************/

/********* Backend agnostic API **********/

// This enum is adapted from minifb. Todo: LICENSE
enum au_key {
    AU_KEY_UNKNOWN       = -1,

    AU_KEY_SPACE         = 32, AU_KEY_APOSTROPHE    = 39, AU_KEY_COMMA         = 44,
    AU_KEY_MINUS         = 45, AU_KEY_PERIOD        = 46, AU_KEY_SLASH         = 47,
    AU_KEY_0             = 48, AU_KEY_1             = 49, AU_KEY_2             = 50,
    AU_KEY_3             = 51, AU_KEY_4             = 52, AU_KEY_5             = 53,
    AU_KEY_6             = 54, AU_KEY_7             = 55, AU_KEY_8             = 56,
    AU_KEY_9             = 57, AU_KEY_SEMICOLON     = 59, AU_KEY_EQUAL         = 61,
    AU_KEY_A             = 65, AU_KEY_B             = 66, AU_KEY_C             = 67,
    AU_KEY_D             = 68, AU_KEY_E             = 69, AU_KEY_F             = 70,
    AU_KEY_G             = 71, AU_KEY_H             = 72, AU_KEY_I             = 73,
    AU_KEY_J             = 74, AU_KEY_K             = 75, AU_KEY_L             = 76,
    AU_KEY_M             = 77, AU_KEY_N             = 78, AU_KEY_O             = 79,
    AU_KEY_P             = 80, AU_KEY_Q             = 81, AU_KEY_R             = 82,
    AU_KEY_S             = 83, AU_KEY_T             = 84, AU_KEY_U             = 85,
    AU_KEY_V             = 86, AU_KEY_W             = 87, AU_KEY_X             = 88,
    AU_KEY_Y             = 89, AU_KEY_Z             = 90, AU_KEY_LEFT_BRACKET  = 91,
    AU_KEY_BACKSLASH     = 92, AU_KEY_RIGHT_BRACKET = 93, AU_KEY_GRAVE_ACCENT  = 96,
    AU_KEY_WORLD_1       = 161, AU_KEY_WORLD_2       = 162,

    AU_KEY_ESCAPE        = 256, AU_KEY_ENTER         = 257, AU_KEY_TAB           = 258,
    AU_KEY_BACKSPACE     = 259, AU_KEY_INSERT        = 260, AU_KEY_DELETE        = 261,
    AU_KEY_RIGHT         = 262, AU_KEY_LEFT          = 263, AU_KEY_DOWN          = 264,
    AU_KEY_UP            = 265, AU_KEY_PAGE_UP       = 266, AU_KEY_PAGE_DOWN     = 267,
    AU_KEY_HOME          = 268, AU_KEY_END           = 269, AU_KEY_CAPS_LOCK     = 280,
    AU_KEY_SCROLL_LOCK   = 281, AU_KEY_NUM_LOCK      = 282, AU_KEY_PRINT_SCREEN  = 283,
    AU_KEY_PAUSE         = 284, AU_KEY_F1            = 290, AU_KEY_F2            = 291,
    AU_KEY_F3            = 292, AU_KEY_F4            = 293, AU_KEY_F5            = 294,
    AU_KEY_F6            = 295, AU_KEY_F7            = 296, AU_KEY_F8            = 297,
    AU_KEY_F9            = 298, AU_KEY_F10           = 299, AU_KEY_F11           = 300,
    AU_KEY_F12           = 301, AU_KEY_F13           = 302, AU_KEY_F14           = 303,
    AU_KEY_F15           = 304, AU_KEY_F16           = 305, AU_KEY_F17           = 306,
    AU_KEY_F18           = 307, AU_KEY_F19           = 308, AU_KEY_F20           = 309,
    AU_KEY_F21           = 310, AU_KEY_F22           = 311, AU_KEY_F23           = 312,
    AU_KEY_F24           = 313, AU_KEY_F25           = 314, AU_KEY_KP_0          = 320,
    AU_KEY_KP_1          = 321, AU_KEY_KP_2          = 322, AU_KEY_KP_3          = 323,
    AU_KEY_KP_4          = 324, AU_KEY_KP_5          = 325, AU_KEY_KP_6          = 326,
    AU_KEY_KP_7          = 327, AU_KEY_KP_8          = 328, AU_KEY_KP_9          = 329,
    AU_KEY_KP_DECIMAL    = 330, AU_KEY_KP_DIVIDE     = 331, AU_KEY_KP_MULTIPLY   = 332,
    AU_KEY_KP_SUBTRACT   = 333, AU_KEY_KP_ADD        = 334, AU_KEY_KP_ENTER      = 335,
    AU_KEY_KP_EQUAL      = 336, AU_KEY_LEFT_SHIFT    = 340, AU_KEY_LEFT_CONTROL  = 341,
    AU_KEY_LEFT_ALT      = 342, AU_KEY_LEFT_SUPER    = 343, AU_KEY_RIGHT_SHIFT   = 344,
    AU_KEY_RIGHT_CONTROL = 345, AU_KEY_RIGHT_ALT     = 346, AU_KEY_RIGHT_SUPER   = 347,
    AU_KEY_MENU          = 348, AU_KEY_COMMAND       = 349, AU_KEY_COUNT          = 350
};

enum au_mouse_button {
    AU_MOUSE_BUTTON_LEFT = 0,
    AU_MOUSE_BUTTON_RIGHT = 1,
    // todo: I dont receive events from middle or side buttons on Windows.
    AU_MOUSE_BUTTON_MIDDLE = 2,
    AU_MOUSE_BUTTON_X1 = 3,
    AU_MOUSE_BUTTON_X2 = 4,
    AU_MOUSE_BUTTON_COUNT = 5
};

enum au_cursor {
	AU_CURSOR_NORMAL,
	AU_CURSOR_HORIZONTAL_RESIZE,
	AU_CURSOR_VERTICAL_RESIZE,
	AU_CURSOR_FORWARD_DIAGONAL_RESIZE,
	AU_CURSOR_BACKWARD_DIAGONAL_RESIZE,
	AU_CURSOR_HAND,
	AU_CURSOR_COUNT,
 };

#define MOUSE_BUTTONS_N 5
#define KEYS_N 350
#define AU_TICK_FREQ 1000000000
#define AU_MAX_CURSORS 20

typedef struct char_dynarray {
	char *d;
	u64 length;
	u64 capacity;
	u64 item_size;
} Char_Dynarray;

typedef struct input_state {
	bool quit;

	bool focus_gained;
	bool focus_lost;
	bool window_resized;
	bool window_rescaled;

	bool mouse_pressed[AU_MOUSE_BUTTON_COUNT];
	bool mouse_released[AU_MOUSE_BUTTON_COUNT];
	bool mouse_down[AU_MOUSE_BUTTON_COUNT];
	// @Breaking was mouse_x, etc
	i32 pointer_x, pointer_y;
	i32 pointer_dx, pointer_dy;
	// TODO: bool pointer_entered, pointer_left;

	float wheel_dx, wheel_dy;

	bool key_pressed[AU_KEY_COUNT];
	bool key_released[AU_KEY_COUNT];
	bool key_down[AU_KEY_COUNT];
	// handles modifiers even if remapped
//  	i32 mods;
	Char_Dynarray *text_entered;
	u64 start_time;
	u64 anim_time;
	u64 wall_time;
	u64 anim_dt;
	u64 wall_dt;
	// dt_s = dt / AU_TICK_FREQ
	double anim_dt_s;
	double wall_dt_s;
} Input_State;

// Todo: all windows should have an arena
typedef struct window {
	// allows windows to be passed to au_draw functions as "scenes"
	i32 scene_type;
	i32 scene_x_ignore, scene_y_ignore; // always 0, 0
	i32 width;
	i32 height;
	void *scene_parent_ignore; // NULL
	void *scene_window_ignore; // points to self
	// beginning of line to beginning of next line, in pixels
	u32 stride;
	u32 rows;
	Arena *arena;
	// user data
	void *udata;
	// set by create_window: ui element sizes specified in pixels should be multiplied this.
	// When it changes, rescale_callback will be called if it exists
	float scale;
    struct window *parent;
    Input_State *input;
    double animation_rate;
	// optional
	// if a resize requires a new buffer, this is called before resize_callback. old_buffers
	// is either one or two pointers depending on double_buffer
    // XX change to i32!
	void (*restride_callback)(struct window *, u32 **old_buffers, i32 old_stride,
		i32 old_rows);
	void (*resize_callback)(struct window *, i32 old_width, i32 old_height);
	void (*rescale_callback)(struct window *, float old_scale);
	void (*mouse_move_callback)(struct window *, i32 x, i32 y);
	void (*mouse_leave_callback)(struct window *);
	void (*mouse_press_callback)(struct window *, int mouse_n, bool down);
	void (*mouse_scroll_callback)(struct window *, float amount);
	void (*key_press_callback)(struct window *, enum au_key key, bool down);
	// Used by draw_cpu.c and draw_opengl.c. create_window initializes to NULL.
	void *renderer_data;
	// This is called on commit_changes, in draw_opengl it's used to call glDrawArrays, etc. Also
	// initialized to NULL.
	void (*renderer_callback)(struct window *);
	bool double_buffer;
	bool running;
} Window;

// If you know the backend will be Native/CPU, you can cast this to a Window *; if you know the
// backend is SDL, you can cast to an SDL_Window *
Window *create_window(Arena *arena, u32 width, u32 height, char *title, bool double_buffer);

// Reset input state, then listen for and dispatch events. If milliseconds < 0, run indefinitely
void run_window(Window *window, float milliseconds);

// Reset input state and flush event queue, updating input state
void update_input_state(Window *window);

// In double buffered mode, swap the front and back buffers, then commit the new front buffer
// In single buffered mode, commit the buffer
void commit_changes(Window *window);

bool au_window_set_cursor(Window *window, i32 cursor);

i32 au_window_load_sound(Window *window, const char *path, const char *type);

i32 au_window_load_sound_from_memory(Window *window, const void *data, u64 length,
	const char *type);

void au_window_play_sound(Window *window, i32 sound_i);

void close_window(Window *window);

void destroy_window(Window *window);

/******** Native/CPU window only API ************/

typedef struct cpu_window {
	/* Window */
	i32 scene_type;
	i32 scene_x_ignore, scene_y_ignore; // always 0, 0
	i32 width;
	i32 height;
	void *scene_parent_ignore; // NULL
	void *scene_window_ignore; // points to self
	// beginning of line to beginning of next line, in pixels
	u32 stride;
	u32 rows;
	Arena *arena;
	// user data
	void *udata;
	// set by create_window: ui element sizes specified in pixels should be multiplied this.
	// When it changes, rescale_callback will be called if it exists
	float scale;
    struct window *parent;
    Input_State *input;
    double animation_rate;
	// optional
	// if a resize requires a new buffer, this is called before resize_callback. old_buffers
	// is either one or two pointers depending on double_buffer
	void (*restride_callback)(struct window *, u32 **old_buffers, i32 old_stride,
		i32 old_rows);
	void (*resize_callback)(struct window *, i32 old_width, i32 old_height);
	void (*rescale_callback)(struct window *, float old_scale);
	void (*mouse_move_callback)(struct window *, i32 x, i32 y);
	void (*mouse_leave_callback)(struct window *);
	void (*mouse_press_callback)(struct window *, int mouse_n, bool down);
	void (*mouse_scroll_callback)(struct window *, float amount);
	void (*key_press_callback)(struct window *, enum au_key key, bool down);
	// Used by draw_cpu.c and draw_opengl.c. create_window initializes to NULL.
	void *renderer_data;
	// This is called on commit_changes, in draw_opengl it's used to call glDrawArrays, etc. Also
	// initialized to NULL.
	void (*renderer_callback)(struct window *);
	bool double_buffer;
	bool running;
	/* CPU_Window */
	// If double_buffer is enabled, this is the back(drawing) buffer.
	u32 *pixels;
	// If double_buffer is enabled, this is the front buffer. Otherwise, it's null.
	u32 *front_buffer;
} CPU_Window;

// In both modes, commit the front buffer
void commit_changes_noswap(Window *window);

Window *create_child_window(Arena *arena, Window *parent, u32 width,
    u32 height, char *title, bool double_buffer);

/*************************************** Graphics *************************************************/

/********* Generic API *************/

// 0xRRGGBBAA -> { r, g, b, a}
Vector4 hex2vec(u32 hex);

u32 vec2hex(Vector4 vector);

enum au_scene_type {
    AU_SCENE_TYPE_SCENE = -1,
    AU_SCENE_TYPE_WINDOW = 1,
};

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

// Translate scene coords to window coords, does not out of window results
Vector2 scene_coords_to_window(Scene *scene, float x, float y);

// The boundaries of the scene in window coordinates, returns { x=x1, y=y1, z=x2, w=y2 }
Vector4 scene_get_window_bounds(Scene *scene);

// Does not clamp- window coords are assumed to be good.
Vector2 window_coords_to_scene(Scene *scene, float x, float y);

i32 load_font(Scene *scene, const char *font_file);

i32 load_font_from_memory(Scene *scene, const void *font_data, u64 data_size);

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

void add_image_with_color(Scene *scene, i32 image_i, float x, float y, u32 color);

/********* "in_box" ***********/

// These convenience functions create a temporary scene with corners (x1, y1) and (x2, y2) and call
// the functions above.

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

/*********** OpenGL only ************/

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
