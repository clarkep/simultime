#include <autil.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

typedef struct time_fields {
	i32 hours;
	i32 minutes;
	i32 seconds;
	i32 millis;
	i32 micros;
} Time_Fields;

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

typedef enum {
	CLOCK_TYPE_CLOCK,
	CLOCK_TYPE_TIMER,
	CLOCK_TYPE_STOPWATCH,
	CLOCK_TYPE_COUNT
} clock_type_e;

// Global settings
typedef struct app_settings {
	u32 background_color;
	i32 font;
	float font_size_unscaled;
	float name_font_size_unscaled;
	u32 font_color;
} App_Settings;

typedef struct clock {
	App_Settings *settings;
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
	App_Settings *settings;
	String name;
	PPS_Button play_pause;
	PPS_Button stop_button;
	Time_Input time_input;
	// The time the timer is set for
	u64 interval;
	// If running, the time the timer was started/restarted.
	u64 start_time;
	// If running, the time remaining when the timer was started. Otherwise, the paused value.
	u64 start_value;
	float flash_red;
	timer_state_e state;
	bool initialized;
} Timer;

typedef struct stopwatch {
	App_Settings *settings;
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

typedef struct panel {
	clock_type_e type;
	union {
		Clock *clock;
		Timer *timer;
		Stopwatch *stopwatch;
		void *v;
	} content;
} Panel;

typedef struct app {
	App_Settings settings;
	// todo: tree of panels.
	Panel panel;
} App;

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
	FILETIME ftime;
	GetSystemTimePreciseAsFileTime(&ftime);
	u64 t = ((u64) ftime.dwHighDateTime << 32) | ((u64) ftime.dwLowDateTime);
	return t / 10;
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
	bool hit = window_coords_in_scene(input->pointer_x, input->pointer_y, scene);
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
		i32 field_len = MAX(log_base10(fields[i]) + 1, 2);
		// First handle mouse interactions and possible drag-updates to fields[i].
		float field_w = measure_text_widthf(scene, self->font, font_size, "%02d", fields[i]);
		bool field_hit = in_rect(pointer.x, pointer.y, x, y-font_size, field_w, font_size);
		any_field_hit = any_field_hit || field_hit;
		bool hovered = (field_hit && !input->mouse_down[AU_MOUSE_BUTTON_LEFT]) || self->field_dragging == i;
		creep_towards(&self->fields_hovered_t[i], hovered ? 1.0f : 0.0f, 10.0f*input->anim_dt_s);
		if (field_hit && input->mouse_pressed[AU_MOUSE_BUTTON_LEFT]) {
			if (self->field_selected != i) {
				time_input_void_selection(self);
			}
			self->field_clicking = i;
			self->field_dragging = i;
			self->drag_start_y = pointer.y;
			self->drag_field_start_value = fields[i];
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
			}
		}
		if (self->field_dragging == i) {
			float drag_px_per_val = 5.0f * dpi;
			fields[i] = self->drag_field_start_value + (self->drag_start_y - pointer.y) / drag_px_per_val;
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
						bool leftmost0_skip = i == 0 && len > 2 && self->cursor_j == 0 && c == '0';
						if (self->cursor_j < len - 1 && !leftmost0_skip) {
							self->cursor_j++;
						} else if (self->field_selected == 1) {
							self->field_selected++;
							self->cursor_j = 0;
						} else if (self->field_selected == 0 && !leftmost0_skip) {
							if (fields[i])
								self->insert_mode = true;
							else { // we've entered a zero, don't insert?
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
			u32 cursor_color = my_blend_colors(self->text_color, 0xff000000, 0.25f);
			// TODO: bad and font dependent
			add_rectangle(scene, x - 1*dpi, y - font_size*0.75, 2*dpi, font_size*0.75, cursor_color);
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
	// TODO unix
	// xx for sub-millisecond precision, could use timer path(GetSystemTimePreciseAsFileTime)
	// and convert
	SYSTEMTIME loctime;
	GetLocalTime(&loctime);
	// TODO option somehow for 24-hour time
	char *ampm;
	if (loctime.wHour > 12) {
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
}

void clock_draw_and_respond_input(Clock *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	App_Settings *set = self->settings;

	float name_size = set->name_font_size_unscaled*dpi;
	// Todo: putting baseline at y + font_size could cause overflow, need font metrics api
	// TODO: don't allow name to intersect content
	add_text(scene, set->font, name_size, self->name.d, 10*dpi, name_size + 10*dpi,
		set->font_color);

	char timebuf[20];
	bool suc = get_formatted_current_time(timebuf, 20);
	assertf(suc, NULL);
	add_centered_text(scene, set->font, set->font_size_unscaled*dpi, timebuf, set->font_color);
}

/************************************** Timer *****************************************************/


void timer_draw_and_respond_input(Timer *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	App_Settings *set = self->settings;

	if (!self->initialized) {
		self->start_value = self->interval;
		self->play_pause.type = BUTTON_TYPE_PLAY;
		self->play_pause.color = 0xffd0d0d0;
		self->stop_button.type = BUTTON_TYPE_STOP;
		self->stop_button.color = 0xffd0d0d0;
		self->time_input.font = set->font;
		self->time_input.font_size_unscaled = set->font_size_unscaled;
		self->time_input.text_color = set->font_color;
		self->initialized = true;
	}

	if (self->flash_red) {
		float flash_red_frequency = 1.2;
		float phase_max = 0.9;
		float phase = fmod(self->flash_red, 1.0f);
		float brightness = phase > phase_max ? (1 - phase) / (1 - phase_max)
		                                     : phase / phase_max;
		u32 flash_color = my_blend_colors(set->background_color, 0xff783a39, brightness);
		clear_background(scene, flash_color);
		creep_towards(&self->flash_red, 0.0f, input->anim_dt_s*flash_red_frequency);
	}

	float name_size = set->name_font_size_unscaled*dpi;
	// Todo: putting baseline at y + font_size could cause overflow, need font metrics api
	// TODO: don't allow name to intersect content
	add_text(scene, set->font, name_size, self->name.d, 10*dpi, name_size + 10*dpi,
		set->font_color);

	u64 now = now_timestamp();
	u64 remaining = MAX((i64) self->start_value - (i64) (self->state == TIMER_STATE_RUNNING ? (now - self->start_time) : 0LL), 0LL);
	if (self->state != TIMER_STATE_READY) {
		char timebuf[30];
		bool suc = format_micros(timebuf, 30, remaining);
		assertf(suc, NULL);
		// y == scene->h / 2.0f
		add_centered_text(scene, set->font, set->font_size_unscaled*dpi, timebuf, set->font_color);
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
	}
}

/************************************** Stopwatch *************************************************/

void stopwatch_draw_and_respond_input(Stopwatch *self, Scene *scene, Input_State *input)
{
	float dpi = scene->window->scale;
	App_Settings *set = self->settings;

	if (!self->initialized) {
		self->play_pause.type = BUTTON_TYPE_PLAY;
		self->play_pause.color = 0xffd0d0d0;
		self->stop_button.type = BUTTON_TYPE_STOP;
		self->stop_button.color = 0xffd0d0d0;
		self->initialized = true;
	}

	float name_size = set->name_font_size_unscaled*dpi;
	// Todo: putting baseline at y + font_size could cause overflow, need font metrics api
	// TODO: don't allow name to intersect content
	add_text(scene, set->font, name_size, self->name.d, 10*dpi, name_size + 10*dpi,
		set->font_color);

	u64 now = now_timestamp();
	u64 elapsed = self->start_value + (self->running ? now - self->start_time : 0);

	char timebuf[30];
	bool suc = format_micros(timebuf, 30, elapsed);
	assertf(suc, NULL);
	add_centered_text(scene, set->font, set->font_size_unscaled*dpi, timebuf, set->font_color);

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

void panel_draw_and_respond_input(Panel *panel, Scene *scene, Input_State *input)
{
	switch (panel->type) {
	case CLOCK_TYPE_CLOCK:
		clock_draw_and_respond_input(panel->content.clock, scene, input);
	break;
	case CLOCK_TYPE_TIMER:
		timer_draw_and_respond_input(panel->content.timer, scene, input);
	break;
	case CLOCK_TYPE_STOPWATCH:
		stopwatch_draw_and_respond_input(panel->content.stopwatch, scene, input);
	break;
	default:
	break;
	}
}

int main(int argc, char *argv[])
{
	arena_init(&app_arena, 1 << 26, 1 << 26);
	Window *win = create_window(&app_arena, 600, 600, "SimulTime", true);
	Scene *scene = (Scene *) win;
	Input_State *input = win->input;

	i32 font = load_font(scene, "NotoSans-Regular.ttf");

	App *app = (App *) aalloc(&app_arena, sizeof(App));
	app->settings = (App_Settings) { 0xff202030, font, 50, 20, 0xffffffff };

	Clock *clock = (Clock *) aalloc(&app_arena, sizeof(Clock));
	clock->settings = &app->settings;
	clock->name = string_from("Clock 1");

	Timer *timer = (Timer *) aalloc(&app_arena, sizeof(Timer));
	timer->settings = &app->settings;
	timer->name = string_from("Timer 1");
	timer->interval = 2 * 1000000; // 5 minutes

	Stopwatch *stopwatch = (Stopwatch *) aalloc(&app_arena, sizeof(Stopwatch));
	stopwatch->settings = &app->settings;
	stopwatch->name = string_from("Stopwatch 1");
	stopwatch->start_time = now_timestamp();

	void *widgets[3] = { clock, timer, stopwatch };

	app->panel.type = CLOCK_TYPE_TIMER;
	app->panel.content.timer = timer;

	while (!input->quit) {
		float dpi = win->scale;
		u32 text_color = 0xffffffff;

		i32 old_type = app->panel.type;
		if (input->key_pressed[AU_KEY_TAB]) {
			app->panel.type = (app->panel.type == CLOCK_TYPE_COUNT - 1) ? 0
				: app->panel.type + 1;
			app->panel.content.v = widgets[app->panel.type];
		}
		if (input->key_pressed[AU_KEY_BACKSPACE]) {
			app->panel.type = (app->panel.type == 0) ? CLOCK_TYPE_COUNT -1
				: app->panel.type - 1;
			app->panel.content.v = widgets[app->panel.type];
		}

		clear_background(scene, app->settings.background_color);
 		panel_draw_and_respond_input(&app->panel, scene, input);
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
