#include "autil.h"
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

typedef struct app App;

// Global settings
typedef struct app_settings {
	u32 background_color;
	i32 font;
	float font_size_unscaled;
	float name_font_size_unscaled;
	u32 font_color;
	u32 bottom_bar_color;
	u32 bottom_bar_buttons_bg_color;
	i32 bottom_bar_height;
	u32 tab_bar_color;
	i32 x_button;
	float x_button_size_unscaled;
	float top_bar_h_unscaled;
	u32 overlay_color;
} App_Settings;

/***** Helper types *****/

typedef struct grab_state {
	bool active;
	bool starting;
	bool stopping;
} Grab_State;

typedef struct float_dynarray
{
	float *d;
	u64 length;
	u64 capacity;
	u64 item_size;
	Arena *arena;
} Float_Dynarray;

typedef struct time_fields {
	i32 hours;
	i32 minutes;
	i32 seconds;
	i32 millis;
	i32 micros;
} Time_Fields;

/***** UI Widget types ******/

typedef enum {
	BUTTON_TYPE_PLAY,
	BUTTON_TYPE_PAUSE,
	BUTTON_TYPE_STOP,
	BUTTON_TYPE_COUNT
} button_type_e;

// play, pause, or stop button
typedef struct pps_button {
	button_type_e type;
	u32 color;
	float hover_t;
} PPS_Button;

typedef struct time_input {
	i32 font;
	i32 font_size_unscaled;
	i32 text_color;
	u64 value;
	float fields_hovered_t[4];
	i32 field_clicking;
	i32 field_selected;
	i32 field_dragging;
	i32 cursor_i;
	i32 cursor_j;
	bool insert_mode;
	bool dragging;
	i32 drag_start_y;
	i32 drag_field_start_value;
	bool show_fractional;
	bool initialized;
} Time_Input;

/***** Core clock types *****/

typedef struct layout_node Layout_Node;
typedef struct layout_node_dynarray Layout_Node_Dynarray;

typedef struct clock {
	Layout_Node *node;
	String name;
} Clock;

typedef enum {
	TIMER_STATE_READY,
	TIMER_STATE_RUNNING,
	TIMER_STATE_PAUSED,
	TIMER_STATE_EXPIRED,
	TIMER_STATE_COUNT
} timer_state_e;

typedef struct timer {
	Layout_Node *node;
	String name;
	PPS_Button play_pause;
	PPS_Button stop_button;
	Time_Input time_input;
	// The time the timer is set for
	u64 interval;
	// If running, the timestamp at which the timer was started/restarted.
	u64 start_time;
	// If running, the time remaining when the timer was started. Otherwise, the paused value.
	u64 start_value;
	float flash_red;
	float flash_percent;
	timer_state_e state;
	bool initialized;
} Timer;

typedef struct stopwatch {
	Layout_Node *node;
	String name;
	PPS_Button play_pause;
	PPS_Button stop_button;
	// If running, the time the stopwatch was started/restarted.
	u64 start_time;
	// If running, the time elapsed when the stopwatch was started. Otherwise, the paused value.
	u64 start_value;
	bool running;
	bool initialized;
} Stopwatch;

/***** "layout system" (see comment below): ******/

typedef struct tab_group
{
	Layout_Node *node;
	Layout_Node_Dynarray *children;
	float scroll_x;
	i32 active_tab;
	bool active_tab_dragging;
	bool active_tab_dragging_out;
	float drag_start_pointer_x;
	float drag_start_tab_x;
} Tab_Group;

typedef enum {
	SPLIT_HORIZONTAL,
	SPLIT_VERTICAL,
	SPLIT_COUNT,
} split_e;

typedef struct split_group
{
	Layout_Node *node;
	split_e type;
	Layout_Node_Dynarray *children;
	Float_Dynarray *child_sizes;
	i32 child_border_grabbed;
	bool have_resize_cursor;
	bool initialized;
} Split_Group;

typedef enum {
	NODE_TYPE_SPLIT_GROUP,
	NODE_TYPE_TAB_GROUP,
	NODE_TYPE_CLOCK,
	NODE_TYPE_TIMER,
	NODE_TYPE_STOPWATCH,
	NODE_TYPE_COUNT
} node_type_e;

// Basically:
// -"Layout nodes" are either split groups, tab groups, or panes(clocks, timers, and stopwatches).
// -Split groups contain other split groups, or tab groups.
// 		-Horizontal split groups contain vertical split groups or tab groups.
//		-Vertical split groups contain horizontal split groups or tab groups.
// -Tab groups only contain panes.
typedef struct layout_node {
	Layout_Node *parent;
	node_type_e type;
	union {
		Split_Group *split_group;
		Tab_Group *tab_group;
		Clock *clock;
		Timer *timer;
		Stopwatch *stopwatch;
	} content;
} Layout_Node;

typedef enum {
	SIDE_LEFT,
	SIDE_RIGHT,
	SIDE_TOP,
	SIDE_BOTTOM,
} side_e;

// As you drag a pane around, we generate layout targets representing nodes on the tree where the
// pane will go if dropped. Layout targets are generated with get_layout_target_for_point and
// applied with add_pane_to_target. The current target is put in g_app->drop_target_overlay for
// previewing. The score is an implementation detail of get_layout_target_for_point; it ranks
// targets based on how close your pointer is.
typedef struct layout_target
{
	Layout_Node *node;
	i32 child;
	side_e side;
	float score;
} Layout_Target;

typedef struct layout_node_dynarray
{
	Layout_Node **d;
	u64 length;
	u64 capacity;
	u64 item_size;
	Arena *arena;
} Layout_Node_Dynarray;

typedef struct bottom_bar
{
	// don't even try to make this a reusable widget
	App *app;
	float buttons_hover_t[3];
	Layout_Target drop_target_overlay;
} Bottom_Bar;

typedef struct app {
	Window *window;
	Layout_Node *layout_root;
	Layout_Node *focused_pane;
	Layout_Target drop_target_overlay;
	Bottom_Bar bottom_bar;
	Grab_State grab_state;
	i32 clock_svg;
	i32 timer_svg;
	i32 stopwatch_svg;
	i32 chime_wav;
	float icon_size;
	i32 n_clocks;
	i32 n_timers;
	i32 n_stopwatches;
} App;

App_Settings settings;
// Right now we use the global app to handle pane numbers("Timer 1"), drop target overlay, grab
// state, and focused panes.
App *g_app;

static i32 positive_modulo(i32 x, i32 m)
{
	return ((x % m) + m) % m;
}

bool in_rect(float x, float y, float rect_x, float rect_y, float rect_w, float rect_h)
{
	return x >= rect_x && x < rect_x + rect_w && y >= rect_y && y < rect_y + rect_h;
}

bool window_coords_in_scene(float x, float y, Scene *scene)
{
	Vector4 bounds = scene_get_window_bounds(scene);
	return x >= bounds.x && x < bounds.z && y >= bounds.y && y < bounds.w;
}

void add_centered_text(Scene *scene, i32 font_i, float size, char *text, u32 color)
{
	float w = measure_text_width(scene, font_i, size, text);
	float x = scene->w / 2.0f - w / 2.0f;
	add_text(scene, font_i, size, text, x, scene->h / 2.0f, color);
}

// XX
u32 my_blend_colors(u32 c1, u32 c2, float p)
{
	u8 c1a = c1 >> 24;
	u8 c1r = (c1 >> 16) & 0xff;
	u8 c1g = (c1 >> 8) & 0xff;
	u8 c1b = c1 & 0xff;
	u8 c2a = c2 >> 24;
	u8 c2r = (c2 >> 16) & 0xff;
	u8 c2g = (c2 >> 8) & 0xff;
	u8 c2b = c2 & 0xff;
	u32 a = c1a * (1-p) + floor(c2a*p);
	u32 r = c1r * (1-p) + floor(c2r*p);
	u32 g = c1g * (1-p) + floor(c2g*p);
	u32 b = c1b * (1-p) + floor(c2b*p);
	return (a << 24) | (r << 16) | (g << 8) | b;
}

void creep_towards(float *val, float target, float amount)
{
	if (*val < target) {
		*val = MIN(*val + amount, target);
	} else if (*val > target) {
		*val = MAX(*val - amount, target);
	}
}

Time_Fields split_time_fields(u64 micros)
{
	Time_Fields ret;
	ret.hours = micros / 3600000000;
	u64 rem = micros % 3600000000;
	ret.minutes = rem / 60000000;
	rem = rem % 60000000;
	ret.seconds = rem / 1000000;
	rem = rem % 1000000;
	ret.millis = rem / 1000;
	ret.micros = rem % 1000;

	return ret;
}

u64 combine_time_fields(Time_Fields fields)
{
	return (u64) fields.hours*3600000000 + (u64) fields.minutes*60000000
		   + (u64) fields.seconds * 1000000 + (u64) fields.millis*1000 + (u64) fields.micros;
}

/************************************** Timing basics *********************************************/

