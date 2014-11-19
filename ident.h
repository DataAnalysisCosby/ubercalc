#ifndef _IDENT_H_
#define _IDENT_H_

#include <stdlib.h>

extern char **ident_strings;
extern size_t num_idents;

size_t ident_get_number(char *name);

#endif
