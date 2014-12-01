#include <stdio.h>
#include <stdlib.h>

#include "eval.h"
#include "types.h"
#include "builtin.h"
#include "bytecode.h"

#define STACK_SIZE  0x100000

struct value stack[STACK_SIZE];
struct value *stackp = &stack[0];

#define TOP() (stackp - 1)
#define POP() (*--stackp)
#define PUSH(d) (*stackp++ = (d))

static inline struct func *
walk_env(struct func *env, size_t walk)
{
	while (walk-- > 0)
		env = env->parent;
	return env;
}

static inline struct value *
local(struct func *env, size_t offset)
{
	return env->rt_context->local_start + offset;
}

static inline struct value *
nonlocal(struct func *env, size_t walk, size_t offset)
{
	return walk
		? (walk_env(env, walk)->rt_context->local_start + offset)
		: local(env, offset);
}

static inline struct func *
find_nearest_descendent(struct func *env, struct func *child)
{
	while (child != NULL && child->parent != env)
		child = child->parent;
	return child;
}

void
eval(struct func *env, struct progm *prog)
{
	size_t walk, offset;
	size_t ignored_walks;
	struct func local_func;
	struct progm local_prog = *prog;
	struct context local_context;
	struct value *a1, a2;

	if (env->rt_context != NULL)
		stackp = env->rt_context->local_end;

	/*
	 * When we create a new scope with no new variables, we speed up things
	 * slightly by ignoring those walks.
	 */
	ignored_walks = 0;

	/*
	 * TODO: type checking. Either here or in the compiler.
	 */
	for (;;)
		switch (NEXT_INST(local_prog)) {
		case Add2_opcode:
			a2 = POP();
			a1 = TOP();
			a1->i += a2.i;
			break;

		case Add_imm_si_opcode:
			a1 = TOP();
			a1->i += NEXT_IMM_SI(local_prog);
			break;

		case Alloc_list_opcode:
		case Alloc_stack_opcode:
			fprintf(stderr, "unsupported instruction\n");
			abort();

		case Call_opcode:
		{
			size_t nargs;
			size_t req_args;
			struct func *call;
			struct func *descendent;
			struct value popped;

			nargs = NEXT_IMM_OFFSET(local_prog);
			popped = POP();
			if (popped.type != Function_type) {
				fprintf(stderr, "type error: not function\n");
				abort();
			}
			call = popped.f;
			goto call_func;

		case Call_imm_func_opcode:
			nargs = NEXT_IMM_OFFSET(local_prog);
			call = NEXT_IMM_FUNC(local_prog);
			goto call_func;

		case Call_imm_local_opcode:
			nargs = NEXT_IMM_OFFSET(local_prog);
			call = local(env, NEXT_IMM_OFFSET(local_prog))->f;
			goto call_func;

		case Call_imm_nonlocal_opcode:
			nargs = NEXT_IMM_OFFSET(local_prog);
			walk = NEXT_IMM_OFFSET(local_prog) - ignored_walks;
			offset = NEXT_IMM_OFFSET(local_prog);
			call = nonlocal(env, walk, offset)->f;
			goto call_func;

		case Call_imm_sym_opcode:
			nargs = NEXT_IMM_OFFSET(local_prog);
			(void)NEXT_IMM_OFFSET(local_prog);
			fprintf(stderr, "unsupported instruction\n");
			abort();

		call_func:
			if (call->rt_context != NULL)
				local_context = *call->rt_context;
			else
				call->rt_context = &local_context;

			/* Set up the arguments. */
			req_args = call->args->len - call->flags.variadic;
			if (nargs < req_args) {
				fprintf(stderr,
					"Too few arguments to function.\n");
				abort();
			}
			if (nargs > req_args) {
				if (call->flags.variadic) {
					/* TODO: set up var args. */
					fprintf(stderr, "unsupported!\n");
					abort();
				} else {
					fprintf(stderr,
						"Too many arguments to function.\n");
					abort();
				}
			}

			call->rt_context->local_start = stackp - nargs;
			stackp = call->rt_context->local_end =
				call->rt_context->local_start +
				call->locals->len;

			eval(call, &call->prog);
			call->prog.ip = 0;

			a2 = *TOP();

			/*
			 * Determine if we need to make the returned value a
			 * closure.
			 * TODO: factor this, it's repeated in a couple of
			 * different places.
			 */
			if (a2.type == Function_type &&
			    (descendent = find_nearest_descendent(call, a2.f))
			    != NULL) {
				struct value *p;
				struct func *new_func;

				/*
				 * We need to make the value a closure.
				 * TODO: figure out what assumptions we can make 
				 * to speed this up.
				 * TODO: error checking.
				 */
				a2.f->flags.closure = 1;
				new_func = malloc(sizeof(struct func));
				*new_func = *call;
				new_func->rt_context =
					malloc(sizeof(struct context));
				new_func->rt_context->local_start =
					malloc(sizeof(struct value) *
					       new_func->locals->len);
				new_func->rt_context->local_end =
					new_func->rt_context->local_start;
				for (p = call->rt_context->local_start;
				     p < call->rt_context->local_end;
				     p++) {
					/*
					 * TODO: figure out what to copy and
					 * what to duplicate.
					 */
					*new_func->rt_context->local_end = *p;
					new_func->rt_context->local_end++;
				}
				new_func->flags.closure = 1;
				if (descendent == a2.f) {
					/* Copy the descendent */
					descendent =
						malloc(sizeof(struct func));
					*descendent = *a2.f;
					descendent->rt_context = NULL;
					a2.f = descendent;
				}
				descendent->parent = new_func;
			}


			*call->rt_context->local_start = a2;
			stackp = call->rt_context->local_start + 1;

			if (call->rt_context == &local_context)
				call->rt_context = NULL;
			else
				*call->rt_context = local_context;
			break;
		}

		case Clear_opcode:
			stackp = (env->rt_context != NULL)
				? env->rt_context->local_end
				: &stack[0];
			break;


		case Div2_opcode:
		case Div_imm_si_opcode:
			fprintf(stderr, "unsupported instruction\n");
			abort();

		case Drop_opcode:
			(void)POP();
			break;

		case Dup_opcode:
		{
			/* When to copy, whey to duplicate? */
			struct value v = *TOP();
			PUSH(v);
			break;
		}

		case Halt_opcode:
			*prog = local_prog;
			prog->ip--;
			return;

		case Jmp_eq_opcode:
		case Jmp_eq_imm_si_opcode:
		case Jmp_eq_imm_ui_opcode:
		case Jmp_false_opcode:
		case Jmp_gt_opcode:
		case Jmp_gt_imm_si_opcode:
		case Jmp_gt_imm_ui_opcode:
		case Jmp_lt_opcode:
		case Jmp_lt_imm_si_opcode:
		case Jmp_lt_imm_ui_opcode:
			fprintf(stderr, "unsupported instruction\n");
			abort();

		case Jmp_ne_opcode:
		{
			struct value a1;

			a2 = POP();
			a1 = POP();
			if (a1.i != a2.i)
				goto jmp;
			(void)NEXT_IMM_OFFSET(local_prog);
			break;
		}

		case Jmp_ne_imm_si_opcode:
		case Jmp_ne_imm_ui_opcode:
			fprintf(stderr, "unsupported instruction\n");
			abort();

		case Jmp_true_opcode:
			if (!POP().i) {
				local_prog.ip++;
				break;
			}
			/* Fallthrough */

		case Jmp_opcode:
		jmp:
			local_prog.ip += local_prog.code[local_prog.ip].si + 1;
			break;

		case Jmp_abs_opcode:
			local_prog.ip = local_prog.code[local_prog.ip].si;
			break;

		case Lambda_opcode:
			fprintf(stderr, "unsupported instruction\n");
			abort();

		case Let_opcode:
		{
			symtab *locals;

			locals = NEXT_IMM_SYMTAB(local_prog);
			local_func.parent = env;
			local_func.args = NULL;
			local_func.locals = locals;
			local_func.rt_context = &local_context;
			local_context.local_start = stackp;
			if (locals == NULL) {
				/* Ignore this walk. */
				ignored_walks++;
				local_context.local_end = stackp;
			} else {
				stackp += locals->len;
				local_context.local_end = stackp;
				/* Recurse if we are not ignoring this walk. */
				eval(&local_func, &local_prog);
			}
			break;
		}

		case Load_opcode:
			fprintf(stderr, "unsupported instruction\n");
			abort();

		case Load_imm_local_opcode:
			a1 = local(env, NEXT_IMM_OFFSET(local_prog));
			PUSH(*a1);
			break;

		case Load_imm_nonlocal_opcode:
			walk = NEXT_IMM_OFFSET(local_prog) - ignored_walks;
			offset = NEXT_IMM_OFFSET(local_prog);
			a1 = nonlocal(env, walk, offset);
			PUSH(*a1);
			break;

		case Load_imm_sym_opcode:
			fprintf(stderr, "unsupported instruction\n");
			abort();

		case Make_list_opcode:
			fprintf(stderr, "unsupported instruction\n");
			abort();

		case Mul2_opcode:
			a2 = POP();
			a1 = TOP();
			a1->i *= a2.i;
			break;

		case Mul_imm_si_opcode:
			a1 = TOP();
			a1->i *= NEXT_IMM_SI(local_prog);
			break;

		case Push_imm_func_opcode:
			a2.type = Function_type;
			a2.f = NEXT_IMM_FUNC(local_prog);
			PUSH(a2);
			break;

		case Push_imm_si_opcode:
			a2.type = Integer_type;
			a2.i = NEXT_IMM_SI(local_prog);
			PUSH(a2);
			break;

		case Ret_opcode:
			/* Do not overwrite progm. */
			return;

		case Sto_imm_local_opcode:
			a1 = local(env, NEXT_IMM_OFFSET(local_prog));
			*a1 = POP();
			break;

		case Sto_imm_local_si_opcode:
			a1 = local(env, NEXT_IMM_OFFSET(local_prog));
			a1->type = Integer_type;
			a1->i = NEXT_IMM_SI(local_prog);
			break;

		case Sto_imm_local_func_opcode:
		{
			struct func *f;

			a1 = local(env, NEXT_IMM_OFFSET(local_prog));
			f = NEXT_IMM_FUNC(local_prog);
			a1->type = Function_type;
			a1->f = f;
			break;
		}

		case Sto_imm_nonlocal_opcode:
		{
			struct func *descendent;

			walk = NEXT_IMM_OFFSET(local_prog) - ignored_walks;
			offset = NEXT_IMM_OFFSET(local_prog);
			a1 = nonlocal(env, walk, offset);
			a2 = POP();

			if (a2.type == Function_type &&
			    (descendent = find_nearest_descendent(env, a2.f))
			    != NULL) {
				struct value *p;
				struct func *new_func;

				/*
				 * We need to make the value a closure.
				 * TODO: figure out what assumptions we can make 
				 * to speed this up.
				 * TODO: error checking.
				 */
				a2.f->flags.closure = 1;
				new_func = malloc(sizeof(struct func));
				*new_func = *env;
				new_func->rt_context =
					malloc(sizeof(struct context));
				new_func->rt_context->local_start =
					malloc(sizeof(struct value) *
					       new_func->locals->len);
				new_func->rt_context->local_end =
					new_func->rt_context->local_start;
				for (p = env->rt_context->local_start;
				     p < env->rt_context->local_end;
				     p++) {
					/*
					 * TODO: figure out what to copy and
					 * what to duplicate.
					 */
					*new_func->rt_context->local_end = *p;
					new_func->rt_context->local_end++;
				}
				new_func->flags.closure = 1;
				if (descendent == a2.f) {
					/* Copy the descendent */
					descendent =
						malloc(sizeof(struct func));
					*descendent = *a2.f;
					descendent->rt_context = NULL;
					a2.f = descendent;
				}

				descendent->parent = new_func;
			}

			*a1 = a2;
			break;
		}

		case Sto_imm_nonlocal_func_opcode:
		{
			struct func *f;

			walk = NEXT_IMM_OFFSET(local_prog) - ignored_walks;
			offset = NEXT_IMM_OFFSET(local_prog);
			a1 = nonlocal(env, walk, offset);
			f = NEXT_IMM_FUNC(local_prog);
			a1->type = Function_type;
			a1->f = f;
			break;
		}

		case Sub2_opcode:
			a2 = POP();
			a1 = TOP();
			a1->i -= a2.i;
			break;

		case Sub_imm_si_opcode:
			a1 = TOP();
			a1->i -= NEXT_IMM_SI(local_prog);
			break;

		case Yield_opcode:
			if (ignored_walks == 0) {
				*prog = local_prog;
				return;
			}

			/*
			 * Otherwise, we have have ignored walks we need to
			 * remove.
			 */
			ignored_walks--;
//			env = env->parent;
			break;

		default:
			fprintf(stderr, "unsupported!!\n");
		}
}