// Microseconds since Jan 1, 1601, UTC. TODO: use unix epoch?
u64 now_timestamp()
{
#ifdef _WIN32
	FILETIME ftime;
	GetSystemTimePreciseAsFileTime(&ftime);
	u64 t = ((u64) ftime.dwHighDateTime << 32) | ((u64) ftime.dwLowDateTime);
	return t / 10;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (u64) tv.tv_sec * 1000000 + (u64) tv.tv_usec;
#endif
}

bool format_micros(char *buf, i32 bufsize, u64 micros)
{
	u64 hours = micros / 3600000000;
	u64 rem = micros % 3600000000;
	i32 rem_minutes = rem / 60000000;
	rem = rem % 60000000;
	i32 rem_seconds = rem / 1000000;
	rem = rem % 1000000;
	i32 rem_millis = rem / 1000;

	i32 n = snprintf(buf, bufsize, "%02lld:%02d:%02d.%03d", hours, rem_minutes, rem_seconds,
		rem_millis);
	return n < bufsize;
}

/************************************** Misc widgets **********************************************/

bool play_pause_stop_button_dari(PPS_Button *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	bool hit = window_coords_in_scene(input->pointer_x, input->pointer_y, scene)
		&& !g_app->grab_state.active;
	creep_towards(&self->hover_t, hit ? 1.0f : 0.0f, input->anim_dt_s * 10.0f);
	// if (self->hover_t > 0) {
	// 	u32 alpha = 0x20*self->hover_t;
	// 	add_rounded_rectangle(scene, 0, 0, scene->w, scene->h, 10*dpi, (alpha << 24) | 0xffffff);
	// }
	u32 color = my_blend_colors(self->color, 0xffffffff, self->hover_t);
	// u32 color = self->color;

	float shape_h = scene->h * 0.75;
	switch (self->type) {
	case BUTTON_TYPE_PLAY: {
		// sideways equilateral triangle
		float side_length = shape_h;
		float height = side_length * sqrtf(3) / 2.0f;
		float x1 = (scene->w - height) / 2.0f;
		float y1 = (scene->h - side_length) / 2.0f;
		float x2 = x1;
		float y2 = y1 + side_length;
		float x3 = x1 + height;
		float y3 = y1 + side_length / 2.0f;
		add_triangle(scene, x1, y1, x2, y2, x3, y3, color);
	} break;
	case BUTTON_TYPE_PAUSE: {
		// two vertical rectangles
		float rect_w = shape_h / 4.0f;
		float comb_w = 3 * rect_w;
		float x1 = (scene->w - comb_w) / 2.0f;
		float y1 = (scene->h - shape_h) / 2.0f;
		float x2 = x1 + comb_w - rect_w;
		float y2 = y1;
		add_rectangle(scene, x1, y1, shape_h / 4.0f, shape_h, color);
		add_rectangle(scene, x2, y2, shape_h / 4.0f, shape_h, color);
	} break;
	case BUTTON_TYPE_STOP: {
		// one big square
		// ad hoc, the big square looks too big compared to the others, shave off 5%.
		// TODO: the square should really be a "reset" icon, or we could split buttons
		// into play/pause and stop/reset.
		shape_h -= 0.05 * shape_h;
		float x = (scene->w - shape_h) / 2.0f;
		float y = (scene->h - shape_h) / 2.0f;
		add_rectangle(scene, x, y, shape_h, shape_h, color);
	} break;
	default:
	break;
	}

	// were we pressed this frame?
	return hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT];
}

Vector2 time_input_get_size(Time_Input *self, Scene *parent)
{
	float dpi = parent->window->scale;
	Vector2 ret;
	Time_Fields t = split_time_fields(self->value);
	// Todo: show_frac...
	ret.x = measure_text_widthf(parent, self->font, self->font_size_unscaled*dpi, "%02d:%02d:%02d",
		t.hours, t.minutes, t.seconds);
	// Todo font metrics api
	ret.y = self->font_size_unscaled*dpi;
	return ret;
}

i32 character_index_at_x_offset(Scene *scene, i32 font, i32 font_size, char *text, float x)
{
	float pen_x = 0;
	i32 i = 0;
	while (pen_x < x && text[i]) {
		pen_x += measure_text_widthf(scene, font, font_size, "%c", text[i]);
		i++;
	}
	if (pen_x < x)
		return -1;
	else
		return i-1;
}

static i32 log_base10(i32 x)
{
	i32 ret = 0;
	while (x /= 10) ret++;
	return ret;
}

static i32 replace_base10_digit(i32 x, i32 digit_i, i32 digit_value)
{
	i32 pow10 = 1;
	for (i32 i=0; i<digit_i; i++) pow10 *= 10;
	i32 pow10plus = pow10 * 10;

	i32 d = (x % pow10plus) - (x % pow10);

	return x - d + digit_value*pow10;
}

void time_input_void_selection(Time_Input *self)
{
	self->field_selected = -1;
	self->cursor_j = -1;
	self->insert_mode = false;
}

