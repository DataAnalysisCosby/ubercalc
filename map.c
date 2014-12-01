#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "map.h"

struct bucket {
	size_t  hop;    /* Hop bitmap. */
	size_t  dist;   /* Distance to optimal bucket. */
	void    *key, *data;
};

static inline size_t
min(size_t a1, size_t a2)
{
	return (a1 < a2) ? a1 : a2;
}

static inline void
swap(struct bucket *e1, struct bucket *e2)
{
	size_t th;
	struct bucket te;

	te = *e1;
	*e1 = *e2;
	*e2 = te;

	/* Swap back the hop bitmap. */
	th = e1->hop;
	e1->hop = e2->hop;
	e2->hop = th;
}

static size_t next_prime(size_t);
static struct bucket *find_key(struct map *, void *, size_t);
static size_t linear_probe(struct map *, size_t);

int
map_exists(struct map *mp, void *key)
{
	size_t hashv = mp->hash(key) % mp->size;
	return find_key(mp, key, hashv) != NULL;
}

void **
map_get(struct map *mp, void *key)
{
	size_t i, j;
	size_t dist;
	size_t hashv;
	size_t old_size;
	struct bucket *p;

	if (mp->size == 0)
		goto resize;

	/* Check if the key exists already. */
	hashv = mp->hash(key) % mp->size;
	if ((p = find_key(mp, key, hashv)) != NULL)
		/* It does, return its value. */
		return &p->data;

	/*
	 * We couldn't find the key, so we must insert a new key.
	 * First we check if all the buckets are filled, if they are we know
	 * that we must resize.
	 */
	if (mp->len >= mp->size) {
		// ...
		goto resize;
	}

	/*
	 * Linear probe step: Find the closest empty bucket. If it is in the
	 * neighborhood of the buckets[hashv] then we set the hop map of
	 * buckets[hashv] and are done.
	 */
	i = linear_probe(mp, hashv);
	if (mp->buckets[hashv].key != NULL && i == hashv)
		goto resize;

	dist = ((i < hashv)
		? (i + (mp->size - hashv))
		: (i - hashv));

	if (dist < HOP_DIST) {
		/* We are done. Set the hop map and return. */
		mp->len++;
		mp->buckets[hashv].hop |= 1 << dist;
		mp->buckets[i].dist = dist;
		mp->buckets[i].key = key;
		return &mp->buckets[i].data;
	}

	/*
	 * Swap step: we could not find an available bucket in the neighborhood
	 * of the optimal bucket. We need to progressively swap with closer and
	 * closer buckets until we are within a good enough distance. If we
	 * can't swap with anymore buckets, we know that we need to resize.
	 */
	j = i - 1;
	if (i < hashv) {
		for (; j >= 0; j--) {
			size_t pdist = mp->buckets[j].dist;
			size_t new_dist = pdist + (i - j);
			if (new_dist < HOP_DIST) {
				/* We've found an entry that can be swapped. */
				p = mp->buckets + i;
				swap(mp->buckets + j, p);

				/*
				 * Update the distance and hop bitmap for the
				 * swapped out entry.
				 */
				p->dist = new_dist;

				/*
				 * Prevent underflow when accessing base bucket.
				 */
				p = ((new_dist > i)
				     ? (mp->buckets + (mp->size -
						       (new_dist - i)))
				     : (p - new_dist));
				p->hop &= ~(1 << pdist);
				p->hop |= 1 << new_dist;

				/* Update our entry's information. */
				p = mp->buckets + j;
				i = j;

				/* i is less than hashv. */
				dist = i + (mp->size - hashv);

				if (dist < HOP_DIST) {
					mp->len++;
					mp->buckets[hashv].hop |= 1 << dist;
					p->dist = dist;
					p->key = key;
					return &p->data;
				}
			}

			if (j == 0)
				break;
		}
		j = mp->size - 1;
	}

	for (; j > hashv; j--) {
		size_t pdist = mp->buckets[j].dist;
		size_t new_dist = pdist + ((i < j)
					   ? (i + mp->size - j) //check math
					   : (i - j));
		if (new_dist < HOP_DIST) {
			p = mp->buckets + i;
			swap(mp->buckets + j, p);
			p->dist = new_dist;
			p = ((new_dist > i)
			     ? (mp->buckets + (mp->size -
					       (new_dist - i)))
			     : (p - new_dist));
			p->hop &= ~(1 << pdist);
			p->hop |= 1 << new_dist;
			p = mp->buckets + j;
			p->dist = j;
			i = j;

			/* i is greater than hashv. */
			dist = i - hashv;

			if (dist < HOP_DIST) {
				mp->len++;
				mp->buckets[hashv].hop |= 1 << dist;
				p->dist = dist;
				p->key = key;
				return &p->data;
			}
		}
	}

resize:
	/*
	 * There are no empty buckets that we could swap to, so we must resize
	 * the table. I'm unaware of any efficient means to do this.
	 */
	p = mp->buckets;
	old_size = mp->size;
	mp->size = next_prime(mp->size);
	mp->buckets = calloc(mp->size, sizeof(struct bucket));
	if (mp->buckets == NULL)
		/* Could not resize. */
		return NULL;
	mp->len = 0;

	for (i = 0; i < old_size; i++)
		if (p[i].key != NULL)
			*map_get(mp, p[i].key) = p[i].data;

	free(p);

	/*
	 * Add the remaining entry, the one we were trying to add in the first
	 * place.
	 */
	return map_get(mp, key);
}

