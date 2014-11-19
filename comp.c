#include <stdlib.h>

#include "types.h"
#include "symtab.h"
#include "builtin.h"
#include "bytecode.h"

/*
 * Information only useful for compiling; also keeps a singular record of the
 * allocated func data.
 */
static struct scope {
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
 * I kind of hate that this allocates memory, it clearly shouldn't have to.
 * Will fix.
 */
static struct var_loc *
find_var_loc(struct scope *curr, size_t sym)
{
	size_t walk;
	struct var_loc *var;

	walk = 0;
	while (curr != NULL) {
		if (sym_exists(curr->locals, sym)) {
			var = malloc(sizeof (struct var_loc));
			var->walk = walk;
			var->scope = curr;
			var->offset = sym_offset(curr->locals, sym);
			return var;
		}
		walk++;
		curr = curr->parent;
	}

	return NULL;
}


void
set_compiler_global_context(struct func *env)
{
	global.fdat = env;
	env->locals = global.locals = malloc(sizeof(symtab));
	symtab_init(global.locals);
}

/*
        Call graphs are for wusses and lesser wimps!
                                                -Strong Bad
 */

enum type compile_list(struct scope *, struct progm *, struct list *);
enum type compile_builtin(struct scope *, struct progm *, struct list *,
			  enum builtin);
enum type compile_if(struct scope *, struct progm *, struct list *);
enum type compile_funcall(struct scope *, struct progm *, struct list *);
enum type compile_item(struct scope *, struct progm *, struct value *);
enum type compile_define(struct scope *, struct progm *, struct list *);
enum type compile_var_def(struct scope *, struct progm *, struct list *);
enum type compile_func_def(struct scope *, struct progm *, struct list *);

enum type
compile(struct progm *prog, struct list *lp)
{
	return compile_list(&global, prog, lp);
}

/*
 * Compile a list in place, converting it into a format that the compiler can
 * run. The value returned is the inferred type of the expression.
 */
enum type
compile_list(struct scope *env, struct progm *prog, struct list *lp)
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
			? compile_builtin(env, prog, lp, lp->items->sym)
			: compile_funcall(env, prog, lp));

	default:
		return Error_type;
	}
}

enum type
compile_builtin(struct scope *env, struct progm *prog, struct list *lp,
		enum builtin sym)
{
	size_t i;
	enum type argt;
	enum opcode mtab[] = {
		[Add_builtin] = Add2_opcode,
		[Sub_builtin] = Sub2_opcode,
		[Mul_builtin] = Mul2_opcode,
		[Div_builtin] = Div2_opcode,
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
		for (i = 1; i < lp->len; i++) {
			/* TODO: check to make sure types are correct. */
			if ((argt = compile_item(env, prog, lp->items + i))
			    == Error_type)
				return Error_type;
			if (i > 1)
				code_inst(prog, mtab[sym]);
		}
		return Integer_type;

	case Define_builtin:
		return compile_define(env, prog, lp);

	case If_builtin:
		return compile_if(env, prog, lp);

	case Set_builtin:
//		return compile_set(env, prog, lp);
		/* ... */
	default:
		return Error_type;
	}
}

