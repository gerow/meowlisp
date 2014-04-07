#include <stdio.h>
#include <stdlib.h>
#include <histedit.h>
#include "mpc.h"

enum {
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_EXPR,
	LVAL_SEXPR
};

enum {
	LERROR_DIV_ZERO,
	LERROR_BAD_OP,
	LERROR_BAD_NUM
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

void lval_cpy(lval_t *dst, const lval_t *src)
{
	memcpy(dst, src, sizeof(*dst));
}

lval_t *lval_num(long num)
{
	lval_t *v = malloc(sizeof(*v));
	v->type   = LVAL_NUM;
	v->num = num;

	return v;
}

lval_t *lval_err(char *m)
{
	lval_t *v = malloc(sizeof(*v));
	v->type  = LVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);

	return v;
}

lval_t *lval_sym(char *m)
{
	lval_t *v = malloc(sizeof(*v));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(m) + 1);
	strcpy(v->sym, m);

	return v;
}

lval_t *lval_sexpr()
{
	lval_t *v = malloc(sizeof(*v));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;

	return v;
}

void lval_del(lval_t *v)
{
	switch (v->type) {
		case LVAL_NUM:
		break;
		case LVAL_ERR:
		free(v->err);
		break;
		case LVAL_SYM:
		free(v->sym);
		break;
		case LVAL_SEXPR:
		for (int i = 0; i < v->count; i++) {
			lval_del(v->cell[i]);
		}
		free(v->cell);
		break;
	}

	free(v);
}

lval_t *lval_add(lval_t *v, lval_t *x)
{
	v->count++;
	v->cell = realloc(v->cell, v->count * sizeof(*v->cell));
	v->cell[v->count - 1] = x;

	return v;
}

lval_t *lval_read_num(mpc_ast_t *t)
{
	long x = strtol(t->contents, NULL, 10);

	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval_t *lval_read(mpc_ast_t *t)
{
	if (strstr(t->tag, "number")) {
		return lval_read_num(t);
	}
	if (strstr(t->tag, "symbol")) {
		return lval_sym(t->contents);
	}

	/* at this point it looks like an S-Expression */
	lval_t *x = NULL;
	if (strcmp(t->tag, ">") == 0 || strcmp(t->tag, "sexpr")) {
		x = lval_sexpr();
	}

	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0 ||
		    strcmp(t->children[i]->contents, ")") == 0 ||
		    strcmp(t->children[i]->contents, "{") == 0 ||
		    strcmp(t->children[i]->contents, "}") == 0 ||
		    strcmp(t->children[i]->tag, "regex")  == 0) {
			continue;
		}

		x = lval_add(x, lval_read(t->children[i]));
	}

	return x;
}

void lval_print(lval_t *v);

void lval_expr_print(lval_t *v, char open, char close)
{
	putchar(open);

	for (int i = 0; i < v->count; i++) {
		lval_print(v->cell[i]);

		if (i != v->count - 1) {
			putchar(' ');
		}
	}

	putchar(close);
}

void lval_print(lval_t *v)
{
	switch (v->type) {
		case LVAL_NUM:
		printf("%li", v->num);
		break;
		case LVAL_ERR:
		printf("Error: %s", v->err);
		break;
		case LVAL_SYM:
		printf("%s", v->sym);
		break;
		case LVAL_SEXPR:
		lval_expr_print(v, '(', ')');
		break;
	}
}

void lval_println(lval_t *v)
{
	lval_print(v);
	putchar('\n');
}

char *get_prompt(EditLine *el)
{
	return "meowlisp> ";
}

/*
void eval_op(lval_t *r, const lval_t *x, const char *op, const lval_t *y)
{
	if (x->type == LVAL_ERR) {
		lval_cpy(r, x);
		return;
	}

	if (y->type == LVAL_ERR) {
		lval_cpy(r, y);
		return;
	}

	if (strcmp(op, "+") == 0) {
		lval_num(r, x->num + y->num);
		return;
	}
	if (strcmp(op, "-") == 0) {
		lval_num(r, x->num - y->num);
		return;
	}
	if (strcmp(op, "*") == 0) {
		lval_num(r, x->num * y->num);
		return;
	}
	if (strcmp(op, "/") == 0) {
		if (y->number == 0) {
			lval_err(r, LERROR_DIV_ZERO);
			return;
		} else {
			lval_num(r, x->num / y->num);
			return;
		}
	}

	lval_err(r, LERROR_BAD_OP);
	return;
}

void eval(lval_t *r, lval_t *t)
{
	if(t->type = LVAL_NUM) {
		lval_cpy(r, t);
		return;
	}

	char *op = t->children[1]->contents;

	//lval_t x;
	eval(r, t->children[2]);

	int i = 3;
	while (strstr(t->children[i]->tag, "expr")) {
		lval_t y;
		eval(&y, t->children[i]);
		eval_op(r, r, op, &y);
		i++;
	}

	return;
}
*/

