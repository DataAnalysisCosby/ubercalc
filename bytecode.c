#include <stdio.h>
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

static struct inst_info {
	char *opcode, *args;
} opcodes[] = {
	[Add2_opcode] = { "add2", "" },
	[Add_imm_si_opcode] = { "add", "d" },
	[Alloc_list_opcode] = { "alloc_list", "" },
	[Alloc_stack_opcode] = { "alloc_stack" , "" },
	[Call_opcode] = { "call", "o" },
	[Call_imm_func_opcode] = { "call", "of" },
	[Call_imm_local_opcode] = { "call", "ol" },
	[Call_imm_nonlocal_opcode] = { "call", "on" },
	[Call_imm_sym_opcode] = { "call", "oo" },
	[Car_opcode] = { "car" , "" },
	[Cdr_opcode] = { "cdr" , "" },
	[Clear_opcode] = { "clear", "" },
	[Div2_opcode] = { "div2", "" },
	[Div_imm_si_opcode] = { "div", "d" },
	[Drop_opcode] = { "drop", "" },
	[Dup_opcode] = { "dup", "" },
	[Halt_opcode] = { "halt", "" },
	[Jmp_opcode] = { "jmp", "d" },
	[Jmp_abs_opcode] = { "jmp_abs", "o" },
	[Jmp_eq_opcode] = { "jmp_eq", "d" },
	[Jmp_ne_opcode] = { "jmp_ne", "d" },
	[Lambda_opcode] = { "lambda", "" },
	[Let_opcode] = { "let", "o" },
	[Load_opcode] = { "load", "" },
	[Load_imm_local_opcode] = { "load", "l" },
	[Load_imm_nonlocal_opcode] = { "load", "n" },
	[Load_imm_sym_opcode] = { "load", "s" },
	[Make_list_opcode] = { "list", "o" },
	[Mul2_opcode] = { "mul2", "" },
	[Mul_imm_si_opcode] = { "mul", "d" },
	[Push_imm_func_opcode] = { "push", "f" },
	[Push_imm_si_opcode] = { "push", "d" },
	[Ret_opcode] = { "ret", "" },
	[Sto_imm_local_opcode] = { "sto", "l" },
	[Sto_imm_local_func_opcode] = { "sto", "lf" },
	[Sto_imm_local_si_opcode] = { "sto", "ld" },
	[Sto_imm_nonlocal_opcode] = { "sto", "nd" },
	[Sto_imm_nonlocal_func_opcode] = { "sto", "nf" },
	[Sub2_opcode] = { "sub2", "" },
	[Sub_imm_si_opcode] = { "sub", "d" },
	[Yield_opcode] = { "yield", "" },
};

void
disassemble(struct progm prog)
{
	prog.ip = 0;
	while (prog.ip < prog.len) {
		size_t ip = prog.ip;
		struct inst_info inst = opcodes[NEXT_INST(prog)];
		char *args = inst.args;
		printf("%zu:\t%s\t", ip, inst.opcode);
		for (;*args != '\0';args++)
			switch (*args) {
			case 'd':
				printf("%d\t", NEXT_IMM_SI(prog));
				break;

			case 'o':
				printf("%zu\t", NEXT_IMM_OFFSET(prog));
				break;

			case 'l':
				printf("loc(%zu)\t", NEXT_IMM_OFFSET(prog));
				break;

			case 'n':
			{
				size_t walk, offset;

				walk = NEXT_IMM_OFFSET(prog);
				offset = NEXT_IMM_OFFSET(prog);
				printf("nonl(%zu, %zu)\t", walk, offset);
				break;
			}

			case 'f':
				(void)NEXT_IMM_FUNC(prog);
				printf("func\t");
				break;

			default:
				break;
			}
		printf("\n");
	}
}

