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

#ifdef __cplusplus
}
#endif

#endif
