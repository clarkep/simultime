#include <string.h>

#include "au_core.h"
#include "au_containers.h"

Dynarray *dynarray_from_data(Arena *arena, void *copy_items, u64 item_size,
	u64 length)
{
	u8 sz = 1;
	while (length >> sz) { sz++; }
	u64 capacity_bytes = (1ULL << sz) * item_size;

	Dynarray *res = (Dynarray *) aalloc(arena, sizeof(Dynarray));
	res->d = adalloc(arena, capacity_bytes);
	res->item_size = item_size;
	res->length = length;
	res->capacity = 1ULL << sz;
	res->arena = arena;
	if (copy_items)
		memmove(res->d, copy_items, length*item_size);
	else
		memset(res->d, 0, length*item_size);
	return res;
}

struct dynarray *new_dynarray(Arena *arena, u64 item_size)
{
	return dynarray_from_data(arena, NULL, item_size, 0);
}

static void dynarray_up_size(struct dynarray *arr)
{
	u8 *old_data = arr->d;
	arr->capacity *= 2;
	arr->d = arealloc(arr->arena, arr->d, arr->capacity*arr->item_size);
}

void dynarray_expand_to(void *dynarray, u64 new_capacity)
{
	Dynarray *arr = (Dynarray *) dynarray;
	while (new_capacity > arr->capacity) {
		dynarray_up_size(arr);
	}
}

void dynarray_expand_by(void *dynarray, u64 added_capacity)
{
	Dynarray *arr = (Dynarray *) dynarray;
	u64 new_capacity = arr->length + added_capacity;
	while (new_capacity > arr->capacity) {
		dynarray_up_size(arr);
	}
}

void dynarray_add(void *arrp, void *item)
{
	struct dynarray *arr = (struct dynarray *) arrp;
	while ((arr->length+1) > arr->capacity) {
		dynarray_up_size(arr);
	}
	memmove(arr->d + arr->length*arr->item_size, item, arr->item_size);
	arr->length++;
}

void dynarray_insert(void *arrp, void *item, u64 i)
{
	struct dynarray *arr = (struct dynarray *) arrp;
	assertf(i < arr->length+1, "Error: tried to insert at index %llu in a dynarray of length %llu", i, arr->length);
	if (i == arr->length)
		dynarray_add(arrp, item);
	else {
		memmove(arr->d + (i+1)*arr->item_size, arr->d + i*arr->item_size,
			(arr->length-i)*arr->item_size);
		memmove(arr->d + i*arr->item_size, item, arr->item_size);
		arr->length++;
	}
}

void dynarray_remove(void *arrp, u64 i)
{
	struct dynarray *arr = (struct dynarray *) arrp;
	assertf(i < arr->length, "Error: tried to remove item %llu from a dynarray of length %llu", i, arr->length);
	if (i < arr->length - 1)
		memmove(arr->d + i*arr->item_size, arr->d + (i+1)*arr->item_size,
			(arr->length - (i+1))*arr->item_size);
	arr->length--;
}

void *dynarray_get(void *arrp, u64 i) {
	struct dynarray *arr = (struct dynarray *) arrp;
	return (void *) (arr->d + i*arr->item_size);
}

/************************************** Hash Table ************************************************/

u64 fnv1a_64(void *data, u64 len)
{
	u8 *p = data;
	u64 prime = 0x00000100000001b3;
	u64 hash = 0xcbf29ce484222325;
	for (size_t i=0; i<len; i++) {
		hash ^= p[i];
		hash *= prime;
	}
	return hash;
}

Hash_Table create_hash_table(Arena *arena, u64 capacity)
{
	Hash_Table res;
	res.d = (Hash_Entry *) adalloc(arena, capacity*sizeof(Hash_Entry));
	memset(res.d, 0, capacity * sizeof(Hash_Entry));
	res.capacity = capacity;
	res.n_entries = 0;
	res.expand_threshold = 0.75f;
	res.copy_keys = true;
	res.arena = arena;
	return res;
}

Hash_Table create_nocopy_hash_table(Arena *arena, u64 capacity)
{
	Hash_Table res;
	res.d = (Hash_Entry *) adalloc(arena, capacity*sizeof(Hash_Entry));
	memset(res.d, 0, capacity * sizeof(Hash_Entry));
	res.capacity = capacity;
	res.n_entries = 0;
	res.expand_threshold = 0.75f;
	res.copy_keys = false;
	// still need arena for expandsion
	res.arena = arena;
	return res;
}

