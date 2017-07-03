#ifndef _TYPES_H_
#define _TYPES_H_

#include "bytecode.h"

/*
 * TODO: make types better. Unsure in what direction to proceed in that regard,
 * but I'm thinking Haskell like type classes?
 * All I know is that right now there is only one function type, which is
 * ridiculous.
 */

enum type {
	Error_type = 0, /* Still deciding how to use this. */
	Nil_type,
	Integer_type,
	Real_type,
	Symbol_type,
	Pair_type,
	/*
	List_type,
	*/
	Vector_type,
	Slice_type,
	Function_type,
};

static inline const char *
type_to_str(enum type id)
{
	static const char *nametab[] = {
		"error",
		"nil",
		"integer",
		"real",
		"symbol",
		"list",
		"vector",
		"vector",
//		"list",         /* Slices are "lists". */
		"function",
		"???",
		"???",
		"???",
	};
	return nametab[id];
}

struct pair;
struct vector;
struct slice;
struct func;

struct value {
	enum type               type;
	union {
		int32_t         i;      /* integer      */
		float           r;      /* real         */
		size_t          sym;    /* symbol       */
		size_t          o;      /* offset       */
//		struct list     *l;     /* list         */
		struct pair     *p;
		struct vector   *v;     /* vector       */
		struct slice    *slice; /* slice        */
		struct func     *f;     /* function     */
	};
};

struct pair {
	struct value    car;
	struct pair     *cdr;
};

struct vector {
	size_t          len, cap;
	struct value    *items;
};

/*
 * Slices are an internal type and appear as vectors to the program. They
 * are immutable references to a portion of a vector. A slice must be
 * copied to a list before it may be modified.
 */
struct slice {
	size_t          len;
	struct value    *start;
};

void append(struct vector *, struct value v);

struct slice slice(struct vector *, size_t from, size_t to);
struct slice slice_to(struct vector *, size_t to);
struct slice slice_from(struct vector *, size_t from);

/*
 * Copy a slice into a new vector. Caller is responsible for cleaning up memory.
 */
struct list *make_vector(struct slice);

struct context;

/*
 * TODO: add way to determine the symbol of a local variable.
 */
struct func {
	struct func     *parent;        /* Perhaps remove? */
	struct vector   *args;          /* Must be a list of symbols. */
	symtab          *locals;
	struct progm    prog;
	struct context  *rt_context;

	struct {
		unsigned int    variadic : 1;
		/*
		 * If the function is a closure, the rt_context will not be
		 * cleared upon return. Instead, only symbols listed in
		 * local_vars will be.
		 */
		unsigned int    closure : 1;
	} flags;

	enum type       return_type;

	/*
	 * Possible idea for clean up: keep track of every variable outside the
	 * scope of the function that has been set at least once.
	 * Then, on tear down, we check the variables and see if any of them
	 * refer to local content. Then copy the data for each variable that
	 * needs it.
	 */
};

/*
 * contexts are runtime environments that specify locations of variable bindings
 * and identifier mappings.
 */
struct context {
	struct value    *local_start;
	struct value    *local_end;
};

#endif