bool time_input_draw_and_respond_input(Time_Input *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	i32 font_size = self->font_size_unscaled * dpi;
	Vector2 pointer = window_coords_to_scene(scene, input->pointer_x, input->pointer_y);

	if (!self->initialized) {
		self->field_selected = -1;
		self->cursor_j = -1;
		self->insert_mode = false;
		self->field_clicking = -1;
		self->field_dragging = -1;
		self->initialized = true;
	}

	Time_Fields t = split_time_fields(self->value);
	float est_width = measure_text_widthf(scene, self->font, self->font_size_unscaled*dpi,
		"%02d:%02d:%02d", t.hours, t.minutes, t.seconds);
	float x = (scene->w - est_width) / 2.0f;
	float y = scene->h / 2.0f;
	// fields[3] will be some conversion to int when show_fraction
	i32 fields[4] = { t.hours, t.minutes, t.seconds, 0 };
	// fields[2] will be "." when show_fraction
	char *seps[4] = { ":", ":", NULL, NULL};
	bool any_field_hit = false;
	bool text_input_consumed = false;
	bool kb_commands_consumed = false;
	i32 n_fields = 3;
	for (i32 i=0; i<n_fields; i++) {
		// First, handle mouse interactions and possible drag-updates to fields[i].
		i32 field_len = MAX(log_base10(fields[i]) + 1, 2);
		float field_w = measure_text_widthf(scene, self->font, font_size, "%02d", fields[i]);
		bool field_hit = in_rect(pointer.x, pointer.y, x, y-font_size, field_w, font_size);
		any_field_hit = any_field_hit || field_hit;
		bool hovered = (field_hit && !input->mouse_down[AU_MOUSE_BUTTON_LEFT]
			&& !g_app->grab_state.active) || self->field_dragging == i;
		creep_towards(&self->fields_hovered_t[i], hovered ? 1.0f : 0.0f, 10.0f*input->anim_dt_s);
		if (field_hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT] && !g_app->grab_state.active) {
			if (self->field_selected != i) {
				time_input_void_selection(self);
			}
			self->field_clicking = i;
			self->field_dragging = i;
			self->drag_start_y = pointer.y;
			self->drag_field_start_value = fields[i];
			g_app->grab_state.active = true;
			g_app->grab_state.starting = true;
		}
		if (self->field_clicking == i && !field_hit) {
			self->field_clicking = -1;
		}
		if (input->mouse_released[AU_MOUSE_BUTTON_LEFT]) {
			if (self->field_clicking == i) {
				// don't unselect on second click
				// if (self->field_selected == i)
				// 	self->field_selected = -1;
				// else
				self->field_selected = i;
				char field_text[20];
				// fractional part needs special formatting when implemented
				snprintf(field_text, 20, "%02d", fields[i]);
				self->cursor_j = character_index_at_x_offset(scene, self->font, font_size,
					field_text, pointer.x - x);
			}
			if (self->field_dragging == i) {
				self->field_dragging = -1;
				g_app->grab_state.active = false;
				g_app->grab_state.stopping = true;
			}
		}
		if (self->field_dragging == i) {
			float drag_px_per_val = 5.0f * dpi;
			fields[i] = roundf(self->drag_field_start_value
			                   + (self->drag_start_y - pointer.y) / drag_px_per_val);
			if (i == 1 || i == 2) { // hour or minutes wrap around
				fields[i] = positive_modulo(fields[i], 60);
			} else {
				i32 max = i == 0 ? 1000000000 : 1000;
				fields[i] = CLAMP(fields[i], 0, max-1);
				if (fields[i] == max || fields[i] == 0) {
					// so that you can go past the end, and get immediate changes coming back
					self->drag_field_start_value = fields[i];
					self->drag_start_y = pointer.y;
				}
			}
			// adjust cursor index for potential field length change
			if (self->field_selected == i) {
				i32 new_len = MAX(log_base10(fields[i]) + 1, 2);
				self->cursor_j += (new_len - field_len);
			}
		}

		// Handle keyboard updates to fields[i]
		if (!text_input_consumed && self->field_selected == i && self->cursor_j >= 0
			&& input->text_entered->length > 0) {
			// prevent bug where we move cursor to next field, and it consumes input too.
			text_input_consumed = true;
			for (u64 entered_i=0; entered_i<input->text_entered->length; entered_i++) {
				char c = input->text_entered->d[entered_i];
				if (c >= '0' && c <= '9') {
					i32 len = field_len;
					i32 field_new;
					if (self->insert_mode) { // "insert mode"
						if (len < 9) // prevent overflow by only allowing 9 digits.
							field_new = fields[i]*10 + c - '0';
						else
							field_new = fields[i];
					} else {
						field_new = replace_base10_digit(fields[i], len - 1 - self->cursor_j,
							c - '0');
					}
					if (field_new < 60 || (i != 1 && i != 2)) {
						fields[i] = field_new;
						// This leftmost 0 is disappearing, so don't advance cursor
						bool leftmost0_skip = i == 0 && len > 2 && self->cursor_j == 0 && c == '0';
						if (self->cursor_j < len - 1 && !leftmost0_skip) {
							self->cursor_j++;
						} else if (self->field_selected == 1) {
							self->field_selected++;
							self->cursor_j = 0;
						} else if (self->field_selected == 0 && !leftmost0_skip) {
							if (fields[i])
								self->insert_mode = true;
							else { // xx we've entered a zero, don't insert
								self->field_selected++;
								self->cursor_j = 0;
							}
						}
					}
				}
			}
		} else if (!kb_commands_consumed && self->field_selected == i && self->cursor_j >= 0) {
			kb_commands_consumed = true;
			// xx Handle other keys after all text input this frame, which technically could be
			// incorrect, because e.g. text that comes in post-enter should be ignored.
			if (input->key_pressed[AU_KEY_ENTER]) {
				time_input_void_selection(self);
			} else if (input->key_pressed[AU_KEY_ESCAPE]) {
				fields[i] = 0;
				self->cursor_j = CLAMP(self->cursor_j, 0, 1);
				self->insert_mode = false;
			} else if (input->key_pressed[AU_KEY_LEFT]) {
				if (self->cursor_j > 0)
					self->cursor_j--;
				else if (i != 0) {
					i32 left_field_len = MAX(log_base10(fields[i-1]) + 1, 2);
					self->field_selected = i-1;
					self->cursor_j = left_field_len-1;
				}
			} else if (input->key_pressed[AU_KEY_RIGHT]) {
				if (!self->insert_mode && self->cursor_j < field_len - 1) {
					self->cursor_j++;
				} else if (i < n_fields-1) {
					self->field_selected++;
					self->cursor_j = 0;
					self->insert_mode = false;
				}
			}
		}

		// fields[i] is now finalized, let's draw it.
		i32 field_n = fields[i];

		if (self->fields_hovered_t[i] > 0.0f || self->field_selected == i) {
			float off_baseline = font_size*0.1f;
			float margin_x = font_size*0.125f;
			bool sel = self->field_selected == i;
			u32 alpha = sel ? 0x30 : 0x20*self->fields_hovered_t[i];
			// todo again, font metrics api
			add_rounded_rectangle(scene, x-margin_x, y-font_size+off_baseline, field_w+2*margin_x,
				font_size, 10*dpi, (alpha << 24) | 0xffffff);
		}

		i8 field_digits[15] = { 0, 0 };
		i8 field_digits_length = 0;
		while (field_n) {
			field_digits[field_digits_length++] = field_n % 10;
			field_n /= 10;
		}
		field_digits_length = MAX(field_digits_length, 2);
		for (i32 j=0; j < field_digits_length; j++) {
			i8 digit = field_digits[field_digits_length-1-j];
			u32 char_color = self->text_color;
			if (self->field_dragging < 0 && self->field_selected == i && !self->insert_mode
				&& self->cursor_j == j) {
				char_color = my_blend_colors(char_color, 0xff000000, 0.25f);
			}
			float adv;
			add_character(scene, self->font, font_size, '0' + digit, x, y, char_color, &adv);
			x += adv;
		}
		if (self->field_dragging < 0 && self->field_selected == i && self->insert_mode) {
			// insert mode cursor
			u32 cursor_color = my_blend_colors(self->text_color, 0xff000000, 0.25f);
			// TODO: bad and font dependent
			add_rectangle(scene, x - 1*dpi, y - font_size*0.75, 2*dpi, font_size*0.75,
				cursor_color);
		}
		if (seps[i]) {
			float adv;
			add_character(scene, self->font, font_size, seps[i][0], x, y, self->text_color, &adv);
			x += adv;
		}
	}
	if (!any_field_hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]) {
		time_input_void_selection(self);
	}
	// reincorporate changes
	u64 new_value = combine_time_fields(
		(Time_Fields) {fields[0], fields[1], fields[2], fields[3], t.micros }
	);
	bool changed = new_value != self->value;
	self->value = new_value;
	return changed;
}

/************************************** Clock *****************************************************/

bool get_formatted_current_time(char *buf, i32 bufsize)
{
#ifdef _WIN32
	// TODO unix
	// xx for sub-millisecond precision, could use timer path(GetSystemTimePreciseAsFileTime)
	// and convert
	SYSTEMTIME loctime;
	GetLocalTime(&loctime);
	// TODO option somehow for 24-hour time
	char *ampm;
	if (loctime.wHour >= 12) {
		if (loctime.wHour > 12)
			loctime.wHour -= 12;
		ampm = "PM";
	} else {
		if (loctime.wHour == 0) {
			loctime.wHour = 12;
		}
		ampm = "AM";
	}
	i32 n = snprintf(buf, bufsize, "%02d:%02d:%02d %s", loctime.wHour, loctime.wMinute,
		loctime.wSecond, ampm);
	return n < bufsize;
#else
	time_t t;
	time(&t);
	struct tm *tm = localtime(&t);
	size_t n = strftime(buf, bufsize, "%I:%M:%S %p", tm);
	return n < bufsize;
#endif
}

void clock_draw_and_respond_input(Clock *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	Vector2 pointer = window_coords_to_scene(scene, input->pointer_x, input->pointer_y);

	bool hit = in_rect(pointer.x, pointer.y, 0, 0, scene->w, scene->h) && !g_app->grab_state.active;
	if (hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]) {
		g_app->focused_pane = self->node;
	}

	// float name_size = settings.name_font_size_unscaled*dpi;
	// // Todo: putting baseline at y + font_size could cause overflow, need font metrics api
	// // TODO: don't allow name to intersect content
	// add_text(scene, settings.font, name_size, self->name.d, 10*dpi, name_size + 10*dpi,
	// 	settings.font_color);

	char timebuf[20];
	bool suc = get_formatted_current_time(timebuf, 20);
	assertf(suc, NULL);
	add_centered_text(scene, settings.font, settings.font_size_unscaled*dpi, timebuf,
		settings.font_color);
}

/************************************** Timer *****************************************************/


void timer_draw_and_respond_input(Timer *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	Vector2 pointer = window_coords_to_scene(scene, input->pointer_x, input->pointer_y);

	if (!self->initialized) {
		self->interval = 60000000;
		self->start_value = self->interval;
		self->state = TIMER_STATE_READY;
		self->play_pause.type = BUTTON_TYPE_PLAY;
		self->play_pause.color = 0xffd0d0d0;
		self->stop_button.type = BUTTON_TYPE_STOP;
		self->stop_button.color = 0xffd0d0d0;
		self->time_input.font = settings.font;
		self->time_input.font_size_unscaled = settings.font_size_unscaled;
		self->time_input.text_color = settings.font_color;
		self->initialized = true;
	}

	bool hit = in_rect(pointer.x, pointer.y, 0, 0, scene->w, scene->h) && !g_app->grab_state.active;
	if (hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]) {
		g_app->focused_pane = self->node;
	}

	if (self->flash_red) {
		float flash_red_frequency = 1.2;
		float phase_max = 0.9;
		float phase = fmod(self->flash_red, 1.0f);
		float brightness = phase > phase_max ? (1 - phase) / (1 - phase_max)
											 : phase / phase_max;
		u32 flash_color = my_blend_colors(settings.background_color, 0xff783a39, brightness);
		add_rectangle(scene, 0, 0, scene->w, scene->h, flash_color);
		self->flash_percent = brightness;
		creep_towards(&self->flash_red, 0.0f, input->anim_dt_s*flash_red_frequency);
	}

	// float name_size = settings.name_font_size_unscaled*dpi;
	// // Todo: putting baseline at y + font_size could cause overflow, need font metrics api
	// // TODO: don't allow name to intersect content
	// add_text(scene, settings.font, name_size, self->name.d, 10*dpi, name_size + 10*dpi,
	// 	settings.font_color);

	u64 now = now_timestamp();
	i64 elapsed_since_start = self->state == TIMER_STATE_RUNNING ? (now - self->start_time) : 0LL;
	u64 remaining = MAX((i64) self->start_value - elapsed_since_start, 0LL);
	if (self->state != TIMER_STATE_READY) {
		char timebuf[30];
		bool suc = format_micros(timebuf, 30, remaining);
		assertf(suc, NULL);
		// y == scene->h / 2.0f
		add_centered_text(scene, settings.font, settings.font_size_unscaled*dpi, timebuf,
			settings.font_color);
	} else {
		// xx for now, time_input sizes and places itself in the parent scene, so that we can
		// guarantee its text baseline is the same as above. Could of course just remove the above
		// path and make it one widget for ready, running, etc.
		self->time_input.value = self->interval;
		time_input_draw_and_respond_input(&self->time_input, scene, input);
		self->interval = self->time_input.value;
		self->start_value = self->interval;
	}

	float button_gap = 20*dpi;

	float play_pause_w = 40*dpi;
	float play_pause_h = 40*dpi;
	float play_pause_x = scene->w / 2.0f - button_gap / 2.0f - play_pause_w;
	float play_pause_y = scene->h / 2.0f + 25*dpi;
	Scene play_pause_scene = make_child_scene(scene, play_pause_x, play_pause_y, play_pause_w,
		play_pause_h);
	bool play_pause_pressed = play_pause_stop_button_dari(&self->play_pause, &play_pause_scene,
		input);

	float stop_button_w = 40*dpi;
	float stop_button_h = 40*dpi;
	float stop_button_x = scene->w / 2.0f + button_gap / 2.0f;
	float stop_button_y = play_pause_y;
	Scene stop_button_scene = make_child_scene(scene, stop_button_x, stop_button_y, stop_button_w,
		stop_button_h);
	bool stop_pressed = play_pause_stop_button_dari(&self->stop_button, &stop_button_scene, input);

	if (play_pause_pressed) {
		switch (self->state) {
		case TIMER_STATE_READY:
			self->start_value = self->interval;
		case TIMER_STATE_PAUSED: {
			self->state = TIMER_STATE_RUNNING;
			self->play_pause.type = BUTTON_TYPE_PAUSE;
			self->start_time = now;
		} break;
		case TIMER_STATE_RUNNING: {
			self->state = TIMER_STATE_PAUSED;
			self->play_pause.type = BUTTON_TYPE_PLAY;
			self->start_value = remaining;
		} break;
		// don't allow "play" on expired timers.
		default:
		break;
	}
	} else if (stop_pressed) {
		self->state = TIMER_STATE_READY;
		self->start_value = self->interval;
		self->play_pause.type = BUTTON_TYPE_PLAY;
	} else if (self->state == TIMER_STATE_RUNNING && remaining <= 0) {
		// beep! beep!
		self->flash_red = 3.0f;
		self->state = TIMER_STATE_EXPIRED;
		self->start_value = 0;
		self->play_pause.type = BUTTON_TYPE_PLAY;
		au_window_play_sound(scene->window, g_app->chime_wav);
	}
}

