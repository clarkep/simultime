#define AUTIL_IMPLEMENTATION
#include "au_core.h"
#include "tlsf.h"

// don't use _debug_mem macros internally
#undef aalloc
#undef adalloc
#undef arealloc
#undef afree

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_RETRIES 3

#ifdef _WIN32
/* define ffsll for msvc, from [1] */
	#include <intrin.h>
	#pragma intrinsic(_BitScanForward64)
	#define WIN32_LEAN_AND_MEAN
	#define VC_EXTRALEAN
	#include <windows.h>
int ffsll(long long x)
{
	unsigned long i;
	if (_BitScanForward64(&i, x))
		return (i + 1);
	return (0);
}

#else // no _WIN32
	#include <stdlib.h>
	#include <sys/mman.h>
#endif

DLL_LINK Arena app_arena = { 0 };

void errexit(char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args );
	va_end(args);
	exit(1);
}

void fire_assert(char *format, va_list args)
 {
	if (format) {
		vfprintf(stderr, format, args );
	} else {
		fprintf(stderr, "Programmer error, have to stop.\n");
	}
	exit(1);
}

void assertf(bool condition, char *format, ...)
{
	va_list args;
	va_start(args, format);
	if (!condition) {
		fire_assert(format, args);
	}
	va_end(args);
}

/***************************** Memory management ******************************/

