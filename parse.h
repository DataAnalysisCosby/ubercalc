#ifndef _PARSE_H_
#define _PARSE_H_

struct token {
	enum token_id   id;
	char *          src;
};

struct list;

struct list parse(struct token (*)(void));

#endif
