#include <stdlib.h>

#include "strmap.h"

/*
 * We want to replace the string representation of identifiers with a numerical
 * representation, which is accomplished by this file.
 */

static strmap ident_table[1] = { STRMAP_INIT };

/*
 * Maps identifier numbers to strings. This table is managed by this file, so
 * don't fuck with it elsewhere! You are, however, permitted to read it.
 */
size_t num_idents;
char **ident_strings = NULL;

/*
 * Finds the number of the ident. If ident previously did not have a number this 
 * function will take ownership of the pointer.
 */
size_t
ident_get_number(char *name)
{
	size_t *num;

	/* Remember, map_gets create a new entry if it doesn't exist already. */
	if (strmap_exists(ident_table, name)) {
		num = (size_t *)strmap_get(ident_table, name);
	} else {
		num = (size_t *)strmap_get(ident_table, name);
		/*
		 * Create a new identifier and take ownership of the string.
		 * We don't anticipate that new identifiers will be introduced
		 * very often during runtime (or at the very least they
		 * shouldn't be!) so we grow this table linearly.
		 */
		*num = num_idents++;
		ident_strings = realloc(ident_strings,
					sizeof (char *) * num_idents);
		ident_strings[num_idents - 1] = name;
	}

	return *num;
}
