#ifndef MEOWLISP_H_
#define MEOWLISP_H_

struct lval;
struct lenv;
typedef struct lval lval_t;
typedef struct lenv lenv_t;

enum {
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_FUN,
	LVAL_SEXPR,
	LVAL_QEXPR
};

typedef lval_t *(*lbuiltin_t)(lenv_t *, lval_t *);

struct lval {
	int type;
	union {
		long num;
		char *err;
		char *sym;
		struct {
			int count;
			struct lval **cell;
		};
		lbuiltin_t fun;
	};
};

struct lenv {
	int count;
	char **syms;
	lval_t **vals;
};

lval_t *lval_read(const char *input);
lval_t *lval_eval(lenv_t *e, lval_t *v);
void lval_println(const lval_t *v);
void lval_del(lval_t *v);
void lenv_add_builtin(lenv_t *e, char *name, lbuiltin_t func);
void lenv_add_builtins(lenv_t *e);
lenv_t *lenv_new(void);
void lenv_del(lenv_t *e);

#endif /* MEOWLISP_H_ */
