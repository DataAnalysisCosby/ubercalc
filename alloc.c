#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "types.h"

struct heap_item global_heap_start = { NULL, 0, NULL, };
struct heap_item *global_heap = &global_heap_start;

/*
 * TODO:
 *  - We should consider a way to reduce the number of allocations we make for
 *    the heap items as much as possible.
 *  - Fix error handling (for now we just return null).
 */

struct value *
alloc_value(struct heap_item **heap)
{
	struct value *v;
	(*heap)->next = malloc(sizeof (struct heap_item));
	if ((*heap)->next == NULL)
		return NULL;
	if ((v = malloc(sizeof (struct value))) == NULL)
		return NULL;
	memset(v, 0, sizeof (struct value));
	(*heap)->data = v;
	(*heap)->next->data = (*heap)->next->next = NULL;
	(*heap)->next->locality = 0;
	*heap = (*heap)->next;
	return v;
}

struct func *
alloc_func(struct heap_item **heap)
{
	struct func *f;
	(*heap)->next = malloc(sizeof (struct heap_item));
	if ((*heap)->next == NULL)
		return NULL;
	if ((f = malloc(sizeof (struct func))) == NULL)
		return NULL;
	memset(f, 0, sizeof (struct func));
	(*heap)->data = f;
	(*heap)->next->data = (*heap)->next->next = NULL;
	(*heap)->next->locality = 0;
	*heap = (*heap)->next;
	f->args = alloc_vector(heap, 1);
	return f;
}

struct pair *
alloc_pair(struct heap_item **heap)
{
	struct pair *p;
	(*heap)->next = malloc(sizeof (struct heap_item));
	if ((*heap)->next == NULL)
		return NULL;
	if ((p = malloc(sizeof (struct pair))) == NULL)
		return NULL;
	memset(p, 0, sizeof (struct pair));
	(*heap)->data = p;
	(*heap)->next->data = (*heap)->next->next = NULL;
	(*heap)->next->locality = 0;
	*heap = (*heap)->next;
	return p;
}

struct vector *
alloc_vector(struct heap_item **heap, size_t min_cap)
{
	struct vector *v;
	(*heap)->next = malloc(sizeof (struct heap_item));
	if ((*heap)->next == NULL)
		return NULL;
	if ((v = malloc(sizeof (struct vector))) == NULL)
		return NULL;
	v->len = 0;
	v->cap = min_cap;
	if ((v->items = calloc(min_cap, sizeof(struct value))) == NULL)
		return NULL;
	(*heap)->data = v;
	(*heap)->next->data = (*heap)->next->next = NULL;
	(*heap)->next->locality = 0;
	*heap = (*heap)->next;
	return v;
}

struct slice *
alloc_slice(struct heap_item **heap)
{
	struct slice *s;
	(*heap)->next = malloc(sizeof (struct heap_item));
	if ((*heap)->next == NULL)
		return NULL;
	if ((s = malloc(sizeof (struct slice))) == NULL)
		return NULL;
	s->len = 0;
	s->start = NULL;
	(*heap)->data = s;
	(*heap)->next->data = (*heap)->next->next = NULL;
	(*heap)->next->locality = 0;
	*heap = (*heap)->next;
	return s;
}

/*
 * Free the entire heap.
 */
void
clear_heap(struct heap_item *curr_item)
{
	while (curr_item != NULL && curr_item->data != NULL) {
		void *f1 = curr_item, *f2 = curr_item->data;
		curr_item = curr_item->next;
		free(f1);
		free(f2);
	}
	free(curr_item);
}

/*
 * Removes an item from the heap.
 */
static struct heap_item *
remove_item(struct heap_item *heap_start, void *item)
{
	struct heap_item *p, **prevp;

	/*
	 * Check if the item is the first in the heap. If it is we'll need to
	 * reclaim some new space for its return value.
	 */
	if (heap_start->data == item) {
		struct heap_item saved = *heap_start;
		p = heap_start->next;
		*heap_start = *p;
		*p = saved;
		return p;
	}

	prevp = &heap_start->next;
	for (p = *prevp; p->data != NULL; prevp = &p->next, p = p->next)
		if (p->data == (void *)item) {
			*prevp = p->next;
			return p;
		}
	return NULL;
}

static struct heap_item *
remove_nonlocals(struct heap_item *heap_start)
{
	struct heap_item *p, *saved = NULL;

	if (heap_start == NULL)
		return NULL;
	while (heap_start->locality > 0) {
		struct heap_item curr = *heap_start;
		curr.locality--;
		p = heap_start->next;
		*heap_start = *heap_start->next;
		*p = curr;
		p->next = saved;
		saved = p;
	}

	return saved;
}

void
make_nonlocal(struct heap_item *heap_start, void *item, size_t walk)
{
	struct heap_item *p, **prevp;

	if (heap_start->data == item) {
		if (heap_start->locality < walk)
			heap_start->locality = walk;
		return;
	}
	prevp = &heap_start->next;
	for (p = *prevp; p != NULL; prevp = &p->next, p = p->next)
		if (p->data == item && p->locality < walk) {
			struct heap_item saved;

			p->locality = walk;
			/*
			 * Move the nonlocal to the front so that they are easy
			 * to pick off.
			 */
			*prevp = p->next;
			saved = *heap_start;
			*heap_start = *p;
			heap_start->next = p;
			*p = saved;
			return;
		}
}

/*
 * Finds and returns a vector of values that must be saved on the heap should
 * some value be retained. Items that must be saved are removed from the heap
 * and are put in a vector.
 */
struct heap_item *
mark_heap(struct heap_item *heap_start, struct value retained)
{
	size_t i;
	struct value next;
	struct heap_item *r, *p = NULL;

	switch (retained.type) {
	case Pair_type:
		if (retained.p == NULL)
			break;
		if ((r = remove_item(heap_start, retained.p)) != NULL) {
			r->next = NULL;
			p = r;
		}
		if ((r = mark_heap(heap_start, retained.p->car)) != NULL) {
			r->next = p;
			p = r;
		}
		next.type = Pair_type;
		next.p = retained.p->cdr;
		if (retained.p->cdr != NULL &&
		    (r = mark_heap(heap_start, next)) != NULL) {
			r->next = p;
			p = r;
		}
		break;

	case Function_type:
		if ((r = remove_item(heap_start, retained.f)) != NULL) {
			r->next = NULL;
			p = r;
		}
		if ((r = remove_item(heap_start, retained.f->args)) != NULL) {
			r->next = p;
			p = r;
		}
		break;

	case Vector_type:
		if ((r = remove_item(heap_start, retained.v)) != NULL) {
			r->next = NULL;
			p = r;
		}
		for (i = 0; i < retained.v->len; i++) {
			r = mark_heap(heap_start, retained.v->items[i]);
			if (r != NULL) {
				r->next = p;
				p = r;
			}
		}
		break;

	case Slice_type:
		if ((r = remove_item(heap_start, retained.slice)) != NULL) {
			r->next = NULL;
			p = r;
		}
		for (i = 0; i < retained.slice->len; i++) {
			r = mark_heap(heap_start, retained.slice->start[i]);
			if (r != NULL) {
				r->next = p;
				p = r;
			}
		}
		break;

	default:
		break;
	}

	if ((r = remove_nonlocals(heap_start)) != NULL) {
		r->next = p;
		p = r;
	}

	return p;
}