void timer_run_in_background(Timer *self, double anim_dt_s)
{
	if (self->flash_red) {
		float flash_red_frequency = 1.2;
		float phase_max = 0.9;
		float phase = fmod(self->flash_red, 1.0f);
		float brightness = phase > phase_max ? (1 - phase) / (1 - phase_max)
											 : phase / phase_max;
		u32 flash_color = my_blend_colors(settings.background_color, 0xff783a39, brightness);
		// clear_background(scene, flash_color);
		self->flash_percent = brightness;
		creep_towards(&self->flash_red, 0.0f, anim_dt_s*flash_red_frequency);
	} else {
		self->flash_percent = 0;
	}

	u64 now = now_timestamp();
	i64 elapsed_since_start = self->state == TIMER_STATE_RUNNING ? (now - self->start_time) : 0LL;
	u64 remaining = MAX((i64) self->start_value - elapsed_since_start, 0LL);
	if (self->state == TIMER_STATE_RUNNING && remaining <= 0) {
		// beep! beep!
		self->flash_red = 3.0f;
		self->state = TIMER_STATE_EXPIRED;
		self->start_value = 0;
		self->play_pause.type = BUTTON_TYPE_PLAY;
		au_window_play_sound(g_app->window, g_app->chime_wav);
	}
}

/************************************** Stopwatch *************************************************/

void stopwatch_draw_and_respond_input(Stopwatch *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	Vector2 pointer = window_coords_to_scene(scene, input->pointer_x, input->pointer_y);

	if (!self->initialized) {
		self->play_pause.type = BUTTON_TYPE_PLAY;
		self->play_pause.color = 0xffd0d0d0;
		self->stop_button.type = BUTTON_TYPE_STOP;
		self->stop_button.color = 0xffd0d0d0;
		self->initialized = true;
	}

	bool hit = in_rect(pointer.x, pointer.y, 0, 0, scene->w, scene->h) && !g_app->grab_state.active;
	if (hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]) {
		g_app->focused_pane = self->node;
	}

	// float name_size = settings.name_font_size_unscaled*dpi;
	// // Todo: putting baseline at y + font_size could cause overflow, need font metrics api
	// // TODO: don't allow name to intersect content
	// add_text(scene, settings.font, name_size, self->name.d, 10*dpi, name_size + 10*dpi,
	// 	settings.font_color);

	u64 now = now_timestamp();
	u64 elapsed = self->start_value + (self->running ? now - self->start_time : 0);

	char timebuf[30];
	bool suc = format_micros(timebuf, 30, elapsed);
	assertf(suc, NULL);
	add_centered_text(scene, settings.font, settings.font_size_unscaled*dpi, timebuf,
		settings.font_color);

	float button_gap = 20*dpi;

	float play_pause_w = 40*dpi;
	float play_pause_h = 40*dpi;
	float play_pause_x = scene->w / 2.0f - button_gap / 2.0f - play_pause_w;
	float play_pause_y = scene->h / 2.0f + 25*dpi;
	Scene play_pause_scene = make_child_scene(scene, play_pause_x, play_pause_y, play_pause_w,
		play_pause_h);
	bool play_pressed = play_pause_stop_button_dari(&self->play_pause, &play_pause_scene, input);

	float stop_button_w = 40*dpi;
	float stop_button_h = 40*dpi;
	float stop_button_x = scene->w / 2.0f + button_gap / 2.0f;
	float stop_button_y = play_pause_y;
	Scene stop_button_scene = make_child_scene(scene, stop_button_x, stop_button_y, stop_button_w,
		stop_button_h);
	bool stop_pressed = play_pause_stop_button_dari(&self->stop_button, &stop_button_scene, input);

	if (play_pressed) {
		self->running = !self->running;
		if (self->running) {
			self->start_time = now;
		} else {
			self->start_value = elapsed;
		}
		self->play_pause.type = self->running ? BUTTON_TYPE_PAUSE : BUTTON_TYPE_PLAY;
	} else if (stop_pressed) {
		self->running = false;
		self->start_value = 0;
		self->play_pause.type = BUTTON_TYPE_PLAY;
	}
}

/************************************** Layout ****************************************************/

// Tab group to add panes to if no pane is currently focused.
Layout_Node *get_default_focused_tab_group(Layout_Node *root)
{
	switch (root->type) {
	case NODE_TYPE_CLOCK:
	case NODE_TYPE_TIMER:
	case NODE_TYPE_STOPWATCH:
		return NULL;
	break;
	case NODE_TYPE_TAB_GROUP:
		return root;
	break;
	case NODE_TYPE_SPLIT_GROUP: {
		Split_Group *split_group = root->content.split_group;
		if (split_group->children->length > 0) {
			return get_default_focused_tab_group(split_group->children->d[0]);
		} else {
			return NULL;
		}
	} break;
	default:
		return NULL;
	break;
	}
}

Layout_Target get_layout_target_for_point_recurse(Layout_Node *node, float w, float h, float x,
	float y)
{
	float dpi = g_app->window->scale;
	Layout_Target null_target = { NULL, 0, 0 };
	switch (node->type) {
	case NODE_TYPE_CLOCK:
	case NODE_TYPE_STOPWATCH:
	case NODE_TYPE_TIMER:
	return null_target;
	break;
	case NODE_TYPE_TAB_GROUP:
		float top_bar_h = settings.top_bar_h_unscaled * dpi;
		if (y < top_bar_h) {
			return (Layout_Target) { node, 0, 0, 10e9 };
		} else {
			Tab_Group *tabs = node->content.tab_group;
			if (tabs->active_tab)
				return null_target;
			Layout_Node *child = tabs->children->d[tabs->active_tab];
			return get_layout_target_for_point_recurse(child, w, h-top_bar_h, x, y-top_bar_h);
		}
	break;
	case NODE_TYPE_SPLIT_GROUP:
		Split_Group *split = node->content.split_group;
		switch (split->type) {
		case SPLIT_HORIZONTAL: {
			side_e side = x < w - x ? SIDE_LEFT : SIDE_RIGHT;
			float dist = MIN(x, w - x);
			float my_score = 10000.0f - dist;
			Layout_Target my_target = { node, -1, side, my_score };
			float child_x = 0;
			// todo splitter width
			float children_total_w = w - (split->children->length - 1.0f) * 1.0f;
			for (u64 i=0; i<split->children->length; i++) {
				Layout_Node *child = split->children->d[i];
				float child_proportion = split->child_sizes->d[i];
				float child_w = child_proportion*children_total_w;
				if (x >= child_x && x < child_x + child_w) {
					// If the child is not a vertical split group, propose adding one
					if (!(child->type == NODE_TYPE_SPLIT_GROUP
						&& child->content.split_group->type == SPLIT_VERTICAL)) {
						float dist = MIN(y, h-y);
						float score = 10000.0f - dist;
						if (score > my_score) {
							side_e side = y < h - y ? SIDE_TOP : SIDE_BOTTOM;
							my_target = (Layout_Target) { node, i, side, score };
						}
					}
					Layout_Target child_target = get_layout_target_for_point_recurse(child,
						child_w, h, x - child_x, y);
					if (child_target.score > my_score)
						return child_target;
					else
						return my_target;
				}
				child_x += child_w + (i<split->children->length-1 ? 1 : 0);
			}
			// xx ?
			return my_target;
		} break;
		case SPLIT_VERTICAL: {
			side_e side = y < h - y ? SIDE_TOP : SIDE_BOTTOM;
			float dist = MIN(y, h - y);
			float my_score = 10000.0f - dist;
			Layout_Target my_target = { node, -1, side, my_score };
			float child_y = 0;
			// todo splitter width
			float children_total_h = h - (split->children->length - 1.0f) * 1.0f;
			for (u64 i=0; i<split->children->length; i++) {
				Layout_Node *child = split->children->d[i];
				float child_proportion = split->child_sizes->d[i];
				float child_h = child_proportion*children_total_h;
				if (y >= child_y && y < child_y + child_h) {
					// If the child is not a horizontal split group, propose adding one
					if (!(child->type == NODE_TYPE_SPLIT_GROUP
						&& child->content.split_group->type == SPLIT_HORIZONTAL)) {
						float dist = MIN(x, w-y);
						float score = 10000.0f - dist;
						if (score > my_score) {
							side_e side = x < w - x ? SIDE_LEFT : SIDE_RIGHT;
							my_target = (Layout_Target) { node, i, side, score };
						}
					}
					Layout_Target child_target = get_layout_target_for_point_recurse(child,
						w, child_h, x, y - child_y);
					if (child_target.score > my_score)
						return child_target;
					else return my_target;
				}
				child_y += child_h + (i<split->children->length-1 ? 1 : 0);
			}
			return my_target;
		} break;
		}
	break;
	default:
	break;
	}
	return null_target;
}

