#ifndef _MAP_H_
#define _MAP_H_

/*
 * HOP_DIST, more commonly referred to as the constant H is the number of
 * possible neighbor each bucket has. This number should somewhat correspond
 * with cache lines, probably.
 */
#define HOP_DIST 32

/*
 * maps word-size hash values to void pointers.
 * Possible optimizaton: make nbuckets always a power of two. Makes looping
 * easier.
 */
struct map {
	size_t          len;    /* Number of full buckets. */
	size_t          size;
	struct bucket   *buckets;
	int             (*compare)(void *, void *);
	size_t          (*hash)(void *);
};

void **map_get(struct map *, void *);
int map_exists(struct map *, void *);

#endif
