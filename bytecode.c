#include <stdlib.h>

#include "bytecode.h"

static inline void
expand_if_needed(struct progm *prog)
{
	if (prog->len + 1 < prog->cap)
		return;
	if (prog->cap == 0)
		prog->cap = 1;
	else
		prog->cap <<= 1;
	prog->code = realloc(prog->code, sizeof(union op_or_imm) * prog->cap);
}

size_t
code_inst(struct progm *prog, enum opcode inst)
{
	expand_if_needed(prog);
	prog->code[prog->len].inst = inst;
	return prog->len++;
}

size_t
code_ui(struct progm *prog, uint32_t imm)
{
	expand_if_needed(prog);
	prog->code[prog->len].ui = imm;
	return prog->len++;
}

size_t
code_si(struct progm *prog, int32_t imm)
{
	expand_if_needed(prog);
	prog->code[prog->len].si = imm;
	return prog->len++;
}

size_t
code_f(struct progm *prog, float imm)
{
	expand_if_needed(prog);
	prog->code[prog->len].f = imm;
	return prog->len++;
}

size_t
code_sym(struct progm *prog, size_t imm)
{
	expand_if_needed(prog);
	prog->code[prog->len].sym = imm;
	return prog->len++;
}

size_t
code_func(struct progm *prog, struct func *imm)
{
	expand_if_needed(prog);
	prog->code[prog->len].func = imm;
	return prog->len++;
}

size_t
code_symtab(struct progm *prog, symtab *imm)
{
	expand_if_needed(prog);
	prog->code[prog->len].symtab = imm;
	return prog->len++;
}