void
map_copy(struct map *src, struct map *dst)
{
	size_t i;

	dst->len = src->len;
	dst->size = src->size;
	dst->buckets = realloc(dst->buckets, sizeof(struct bucket) * dst->size);

	for (i = 0; i < dst->size; i++)
		dst->buckets[i] = src->buckets[i];
}

/*
void
printtab(struct map *p)
{
	size_t i;

	for (i = 0; i < p->size; i++)
		if (p->buckets[i].key != NULL)
			printf("(%zu) %s : %zu\n", i, (char *)p->buckets[i].key,
			       (size_t) p->buckets[i].data);
}
*/
static size_t
next_prime(size_t n)
{
	static const size_t primetab[] = {
		17ul, 29ul, 37ul, 53ul, 67ul, 79ul, 97ul, 131ul, 193ul, 257ul,
		389ul, 521ul, 769ul, 1031ul, 1543ul, 2053ul, 3079ul, 6151ul,
		12289ul, 24593ul, 49157ul, 98317ul, 196613ul, 393241ul,
		786433ul, 1572869ul, 3145739ul, 6291469ul, 12582917ul,
		25165843ul, 50331653ul, 100663319ul, 201326611ul, 402653189ul,
		805306457ul, 1610612741ul, 3221225473ul, 4294967291ul
	};
	size_t i;

	for (i = 0; primetab[i] <= n; i++)
		;

	return primetab[i];
}

static struct bucket *
find_key(struct map *mp, void *key, size_t base)
{
	size_t i;
	size_t hop;
	size_t lim;
	struct bucket *p;

	hop = mp->buckets[base].hop;
	p = mp->buckets + base;

	lim = min(base + HOP_DIST, mp->size);
	for (i = base; i < lim; i++, p++, hop >>= 1)
		if (hop & 1 && p->key != NULL && mp->compare(p->key, key) == 0)
			return p;

	/* We may have looped around. */
	if (i < base + HOP_DIST) {
		lim = base + HOP_DIST - i;
		p = mp->buckets;
		for (i = 0; i < lim; i++, p++, hop >>= 1)
			if (hop & 1 && p->key != NULL &&
			    mp->compare(p->key, key) == 0)
				return p;
	}

	return NULL;
}

/*
 * Finds an empty bucket starting at base. Doesn't check keys.
 */
static size_t
linear_probe(struct map *mp, size_t start)
{
	size_t i;
	struct bucket *p;

	p = mp->buckets + start;
	for (i = start; i < mp->size; i++, p++)
		if (p->key == NULL)
			return i;

	/* We have looped around, unless start is zero. */
	p = mp->buckets;
	for (i = 0; i < start; i++, p++)
		if (p->key == NULL)
			return i;

	return start;
}
