#ifndef _BYTECODE_H_
#define _BYTECODE_H_

#include <stdint.h>

#include "symtab.h"

/*
 * Bytecode is not intended to ever be stored on disk. Immediate values may (and 
 * likely do) contain pointers only valid during the lifetime of the
 * interpreter.
 *
 * Bytecode's sole purpose is to speed up and simplify execution.
 *
 * I used to prefer RISC machines but fuck that. There's no reason to favor
 * elegence in internals.
 */

/*
 * Immediate suffix explanation:
 *      bi      - big integer
 *      br      - big real
 *      ui      - unsigned 32-bit integer
 *      si      - signed 32-bit integer
 *      sym     - symbol
 *      f       - 32-bit float
 *      func    - pointer to a struct func.
 *      symtab  - pointer to a symbol table.
 *
 * These suffixes are inconsistent and must be made consistent.
 */

enum opcode {
	Add2_opcode,
	/*
	AddN_opcode,
	Add_imm_bi_opcode,
	Add_imm_br_opcode,
	Add_imm_f_opcode,
	*/
	Add_imm_si_opcode,
	/*
	Add_imm_ui_opcode,
	*/

	Alloc_list_opcode,
	Alloc_stack_opcode,

	/*
	 * All call instructions have at least one argument that specifies the
	 * number of arguments being passed to the function.
	 */

	Call_opcode,
	Call_current_opcode,
	Call_imm_func_opcode,
	Call_imm_local_opcode,
	Call_imm_nonlocal_opcode,
	Call_imm_sym_opcode,

	Car_opcode,
	Cdr_opcode,

	Clear_opcode,

	Div2_opcode,
	/*
	DivN_opcode,
	Div_imm_bi_opcode,
	Div_imm_br_opcode,
	Div_imm_f_opcode,
	*/
	Div_imm_si_opcode,
	/*
	Div_imm_ui_opcode,
	*/

	Drop_opcode,
	Dup_opcode,     /* Duplicate the item at the top of the stack. */

	Halt_opcode,    /* Similar to Ret_opcode. See implementation. */

	/*
	 * All branch instructions have at least one argument that specifies the
	 * destination of the jump. This immediate is always the first supplied
	 * and is a unsigned integer.
	 *
	 * Bytecode addresses are context dependent. That is to say, a
	 * function's bytecode address 0 is different from a disparate
	 * function's bytecode address 0. Beware of this!
	 */
	Jmp_opcode,     /* Unconditional jump. */
	Jmp_eq_opcode,
	Jmp_eq_imm_si_opcode,
	Jmp_eq_imm_ui_opcode,
	Jmp_false_opcode,
	Jmp_gt_opcode,
	Jmp_gt_imm_si_opcode,
	Jmp_gt_imm_ui_opcode,
	Jmp_lt_opcode,
	Jmp_lt_imm_si_opcode,
	Jmp_lt_imm_ui_opcode,
	Jmp_ne_opcode,
	Jmp_ne_imm_si_opcode,
	Jmp_ne_imm_ui_opcode,
	Jmp_true_opcode,

	Lambda_opcode,  /* Possibly useless? */

	/*
	 * Let requires a symbol table as its only argument.
	 */
	Let_opcode,     /* Creates a new anonymous stack frame. */

	Load_opcode,
	Load_imm_local_opcode,
	Load_imm_nonlocal_opcode,
	Load_imm_sym_opcode,

	Make_list_opcode,
	Make_pair_opcode,

	Mul2_opcode,
	/*
	MulN_opcode,
	Mul_imm_bi_opcode,
	Mul_imm_br_opcode,
	Mul_imm_f_opcode,
	*/
	Mul_imm_si_opcode,
	/*
	Mul_imm_ui_opcode,
	*/

	/*
	Push_imm_bi_opcode,
	Push_imm_br_opcode,
	Push_imm_f_opcode,
	*/
	Push_imm_func_opcode,
	Push_imm_si_opcode,
//	Push_imm_sym_opcode,
	/*
	Push_imm_ui_opcode,
	*/

	Ret_opcode,

	/*
	Sto_opcode,
	*/
	Sto_imm_local_opcode,
	/*
	Sto_imm_local_bi_opcode,
	Sto_imm_local_br_opcode,
	Sto_imm_local_f_opcode,
	*/
	Sto_imm_local_func_opcode,
	Sto_imm_local_si_opcode,
	/*
	Sto_imm_local_sym_opcode,
	Sto_imm_local_ui_opcode,
	*/
	Sto_imm_nonlocal_opcode,
	/*
	Sto_imm_nonlocal_bi_opcode,
	Sto_imm_nonlocal_br_opcode,
	Sto_imm_nonlocal_f_opcode,
	*/
	Sto_imm_nonlocal_func_opcode,
//	Sto_imm_nonlocal_si_opcode,
	/*
	Sto_imm_nonlocal_sym_opcode,
	Sto_imm_nonlocal_ui_opcode,
	Sto_imm_sym_opcode,
	Sto_imm_sym_bi_opcode,
	Sto_imm_sym_br_opcode,
	Sto_imm_sym_f_opcode,
	Sto_imm_sym_func_opcode,
	Sto_imm_sym_si_opcode,
	Sto_imm_sym_sym_opcode,
	Sto_imm_sym_ui_opcode,
	*/

	Sub2_opcode,
	/*
	SubN_opcode,
	Sub_imm_bi_opcode,
	Sub_imm_br_opcode,
	Sub_imm_f_opcode,
	*/
	Sub_imm_si_opcode,
	/*
	Sub_imm_ui_opcode,
	*/

	Yield_opcode,
};

struct func;

struct progm {
	union op_or_imm {
		/* Instruction:         */
		enum opcode     inst;

		/* Immediate values:    */
		uint32_t        ui;
		int32_t         si;
		float           f;
		size_t          sym;
		size_t          o;
		symtab          *symtab;
		struct func     *func;

	}       *code;
	size_t  ip;
	size_t  len;
	size_t  cap;

};

#define NEXT_INST(progm)        ((enum opcode)(progm).code[(progm).ip++].inst)

#define NEXT_IMM_F(progm)       ((float)(progm).code[(progm).ip++].f)
#define NEXT_IMM_SI(progm)      ((int32_t)(progm).code[(progm).ip++].si)
#define NEXT_IMM_UI(progm)      ((uint32_t)(progm).code[(progm).ip++].ui)
#define NEXT_IMM_SYMBOL(progm)  ((size_t)(progm).code[(progm).ip++].sym)
#define NEXT_IMM_OFFSET(progm)  ((size_t)(progm).code[(progm).ip++].o)
#define NEXT_IMM_FUNC(progm)    ((struct func *)(progm).code[(progm).ip++].func)
#define NEXT_IMM_SYMTAB(progm)  ((symtab *)(progm).code[(progm).ip++].func)

/*
 * Each code function returns the index of the added intermmediate/instruction
 * in the program. We cannot return addresses as they may be relocated.
 */

size_t code_f(struct progm *, float);
size_t code_sym(struct progm *, size_t);
size_t code_si(struct progm *, int32_t);
size_t code_ui(struct progm *, uint32_t);
size_t code_symtab(struct progm *, symtab *);
size_t code_inst(struct progm *, enum opcode);
size_t code_func(struct progm *, struct func *);

static inline size_t
code_offset(struct progm *prog, size_t offset)
{
	return code_sym(prog, offset);
}

void disassemble(struct progm prog);

#endif