Layout_Target get_layout_target_for_point(float x, float y)
{
	Window *window = g_app->window;
	return get_layout_target_for_point_recurse(g_app->layout_root, window->width, window->height,
		x, y);
}

void tab_group_add_pane(Layout_Node *tabs, Layout_Node *pane);
void split_group_add_node(Layout_Node *split, Layout_Node *node, i32 index, float size);
Layout_Node *create_tab_group_node(Arena *arena);
Layout_Node *create_split_group_node(Arena *arena, split_e type);

void add_pane_to_target(Layout_Node *pane, Layout_Target target)
{
	Layout_Node *node = target.node;
	if (!node)
		return;
	switch (node->type) {
	case NODE_TYPE_CLOCK:
	case NODE_TYPE_TIMER:
	case NODE_TYPE_STOPWATCH:
		return;
	break;
	case NODE_TYPE_TAB_GROUP: {
		tab_group_add_pane(node, pane);
	} break;
	case NODE_TYPE_SPLIT_GROUP: {
		Split_Group *split = node->content.split_group;
		if (target.child >= 0) {
			// child >= 0 means we're splitting one of the children of target.node, not just
			// adding a child to target.node. Split the specified child by removing it, adding a
			// split group in its place, and readding it along with the new node.
			Layout_Node *child = split->children->d[target.child];
			split_e split_type = split->type == SPLIT_HORIZONTAL ? SPLIT_VERTICAL
			: SPLIT_HORIZONTAL;
			Layout_Node *new_split = create_split_group_node(&app_arena, split_type);
			split_group_add_node(new_split, child, 0, 1.0f);

			// as below
			Layout_Node *tabs = create_tab_group_node(&app_arena);
			tab_group_add_pane(tabs, pane);
			i32 index = target.side == SIDE_LEFT || target.side == SIDE_TOP ? 0
				: 1;
			split_group_add_node(new_split, tabs, index, 0.5f);

			split->children->d[target.child] = new_split;
			new_split->parent = node;
		} else {
			Layout_Node *tabs = create_tab_group_node(&app_arena);
			tab_group_add_pane(tabs, pane);
			i32 index = target.side == SIDE_LEFT || target.side == SIDE_TOP ? 0
				: split->children->length;
			split_group_add_node(node, tabs, index, 1.0f / (split->children->length+1));
		}
	} break;
	}
}

void destroy_pane(Arena *arena, Layout_Node *pane)
{
	if (g_app->focused_pane == pane)
		g_app->focused_pane = NULL;
	afree(arena, pane->content.clock->name.d);
	afree(arena, (void *) pane->content.clock);
	afree(arena, pane);
}

Layout_Node *create_pane_with_default_name(Arena *arena, node_type_e type)
{
	Layout_Node *res = adalloc(arena, sizeof(Layout_Node));
	char *name = adalloc(arena, 20);
	res->type = type;
	switch (type) {
		case NODE_TYPE_CLOCK:
		snprintf(name, 20, "Clock %d", ++g_app->n_clocks);
		res->content.clock = adalloc(arena, sizeof(Clock));
		res->content.clock->name = string_from(name);
		res->content.clock->node = res;
		break;
		case NODE_TYPE_TIMER:
		snprintf(name, 20, "Timer %d", ++g_app->n_timers);
		res->content.timer = adalloc(arena, sizeof(Timer));
		res->content.timer->name = string_from(name);
		res->content.timer->node = res;
		break;
		case NODE_TYPE_STOPWATCH:
		snprintf(name, 20, "Stopwatch %d", ++g_app->n_stopwatches);
		res->content.stopwatch = adalloc(arena, sizeof(Stopwatch));
		res->content.stopwatch->name = string_from(name);
		res->content.stopwatch->node = res;
		break;
		default:
		afree(arena, res);
		afree(arena, name);
		return NULL;
		break;
		}
	return res;
}

Tab_Group *create_tab_group(Arena *arena)
{
	Tab_Group *res = adalloc(arena, sizeof(Tab_Group));
	res->children = (Layout_Node_Dynarray *) new_dynarray(arena, sizeof(Layout_Node *));
	res->active_tab = -1;
	res->scroll_x = 0;
	return res;
}

Layout_Node *create_tab_group_node(Arena *arena)
{
	Layout_Node *res = adalloc(arena, sizeof(Layout_Node));
	res->type = NODE_TYPE_TAB_GROUP;
	res->content.tab_group = create_tab_group(arena);
	res->content.tab_group->node = res;
	return res;
}

void destroy_tab_group(Arena *arena, Layout_Node *node)
{
	Tab_Group *tabs = node->content.tab_group;
	// destroy_dynarray(tabs->children)
	afree(arena, tabs);
	afree(arena, node);
}

void tab_group_add_pane(Layout_Node *tabs, Layout_Node *pane)
{
	assertf(tabs->type == NODE_TYPE_TAB_GROUP, NULL);
	Tab_Group *self = tabs->content.tab_group;
	dynarray_add(self->children, &pane);
	pane->parent = tabs;
	if (self->active_tab < 0)
		self->active_tab = 0;
}

// feels dumb
String pane_name(Layout_Node *p)
{
	switch (p->type) {
	case NODE_TYPE_CLOCK:
		return p->content.clock->name;
	break;
	case NODE_TYPE_TIMER:
		return p->content.timer->name;
	break;
	case NODE_TYPE_STOPWATCH:
		return p->content.stopwatch->name;
	break;
	default:
		return NULLSTRING;
	break;
	}
}

void tab_group_swap_children(Tab_Group *self, i32 i, i32 j)
{
	Layout_Node *child_i = self->children->d[i];
	self->children->d[i] = self->children->d[j];
	self->children->d[j] = child_i;
}

i32 split_group_remove_node(Layout_Node *split, Layout_Node *node);

