#include <stdlib.h>
#include <stdbool.h>

#include "alloc.h"
#include "types.h"
#include "symtab.h"
#include "builtin.h"
#include "bytecode.h"

/*
 * Some initial commentary on this file:
 * All of the compilation work I've done has started as a big mess and remained
 * that way. This file is really no different, but I plan on fixing that at some
 * point.
 * I don't believe most optimizations should be put in here, I'm thinking of
 * adding an "opt" file that does most of the heavy lifting.
 * However, optimizations that are required - such as tail calls - should be
 * placed in here.
 */

/*
 * Information only useful for compiling; also keeps a singular record of the
 * allocated func data.
 */
static struct scope {
	size_t          func_sym;       /* For tail call opts. */
	symtab          *locals;
	struct list     *local_funcs;
	struct func     *fdat;
	struct scope    *parent;
} global;

struct var_loc {
	size_t          walk;
	size_t          offset;
	struct scope    *scope;
};

/*
 * Finds the variable. If no variable was found, the scope is set to NULL.
 */
static struct var_loc
find_var_loc(struct scope *curr, size_t sym)
{
	size_t walk;
	struct var_loc var = { 0, 0, NULL };

	walk = 0;
	while (curr != NULL) {
		if (sym_exists(curr->locals, sym)) {
			var.walk = walk;
			var.scope = curr;
			var.offset = sym_offset(curr->locals, sym);
			return var;
		}
		walk++;
		curr = curr->parent;
	}

	return var;
}


void
set_compiler_global_context(struct func *env)
{
	global.fdat = env;
	env->locals = global.locals = malloc(sizeof(symtab));
	symtab_init(global.locals);
}

/*
        Only big wusses and lesser wimps use [call graphs].
                                                -Strong Bad
 */

enum type compile_list(struct scope *, struct progm *, struct list *, bool);
enum type compile_builtin(struct scope *, struct progm *, struct list *,
			  enum builtin, bool);
enum type compile_if(struct scope *, struct progm *, struct list *, bool);
enum type compile_set(struct scope *, struct progm *, struct list *);
enum type compile_funcall(struct scope *, struct progm *, struct list *, bool);
enum type compile_item(struct scope *, struct progm *, struct value *, bool);
enum type compile_define(struct scope *, struct progm *, struct list *);
enum type compile_lambda(struct scope *, struct progm *, struct list *);
enum type compile_var_def(struct scope *, struct progm *, struct list *);
enum type compile_func_def(struct scope *, struct progm *, struct list *);

enum type
compile(struct progm *prog, struct list *lp)
{
	return compile_list(&global, prog, lp, false);
}

/*
 * Compile a list in place, converting it into a format that the compiler can
 * run. The value returned is the inferred type of the expression.
 */
enum type
compile_list(struct scope *env, struct progm *prog, struct list *lp,
	     bool tailcall)
{
	if (lp->len == 0)
		return Error_type; /* I don't know what to do here. */

	switch (lp->items->type) {
	case Nil_type:
		/* ERROR: how the hell would this happen?? */
		return Error_type;

	case Integer_type:
	case Real_type:
		/* ERROR: numeric types cannot be called. */
		return Error_type;

	case Symbol_type:
		return ((lp->items->sym < Num_builtins)
			? compile_builtin(env, prog, lp, lp->items->sym,
					  tailcall)
			: compile_funcall(env, prog, lp, tailcall));

	default:
		return compile_funcall(env, prog, lp, tailcall);
	}
}

enum type
compile_builtin(struct scope *env, struct progm *prog, struct list *lp,
		enum builtin sym, bool tailcall)
{
	size_t i;
	enum opcode mtab[][2] = {
		[Add_builtin] = { Add2_opcode, Add_imm_si_opcode },
		[Sub_builtin] = { Sub2_opcode, Sub_imm_si_opcode },
		[Mul_builtin] = { Mul2_opcode, Mul_imm_si_opcode },
		[Div_builtin] = { Div2_opcode, Div_imm_si_opcode },
	};