void *amap_memory(void *loc, u64 size)
{
#ifdef _WIN32
	void *result = (u8 *) VirtualAlloc(loc, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	DWORD err = GetLastError();
	if (!result)
		printf("err: %lu\n", err);
#else
	void *result = (u8 *) mmap(loc, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == (u8 *) MAP_FAILED)
		printf("errno: %d\n", errno);
	result = (result == (u8 *) MAP_FAILED) ? NULL : result;
#endif
	return result;
}

bool aunmap_memory(void *loc, u64 size)
{
#ifdef _WIN32
	return VirtualFree(loc, 0, MEM_RELEASE);
#else
	return munmap(loc, size) + 1;
#endif
}

typedef struct allocation_record {
	void *addr;
	u64 size;
	bool freed;
	char *file;
	i32 line;
	i32 type;
} Allocation_Record;

Arena _allocation_log_arena;

static void arena_init_calloc(Arena *arena, u64 size, u64 dyn_size);

static void init_allocation_log(void)
{
	if (!arena_init_at(&_allocation_log_arena, (void *) 0x200000010000, 1 << 20, NULL, 0)) {
		arena_init_calloc(&_allocation_log_arena, 1 << 20, 0);
	}
}

static void log_allocation(void *addr, u64 size, char *file, i32 line)
{
	if (!_allocation_log_arena.start) {
		init_allocation_log();
	}
	Allocation_Record *record = aalloc(&_allocation_log_arena, size);
	record->addr = addr;
	record->size = size;
	record->file = file;
	record->line = line;
}

static void log_free(const void *addr)
{
	Allocation_Record *a = (Allocation_Record *) _allocation_log_arena.start;
	for (; (u8 *) a<_allocation_log_arena.end; a++) {
		if (a->addr == addr)
			a->freed = true;
	}
}

void arena_print_logged_allocations(Arena *arena)
{
	Allocation_Record *a = (Allocation_Record *) _allocation_log_arena.start;
	printf("Static allocations:\n");
	for (; (u8 *) a<_allocation_log_arena.end; a++) {
		u8 *addr = (u8 *) a->addr;
		if (addr >= arena->start && addr < arena->end) {
			printf("%p: size: %llu origin:%s:%d\n", a->addr, a->size, a->file, a->line);
		}
	}
	struct dyn_header *dyn = (struct dyn_header *) arena->dyn_data;
	printf("Dynamic allocations:\n");
	for (; (u8 *) a<_allocation_log_arena.end; a++) {
		u8 *addr = (u8 *) a->addr;
		// if (addr >= arena->dyn_data && addr < dyn->end) {
		// 	printf("%llx: size: %llu origin:%s:%d\n", a->addr, a->size, a->file, a->line);
		// }
	}
}

// Windows requires 64k alignment for VirtualAlloc
u64 arena_start_locations[] = {
	0x1a0000010000,
	0x1ad000010000,
	0x1b0000020000,
	0x1bd000020000,
	0x1c0000030000,
	0x1cd000030000,
	0x1d0000040000,
	0x1dd000040000,
	0x1e0000050000,
	0x1ed000050000,
	0x1f0000060000,
	0x1fd000060000,
};
i32 arena_start_locations_length = sizeof(arena_start_locations)/sizeof(u64);
i32 arena_start_locations_i = 0;

bool arena_init_at(Arena *arena, void *static_start, u64 static_size, void *dyn_start,
	u64 dyn_size)
{
	if (static_size > 0) {
		arena->start = (u8 *) amap_memory(static_start, static_size);
		//assertf(arena->start, "Failed to map %lld bytes at %llx.\n", static_size,
		// 	static_start);
		if (!arena->start)
			return false;
		arena->next = arena->start;
		arena->end = arena->start + static_size;
	} else {
		arena->start = arena->next = arena->end = NULL;
	}
	if (dyn_size > 0) {
		arena->dyn_data = (u8 *) amap_memory(dyn_start, dyn_size);
		// assertf(arena->dyn_data, "Failed to map %lld bytes at %llx.\n", dyn_size,
		//	dyn_start);
		if (!arena->dyn_data) {
			if (arena->start)
				aunmap_memory(arena->start, arena->end - arena->start);
			return false;
		}
		arena->dyn_end = arena->dyn_data + dyn_size;
		tlsf_create_with_pool(arena->dyn_data, dyn_size);
	} else {
		arena->dyn_data = NULL;
	}
	arena->type = 1;
	arena->expand_policy = AU_EXPAND_POLICY_EXTEND_OR_CRASH;
	arena->expand_by = static_size;
	arena->dyn_expand_by = dyn_size;
	return true;
}

static void arena_init_calloc(Arena *arena, u64 static_size, u64 dyn_size)
{
	if (static_size > 0) {
		arena->start = (u8 *) calloc(static_size, 1);
		assertf(arena->start, "Could not calloc %lld bytes.\n", static_size);
		arena->next = arena->start;
		arena->end = arena->start + static_size;
	} else {
		arena->start = arena->next = arena->end = NULL;
	}
	if (dyn_size > 0) {
		arena->dyn_data = (u8 *) calloc(dyn_size, 1);
		arena->dyn_end = arena->dyn_data + dyn_size;
		tlsf_create_with_pool(arena->dyn_data, dyn_size);
	} else {
		arena->dyn_data = NULL;
	}
	arena->type = 1;
	arena->expand_policy = AU_EXPAND_POLICY_EXTEND_OR_CRASH;
	arena->expand_by = static_size;
	arena->dyn_expand_by = dyn_size;
}

void arena_init(Arena *arena, u64 size, u64 dyn_size)
{
	// Try one of the preset addresses.
	if (arena_start_locations_i < arena_start_locations_length) {
		bool result = arena_init_at(arena, (void *) arena_start_locations[arena_start_locations_i],
			size, (void *) arena_start_locations[arena_start_locations_i+1], dyn_size);
		// XXX
		if (result) {
			arena_start_locations_i += 2;
			return;
		}
	}

	// If that fails, or out of preset addresses, use calloc.
	arena_init_calloc(arena, size, dyn_size);
}

void arena_reset(Arena *arena)
{
	if (arena->start) {
		arena->next = arena->start;
		memset(arena->start, 0, arena->end - arena->start);
	}
	if (arena->dyn_data) {
		u64 dyn_size = arena->dyn_end - arena->dyn_data;
		memset(arena->dyn_data, 0, dyn_size);
		tlsf_create_with_pool(arena->dyn_data, dyn_size);
	}
}

// 'align' must be a power of 2
void arena_align(Arena *arena, i16 align)
{
	// RHS == align - (arena->start % align). Also, u64 could be uintptr_t.
	arena->next += -(u64)arena->next & (align-1);
}

Arena arena_copy(Arena *arena)
{
	Arena res;
	res.start = arena->start;
	res.next = arena->next;
	res.end = arena->end;
	return res;
}

// Expand arena based on policy and expand amount
static void expand_arena(Arena *arena, bool expand_dynamic) {
	if (arena->expand_policy == AU_EXPAND_POLICY_ALWAYS_CRASH) {
		errexit("Arena: out of memory.\n");
	} else {
		tlsf_t tlsf = (tlsf_t) arena->dyn_data;
		void *try_expand_addr = expand_dynamic ? arena->dyn_end : arena->end;
		assertf(try_expand_addr, "Arena: cannot expand unitialized arena.\n");
		u64 try_expand_size = expand_dynamic ? arena->dyn_expand_by : arena->expand_by;
		void *result = amap_memory(try_expand_addr, try_expand_size);
		if (result != try_expand_addr) {
			if (result != NULL)
				aunmap_memory(result, try_expand_size);
			if (arena->expand_policy == AU_EXPAND_POLICY_EXTEND_OR_CRASH)
				errexit("Arena: out of memory and could not extend contiguously(AU_EXPAND_POLICY_EXTEND_OR_CRASH).\n");
			else
				errexit("Arena: out of memory and could not extend contiguously, arena move unimplemented(AU_EXPAND_POLICTY_EXTEND_OR_MOVE).\n");
		} else if (expand_dynamic) {
			arena->dyn_end += try_expand_size;
			tlsf_add_pool(tlsf, result, try_expand_size);
		} else {
			arena->end += try_expand_size;
		}
	}
}

void *aalloc(Arena *arena, u64 size)
{
	if (arena) {
		if (!arena->start)
			return NULL;
		void *ret = (void *) arena->next;
		assertf(arena->next + size > arena->next, "Arena: pointer arithmetic overflow.\n");
		while (arena->next + size >= arena->end) {
			expand_arena(arena, false);
		}
		arena->next += size;
		assertf(arena->next < arena->end, "Arena: static area over memory limit.\n");
		return ret;
	} else {
		void *ret = calloc(size, 1);
		assertf(ret, "Arena: calloc fallback failed.\n");
		return ret;
	}
}

void *aalloc_debug_mem(Arena *arena, u64 size, char *file, i32 line)
{
	void *res = aalloc(arena, size);
	log_allocation(res, size, file, line);
	return res;
}

void *adalloc(Arena *arena, u64 size)
{
	tlsf_t tlsf = (tlsf_t) arena->dyn_data;
	i32 retries = 0;
	void *ret = tlsf_malloc(tlsf, size);
	while (!ret && retries < MAX_RETRIES) {
		expand_arena(arena, true);
		ret = tlsf_malloc(tlsf, size);
		retries++;
	}
	if (ret)
		memset(ret, 0, size);
	return ret;
}

void *adalloc_log_mem(Arena *arena, u64 size, char *file, i32 line)
{
	void *ret = adalloc(arena, size);
	log_allocation(ret, size, file, line);
	return ret;
}

void afree(Arena *arena, const void *ptr)
{
	tlsf_t tlsf = (tlsf_t) arena->dyn_data;
	tlsf_free(tlsf, (void *) ptr);
}

void afree_debug_mem(Arena *arena, const void *ptr, char *file, i32 line)
{
	afree(arena, ptr);
	log_free(ptr);
}

void *arealloc(Arena *arena, const void *ptr, u64 size)
{
	tlsf_t tlsf = (tlsf_t) arena->dyn_data;
	i32 retries = 0;
	void *ret = tlsf_realloc(tlsf, (void *) ptr, size);
	while (!ret && retries < MAX_RETRIES) {
		expand_arena(arena, true);
		ret = tlsf_realloc(tlsf, (void *) ptr, size);
		retries++;
	}
	return ret;
}

void *arealloc_debug_mem(Arena *arena, const void *ptr, u64 size, char *file, i32 line)
{
	void *ret = arealloc(arena, ptr, size);
	if (ret != ptr) {
		log_free(ptr);
		log_allocation(ret, size, file, line);
	}
	return ret;
}

void arena_print_usage(Arena *arena)
{
}

// Refs:
// [1] https://github.com/aerospike/jemArena/blob/master/include/msvc_compat/strings.h
