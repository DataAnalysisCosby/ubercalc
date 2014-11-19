#ifndef _COMP_H_
#define _COMP_H_

#include "types.h"

enum type compile(struct progm *, struct list *);
void set_compiler_global_context(struct func *);

#endif
