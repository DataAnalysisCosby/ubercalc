CFLAGS = -c -Wall -g3 #-O3 #-g3
LDFLAGS = -ledit -ltermcap -pg
SRCS = map.c lex.c parse.c builtin.c ident.c vector.c comp.c eval.c main.c \
	bytecode.c symtab.c strmap.c alloc.c
OBJS = $(SRCS:.c=.o)
EXEC = ucalc

all: $(SRCS) $(EXEC)

$(EXEC): $(OBJS)
	gcc $(OBJS) -o $@ $(LDFLAGS)

.c.o:
	gcc $(CFLAGS) $< -o $@

clean:
	-rm -rf *.o
	-rm $(EXEC)
