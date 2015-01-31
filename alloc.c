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
	f->args = alloc_list(heap, 1);
	return f;
}

struct list *
alloc_list(struct heap_item **heap, size_t min_cap)
{
	struct list *l;
	(*heap)->next = malloc(sizeof (struct heap_item));
	if ((*heap)->next == NULL)
		return NULL;
	if ((l = malloc(sizeof (struct list))) == NULL)
		return NULL;
	l->len = 0;
	l->cap = min_cap;
	if ((l->items = calloc(min_cap, sizeof(struct value))) == NULL)
		return NULL;
	(*heap)->data = l;
	(*heap)->next->data = (*heap)->next->next = NULL;
	(*heap)->next->locality = 0;
	*heap = (*heap)->next;
	return l;
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
remove_item(struct heap_item **heap_start, void *item)
{
	struct heap_item *p, **prevp;

	prevp = heap_start;
	for (p = *heap_start; p->data != NULL; prevp = &p->next, p = p->next)
		if (p->data == (void *)item) {
			*prevp = p->next;
			return p;
		}
	return NULL;
}

static struct heap_item *
remove_nonlocals(struct heap_item **heap_start)
{
	struct heap_item *p, *saved = NULL;

	if (*heap_start == NULL)
		return NULL;
	while ((p = *heap_start)->locality > 0) {
		p->locality--;
		*heap_start = p->next;
		p->next = saved;
		saved = p;
	}

	return saved;
}

void
make_nonlocal(struct heap_item **heap_start, void *item, size_t walk)
{
	struct heap_item *p, **prevp;

	prevp = heap_start;
	for (p = *heap_start; p != NULL; prevp = &p->next, p = p->next)
		if (p->data == item && p->locality < walk) {
			p->locality = walk;
			/*
			 * Move the nonlocal to the front so that they are easy
			 * to pick off.
			 */
			*prevp = p->next;
			p->next = *heap_start;
			*heap_start = p;
			return;
		}
}

/*
 * Finds and returns a list of values that must be saved on the heap should
 * some value be retained. Items that must be saved are removed from the heap
 * and are put in a list.
 */
struct heap_item *
mark_heap(struct heap_item **heap_start, struct value retained)
{
	size_t i;
	struct heap_item *r, *p = NULL;

	switch (retained.type) {
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

	case List_type:
		if ((r = remove_item(heap_start, retained.l)) != NULL) {
			r->next = NULL;
			p = r;
		}
		for (i = 0; i < retained.l->len; i++) {
			r = mark_heap(heap_start, retained.l->items[i]);
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
