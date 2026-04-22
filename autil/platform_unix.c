// #define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>

#include "au_core.h"
#include "au_platform.h"

struct au_unix_thread {
	/* keep in line with autil.h: */
	au_thread_func f;
	void *arg;
	bool running;
	/* unix specific: */
	pthread_t pthread;
};

static void *au_unix_thread_wrapper(void *arg)
{
	struct au_unix_thread *thr = (struct au_unix_thread *) arg;
	thr->running = true;
	thr->f(thr->arg);
	thr->running = false;
}

struct au_thread *au_create_thread(Arena *arena, au_thread_func f, void *arg)
{
	struct au_unix_thread *res = aalloc(arena, sizeof(struct au_unix_thread));
	res->f = f;
	res->arg = arg;
	return (struct au_thread *) res;
}

bool au_start_thread(struct au_thread *thread)
{
	struct au_unix_thread *thr = (struct au_unix_thread *) thread;
	// todo: error code
	return !pthread_create(&thr->pthread, NULL, au_unix_thread_wrapper, (void *) thr);
}

void au_join_thread(struct au_thread *thread, i32 timeout_ms)
{
	struct au_unix_thread *thr = (struct au_unix_thread *) thread;
	if (timeout_ms >= 0) {
		errexit("au_join_thread with timeout >= 0 is unimplemented.\n");
		/*
		// TODO: Need cross-platform implementation
		struct timespec ts = { timeout_ms / 1000, (timeout_ms % 1000) * 1000000 };
		pthread_timedjoin_np(thr->pthread, NULL, &ts);
		*/
	} else {
		pthread_join(thr->pthread, NULL);
	}
}

// xx au_unix_mutex?
struct au_mutex {
	pthread_mutex_t pthread_mutex;
};

struct au_mutex *au_create_mutex(Arena *arena)
{
	// Did you know? If you try to use a pthread_mutex_t on linux at an unaligned address, your
	// program crashes with an undiagnosable error message.
	arena_align(arena, 16);
	struct au_mutex *res = aalloc(arena, sizeof(struct au_mutex));
	pthread_mutex_init(&res->pthread_mutex, NULL);
	return res;
}

bool au_lock_mutex(struct au_mutex *mutex)
{
	return !pthread_mutex_lock(&mutex->pthread_mutex);
}

bool au_unlock_mutex(struct au_mutex *mutex)
{
	// todo: error code
	return !pthread_mutex_unlock(&mutex->pthread_mutex);
}

void au_sleep(i32 millis)
{
	usleep(millis*1000);
}

u64 get_os_time()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000000000 + t.tv_usec * 1000;
}

double get_os_time_s()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (double) t.tv_sec + t.tv_usec / 1e6;
}

u64 au_get_modification_time(char **filenames, int n)
{
	u64 latest_time = 0;
	for (int i=0; i<n; i++) {
		struct stat info;
		i32 retries = 0;
		while (retries < 10 && stat(filenames[i], &info) < 0) {
			retries++;
			au_sleep(10);
		}
		assertf(retries >= 10, "au_get_modification_time: Failed to stat file: %s\n", filenames[i]);
		#ifdef __APPLE__
		u64 time = info.st_mtimespec.tv_sec * 1000 + info.st_mtimespec.tv_nsec / 1000000;
		#else
		u64 time = info.st_mtim.tv_sec * 1000 + info.st_mtim.tv_nsec / 1000000;
		#endif
		latest_time = MAX(time, latest_time);
	}
	return latest_time;
}

void au_run_command_in_dir(char *command, char *dir)
{
	char old_dir[1024];
	assertf(getcwd(old_dir, 1024), "au_run_command_in_dir: Failed to get current directory");
	chdir(dir);
	system(command);
	chdir(old_dir);
}

void *au_load_library(char *path)
{
	return dlopen(path, RTLD_LAZY);
}

void au_unload_library(void *library)
{
	dlclose(library);
}

void *au_get_function(void *library, char *func_name)
{
	return dlsym(library, func_name);
}