void tab_group_draw_and_respond_input(Tab_Group *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	Vector2 pointer = window_coords_to_scene(scene, input->pointer_x, input->pointer_y);

	float top_bar_h = 40*dpi;
	float tab_h = top_bar_h - 5*dpi;
	float min_tab_w = 150*dpi;
	i32 font = settings.font;
	float font_size = settings.name_font_size_unscaled*dpi;
	float text_y = (top_bar_h + font_size) / 2.0f + 2.0f;
	float name_margin_left = 15*dpi;
	float name_margin_right = 30*dpi;
	float x_button_size = settings.x_button_size_unscaled*dpi;
	float x_button_margin = 8*dpi;
	float tab_x_button_y = top_bar_h - tab_h + (tab_h - x_button_size) / 2.0f + 2.0f*dpi;
	float solo_x_button_y = (top_bar_h - x_button_size) / 2.0f + 2.0f*dpi;
	u32 x_button_hover_color = 0xffe0e0e0;
	u32 x_button_nohover_color = 0xffa0a0a0;
	i32 tab_to_remove = -1;
	bool kill_tab_to_remove = false;
	float active_tab_slot_x;
	float active_tab_w = -1;
	float active_tab_left_w = -1;
	float active_tab_right_w = -1;
	if (self->children->length > 1) {
		// Draw the inactive tabs, record position and width of active tab, run inactive timers,
		// handle clicks and hovers on inactive tabs.
		float x = 0;
		add_rectangle(scene, 0, 0, scene->w, top_bar_h, settings.tab_bar_color);
		for (u64 i=0; i<self->children->length; i++) {
			Layout_Node *child = self->children->d[i];
			String name = pane_name(child);
			float name_w = measure_text_width(scene, font, font_size, name.d);
			float tab_w = MAX(name_w + name_margin_left + name_margin_right + x_button_size
				+ x_button_margin, min_tab_w);
			float tab_right = x + tab_w;
			bool hit = in_rect(pointer.x, pointer.y, x, 0, tab_w, top_bar_h);

			float x_button_x = tab_right - x_button_size - x_button_margin;
			// float x_button_y = top_bar_h - 12.5*dpi - x_button_size/2.0f;
			bool x_button_hit = in_rect(pointer.x, pointer.y, x_button_x, tab_x_button_y,
				x_button_size, x_button_size);

			if (x_button_hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]
				&& !g_app->grab_state.active) {
				tab_to_remove = i;
				kill_tab_to_remove = true;
			} else if (hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]
				&& !g_app->grab_state.active) {
				self->active_tab = i;
				self->active_tab_dragging = true;
				self->drag_start_pointer_x = pointer.x;
				self->drag_start_tab_x = x;
				g_app->grab_state.active = true;
				g_app->grab_state.starting = true;
				g_app->focused_pane = self->children->d[i];
			}

			// save these for the swap calculation below
			if (self->active_tab == i + 1)
				active_tab_left_w = tab_w;
			else if (self->active_tab == i - 1)
				active_tab_right_w = tab_w;

			if (self->active_tab == i) {
				active_tab_slot_x = x;
				active_tab_w = tab_w;
			} else if (i != self->children->length - 1 && i+1 != self->active_tab) {
				add_rectangle(scene, tab_right, top_bar_h-22*dpi, 1*dpi, 15*dpi, 0xffa0a0a0);
			}
			if (i != self->active_tab) {
				if (child->type == NODE_TYPE_TIMER) {
					Timer *timer = child->content.timer;
					timer_run_in_background(timer, input->anim_dt_s);
					if (timer->flash_percent) {
						u32 color = my_blend_colors(settings.background_color, 0xff783a39,
							timer->flash_percent);
						add_rectangle(scene, x, top_bar_h - tab_h, tab_w, tab_h, color);
					}
				}
				add_text(scene, font, font_size, name.d, x + name_margin_left, text_y,
					0xffffffff);

				u32 button_color = x_button_hit && !g_app->grab_state.active ? x_button_hover_color
					: x_button_nohover_color;
				add_image_with_color(scene, settings.x_button, x_button_x, tab_x_button_y,
					button_color);
			}
			x = tab_right;
			// Todo: will need to run some checks on tabs even if not showing them
			if (x > scene->w)
				break;
		}
	} else if (self->children->length == 1) {
		bool hit = in_rect(pointer.x, pointer.y, 0, 0, scene->w, top_bar_h);
		if (hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT] && !g_app->grab_state.active) {
			self->active_tab_dragging = true;
			self->drag_start_pointer_x = pointer.x;
			g_app->grab_state.active = true;
			g_app->grab_state.starting = true;
		}
	}

	if (self->active_tab_dragging && input->mouse_released[AU_MOUSE_BUTTON_LEFT]) {
		self->active_tab_dragging = false;
		g_app->grab_state.active = false;
		g_app->grab_state.stopping = true;
	}

	if (self->active_tab >= 0) {
		// Draw the active tab(header), handle clicks hovers, and drags, including drags out of the
		// tab group.
		bool solo = self->children->length == 1;
		float x = solo ? 0 : active_tab_slot_x;
		Layout_Node *child = self->children->d[self->active_tab];
		String name = pane_name(child);
		float flash_percent = 0.0f;
		if (child->type == NODE_TYPE_TIMER) {
			flash_percent = child->content.timer->flash_percent;
		}
		if (solo) {
			// put x at the right of the pane, TODO: it doesn't work yet, should delete whole
			// tab group.
			active_tab_w = scene->w;
			// float name_w = measure_text_width(scene, font, font_size, name.d);
			// active_tab_w = name_w + name_margin_left + name_margin_right + x_button_size
			// 	+ x_button_margin;
			if (in_rect(pointer.x, pointer.y, 0, 0, scene->w, top_bar_h)
				&& !g_app->grab_state.active) {
				// u32 top_bar_hl_color = my_blend_colors(settings.background_color,
				// 	settings.tab_bar_color, 0.75);
				u32 top_bar_color;
				if (flash_percent == 0.0f)
					top_bar_color = my_blend_colors(settings.background_color,
						0xffffffff, 0.08f);
				else
					top_bar_color = my_blend_colors(settings.background_color,
						0xff783a39, flash_percent);
				add_rectangle(scene, 0, 0, scene->w, top_bar_h, top_bar_color);
			}
		}
		float slot_right = x + active_tab_w;

		if (self->active_tab_dragging) {
			if (in_rect(pointer.x, pointer.y, 0, 0, scene->w, top_bar_h)) {
				if (self->active_tab_dragging_out) {
					g_app->drop_target_overlay.node = NULL;
				}
				self->active_tab_dragging_out = false;
			} else {
				self->active_tab_dragging_out = true;
			}
		}
		if (self->active_tab_dragging_out) {
			Layout_Target target = get_layout_target_for_point(input->pointer_x, input->pointer_y);
			if (input->mouse_released[AU_MOUSE_BUTTON_LEFT]) {
				tab_to_remove = self->active_tab;
				self->active_tab_dragging = false;
				self->active_tab_dragging_out = false;
				add_pane_to_target(self->children->d[self->active_tab], target);
				g_app->drop_target_overlay.node = NULL;
			} else {
				g_app->drop_target_overlay = target;
			}
		}

		if (self->active_tab_dragging && !solo && !self->active_tab_dragging_out) {
			x = self->drag_start_tab_x + pointer.x - self->drag_start_pointer_x;
			float tab_swap_threshold = 0.6f;
			// Swap condition: are we closer to our current spot, or where we would be if we
			// swapped? (this is a good condition to avoid flipping back and forth).
			if (self->active_tab < self->children->length - 1 &&
				ABS(x - active_tab_slot_x) > ABS(x - (active_tab_slot_x + active_tab_right_w))) {
				tab_group_swap_children(self, self->active_tab, self->active_tab + 1);
				self->active_tab++;
			} else if (self->active_tab > 0
				&& ABS(x - active_tab_slot_x) > ABS(x - (active_tab_slot_x - active_tab_left_w))) {
				tab_group_swap_children(self, self->active_tab, self->active_tab - 1);
				self->active_tab--;
			}
		}
		if (!solo) {
			float tab_right = x + active_tab_w;
			float tab_top = top_bar_h - tab_h;
			Vector2 corners[4] = {
				{ x, tab_top},
				{ tab_right, tab_top },
				{ tab_right, top_bar_h },
				{ x, top_bar_h }
			};
			bool rounded[4] = { true, true, false, false };
			u32 color = settings.background_color;
			if (flash_percent != 0.0f)
				color = my_blend_colors(color, 0xff783a39, flash_percent);
			add_rounded_quad(scene, corners, rounded, 8*dpi, color);
		}

		add_text(scene, font, font_size, name.d, x + name_margin_left, text_y, 0xffffffff);

		float button_x = x + active_tab_w - x_button_size - x_button_margin;
		float x_button_y = solo ? solo_x_button_y : tab_x_button_y;
		bool button_hit = in_rect(pointer.x, pointer.y, button_x, x_button_y, x_button_size,
			x_button_size);
		// if (button_hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT])
		// 	x_button_pressed = self->active_tab;
		u32 button_color = button_hit && !g_app->grab_state.active ? x_button_hover_color
			: x_button_nohover_color;
		add_image_with_color(scene, settings.x_button, button_x, x_button_y, button_color);
	}

	// Remove a tab if it's deleted or moved out of the tab group
	// xx Could inform the tab that it's being killed, and then run one last goodbye frame...
	if (tab_to_remove >= 0) {
		if (kill_tab_to_remove)
			destroy_pane(&app_arena, self->children->d[tab_to_remove]);
		dynarray_remove(self->children, tab_to_remove);
		if (tab_to_remove < self->active_tab) {
			self->active_tab--;
		} else if (tab_to_remove == self->active_tab) {
			if (self->active_tab >= self->children->length) {
				self->active_tab--;
			}
		}
		if (self->children->length == 0) {
			Layout_Node *node = self->node;
			split_group_remove_node(node->parent, node);
			destroy_tab_group(&app_arena, node);
			return;
		}
	}

	// Pass control the active pane
	Scene child_scene = make_child_scene(scene, 0, top_bar_h, scene->w, scene->h - top_bar_h);

	if (self->active_tab >= 0) {
		if (input->key_pressed[AU_KEY_TAB]) {
			self->active_tab = positive_modulo(self->active_tab + 1, self->children->length);
		}
		if (input->key_pressed[AU_KEY_BACKSPACE]) {
			self->active_tab = positive_modulo(self->active_tab - 1, self->children->length);
		}

		Layout_Node *active_pane = self->children->d[self->active_tab];
		switch (active_pane->type) {
		case NODE_TYPE_CLOCK:
			clock_draw_and_respond_input(active_pane->content.clock, &child_scene, input);
		break;
		case NODE_TYPE_TIMER:
			timer_draw_and_respond_input(active_pane->content.timer, &child_scene, input);
		break;
		case NODE_TYPE_STOPWATCH:
			stopwatch_draw_and_respond_input(active_pane->content.stopwatch, &child_scene, input);
		break;
		default:
		break;
		}
	}

	// Draw a potential overlay
	Layout_Target *overlay = &g_app->drop_target_overlay;
	if (overlay->node == self->node) {
			Vector2 corners[4] = {
				{ 1.0f, top_bar_h - tab_h},
				{ min_tab_w, top_bar_h - tab_h },
				{ min_tab_w, top_bar_h},
				{ 1.0f, top_bar_h }
			};
			bool rounded[4] = { true, true, false, false };
		add_rounded_quad(scene, corners, rounded, 8.0f*dpi, settings.overlay_color);
	}
}

