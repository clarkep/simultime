#ifndef AU_WINDOW_SDL_H
#define AU_WINDOW_SDL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <SDL3/SDL.h>

#include "au_core.h"
#include "au_window.h"

typedef struct au_sdl_audio_buffer
{
	SDL_AudioSpec spec;
	u8 *buf;
	u32 len;
} AU_SDL_Audio_Buffer;

typedef struct au_sdl_audio_buffer_dynarray
{
	AU_SDL_Audio_Buffer *d;
	u64 length;
	u64 capacity;
	u64 item_size;
	Arena *arena;
} AU_SDL_Audio_Buffer_Dynarray;

typedef struct au_sdl_window {
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
	/* SDL specific */
	SDL_Window *sdl_window;
	float sdl_window_coords_to_pixels;
	AU_SDL_Audio_Buffer_Dynarray *audio_buffers;
	SDL_AudioStream *audio_stream;
} AU_SDL_Window;

#ifdef __cplusplus
}
#endif

#endif