	switch (sym) {
	case Add_builtin:
	case Sub_builtin:
	case Mul_builtin:
	case Div_builtin:
		if (lp->len < 3) {
			/*
			 * Error: math builtin expects at least two arguments.
			 */
			return Error_type;
		}
		/* Compile the first item. */
		if (compile_item(env, prog, lp->items + 1, false) == Error_type)
			return Error_type;
		for (i = 2; i < lp->len; i++)
			switch (lp->items[i].type) {
			case Integer_type:
				code_inst(prog, mtab[sym][1]);
				code_si(prog, lp->items[i].i);
				break;

			default:
				if (compile_item(env, prog, lp->items + i, false)
				    == Error_type)
					return Error_type;
				code_inst(prog, mtab[sym][0]);
				break;
			}
		return Integer_type;

	case Define_builtin:
		return compile_define(env, prog, lp);

	case Car_builtin:
		if (lp->len != 2)
			return Error_type;
		compile_item(env, prog, lp->items + 1, false);
		code_inst(prog, Car_opcode);
		return Integer_type;

	case Cdr_builtin:
		if (lp->len != 2)
			return Error_type;
		compile_item(env, prog, lp->items + 1, false);
		code_inst(prog, Cdr_opcode);
		return Integer_type;

	case If_builtin:
		return compile_if(env, prog, lp, tailcall);

	case Set_builtin:
		return compile_set(env, prog, lp);

	case Lambda_builtin:
		return compile_lambda(env, prog, lp);

	case List_builtin:
		if (lp->len < 2) {
			return Error_type;
		}
		for (i = 1; i < lp->len; i++)
			if (compile_item(env, prog, lp->items + i, false)
			    == Error_type)
				return Error_type;
		code_inst(prog, Make_list_opcode);
		code_offset(prog, lp->len - 1);
		return List_type;

	default:
		return Error_type;
	}
}

enum type
compile_if(struct scope *env, struct progm *prog, struct list *lp,
	   bool tailcall)
{
	enum type ret_type;
	struct var_loc loc;
	struct scope scope = {
		.func_sym = 0,
		.fdat = NULL,
		.local_funcs = NULL,
		.parent = env,
	};

	switch (lp->items[1].type) {
	case Error_type:
		return Error_type;

	case Integer_type:
	{
		size_t localtab;

		scope.locals = malloc(sizeof(symtab));
		symtab_init(scope.locals);
		code_inst(prog, Let_opcode);
		localtab = code_symtab(prog, NULL); /* Let(NULL) is valid. */
		ret_type = compile_item(&scope, prog,
					&lp->items[lp->items[0].i ? 2 : 3],
					tailcall);
		if (scope.locals->len > 0)
			prog->code[localtab].symtab = scope.locals;
		else
			free(scope.locals);
		code_inst(prog, Ret_opcode);
		return ret_type;
	}

	case Function_type:
		return Error_type;

	case List_type:
	{
		size_t localtab;
		size_t offset1, offset2;
		enum opcode jmp_op = Jmp_true_opcode;

		/*
		 * Todo: optimize here.
		 */
		switch (lp->items[1].l->items->sym) {
		case Equal_builtin:
			jmp_op = Jmp_ne_opcode;
			compile_item(env, prog, lp->items[1].l->items + 1,
				     false);
			compile_item(env, prog, lp->items[1].l->items + 2,
				     false);
			break;

		default:
			compile_item(env, prog, lp->items + 1, false);
		}
		goto no_var_lookup;

	case Symbol_type:
		jmp_op = Jmp_true_opcode;
		loc = find_var_loc(env, lp->items[1].sym);
		if (loc.scope == NULL) {
			return Error_type;
		}
		if (loc.walk == 0) {
			/* Local type. */
			code_inst(prog, Load_imm_local_opcode);
			code_offset(prog, loc.offset);
		} else {
			/* Non local type. */
			code_inst(prog, Load_imm_nonlocal_opcode);
			code_offset(prog, loc.walk);
			code_offset(prog, loc.offset);
		}

	no_var_lookup:
		scope.locals = malloc(sizeof(symtab));
		symtab_init(scope.locals);

		/* Conditional test. */
		code_inst(prog, jmp_op);
		offset1 = code_si(prog, 0);

		/* True body. */
		code_inst(prog, Let_opcode);
		localtab = code_symtab(prog, NULL);
		ret_type = compile_item(&scope, prog, lp->items + 2, tailcall);
		if (scope.locals->len > 0)
			prog->code[localtab].symtab = scope.locals;
		else
			free(scope.locals);
		code_inst(prog, Yield_opcode);
		code_inst(prog, Jmp_opcode);
		offset2 = code_si(prog, 0);
		prog->code[offset1].o = offset2 - offset1;

		/* False body. */
		scope.locals = malloc(sizeof(symtab));
		symtab_init(scope.locals);
		code_inst(prog, Let_opcode);
		localtab = code_symtab(prog, NULL);
		compile_item(&scope, prog, lp->items + 3, tailcall);
		if (scope.locals->len > 0)
			prog->code[localtab].symtab = scope.locals;
		else
			free(scope.locals);
		offset1 = code_inst(prog, Yield_opcode);
		prog->code[offset2].o = offset1 - offset2;
		return ret_type;
	}

	default:
		break;
	}
	return Error_type;
}