Split_Group *create_split_group(Arena *arena, split_e type) {
	Split_Group *res = adalloc(arena, sizeof(Split_Group));
	res->type = type;
	res->children = (Layout_Node_Dynarray *) new_dynarray(arena, sizeof(Layout_Node *));
	res->child_sizes = (Float_Dynarray *) new_dynarray(arena, sizeof(float));
	return res;
}

Layout_Node *create_split_group_node(Arena *arena, split_e type)
{
	Layout_Node *res = adalloc(arena, sizeof(Layout_Node));
	res->type = NODE_TYPE_SPLIT_GROUP;
	res->content.split_group = create_split_group(arena, type);
	res->content.split_group->node = res;
	return res;
}

void split_group_add_node(Layout_Node *split, Layout_Node *node, i32 index, float size)
{
	assertf(split->type == NODE_TYPE_SPLIT_GROUP, NULL);
	Split_Group *self = split->content.split_group;
	assertf(size >= 0.0f && size <= 1.0f, NULL);
	float remaining = 1 - size;
	for (u64 i=0; i<self->child_sizes->length; i++) {
		self->child_sizes->d[i] = self->child_sizes->d[i] * remaining;
	}
	dynarray_insert(self->children, &node, index);
	dynarray_insert(self->child_sizes, &size, index);
	node->parent = split;
}

i32 split_group_find_child(Layout_Node *split, Layout_Node *node)
{
	assertf(split->type == NODE_TYPE_SPLIT_GROUP, NULL);
	Split_Group *self = split->content.split_group;
	for (u64 i=0; i<self->children->length; i++) {
		Layout_Node *child = self->children->d[i];
		if (child == node) {
			return i;
		}
	}
	return -1;
}

i32 split_group_remove_node(Layout_Node *split, Layout_Node *node)
{
	assertf(split->type == NODE_TYPE_SPLIT_GROUP, NULL);
	Split_Group *self = split->content.split_group;
	i32 found = -1;
	// Iterate children to find child index 'found' of node
	float remaining_size = 1.0f;
	for (u64 i=0; i<self->children->length; i++) {
		Layout_Node *child = self->children->d[i];
		if (child == node) {
			remaining_size = 1 - self->child_sizes->d[i];
			dynarray_remove(self->children, i);
			dynarray_remove(self->child_sizes, i);
			found = i;
			break;
		}
	}
	if (found >= 0 && self->children->length > 1) {
		for (u64 i=0; i<self->children->length; i++) {
			Layout_Node *child = self->children->d[i];
			self->child_sizes->d[i] = self->child_sizes->d[i] / remaining_size;
		}
		return found;
	} else if (found >= 0) {
		// If a split group is left with only one child, and it's not the root split group, it
		// is pruned since it's not actually splitting anything.
		Layout_Node *parent = split->parent;
		if (parent) {
			i32 self_i = split_group_find_child(parent, split);
			float self_size = parent->content.split_group->child_sizes->d[self_i];
			split_group_remove_node(parent, split);
			split_group_add_node(parent, self->children->d[0], self_i, self_size);
			afree(&app_arena, self);
			afree(&app_arena, split);
		} else if (self->children->length == 1) {
			self->child_sizes->d[0] = 1.0f;
		}
		return found;
	} else {
		return -1;
	}
}

void layout_node_draw_and_respond_input(Layout_Node *node, Scene *scene, Input_State *input);

void split_group_draw_and_respond_input(Split_Group *self, Scene *scene, Input_State *input)
{
	if (!self->initialized) {
		self->child_border_grabbed = -1;
		self->initialized = true;
	}
	Vector2 pointer = window_coords_to_scene(scene, input->pointer_x, input->pointer_y);
	float border_w = 1.0f;
	u32 border_color = 0xffb0b0b0;
	Layout_Target *overlay = &g_app->drop_target_overlay;
	i32 overlay_child_i = -1;
	if (overlay->node == self->node) {
		if (overlay->child >= 0)
			overlay_child_i = overlay->child;
	}

	if (self->type == SPLIT_HORIZONTAL) {
		float x = 0;
		float children_total_w = scene->w - (self->children->length - 1);
		for (u64 i=0; i<self->children->length; i++) {
			Layout_Node *child = self->children->d[i];
			float child_proportion = self->child_sizes->d[i];
			float child_w = children_total_w * child_proportion;
			if (i < self->children->length - 1) {
				bool hit = in_rect(pointer.x, pointer.y, x+child_w-5.0f, 0, 10.0f, scene->h);
				if (hit && !g_app->grab_state.active) {
					au_window_set_cursor(g_app->window, AU_CURSOR_HORIZONTAL_RESIZE);
					self->have_resize_cursor = true;
				}
				if (!hit && self->have_resize_cursor && self->child_border_grabbed < 0) {
					au_window_set_cursor(g_app->window, AU_CURSOR_NORMAL);
					self->have_resize_cursor = false;
				}
				if (hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]
					&& !g_app->grab_state.active) {
					self->child_border_grabbed = i;
					g_app->grab_state.active = true;
					g_app->grab_state.starting = true;
				}
				// TODO: BUG: resize doesn't properly follow cursor
				if (self->child_border_grabbed == i) {
					child_w += input->pointer_dx;
					float new_prop = child_w / children_total_w;
					float dprop = new_prop - child_proportion;
					i32 children_left = (i64) self->children->length - i;
					self->child_sizes->d[i] = new_prop;
					for (u64 j=i; j<self->children->length; j++) {
						self->child_sizes->d[j] -= dprop / (float) children_left;
					}
				}
			}

			Scene child_scene = make_child_scene(scene, x, 0, child_w, scene->h);
			layout_node_draw_and_respond_input(child, &child_scene, input);

			if (overlay_child_i == i) {
				float preview_frac = 1 / 3.0f;
				if (overlay->side == SIDE_TOP) {
					add_rectangle(scene, x, 0, child_w, scene->h * preview_frac,
						settings.overlay_color);
				} else if (overlay->side == SIDE_BOTTOM) {
					add_rectangle(scene, x, scene->h - scene->h*preview_frac, child_w,
						scene->h*preview_frac, settings.overlay_color);
				}
			}
			x += child_w;
			if (i < self->children->length - 1) {
				add_rectangle(scene, x, 0, border_w, scene->h, border_color);
				x += border_w;
			}
		}
	} else if (self->type == SPLIT_VERTICAL) {
		float y = 0;
		float children_total_height = scene->h - (self->children->length - 1);
		for (u64 i=0; i<self->children->length; i++) {
			Layout_Node *child = self->children->d[i];
			float child_proportion = self->child_sizes->d[i];
			float child_h = children_total_height * child_proportion;
			if (i < self->children->length - 1) {
				bool hit = in_rect(pointer.x, pointer.y, 0, y+child_h-5.0f, scene->w, 10.0f);
				if (hit && !g_app->grab_state.active) {
					au_window_set_cursor(scene->window, AU_CURSOR_VERTICAL_RESIZE);
					self->have_resize_cursor = true;;
				}
				if (!hit && self->have_resize_cursor && self->child_border_grabbed < 0) {
					au_window_set_cursor(scene->window, AU_CURSOR_NORMAL);
					self->have_resize_cursor = false;
				}
				if (hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]
					&& !g_app->grab_state.active) {
					self->child_border_grabbed = i;
					g_app->grab_state.active = true;
					g_app->grab_state.starting = true;
				}
				// TODO: BUG: resize doesn't properly follow cursor
				if (self->child_border_grabbed == i) {
					child_h += input->pointer_dy;
					float new_prop = child_h / children_total_height;
					float dprop = new_prop - child_proportion;
					i32 children_left = (i64) self->children->length - i;
					self->child_sizes->d[i] = new_prop;
					for (u64 j=i; j<self->children->length; j++) {
						self->child_sizes->d[j] -= dprop / (float) children_left;
					}
				}
			}

			Scene child_scene = make_child_scene(scene, 0, y, scene->w, child_h);
			layout_node_draw_and_respond_input(child, &child_scene, input);

			if (overlay_child_i == i) {
				float preview_frac = 1 / 3.0f;
				if (overlay->side == SIDE_LEFT) {
					add_rectangle(scene, 0, y, scene->w*preview_frac, child_h,
						settings.overlay_color);
				} else if (overlay->side == SIDE_RIGHT) {
					add_rectangle(scene, scene->w - scene->w*preview_frac, y, scene->w*preview_frac,
						child_h, settings.overlay_color);
				}
			}
			y += child_h;
			if (i < self->children->length - 1) {
				add_rectangle(scene, 0, y, scene->w, border_w, border_color);
				y += border_w;
			}
		}
	}
	if (self->child_border_grabbed >= 0 && input->mouse_released[AU_MOUSE_BUTTON_LEFT]) {
		self->child_border_grabbed = -1;
		g_app->grab_state.active = false;
		g_app->grab_state.stopping = true;
	}

	if (overlay->node == self->node && overlay_child_i < 0) {
		float frac = 1 / 3.0f;
		u32 color = settings.overlay_color;
		switch (overlay->side) {
		case SIDE_LEFT:
			add_rectangle(scene, 0, 0, scene->w * frac, scene->h, color);
		break;
		case SIDE_RIGHT: {
			float w = scene->w * frac;
			add_rectangle(scene, scene->w - w, 0, w, scene->h, color);
		} break;
		case SIDE_TOP:
			add_rectangle(scene, 0, 0, scene->w, scene->h * frac, color);
		break;
		case SIDE_BOTTOM: {
			float h = scene->h * frac;
			add_rectangle(scene, 0, scene->h - h, scene->w, h, color);
		} break;
		}
	}
}

