#ifndef _PARSE_H_
#define _PARSE_H_

#include "types.h"

struct token {
	enum token_id   id;
	char *          src;
};

/*
 * Maps line numbers to parsed elements.
 * A list of source maps is a source mapping.
 * Source maps are always sorted by linum, smallest to largest.
 */
struct source_mapping {
	struct vect2src {
		struct source_map {
			size_t  linum, voffset;
		}               *lines;
		size_t          linum;
		size_t          len, cap;
		struct vector   src;
	}       *vects;
	size_t  len, cap;
	size_t  residual;
};

struct vector parse(struct token (*)(void), struct source_mapping *);

#endif
