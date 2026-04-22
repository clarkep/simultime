#ifndef AUTIL_PLATFORM_H
#define AUTIL_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "au_core.h"

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


#ifdef __cplusplus
}
#endif

#endif