enum type
compile_if(struct scope *env, struct progm *prog, struct list *lp)
{
	enum type ret_type;
	struct var_loc *loc;
	struct scope scope = {
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
					&lp->items[lp->items[0].i ? 2 : 3]);
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
			compile_item(env, prog, lp->items[1].l->items + 1);
			compile_item(env, prog, lp->items[1].l->items + 2);
			break;

		default:
			compile_item(env, prog, lp->items + 1);
		}
		goto no_var_lookup;

	case Symbol_type:
		jmp_op = Jmp_true_opcode;
		loc = find_var_loc(env, lp->items[1].sym);
		if (loc == NULL) {
			return Error_type;
		}
		if (loc->walk == 0) {
			/* Local type. */
			code_inst(prog, Load_imm_local_opcode);
			code_offset(prog, loc->offset);
		} else {
			/* Non local type. */
			code_inst(prog, Load_imm_nonlocal_opcode);
			code_offset(prog, loc->walk);
			code_offset(prog, loc->offset);
		}
		free(loc);

	no_var_lookup:
		scope.locals = malloc(sizeof(symtab));
		symtab_init(scope.locals);

		/* Conditional test. */
		code_inst(prog, jmp_op);
		offset1 = code_offset(prog, 0);

		/* True body. */
		code_inst(prog, Let_opcode);
		localtab = code_symtab(prog, NULL);
		ret_type = compile_item(&scope, prog, lp->items + 2);
		if (scope.locals->len > 0)
			prog->code[localtab].symtab = scope.locals;
		else
			free(scope.locals);
		code_inst(prog, Yield_opcode);
		code_inst(prog, Jmp_opcode);
		offset2 = code_offset(prog, 0);
		prog->code[offset1].o = offset2 - offset1;

		/* False body. */
		scope.locals = malloc(sizeof(symtab));
		symtab_init(scope.locals);
		code_inst(prog, Let_opcode);
		localtab = code_symtab(prog, NULL);
		compile_item(&scope, prog, lp->items + 3);
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
compile_funcall(struct scope *env, struct progm *prog, struct list *lp)
{
	size_t i;
//	enum type ret_type;
	struct var_loc *func_loc;

	if ((func_loc = find_var_loc(env, lp->items->sym)) == NULL) {
		/* ERROR: could not find symbol. */
		return Error_type;
	}

#if 0
	size_t req_args;
	struct value var;
	struct func *new_func;
	struct func *sym_parent;

	/*
	 * Obtain the variable definition of the symbol being called.
	 */
	var = func_loc->scope->items[func_loc->offset];

	if (var.type != Function_type) {
		/* ERROR: cannot perform function call on non-function. */
		/*
		 * TODO: fix this. Variables can be rebound.
		 */
		free(func_loc);
		return Error_type;
	}

	/*
	 * Check arguments to the function.
	 */
	req_args = var.f->args->len - (var.f->flags.variadic ? 1 : 0);
	ret_type = var.f->return_type;
	if (lp->len - 1 < req_args) {
		/* ERROR: too few arguments to function. */
		free(func_loc);
		return Error_type;
	}

	if (lp->len - 1 > req_args && !var.f->flags.variadic) {
		/* ERROR: too many arguments to function. */
		free(func_loc);
		return Error_type;
	}
#endif

	/*
	 * Compile all of the arguments.
	 */
	for (i = 1; i < lp->len; i++)
		if (compile_item(env, prog, lp->items + i) == Error_type) {
			/* ERROR */
			free(func_loc);
			return Error_type;
		}

#if 0
	/*
	 * If the function is variadic and the call exploits this, we want to
	 * put the rest of the arguments in a call to list.
	 */
	if (lp->len - 1 > req_args) {
		for (i = 1 + req_args; i < lp->len; i++)
			compile_item(env, prog, lp->items + i);
		code_inst(prog, Make_list_opcode);
		code_offset(prog, lp->len - req_args - 1);
	} else if (var.f->flags.variadic) {
		/*
		 * If we aren't supplying more variables than are required but
		 * the function is variadic we want to pass Nil to the variable
		 * that holds extra arguments.
		 */
		code_inst(prog, Alloc_list_opcode);
		code_offset(prog, 0);
	}
#endif

	/*
	 * Compile the function call.
	 */
	if (func_loc->walk != 0) {
//		new_func = alloc_func();
//		*new_func = *var.f;
//		new_func->rt_context = NULL;
		code_inst(prog, Call_imm_nonlocal_opcode);
		code_offset(prog, lp->len - 1);
		code_offset(prog, func_loc->walk);
		code_offset(prog, func_loc->offset);
//		code_func(prog, new_func);
	} else {
		/* Local function. */
		code_inst(prog, Call_imm_local_opcode);
		code_offset(prog, lp->len - 1);
		code_offset(prog, func_loc->offset);
	}


	free(func_loc);
	return Nil_type;//ret_type;
}

enum type
compile_item(struct scope *env, struct progm *prog, struct value *vp)
{
	struct var_loc *loc;

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
		if ((loc = find_var_loc(env, vp->sym)) == NULL) {
			/* Could not find variable, leave a symbol? */
			break;
		}
		if (loc->scope == env) {
			/* Local variable. */
			code_inst(prog, Load_imm_local_opcode);
			code_offset(prog, loc->offset);
		} else {
			/* Non-local variable. */
			code_inst(prog, Load_imm_nonlocal_opcode);
			code_offset(prog, loc->walk);
			code_offset(prog, loc->offset);
		}
		free(loc);
		return Integer_type;

	case List_type:
		return compile_list(env, prog, vp->l);

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
		expr_res = compile_list(env, prog, lp->items[2].l);
		if (expr_res == Nil_type)
			/*
			 * ERROR: expression in definition must return some
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

	new_func = alloc_func();
	new_scope.fdat = new_func;
	new_scope.parent = env;
	new_scope.locals = malloc(sizeof(symtab));
	symtab_init(new_scope.locals);
	args = alloc_list(p->len - 1);
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
			append(args, p->items[i]);
			sym_add(new_scope.locals, p->items[i].sym);
 		}


	new_func->parent = env->fdat;
	new_func->args = args;
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
	 * Compile the body of the function.
	 * Todo: tail call elimination.
	 */
	p = lp;
	for (i = 2; i < p->len; i++)
		if ((ret_type = compile_item(&new_scope, &new_func->prog,
					     p->items + i)) == Error_type) {
			/* ERRORROROR */
			free(args);
			free(new_func);
			symtab_clear(new_scope.locals);
			free(new_scope.locals);
			return Error_type;
		} else if (i < p->len - 1) {
			code_inst(&new_func->prog, Drop_opcode);
		} else {
			code_inst(&new_func->prog, Ret_opcode);
		}

	new_func->return_type = ret_type;
	code_inst(prog, Sto_imm_local_func_opcode);
	code_offset(prog, offset);
	code_func(prog, new_func);

	return Nil_type;
}
