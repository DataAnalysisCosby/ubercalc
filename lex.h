#ifndef _LEX_H_
#define _LEX_H_

enum token_id {
	Illegal_tok,
	Empty_tok,
	Identifier_tok,
	Number_tok,
	String_tok,
	Paren_op_tok,   /* Opening parenthesis. */
	Paren_cl_tok,   /* Closing paren. */
};

enum token_id lex_token(char **, size_t *);

#endif

