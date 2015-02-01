#ifndef _ALLOC_H_
#define _ALLOC_H_

#include <stdbool.h>

#include "types.h"

/*
 * Whenever an item is allocated, it is added to a singularly linked list that
 * we call the heap. A new heap should be created every function call, and at
 * the end of the call all items in the heap will be scanned and freed if they
 * can be.
 * The first heap_item in the heap list must be stack allocated. The garbage
 * collection algorithms will assume this.
 */

struct heap_item {
	void                    *data;
	size_t                  locality;
	struct heap_item        *next;
};

// This is a bad idea; Do fix.
extern struct heap_item global_heap_start, *global_heap;

struct list;
struct slice;
struct func;
struct value;

struct func *alloc_func(struct heap_item **);
struct list *alloc_list(struct heap_item **, size_t min_cap);
struct slice *alloc_slice(struct heap_item **);

void clear_heap(struct heap_item *curr_item);

void make_nonlocal(struct heap_item **, void *, size_t);
struct heap_item *mark_heap(struct heap_item **, struct value);

static inline bool
is_heap_allocated(struct value v)
{
	return v.type == List_type || v.type == Slice_type ||
		v.type == Function_type;
}

#endif