void layout_node_draw_and_respond_input(Layout_Node *node, Scene *scene, Input_State *input)
{
	switch (node->type) {
	case NODE_TYPE_SPLIT_GROUP:
		split_group_draw_and_respond_input(node->content.split_group, scene, input);
	break;
	case NODE_TYPE_TAB_GROUP:
		tab_group_draw_and_respond_input(node->content.tab_group, scene, input);
	break;
	default:
	break;
	}
}

void bottom_bar_draw_and_respond_input(Bottom_Bar *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	App *app = self->app;
	Vector2 pointer = window_coords_to_scene(scene, input->pointer_x, input->pointer_y);

	add_rectangle(scene, 0, 0, scene->w, scene->h, settings.bottom_bar_color);
	float margin = 15*dpi;
	float x = scene->w / 2.0f - (app->icon_size*1.5 + margin);

	{
		float plus_size = app->icon_size - 18.0f;
		float plus_margin = 16.0f*dpi;

		// u32 container_bg = 0xff303060;
		u32 container_bg = settings.bottom_bar_buttons_bg_color;
		float container_w = 3*app->icon_size + 3*margin + plus_size + 2*plus_margin;
		float container_h = scene->h - 6*dpi;
		float container_x = x - plus_size - 2*plus_margin;
		float container_y = (scene->h - container_h) / 2.0f;
		add_rounded_rectangle(scene, container_x, container_y, container_w, container_h,
			container_h / 2.0f, container_bg);

		// u32 plus_color = 0xffb0b0d0;
		u32 plus_color = 0xffd0d0d0;
		float plus_w = 4.0f*dpi;
		float plus_x = container_x + plus_margin;
		float plus_y = container_y + (container_h - plus_size) / 2.0f;
		add_rectangle(scene, plus_x, plus_y + plus_size/2.0f - plus_w/2.0f, plus_size, plus_w,
			plus_color);
		add_rectangle(scene, plus_x + plus_size/2.0f - plus_w/2.0f, plus_y, plus_w, plus_size,
			plus_color);
	}

	i32 images[3] = { app->clock_svg, app->timer_svg, app->stopwatch_svg };
	node_type_e types[3] = { NODE_TYPE_CLOCK, NODE_TYPE_TIMER, NODE_TYPE_STOPWATCH };
	u32 icon_color = 0xffd0d0d0;
	for (i32 i=0; i<3; i++) {
		u32 color = my_blend_colors(icon_color, 0xffffffff, self->buttons_hover_t[i]);
		add_image_with_color(scene, images[i], x, 4*dpi, color);
		bool hit = in_rect(pointer.x, pointer.y, x, 4*dpi, app->icon_size, app->icon_size)
			&& !g_app->grab_state.active;
		creep_towards(&self->buttons_hover_t[i], hit ? 1.0f : 0.0f, 10.0f*input->anim_dt_s);
		if (hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]) {
			Layout_Node *tabs_node;
			if (g_app->focused_pane) {
				tabs_node = g_app->focused_pane->parent;
			} else {
				tabs_node = get_default_focused_tab_group(g_app->layout_root);
			}
			Layout_Node *p = create_pane_with_default_name(&app_arena, types[i]);
			tab_group_add_pane(tabs_node, p);
			Tab_Group *tabs = tabs_node->content.tab_group;
			tabs->active_tab = tabs->children->length - 1;
		}
		x += app->icon_size + margin;
	}
}

int main(int argc, char *argv[])
{
	arena_init(&app_arena, 1 << 26, 1 << 26);
	Window *win = create_window(&app_arena, 1200, 800, "SimulTime", true);
	Scene *scene = (Scene *) win;
	Input_State *input = win->input;
	float dpi = win->scale;

	i32 font = load_font(scene, "NotoSans-Regular.ttf");

	App *app = (App *) aalloc(&app_arena, sizeof(App));
	g_app = app;
	app->window = win;
	app->bottom_bar.app = app;

 	settings = (App_Settings) {
 		.background_color = 0xff202038,
 		.font = font,
 		.font_size_unscaled = 50.0f,
 		.name_font_size_unscaled = 18.0f,
 		.font_color = 0xffffffff,
		.bottom_bar_color = 0xff232833,
		.bottom_bar_buttons_bg_color = 0xff2a303f,
		.bottom_bar_height = 40,
		.tab_bar_color = 0xff303040,
		.x_button_size_unscaled = 12.0f,
		.top_bar_h_unscaled = 40.0f,
		.overlay_color = 0x803030c0,
	};

	float icon_size = (settings.bottom_bar_height - 8)*dpi;
	app->icon_size = icon_size;
	app->clock_svg = load_image_at_size(scene, "res/clock.svg", "svg_alpha", icon_size, icon_size);
	app->timer_svg = load_image_at_size(scene, "res/timer.svg", "svg_alpha", icon_size, icon_size);
	app->stopwatch_svg = load_image_at_size(scene, "res/sw.svg", "svg_alpha", icon_size, icon_size);

	app->chime_wav = au_window_load_sound(win, "res/chime.wav", "wav");

	// TODO: reload svgs on rescale
	float x_button_size = settings.x_button_size_unscaled*dpi;
	settings.x_button = load_image_at_size(scene, "res/x_button.svg", "svg_alpha", x_button_size,
		x_button_size);

	app->layout_root = create_split_group_node(&app_arena, SPLIT_HORIZONTAL);
	Layout_Node *tabs = create_tab_group_node(&app_arena);
	split_group_add_node(app->layout_root, tabs, 0, 1.0f);

	Layout_Node *clock = create_pane_with_default_name(&app_arena, NODE_TYPE_CLOCK);
	Layout_Node *timer = create_pane_with_default_name(&app_arena, NODE_TYPE_TIMER);
	Layout_Node *stopwatch = create_pane_with_default_name(&app_arena, NODE_TYPE_STOPWATCH);
	tab_group_add_pane(tabs, clock);
	tab_group_add_pane(tabs, timer);
	tab_group_add_pane(tabs, stopwatch);

	// Layout_Node *tabs2 = create_tab_group_node(&app_arena);
	// split_group_add_node(app->layout_root->content.split_group, tabs2, 1, 0.5f);
	// Layout_Node *clock2 = create_pane_with_default_name(&app_arena, NODE_TYPE_CLOCK);
	// tab_group_add_pane(tabs2->content.tab_group, clock2);

	while (!input->quit) {
		dpi = win->scale;

		app->grab_state.starting = false;
		app->grab_state.stopping = false;

		/*
		i32 old_type = app->layout.type;
		if (input->key_pressed[AU_KEY_TAB]) {
			app->layout.type = (app->layout.type == NODE_TYPE_COUNT - 1) ? 0
				: app->layout.type + 1;
			app->layout.content.v = widgets[app->layout.type];
		}
		if (input->key_pressed[AU_KEY_BACKSPACE]) {
			app->layout.type = (app->layout.type == 0) ? NODE_TYPE_COUNT -1
				: app->layout.type - 1;
			app->layout.content.v = widgets[app->layout.type];
		}
		*/

		clear_background(scene, settings.background_color);

		Scene main_scene = make_child_scene(scene, 0, 0, scene->w,
			scene->h - settings.bottom_bar_height*dpi);
		split_group_draw_and_respond_input(app->layout_root->content.split_group, &main_scene,
			input);

		Scene bottom_bar_scene = make_child_scene(scene, 0,
			scene->h - settings.bottom_bar_height*dpi, scene->w, settings.bottom_bar_height*dpi);
		bottom_bar_draw_and_respond_input(&app->bottom_bar, &bottom_bar_scene, input);

		commit_changes(win);
		au_sleep(10);
		update_input_state(win);
	}
}

#ifdef _WIN32

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	main(1, &cmdline);
}

#endif
