#include <stdio.h>
#include <stdlib.h>

#include "eval.h"
#include "alloc.h"
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

struct heap_item *
eval(struct func *env, struct progm *prog)
{
	size_t ignored_walks;
	struct func local_func;
	struct progm local_prog = *prog;
	struct context local_context;
	struct heap_item *heap_start, *curr_heap;

#define INST(n) [n##_opcode] = &&INST_##n
	static const void *inst_tab[] = {
		INST(Add2), INST(Add_imm_si), INST(Alloc_list),
		INST(Alloc_stack), INST(Call), INST(Call_imm_func),
		INST(Call_imm_local), INST(Call_imm_nonlocal),
		INST(Call_imm_sym), INST(Car), INST(Cdr), INST(Clear),
		INST(Div2), INST(Div_imm_si), INST(Drop), INST(Dup), INST(Halt),
		INST(Jmp), INST(Jmp_abs), INST(Jmp_eq), INST(Jmp_eq_imm_si),
		INST(Jmp_eq_imm_ui), INST(Jmp_false), INST(Jmp_gt),
		INST(Jmp_gt_imm_si), INST(Jmp_gt_imm_ui), INST(Jmp_lt),
		INST(Jmp_lt_imm_si), INST(Jmp_lt_imm_ui), INST(Jmp_ne),
		INST(Jmp_ne_imm_si), INST(Jmp_ne_imm_ui), INST(Jmp_true),
		INST(Lambda), INST(Let), INST(Load), INST(Load_imm_local),
		INST(Load_imm_nonlocal), INST(Load_imm_sym), INST(Make_list),
		INST(Mul2), INST(Mul_imm_si), INST(Push_imm_func),
		INST(Push_imm_si), INST(Ret), INST(Sto_imm_local),
		INST(Sto_imm_local_si), INST(Sto_imm_local_func),
		INST(Sto_imm_nonlocal), INST(Sto_imm_nonlocal_func), INST(Sub2),
		INST(Sub_imm_si), INST(Yield),
	};

#define DEF_INST(n) INST_##n:
#define RUN_NEXT_INST() do { goto *inst_tab[NEXT_INST(local_prog)]; } while (0)

#define UNIMPLEMENTED_INST(n) INST_##n:					\
	do { fprintf(stderr, "Instruction " #n " is unsupported\n");	\
		abort(); } while(0)

	heap_start = curr_heap = NULL;
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

	/* Jump to the first instruction. */
	goto *inst_tab[NEXT_INST(local_prog)];

	/*
	 * Instruction implementations:
	 */
	DEF_INST(Add2) {
		struct value *a1, a2;
		a2 = POP();
		a1 = TOP();
		a1->i += a2.i;

		RUN_NEXT_INST();
	}

	DEF_INST(Add_imm_si) {
		struct value *a;
		a = TOP();
		a->i += NEXT_IMM_SI(local_prog);

		RUN_NEXT_INST();
	}

	UNIMPLEMENTED_INST(Alloc_list);
	UNIMPLEMENTED_INST(Alloc_stack);

	/*
	 * Call instructions.
	 */
	{
		size_t nargs;
		size_t req_args;
		struct func *call;
		struct func *descendent;
		struct value popped, returned;
		struct heap_item *heap, *marked;

		DEF_INST(Call) {
			nargs = NEXT_IMM_OFFSET(local_prog);
			popped = POP();
			if (popped.type != Function_type) {
				fprintf(stderr, "type error: not function\n");
				abort();
			}
			call = popped.f;
			goto call_func;
		}

		DEF_INST(Call_imm_func) {
			nargs = NEXT_IMM_OFFSET(local_prog);
			call = NEXT_IMM_FUNC(local_prog);
			goto call_func;
		}

		DEF_INST(Call_imm_local) {
			nargs = NEXT_IMM_OFFSET(local_prog);
			call = local(env, NEXT_IMM_OFFSET(local_prog))->f;
			goto call_func;
		}

		DEF_INST(Call_imm_nonlocal) {
			size_t walk, offset;

			nargs = NEXT_IMM_OFFSET(local_prog);
			walk = NEXT_IMM_OFFSET(local_prog) - ignored_walks;
			offset = NEXT_IMM_OFFSET(local_prog);
			call = nonlocal(env, walk, offset)->f;
			goto call_func;
		}

		UNIMPLEMENTED_INST(Call_imm_sym);

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

		heap = eval(call, &call->prog);
		call->prog.ip = 0;

		if (stackp == call->rt_context->local_end)
			/* No return value. */
			returned.type = Nil_type;
		else
			returned = *TOP();

		/*
		 * Determine if we need to make the returned value a
		 * closure.
		 * TODO: factor this, it's repeated in a couple of
		 * different places.
		 */
		if (returned.type == Function_type &&
		    (descendent = find_nearest_descendent(call, returned.f))
		    != NULL) {
			struct value *p;
			struct func *new_func;

			/*
			 * We need to make the value a closure.
			 * TODO: figure out what assumptions we can make to
			 * speed this up.
			 * TODO: error checking.
			 */
			returned.f->flags.closure = 1;

			if (heap_start == NULL) {
				heap_start = curr_heap = malloc(sizeof (struct heap_item));
				*heap_start = ((struct heap_item){ NULL, 0, NULL, });
			}
			new_func = alloc_func(&curr_heap);
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
			if (descendent == returned.f) {
				/* Copy the descendent */
				descendent = alloc_func(&curr_heap);
				*descendent = *returned.f;
				descendent->rt_context = NULL;
				returned.f = descendent;
			}
			descendent->parent = new_func;
		}

		/* Garbage collect the function. */
		marked = mark_heap(&heap, returned);
		/* Free the remaining heap. */
		if (heap != NULL)
			clear_heap(heap);
		/* Add the marked data into the current heap. */
		if (marked != NULL) {
			for (curr_heap->next = marked;
			     curr_heap->next != NULL;
			     curr_heap = curr_heap->next)
				;
		}

		*call->rt_context->local_start = returned;
		stackp = call->rt_context->local_start + 1;

		if (call->rt_context == &local_context)
			call->rt_context = NULL;
		else
			*call->rt_context = local_context;

		RUN_NEXT_INST();
	}


	DEF_INST(Car) {
		struct value v = POP();
		/*
		 * TODO: we better make sure we can always assume it's a
		 * list.
		 */
		switch (v.type) {
		case List_type:
			PUSH(v.l->items[0]);
			break;

		case Slice_type:
			PUSH(v.slice->start[0]);
			break;

		default:
			fprintf(stderr, "Cannot car a non-list type.");
			break;
		}
		RUN_NEXT_INST();
	}


	DEF_INST(Cdr) {
		struct value v = POP();
		struct value s = { .type = Slice_type, };

		if (heap_start == NULL) {
			heap_start = curr_heap = malloc(sizeof (struct heap_item));
			*heap_start = ((struct heap_item){ NULL, 0, NULL, });
		}
		s.slice = alloc_slice(&curr_heap);
		switch (v.type) {
		case List_type:
			if (v.l->len == 0)
				break;
			s.slice->len = v.l->len - 1;
			s.slice->start = v.l->items + 1;
			break;

		case Slice_type:
			if (v.slice->len == 0)
				break;
			s.slice->len = v.slice->len - 1;
			s.slice->start = v.slice->start + 1;
			break;

		default:
			fprintf(stderr, "Cannot cdr a non-list type.");
			break;
		}
		PUSH(s);
		RUN_NEXT_INST();
	}

	DEF_INST(Clear) {
		stackp = (env->rt_context != NULL)
			? env->rt_context->local_end
			: &stack[0];
		RUN_NEXT_INST();
	}

	UNIMPLEMENTED_INST(Div2);
	UNIMPLEMENTED_INST(Div_imm_si);

	DEF_INST(Drop) {
		(void)POP();
		RUN_NEXT_INST();
	}

	DEF_INST(Dup) {
		/* When to copy, whey to duplicate? */
		struct value v = *TOP();
		PUSH(v);
		RUN_NEXT_INST();
	}

	DEF_INST(Halt) {
		*prog = local_prog;
		prog->ip--;
		return heap_start;
		RUN_NEXT_INST();
	}

	DEF_INST(Jmp) {
		local_prog.ip += local_prog.code[local_prog.ip].si + 1;
		RUN_NEXT_INST();
	}

	DEF_INST(Jmp_abs) {
		local_prog.ip = local_prog.code[local_prog.ip].o;
		RUN_NEXT_INST();
	}

	UNIMPLEMENTED_INST(Jmp_eq);
	UNIMPLEMENTED_INST(Jmp_eq_imm_si);
	UNIMPLEMENTED_INST(Jmp_eq_imm_ui);
	UNIMPLEMENTED_INST(Jmp_false);
	UNIMPLEMENTED_INST(Jmp_gt);
	UNIMPLEMENTED_INST(Jmp_gt_imm_si);
	UNIMPLEMENTED_INST(Jmp_gt_imm_ui);
	UNIMPLEMENTED_INST(Jmp_lt);
	UNIMPLEMENTED_INST(Jmp_lt_imm_si);
	UNIMPLEMENTED_INST(Jmp_lt_imm_ui);


	DEF_INST(Jmp_ne) {
		struct value a1, a2;

		a2 = POP();
		a1 = POP();
		if (a1.i != a2.i) {
			local_prog.ip += local_prog.code[local_prog.ip].si + 1;
			RUN_NEXT_INST();
		}
		(void)NEXT_IMM_OFFSET(local_prog);
		RUN_NEXT_INST();
	}

	UNIMPLEMENTED_INST(Jmp_ne_imm_si);
	UNIMPLEMENTED_INST(Jmp_ne_imm_ui);

	DEF_INST(Jmp_true) {
		if (POP().i)
			local_prog.ip += local_prog.code[local_prog.ip].si + 1;
		RUN_NEXT_INST();
	}

	UNIMPLEMENTED_INST(Lambda);

	DEF_INST(Let) {
		symtab *locals;

		if ((locals = NEXT_IMM_SYMTAB(local_prog)) == NULL) {
			/* Ignore this walk. */
			ignored_walks++;
			local_context.local_end = stackp;
			RUN_NEXT_INST();
		}

		local_func.parent = env;
		local_func.args = NULL;
		local_func.locals = locals;
		local_func.rt_context = &local_context;
		local_context.local_start = stackp;
		stackp += locals->len;
		local_context.local_end = stackp;
		/* TODO: garbage collection here. */
		eval(&local_func, &local_prog);
		RUN_NEXT_INST();
	}


	UNIMPLEMENTED_INST(Load);

	DEF_INST(Load_imm_local) {
		struct value *v;
		v = local(env, NEXT_IMM_OFFSET(local_prog));
		PUSH(*v);
		RUN_NEXT_INST();
	}

	DEF_INST(Load_imm_nonlocal) {
		size_t walk, offset;
		struct value *v;

		walk = NEXT_IMM_OFFSET(local_prog) - ignored_walks;
		offset = NEXT_IMM_OFFSET(local_prog);
		v = nonlocal(env, walk, offset);
		PUSH(*v);
		RUN_NEXT_INST();
	}

	UNIMPLEMENTED_INST(Load_imm_sym);

	DEF_INST(Make_list) {
		struct value v;
		size_t cap = NEXT_IMM_OFFSET(local_prog);
		if (heap_start == NULL) {
			heap_start = curr_heap = malloc(sizeof (struct heap_item));
			*heap_start = ((struct heap_item){ NULL, 0, NULL, });
		}
		v.type = List_type;
		v.l = alloc_list(&curr_heap, cap);
		v.l->len = cap;
		while (cap > 0) {
			struct value item = POP();
			v.l->items[--cap] = item;
		}
		PUSH(v);
		RUN_NEXT_INST();
	}

	DEF_INST(Mul2) {
		struct value *a1, a2;

		a2 = POP();
		a1 = TOP();
		a1->i *= a2.i;

		RUN_NEXT_INST();
	}

	DEF_INST(Mul_imm_si) {
		struct value *a;

		a = TOP();
		a->i *= NEXT_IMM_SI(local_prog);

		RUN_NEXT_INST();
	}

	DEF_INST(Push_imm_func) {
		struct value a;

		a.type = Function_type;
		a.f = NEXT_IMM_FUNC(local_prog);
		PUSH(a);
		RUN_NEXT_INST();
	}

	DEF_INST(Push_imm_si) {
		struct value a;

		a.type = Integer_type;
		a.i = NEXT_IMM_SI(local_prog);
		PUSH(a);
		RUN_NEXT_INST();
	}

	DEF_INST(Ret) {
		/* Do not overwrite progm. */
		return heap_start;
	}

	DEF_INST(Sto_imm_local) {
		struct value *a;

		a = local(env, NEXT_IMM_OFFSET(local_prog));
		*a = POP();
		RUN_NEXT_INST();
	}

	DEF_INST(Sto_imm_local_si) {
		struct value *a;

		a = local(env, NEXT_IMM_OFFSET(local_prog));
		a->type = Integer_type;
		a->i = NEXT_IMM_SI(local_prog);
		RUN_NEXT_INST();
	}

	DEF_INST(Sto_imm_local_func) {
		struct value *a;
		struct func *f;

		a = local(env, NEXT_IMM_OFFSET(local_prog));
		f = NEXT_IMM_FUNC(local_prog);
		a->type = Function_type;
		a->f = f;
		RUN_NEXT_INST();
	}

	DEF_INST(Sto_imm_nonlocal) {
		size_t walk, offset;
		struct value *a1, a2;
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
			if (heap_start == NULL) {
				heap_start = curr_heap = malloc(sizeof (struct heap_item));
				*heap_start = ((struct heap_item){ NULL, 0, NULL, });
			}
			new_func = alloc_func(&curr_heap);
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
				descendent = alloc_func(&curr_heap);
				*descendent = *a2.f;
				descendent->rt_context = NULL;
				a2.f = descendent;
			}

			descendent->parent = new_func;
		}

		if (is_heap_allocated(a2))
			make_nonlocal(&heap_start, a2.l, walk);

		*a1 = a2;
		RUN_NEXT_INST();
	}

	DEF_INST(Sto_imm_nonlocal_func) {
		struct func *f;
		struct value *a;
		size_t walk, offset;

		walk = NEXT_IMM_OFFSET(local_prog) - ignored_walks;
		offset = NEXT_IMM_OFFSET(local_prog);
		a = nonlocal(env, walk, offset);
		f = NEXT_IMM_FUNC(local_prog);
		a->type = Function_type;
		a->f = f;
		RUN_NEXT_INST();
	}

	DEF_INST(Sub2) {
		struct value *a1, a2;

		a2 = POP();
		a1 = TOP();
		a1->i -= a2.i;
		RUN_NEXT_INST();
	}

	DEF_INST(Sub_imm_si) {
		struct value *a;

		a = TOP();
		a->i -= NEXT_IMM_SI(local_prog);
		RUN_NEXT_INST();
	}

	DEF_INST(Yield) {
		if (ignored_walks == 0) {
			*prog = local_prog;
			return heap_start;
		}

		/*
		 * Otherwise, we have have ignored walks we need to
		 * remove.
		 */
		ignored_walks--;
		RUN_NEXT_INST();
	}

	/* NOTREACHED */
	return heap_start;
}
