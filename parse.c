#include <stdlib.h>
#include <ctype.h>

#include "lex.h"
#include "parse.h"
#include "types.h"
#include "ident.h"
#include "alloc.h"

void
newlines(struct source_mapping *srcmap, size_t lines, size_t voffset)
{
	struct vect2src *p = srcmap->vects + srcmap->len - 1;

	if (p->cap == 0)
		p->cap = 1;
	if (p->len + 1 >= p->cap) {
		p->cap <<= 1;
		p->lines = realloc(p->lines, sizeof(struct source_map)
				   * p->cap);
	}

	if (p->len++ == 0)
		p->lines[p->len - 1] =
			((struct source_map){ lines + p->linum, voffset });
	else
		p->lines[p->len - 1] =
			((struct source_map){
				p->lines[p->len - 2].linum + lines, voffset });
}

static void
newvect(struct source_mapping *srcmap)
{
	if (srcmap->cap == 0)
		srcmap->cap = 1;
	if (srcmap->len + 1 >= srcmap->cap) {
		srcmap->cap <<= 1;
		srcmap->vects = realloc(srcmap->vects,
					sizeof(struct vect2src) * srcmap->cap);
	}

	if (srcmap->len++ == 0) {
		srcmap->vects->lines = NULL;
		srcmap->vects->len = srcmap->vects->cap = 0;
		srcmap->vects->linum = 0;
	} else {
		struct vect2src *p = srcmap->vects + srcmap->len - 1;
		if (p[-1].lines == NULL) {
			p--;
			srcmap->len--;
			p->linum += srcmap->residual;
		} else {
			p->lines = NULL;
			p->linum = p[-1].lines[p[-1].len - 1].linum + srcmap->residual;
		}
		srcmap->residual = 0;
		p->len = p->cap = 0;
	}
}

static void
assignvect(struct source_mapping *srcmap, struct vector v)
{
	struct vect2src *p = srcmap->vects + srcmap->len - 1;

	if (p->len == 0)
		/* Quietly remove this entry. */
		srcmap->len--;
	else
		p->src = v;
}


static struct value parse_num(char *);

/*
 * parse a token stream into a nested list structure representative of the
 * syntax. You get how lisp works.
 * next_token provides the next token in the input stream. If the token supplied
 * is either a number or identifier, the src member of the token is expected to
 * point to the string representation of the token. This will be freed as needed 
 * by this function.
 */
struct vector
parse_until(struct token (*next_token)(void), struct source_mapping *srcmap,
	    enum token_id halt_token)
{
	struct value v;
	struct token curr;
	struct vector res;
	struct vector nil_vect = { 0, 0, NULL };


	res.items = NULL;
	res.len = res.cap = 0;

	for (curr = next_token();; curr = next_token()) {
		size_t preceding_lines = 0;

	repeat:
		if (curr.id == halt_token) {
			srcmap->residual = preceding_lines;
			/* Do something. */
			break;
		}
		switch (curr.id) {
		case Newline_tok:
			do {
				preceding_lines++;
				curr = next_token();
			} while (curr.id == Newline_tok);
			goto repeat;

		case Illegal_tok:
		case Empty_tok:
			/*
			 * There was an error in parsing, as we expected the
			 * list to be terminated by halt_token, but one was
			 * never seen.
			 */
			srcmap->residual = preceding_lines;
			return nil_vect;

		case Number_tok:
			/* Parse number. */
			v = parse_num(curr.src);
			if (v.type != Nil_type) {
				free(curr.src);
			}
			/* Otherwise error parsing the number. */
			append(&res, v);
			break;

		case Identifier_tok:
			v.type = Symbol_type;
			v.sym = ident_get_number(curr.src);
			/*
			 * TODO: more parsing here.
			 */
			if (ident_strings[v.sym] != curr.src)
				/*
				 * We should free curr.src only if it is not
				 * owned by ident.c
				 */
				free(curr.src);
			append(&res, v);
			break;

		case Paren_op_tok:
			srcmap->residual += preceding_lines;
			newvect(srcmap);
			v.type = Vector_type;
			v.v = alloc_vector(&global_heap, 0);
			*v.v = parse_until(next_token, srcmap, Paren_cl_tok);
			append(&res, v);
			assignvect(srcmap, *v.v);
			newvect(srcmap);
			continue;

		case Paren_cl_tok:
			/*
			 * There was an error, a closing parenthesis should
			 * never precede an opening parenthesis.
			 */
			break;

		default:
			/* WTF?? */
			break;
		}

		if (preceding_lines != 0)
			newlines(srcmap, preceding_lines, res.len - 1);
	}

	return res;
}

struct vector
parse(struct token (*next_token)(void), struct source_mapping *srcmap)
{
	struct vector v;

	newvect(srcmap);
	v = parse_until(next_token, srcmap, Empty_tok);
	assignvect(srcmap, v);
	return v;
}

/*
 * Parse a number and extract its value. Returns a value of nil type on error.
 * TODO: add support for real/complex/imaginary numbers.
 */
static struct value
parse_num(char *src)
{
	char c;
	struct value v = {
		.type = Integer_type,
		.i = 0,
	};

	while ((c = *src) != '\0') {
		if (!isdigit(c)) {
			v.type = Nil_type;
			return v;
		}
		v.i *= 10;
		v.i += c - '0';
		src++;
	}

	return v;
}
