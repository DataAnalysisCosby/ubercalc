#include <stdlib.h>

#include "types.h"

/*
 * Append an item to a list. The list takes ownership of the item, so make sure
 * to do any sort of book keeping before calling this function.
 * The capacity of the list grows exponentially.
 */
void
append(struct list *lp, struct value v)
{
	if (lp->cap == 0)
		lp->cap = 1;
	if (lp->len + 1 >= lp->cap) {
		lp->cap <<= 1;
		lp->items = realloc(lp->items,
				    sizeof(struct value) * lp->cap);
	}

	lp->items[lp->len++] = v;
}

/*
 * Create a slice of a list starting at 'from' and ending at 'to'.
 */
struct slice
slice(struct list *lp, size_t from, size_t to)
{
	struct slice f = { 0, NULL };
	return f;
}

/*
 * Create a slice of a list ...
 */
struct slice
slice_to(struct list *lp, size_t to)
{
	struct slice f = { 0, NULL };
	return f;
}

struct slice
slice_from(struct list *lp, size_t from)
{
	struct slice f = { 0, NULL };
	return f;
}