enum type
compile_set(struct scope *env, struct progm *prog, struct list *lp)
{
	enum type ret_val;

	if ((ret_val = compile_item(env, prog, lp->items + 2, false))
	    == Error_type)
		return Error_type;

	switch (lp->items[1].type) {
	case Symbol_type:
	{
		struct var_loc loc = find_var_loc(env, lp->items[1].sym);

		if (loc.scope == NULL) {
			/* FUCK */
			return Error_type;
		} else if (loc.walk != 0) {
			/* Value is non-local. */
			code_inst(prog, Sto_imm_nonlocal_opcode);
			code_offset(prog, loc.walk);
			code_offset(prog, loc.offset);
		} else {
			code_inst(prog, Sto_imm_local_opcode);
			code_offset(prog, loc.offset);
		}
		break;
	}

	default:
		return Error_type;
	}

	return ret_val;
}

enum type
compile_funcall(struct scope *env, struct progm *prog, struct list *lp,
		bool tailcall)
{
	size_t i;

	/*
	 * Compile all of the arguments.
	 */
	for (i = 1; i < lp->len; i++)
		if (compile_item(env, prog, lp->items + i, false) == Error_type)
			/* ERROR */
			return Error_type;

	/*
	 * Compile the function call.
	 */
	switch (lp->items->type) {
	case List_type:
		if (compile_item(env, prog, lp->items, false) == Error_type) {
			return Error_type;
		}
		code_inst(prog, Call_opcode);
		code_offset(prog, lp->len - 1);
		break;

	case Symbol_type:
	{
		size_t ascopes;
		struct scope *curr;
		struct var_loc loc = find_var_loc(env, lp->items->sym);

		/*
		 * Determine if the call is a tail call. This is quite a doozy
		 * and should be simplified/fixed.
		 */
		if (tailcall) {
			/*
			 * Ignore any anonymous scopes created by let or control 
			 * flow.
			 */
			for (ascopes = 0, curr = env;
			     curr != NULL && curr->fdat == NULL;
			     curr = curr->parent, ascopes++)
				;
			if (curr != NULL && curr->func_sym == lp->items->sym) {
				/* We have found a tail call. */
				/* Produce yields for the anonymous scopes. */
				while (ascopes > 0) {
					code_inst(prog, Yield_opcode);
					ascopes--;
				}
				for (i = 0; i < lp->len - 1; i++) {
					code_inst(prog, Sto_imm_local_opcode);
					code_offset(prog, lp->len - i - 2);
				}
				code_inst(prog, Jmp_abs_opcode);
				code_offset(prog, 0);
				break;
			}
			/*
			 * We have not found a tail call. Continue along normal
			 * compilation paths.
			 */
		}

		if (loc.scope == NULL) {
			/* Could not find the symbol. */
			code_inst(prog, Call_imm_sym_opcode);
			code_offset(prog, lp->len - 1);
			code_sym(prog, lp->items->sym);
		} else if (loc.walk != 0) {
			/* Function is nonlocal. */
			code_inst(prog, Call_imm_nonlocal_opcode);
			code_offset(prog, lp->len - 1);
			code_offset(prog, loc.walk);
			code_offset(prog, loc.offset);
		} else {
			code_inst(prog, Call_imm_local_opcode);
			code_offset(prog, lp->len - 1);
			code_offset(prog, loc.offset);
		}
		break;
	}


	case Function_type:
		/* I don't know when this would happen. */
	default:
		return Error_type;
	}
	return Nil_type;
}

