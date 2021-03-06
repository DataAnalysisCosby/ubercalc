#include <stdio.h>
#include <stdlib.h>
#include <histedit.h>

#include "alloc.h"
#include "types.h"
#include "lex.h"
#include "parse.h"
#include "comp.h"
#include "builtin.h"

/*
 * This interface is purely for testing and should be removed ASAP.
 */

char *input = NULL;
int pcount;
EditLine *el;

struct token
get_next_token(void)
{
	size_t ws;
	struct token tok;
	char *news;
	char *p, *prev;

	prev = input;
	ws = 0;

relex:
	switch (tok.id = lex_token(&input, &ws)) {
	case Number_tok:
	case Identifier_tok:
		prev += ws;
		news = malloc(input - prev + 1);
		p = news;
		while (prev != input)
			*p++ = *prev++;
		*p = '\0';
		tok.src = news;
		break;

	case Paren_op_tok:
		pcount++;
		break;

	case Paren_cl_tok:
		pcount--;
		break;

	case Empty_tok:
		if (pcount != 0) {
			int ignore;
			prev = input = el_gets(el, &ignore);
			goto relex;
		}
		break;

	default:
		fprintf(stderr, "Token id = %d\n", tok.id);
//		fprintf(stderr, "Lexing error?\n");
		break;
	}
	return tok;
}

char *
prompt(EditLine *e)
{
	return pcount == 0 ? "> " : "  ";
}

struct func global;

struct heap_item eval(struct func *, struct progm *);
extern struct value stack[0x100000];
extern struct value *stackp;

int
main(int argc, char **argv)
{
	int ignore;
	char *line;
//	size_t num_vars = 0;
	struct vector code;
	struct context local_context;
	struct source_mapping srcmap = { NULL, 0, 0, 0 };

	el = el_init(argv[0], stdin, stdout, stderr);
	el_set(el, EL_PROMPT, &prompt);
	el_set(el, EL_EDITOR, "emacs");

	global.args = alloc_vector(&global_heap, 1);
	global.prog.ip = global.prog.len = global.prog.cap = 0;
	global.prog.code = NULL;
	global.rt_context = &local_context;
	local_context.local_start = &stack[0];

	set_compiler_global_context(&global);

	init_builtins();

	for (;;) {
		line = el_gets(el, &ignore);
		if (line == NULL || ignore <= 0)
			return 1;
		input = line;
		code = parse(&get_next_token, &srcmap);
		global.rt_context->local_end =
			global.rt_context->local_start +
			global.locals->len + global.args->len;

		if (compile(&global.prog, code.items->v) == Error_type) {
			fprintf(stderr, "There was an error!\n");
		}
/*		while (num_vars < global.locals->len) {
			global.rt_context->local_end->type = Nil_type;
			global.rt_context->local_end++;
			num_vars++;
			} */
		code_inst(&global.prog, Halt_opcode);
		eval(&global, &global.prog);
		if (stackp != &stack[0])
			printf("stackp = %d\n", (stackp - 1)->i);
		global.prog.len--;
	}

	if (global_heap_start.data != NULL) {
		free(global_heap_start.data);
		clear_heap(global_heap_start.next);
	}

	return 0;
}
