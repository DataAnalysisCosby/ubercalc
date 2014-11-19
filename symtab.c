#include <stdlib.h>
#include "symtab.h"

static int
cmp(void *a1, void *a2)
{
	return (size_t) a1 - (size_t) a2;
}

static size_t
hash(void *a)
{
	return ((size_t) a) * 2654435761;
}

void
symtab_init(symtab *p)
{
	p->compare = cmp;
	p->hash = hash;
	p->len = p->size = 0;
	p->buckets = NULL;
}