enum type
compile_item(struct scope *env, struct progm *prog, struct value *vp,
	     bool tailcall)
{
	switch (vp->type) {
	case Error_type:
	case Nil_type:
	case Real_type:
	case Function_type:
		/* Nothing to do here. */
		break;

	case Integer_type:
		code_inst(prog, Push_imm_si_opcode);
		code_si(prog, vp->i);
		return Integer_type;

	case Symbol_type:
	{
		struct var_loc loc = find_var_loc(env, vp->sym);
		if (loc.scope == NULL) {
			/* Could not find variable, leave a symbol? */
			break;
		}
		if (loc.scope == env) {
			/* Local variable. */
			code_inst(prog, Load_imm_local_opcode);
			code_offset(prog, loc.offset);
		} else {
			/* Non-local variable. */
			code_inst(prog, Load_imm_nonlocal_opcode);
			code_offset(prog, loc.walk);
			code_offset(prog, loc.offset);
		}
		return Integer_type;
	}

	case List_type:
		return compile_list(env, prog, vp->l, tailcall);

	default:
		/* WTF?? */
		break;
	}
	return Error_type;
}

enum type
compile_define(struct scope *env, struct progm *prog, struct list *lp)
{
	switch (lp->len) {
		/* len cannot be zero, first element is 'define' sym. */
	case 1:
		/* ERROR: empty define. */
		return Error_type;
	case 2:
		/* ERROR: no value. */
		return Error_type;

	default: /* lp->len > 3 */
		if (lp->items[1].type != List_type) {
			/* ERROR: multiple values for non function defintion. */
			return Error_type;
		}
	case 3:
		switch (lp->items[1].type) {
		case Nil_type:
		case Integer_type:
		case Real_type:
			/* ERROR: cannot define functions. */
			return Error_type;

		case Function_type:
			/* ??? */
			/* ERROR? cannot redefine functions? */
			return Error_type;

		case Symbol_type:
			/*
			 * Define a variable. If we reached here, then len must
			 * be 3.
			 */
			return compile_var_def(env, prog, lp);

		case List_type:
			/*
			 * Define a function.
			 */
			return compile_func_def(env, prog, lp);
		default:
			return Error_type;
		}
	}
	return Error_type;
}

enum type
compile_var_def(struct scope *env, struct progm *prog, struct list *lp)
{
	size_t offset;
	enum type expr_res;
	struct value var;

	var = lp->items[1];
	offset = sym_offset(env->locals, var.sym);

	/*
	if (env->fdat != NULL)
		append(env->fdat->locals, var);
	*/

	switch (lp->items[2].type) {
	case List_type:
		expr_res = compile_list(env, prog, lp->items[2].l, false);
		if (expr_res == Nil_type)
			/*
			 * ERROR: expression in definiation must return some
			 * value.
			 */
			break;
		code_inst(prog, Sto_imm_local_opcode);
		code_offset(prog, offset);
		return expr_res;

	case Integer_type:
		code_inst(prog, Sto_imm_local_si_opcode);
		code_offset(prog, offset);
		code_si(prog, lp->items[2].i);
		return Integer_type;

	default:
		break;
	}

	return Nil_type;
}

