#ifndef AU_CONTAINERS_H
#define AU_CONTAINERS_H

#include "au_core.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/************************************** Hash Table ************************************************/

typedef struct hash_entry {
	u64 hash;
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
// and lookup. But if you have static keys, you can turn copying off by using this constructor.
Hash_Table create_nocopy_hash_table(Arena *arena, u64 capacity);

// TODO: destructors
// void destroy_hash_table(Hash_Table *table);

bool hash_table_set(Hash_Table *table, void *key, u64 key_len, void *value);

void *hash_table_get(Hash_Table *table, void *key, u64 key_len);

bool hash_table_delete(Hash_Table *table, void *key, u64 key_len);

#ifdef __cplusplus
}
#endif

#endif