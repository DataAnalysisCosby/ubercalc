#include <stdlib.h>
#include <ctype.h>

#include "lex.h"
#include "parse.h"
#include "types.h"
#include "ident.h"

static struct value parse_num(char *);

/*
 * parse a token stream into a nested list structure representative of the
 * syntax. You get how lisp works.
 * next_token provides the next token in the input stream. If the token supplied
 * is either a number or identifier, the src member of the token is expected to
 * point to the string representation of the token. This will be freed as needed 
 * by this function.
 */
struct list
parse_until(struct token (*next_token)(void), enum token_id halt_token)
{
	struct list nil_list = { 0, 0, NULL };
	struct list res;
	struct token curr;
	struct value v;

	res.items = NULL;
	res.len = res.cap = 0;

	for (curr = next_token();; curr = next_token()) {
		if (curr.id == halt_token) {
			/* Do something. */
			break;
		}
		switch (curr.id) {
		case Illegal_tok:
		case Empty_tok:
			/*
			 * There was an error in parsing, as we expected the
			 * list to be terminated by halt_token, but one was
			 * never seen.
			 */
			return nil_list;

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
			if (ident_strings[v.sym] != curr.src)
				/*
				 * We should free curr.src only if it is not
				 * owned by ident.c
				 */
				free(curr.src);
			append(&res, v);
			break;

		case Paren_op_tok:
			v.type = List_type;
			v.l = malloc(sizeof (struct list));
			*v.l = parse_until(next_token, Paren_cl_tok);
			append(&res, v);
			break;

		case Paren_cl_tok:
			/*
			 * There was an error, a closing parenthesis should never
			 * precede an opening parenthesis.
			 */
			break;

		default:
			/* WTF?? */
			break;
		}
	}

	return res;
}

struct list
parse(struct token (*next_token)(void))
{
	return parse_until(next_token, Empty_tok);
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
