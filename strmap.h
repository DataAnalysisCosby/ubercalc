#ifndef _STRMAP_H_
#define _STRMAP_H_

#include <stdlib.h>
#include "map.h"

typedef struct map strmap;

int str_cmp(void *, void *);
size_t str_hash(void *);

#define STRMAP_INIT { 0, 0, NULL, str_cmp, str_hash }

static inline void
strmap_init(strmap *p)
{
	p->compare = str_cmp;
	p->hash = str_hash;
	p->len = p->size = 0;
	p->buckets = NULL;
}

static inline void
strmap_clear(strmap *p)
{
	free(p->buckets);
	p->buckets = NULL;
	p->len = p->size = 0;
}

static inline int
strmap_exists(strmap *p, char *str)
{
	return p->len && map_exists((struct map *)p, (void *)str);
}

static inline void *
strmap_get(strmap *p, char *str)
{
	return map_get(p, (void *)str);
}

#endif
