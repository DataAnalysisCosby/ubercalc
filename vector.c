#include <stdlib.h>

#include "types.h"

/*
 * Append an item to a list. The list takes ownership of the item, so make sure
 * to do any sort of book keeping before calling this function.
 * The capacity of the list grows exponentially.
 */
void
append(struct vector *vp, struct value v)
{
	if (vp->cap == 0)
		vp->cap = 1;
	if (vp->len + 1 >= vp->cap) {
		vp->cap <<= 1;
		vp->items = realloc(vp->items,
				    sizeof(struct value) * vp->cap);
	}

	vp->items[vp->len++] = v;
}

/*
 * Create a slice of a list starting at 'from' and ending at 'to'.
 */
struct slice
slice(struct vector *vp, size_t from, size_t to)
{
	struct slice f = { 0, NULL };
	return f;
}

/*
 * Create a slice of a list ...
 */
struct slice
slice_to(struct vector *lp, size_t to)
{
	struct slice f = { 0, NULL };
	return f;
}

struct slice
slice_from(struct vector *lp, size_t from)
{
	struct slice f = { 0, NULL };
	return f;
}
