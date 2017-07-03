#ifndef _BUILTIN_H_
#define _BUILTIN_H_

/*
 * List of functions built into the interpreter. The value of enum should also
 * be their ident number.
 */
enum builtin {
	Add_builtin = 0,
	Define_builtin,
	Div_builtin,
	Dot_builtin,
	Car_builtin,
	Cdr_builtin,
	Cons_builtin,
	Equal_builtin,
	Greater_builtin,
	If_builtin,
	Lambda_builtin,
	Less_builtin,
	Let_builtin,
	List_builtin,
	Mul_builtin,
	Quote_builtin,
	Set_builtin,
	Sub_builtin,

	Num_builtins,   /* Not really a builtin. */
};

void init_builtins(void);

#endif
