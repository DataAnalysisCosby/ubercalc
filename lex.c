#include <stdlib.h>

#include "lex.h"

/*
 * TODO: It is imperative for this file change to support UTF-8.
 */

static enum token_id empty_rule(char **);
static enum token_id number_rule(char **);
static enum token_id identifier_rule(char **);
static enum token_id paren_op_rule(char **);
static enum token_id paren_cl_rule(char **);
static enum token_id newline_rule(char **);

/*
 * lex_token advances src until the end of the token or until the null
 * terminator is reached.
 * Upon reaching the null terminator the Empty_tok will be returned and src will
 * point to the null terminator. Otherwise, src will point to the next character
 * after the current token. The lexer does not check the token for correctness.
 */
enum token_id
lex_token(char **src, size_t *ws)
{
	static enum token_id (*rules[256])(char **) = {
		['\0']          = &empty_rule,
		['0' ... '9']   = &number_rule,
		['a' ... 'z']   = &identifier_rule,
		['A' ... 'Z']   = &identifier_rule,
		['!']           = &identifier_rule,
		['$']           = &identifier_rule,
		['%']           = &identifier_rule,
		['&']           = &identifier_rule,
		['*']           = &identifier_rule,
		['/']           = &identifier_rule,
		[':']           = &identifier_rule,
		['<']           = &identifier_rule,
		['=']           = &identifier_rule,
		['>']           = &identifier_rule,
		['~']           = &identifier_rule,
		['_']           = &identifier_rule,
		['^']           = &identifier_rule,
		['+']           = &identifier_rule,
		['-']           = &identifier_rule,
		['(']           = &paren_op_rule,
		[')']           = &paren_cl_rule,
		['\n']          = &newline_rule,
	};


	/* Remove all whitespace. */
	*ws = 0;
	while (**src == ' ' || **src == '\t') {
		++*src;
		++*ws;
	}

	return rules[(int)**src] == NULL ? Illegal_tok : rules[(int)**src](src);
}

static enum token_id
empty_rule(char **src)
{
	/* Do not advance src. */
	return Empty_tok;
}

static enum token_id
number_rule(char **src)
{
	for (;;) {
		(*src)++;
		switch (**src) {
		default:
			return Number_tok;

		case '0' ... '9':
		case '.':
			break;
		}
	}
	/* NOTREACHED */
	return Illegal_tok;
}

static enum token_id
identifier_rule(char **src)
{
	for (;;) {
		(*src)++;
		switch (**src) {
		default:
			return Identifier_tok;

		case 'a' ... 'z':
		case 'A' ... 'Z':
		case '0' ... '9':
		case '!':
		case '$':
		case '%':
		case '&':
		case '+':
		case '-':
		case '*':
		case '/':
		case ':':
		case '<':
		case '=':
		case '>':
		case '~':
		case '_':
		case '^':
			break;
		}
	}

	/* NOTREACHED */
	return Illegal_tok;
}

static enum token_id
paren_op_rule(char **src)
{
	(*src)++;
	return Paren_op_tok;
}

static enum token_id
paren_cl_rule(char **src)
{
	(*src)++;
	return Paren_cl_tok;
}

static enum token_id
newline_rule(char **src)
{
	(*src)++;
	return Newline_tok;
}
