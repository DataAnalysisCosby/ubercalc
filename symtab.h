#ifndef _SYMTAB_H_
#define _SYMTAB_H_

#include "map.h"

typedef struct map symtab;

void symtab_init(symtab *);

static inline void
symtab_clear(symtab *p)
{
	if (p->buckets == NULL)
		return;
	free(p->buckets);
	p->buckets = NULL;
	p->len = p->size = 0;
}

static inline int
sym_exists(symtab *p, size_t sym)
{
	return p->len && map_exists((struct map *)p, (void *)sym);
}

static inline size_t
sym_offset(symtab *p, size_t sym)
{
	if (!sym_exists(p, sym)) {
		size_t offset = p->len;
		*map_get((struct map *)p, (void *)sym) = (void *)offset;
		return offset;
	}
	return (size_t)*map_get((struct map *)p, (void*)sym);
}

/*
 * Does not check if symbol already exists (somewhat dangerous).
 */
static inline size_t
sym_add(symtab *p, size_t sym)
{
	size_t offset = p->len;
	*map_get((struct map *)p, (void *)sym) = (void *)offset;
	return offset;
}

#endif