lval_t *lval_pop(lval_t *v, int i)
{
	lval_t *x = v->cell[i];

	memmove(&v->cell[i],
		&v->cell[i + 1],
		sizeof(*v->cell) * (v->count - i - 1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(*v->cell) * v->count);

	return x;
}

lval_t *lval_builtin_op(lval_t *a, char *op)
{
	/* make sure all arguments are numbers */
	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del(a);
			return lval_err("Cannot operator on non-number!");
		}
	}

	lval_t *x = lval_pop(a, 0);
	/*
	 * if there aren't any other arguments and this is subtraction
	 * just negate the number
	 */
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}

	while(a->count > 0) {
		lval_t *y = lval_pop(a, 0);

		if (strcmp(op, "+") == 0) {
			x->num = x->num + y->num;
		}
		if (strcmp(op, "-") == 0) {
			x->num = x->num - y->num;
		}
		if (strcmp(op, "*") == 0) {
			x->num = x->num * y->num;
		}
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_del(x);
				lval_del(y);
				lval_del(a);
				return lval_err("Division by Zero!");
			}
			x->num = x->num / y->num;
		}

		lval_del(y);
	}

	lval_del(a);

	return x;
}


lval_t *lval_take(lval_t *v, int i)
{
	lval_t *x = lval_pop(v, i);
	lval_del(v);

	return x;
}

lval_t *lval_eval(lval_t *v);

lval_t *lval_eval_sexpr(lval_t *v)
{
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(v->cell[i]);
	}

	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) {
			return lval_take(v, i);
		}
	}

	if (v->count == 0) {
		return v;
	}

	if (v->count == 1) {
		return lval_take(v, 0);
	}

	lval_t *f = lval_pop(v, 0);
	if (f->type != LVAL_SYM) {
		lval_del(f);
		lval_del(v);
		return lval_err("S-Expression does not begin with symbol!");
	}

	lval_t *res = lval_builtin_op(v, f->sym);
	lval_del(f);

	return res;
}

lval_t *lval_eval(lval_t *v) {
	if (v->type == LVAL_SEXPR) {
		return lval_eval_sexpr(v);
	}

	return v;
}

int main(int argc, char **argv)
{
	int ret;

	History *h =  history_init();
	HistEvent ev;

	EditLine *el = el_init(argv[0], stdin, stdout, stderr);
	el_set(el, EL_PROMPT, get_prompt);
	el_set(el, EL_EDITOR, "emacs");
	el_set(el, EL_HIST, history, h);
	history(h, &ev, H_SETSIZE, 32);

	/* Create some parsers, yo! */
	mpc_parser_t *Number   = mpc_new("number");
	mpc_parser_t *Symbol   = mpc_new("symbol");
	mpc_parser_t *Sexpr    = mpc_new("sexpr");
	mpc_parser_t *Expr     = mpc_new("expr");
	mpc_parser_t *Lispy    = mpc_new("lispy");

	/* Define them with the following language, yo! */
	mpca_lang(MPC_LANG_DEFAULT,
		  "number   : /-?[0-9]+/ ;                    "
		  "symbol   : '+' | '-' | '*' | '/' ;         "
		  "sexpr    : '(' <expr>* ')' ;                "
		  "expr     : <number> | <symbol> | <sexpr> ; "
		  "lispy    : /^/ <expr>* /$/ ;               ",
		  Number, Symbol, Sexpr, Expr, Lispy);

	puts("Meowlisp Version 0.0.1");
	puts(" \\    /\\ \n"
	     "  )  ( ')\n"
	     " (  /  ) \n"
	     "  \\(__)| \n");
	puts("Press Ctrl+c to Exit\n");

	for (;;) {
		int count = 0;
		mpc_result_t r;
		mpc_ast_t *ast;

		const char *input = el_gets(el, &count);
		if (input == NULL) {
			puts("\n");
			break;
		}
		ret = history(h, &ev, H_ENTER, input);
		if (ret == -1) {
			abort();
		}

		if (mpc_parse("<stdin>", input, Lispy, &r)) {
			//mpc_ast_print(r.output);
			ast = r.output;
			lval_t *x = lval_read(r.output);
			x = lval_eval(x);
			lval_println(x);
			lval_del(x);
			/* printf("%li\n", result); */
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
	}

	el_end(el);
	history_end(h);
	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

	return 0;
}
