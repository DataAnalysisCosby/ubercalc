CFLAGS = -c -Wall -g3
LDFLAGS = -ledit -ltermcap
SRCS = map.c lex.c parse.c builtin.c ident.c list.c comp.c eval.c main.c \
	bytecode.c symtab.c strmap.c
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