/*
void destroy_hash_table(Hash_Table *table)
{
	free(table->d);
}
*/

bool keys_equal(void *key1, u64 key1_length, void *key2, u64 key2_length)
{
	return key1_length == key2_length && !memcmp(key1, key2, key1_length);
}

// Inner conflict-resolution loop for set and get
Hash_Entry *entry_for_hash(Hash_Table *table, u64 hash, void *key, u64 key_len)
{
	u64 n = table->capacity;
	u64 h_i_start = hash % n;
	u64 h_i = h_i_start;
	Hash_Entry *e = &table->d[h_i];
	while (e->key && !keys_equal(key, key_len, e->key, e->len)) {
		h_i = (h_i + 1) % n;
		if (h_i == h_i_start) {
			return NULL;
		}
		e = &table->d[h_i];
	}
	return e;
}

void expand_hash_table(Hash_Table *table)
{
	u64 old_capacity = table->capacity;
	table->capacity *= 2;
	u64 n = table->capacity;
	Hash_Entry *new_d = adalloc(table->arena, n*sizeof(Hash_Entry));
	memset(new_d, 0, n * sizeof(Hash_Entry));
	for (u64 i=0; i<old_capacity; i++) {
		Hash_Entry *old_e = &table->d[i];
		if (old_e->alive) {
			u64 h = old_e->hash;
			u64 h_i_start = h % n;
			u64 h_i = h_i_start;
			Hash_Entry *e = &new_d[h_i];
			// no equality check; we are definitely inserting for the first time
			while(e->key) {
				h_i = (h_i + 1) % n;
				// xx also no wrap around check? It should be impossible to wrap around in
				// either this case or the regular case, but not clear what to do here.
				assertf(h_i != h_i_start, NULL);
				e = &new_d[h_i];
			}
			memcpy(e, old_e, sizeof(Hash_Entry));
		}
	}
	afree(table->arena, table->d);
	table->d = new_d;
}

bool hash_table_set(Hash_Table *table, void *key, u64 key_len, void *value)
{
	if (!key)
		return false;

	if ((float) table->n_entries + 1.0f > (float) table->capacity * table->expand_threshold)
		expand_hash_table(table);

	u64 h = fnv1a_64(key, key_len);
	Hash_Entry *e = entry_for_hash(table, h, key, key_len);
	if (!e) { // no slots available
		return false;
	} else if (!e->key) { // empty slot
		if (table->copy_keys) {
			e->key = adalloc(table->arena, key_len);
			memcpy(e->key, key, key_len);
		} else {
			e->key = key;
		}
		e->hash = h;
		e->len = key_len;
		e->value = value;
		e->alive = true;
		table->n_entries += 1;
		return true;
	} else { // either dead or already in hash table
		e->value = value;
		e->alive = true;
		return true;
	}
}

void *hash_table_get(Hash_Table *table, void *key, u64 key_len)
{
	if (!key) {
		return NULL;
	}
	u64 h = fnv1a_64(key, key_len);
	Hash_Entry *e = entry_for_hash(table, h, key, key_len);
	if (!e || !e->key || !e->alive) {
		return NULL;
	}
	return e->value;
}

bool hash_table_delete(Hash_Table *table, void *key, u64 key_len)
{
	if (!key)
		return false;
	u64 h = fnv1a_64(key, key_len);
	u64 n = table->capacity;
	u64 h_i_start = h % n;
	u64 h_i = h_i_start;
	Hash_Entry *e = &table->d[h_i];
	while (e->key && !keys_equal(key, key_len, e->key, e->len)) {
		h_i = (h_i + 1) % n;
		if (h_i == h_i_start) {
			return false;
		}
		e = &table->d[h_i];
	}
	if (!e->key || !e->alive) {
		return false;
	} else {
		e->alive = false;
		Hash_Entry *next_entry = &table->d[(h_i + 1) % n];
		// If deleting the last entry in a chain, clear dead entries backwards.
		if (!next_entry->alive) {
			while (e->key && !e->alive) {
				if (table->copy_keys) {
					afree(table->arena, e->key);
				}
				memset(e, 0, sizeof(Hash_Entry));
				h_i = h_i == 0 ? table->capacity - 1 : h_i - 1;
				e = &table->d[h_i];
			}
		}
		table->n_entries--;
		return true;
	}
}

