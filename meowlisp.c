#include "meowlisp.h"
#include "mpc.h"

static int meowlisp_parse(mpc_result_t *r, const char *input);
static lval_t *lval_read_tag(mpc_ast_t *t);
static lval_t *lval_num(long num);
static lval_t *lval_err(char *m);
static lval_t *lval_sym(char *m);
static lval_t *lval_sexpr();
static lval_t *lval_add(lval_t *v, lval_t *x);
static lval_t *lval_read_num(mpc_ast_t *t);
static void lval_expr_print(lval_t *v, char open, char close);
static void lval_print(const lval_t *v);
static lval_t *lval_pop(lval_t *v, int i);
static lval_t *lval_builtin_op(lval_t *a, char *op);
static lval_t *lval_take(lval_t *v, int i);
static lval_t *lval_eval_sexpr(lval_t *v);

lval_t *lval_read(const char *input)
{
	mpc_result_t r;
	lval_t *v;

	if (meowlisp_parse(&r, input) == 0) {
		char *err = mpc_err_string(r.error);
		mpc_err_delete(r.error);
		v = lval_err(err);
		free(err);
		return v;
	}

	v = lval_read_tag(r.output);
	mpc_ast_delete(r.output);

	return v;
}

lval_t *lval_eval(lval_t *v)
{
	if (v->type == LVAL_SEXPR) {
		return lval_eval_sexpr(v);
	}

	return v;
}

void lval_println(const lval_t *v)
{
	lval_print(v);
	putchar('\n');
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

/* static functions */

static int meowlisp_parse(mpc_result_t *r, const char *input)
{
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
		  "sexpr    : '(' <expr>* ')' ;               "
		  "expr     : <number> | <symbol> | <sexpr> ; "
		  "lispy    : /^/ <expr>* /$/ ;               ",
		  Number, Symbol, Sexpr, Expr, Lispy);

	int ret = mpc_parse("<stdin>", input, Lispy, r);
	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

	return ret;
}

static lval_t *lval_read_tag(mpc_ast_t *t)
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

		x = lval_add(x, lval_read_tag(t->children[i]));
	}

	return x;
}

static lval_t *lval_num(long num)
{
	lval_t *v = malloc(sizeof(*v));
	v->type   = LVAL_NUM;
	v->num = num;

	return v;
}

static lval_t *lval_err(char *m)
{
	lval_t *v = malloc(sizeof(*v));
	v->type  = LVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);

	return v;
}

static lval_t *lval_sym(char *m)
{
	lval_t *v = malloc(sizeof(*v));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(m) + 1);
	strcpy(v->sym, m);

	return v;
}

static lval_t *lval_sexpr()
{
	lval_t *v = malloc(sizeof(*v));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;

	return v;
}

static lval_t *lval_add(lval_t *v, lval_t *x)
{
	v->count++;
	v->cell = realloc(v->cell, v->count * sizeof(*v->cell));
	v->cell[v->count - 1] = x;

	return v;
}

static lval_t *lval_read_num(mpc_ast_t *t)
{
	long x = strtol(t->contents, NULL, 10);

	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

static void lval_expr_print(lval_t *v, char open, char close)
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

static void lval_print(const lval_t *v)
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

static lval_t *lval_pop(lval_t *v, int i)
{
	lval_t *x = v->cell[i];

	memmove(&v->cell[i],
		&v->cell[i + 1],
		sizeof(*v->cell) * (v->count - i - 1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(*v->cell) * v->count);

	return x;
}

static lval_t *lval_builtin_op(lval_t *a, char *op)
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

static lval_t *lval_take(lval_t *v, int i)
{
	lval_t *x = lval_pop(v, i);
	lval_del(v);

	return x;
}

static lval_t *lval_eval_sexpr(lval_t *v)
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