enum type
compile_lambda(struct scope *env, struct progm *prog, struct list *lp)
{
	size_t i;
	int variadic;
	struct list *p;
	enum type ret_type;
	struct func *lambda;
	struct scope lambda_scope;

	variadic = 0;
	lambda = alloc_func(&global_heap);
	lambda_scope.func_sym = 0;
	lambda_scope.fdat = lambda;
	lambda_scope.parent = env;
	lambda->locals = lambda_scope.locals = malloc(sizeof(symtab));
	symtab_init(lambda->locals);
	lambda->parent = env->fdat;
	lambda->return_type = Integer_type;     /* TODO: fix. */

	/*
	 * Add the arguments
	 */
	p = lp->items[1].l;
	for (i = 0; i < p->len; i++)
		if (p->items[i].sym == Dot_builtin) {
			variadic = 1;
		} else if (sym_exists(lambda_scope.locals, p->items[i].sym)) {
			/* Duplicate argument. */
			return Error_type;
		} else {
			append(lambda->args, p->items[i]);
			sym_add(lambda_scope.locals, p->items[i].sym);
		}

	lambda->flags.variadic = variadic;

	/*
	 * Compile the body of the function.
	 * TODO: tail call elimination.
	 */
	p = lp;
	for (i = 2; i < p->len; i++)
		if ((ret_type = compile_item(&lambda_scope, &lambda->prog,
					     p->items + i, false))
		    == Error_type) {
			/* ERRORROROR */
			return Error_type;
		} else if (i < p->len - 1) {
			code_inst(&lambda->prog, Clear_opcode);
		} else {
			code_inst(&lambda->prog, Ret_opcode);
		}

	lambda->return_type = ret_type;
	code_inst(prog, Push_imm_func_opcode);
	code_func(prog, lambda);
	return Function_type;
}

enum type
compile_func_def(struct scope *env, struct progm *prog, struct list *lp)
{
	int variadic;
	size_t i;
	size_t offset;
	enum type ret_type;
	struct list *p;
	struct list *args;
//	struct list *body;
	struct func *new_func;
//	struct value local_form;
	struct scope new_scope;

	p = lp->items[1].l;
	if (sym_exists(env->locals, p->items[0].sym)) {
		/* ERROR: cannot redefine functions? */
		return Error_type;
	}

	new_func = alloc_func(&global_heap);
	new_scope.func_sym = p->items[0].sym;
	new_scope.fdat = new_func;
	new_scope.parent = env;
	new_scope.locals = malloc(sizeof(symtab));
	symtab_init(new_scope.locals);
	args = alloc_list(&global_heap, p->len - 1);
	variadic = 0;
	for (i = 1; i < p->len; i++)
		/*
		 * The parser should have discovered most errors in the argument
		 * list. We can safely assume that every entry is a symbol, at
		 * most only one is the '.' symbol, and if the dot symbol does
		 * exist it is not the last entry.
		 */
		if (p->items[i].sym == Dot_builtin) {
			variadic = 1;
		} else if (sym_exists(new_scope.locals, p->items[i].sym)) {
			/*
			 * We cannot assume that there are no duplicate entries, 
			 * however.
			 */
			/* ERROR: duplicate entries in argument list. */
			free(args);
			free(new_func);
			free(new_scope.locals);
			return Error_type;
		} else {
			append(new_func->args, p->items[i]);
			sym_add(new_scope.locals, p->items[i].sym);
 		}


	new_func->parent = env->fdat;
	new_func->flags.variadic = variadic ? 1 : 0;    /* Redundant. */
	new_func->return_type = Integer_type;
	new_func->locals = new_scope.locals;

	/*
	 * Add the function to the scope of its parent so that it can call
	 * itself recursively.
	append(env->fdat->locals, lp->items[1].l->items[0]);
	 */

	offset = sym_add(env->locals, p->items[0].sym);
//	local_form.type = Function_type;
//	local_form.f = new_func;
//	append(env->fdat->local_funcs, local_form);

	/*
	 * Compile the body of the function that is not returned.
	 */
	p = lp;
	for (i = 2; i < p->len - 1 ; i++)
		if (compile_item(&new_scope, &new_func->prog, p->items + i,
				 false)
		    == Error_type) {
			/* Error. Todo: cleanup. */
			free(args);
			free(new_func);
			symtab_clear(new_scope.locals);
			free(new_scope.locals);
			return Error_type;
		} else {
			code_inst(&new_func->prog, Clear_opcode);
		}

	/*
	 * Compile the tail, the part that is returned.
	 */
	if ((ret_type = compile_item(&new_scope, &new_func->prog, p->items + i,
				     true)) == Error_type) {
		/* Error. Todo: cleanup. */
		return Error_type;
	}

	code_inst(&new_func->prog, Ret_opcode);

	new_func->return_type = ret_type;
	code_inst(prog, Sto_imm_local_func_opcode);
	code_offset(prog, offset);
	code_func(prog, new_func);

	disassemble(new_func->prog);

	return Nil_type;
}
