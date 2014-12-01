#include "builtin.h"
#include "ident.h"

void
init_builtins(void)
{
	static char *builtin_syms[] = {
		"+",
		"define",
		"/",
		".",
		"=",
		">",
		"if",
		"lambda",
		"<",
		"let",
		"list",
		"*",
		"'",
		"set!",
		"-"
	};
	size_t i;

	for (i = 0; i < Num_builtins; i++)
		ident_get_number(builtin_syms[i]);
}
