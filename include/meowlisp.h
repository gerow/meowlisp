#ifndef MEOWLISP_H_
#define MEOWLISP_H_

enum {
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_EXPR,
	LVAL_SEXPR
};

typedef struct lval {
	int type;
	union {
		long num;
		char *err;
		char *sym;
		struct {
			int count;
			struct lval **cell;
		};
	};
} lval_t;

lval_t *lval_read(const char *input);
lval_t *lval_eval(lval_t *v);
void lval_println(const lval_t *v);
void lval_del(lval_t *v);

#endif /* MEOWLISP_H_ */
