#include "meowlisp.h"
#include "mpc.h"

static int meowlisp_parse(mpc_result_t *r, const char *input);
static lval_t *lval_read_tag(mpc_ast_t *t);
static lval_t *lval_num(long num);
static lval_t *lval_err(char *m);
static lval_t *lval_sym(char *m);
static lval_t *lval_sexpr(void);
static lval_t *lval_qexpr(void);
static lval_t *lval_fun(lbuiltin_t func);
static lval_t *lval_add(lval_t *v, lval_t *x);
static lval_t *lval_read_num(mpc_ast_t *t);
static lval_t *lval_copy(lval_t *v);
static lval_t *lenv_get(lenv_t *e, lval_t *v);
static void lenv_put(lenv_t *e, lval_t *k, lval_t *v);
static void lval_expr_print(lval_t *v, char open, char close);
static void lval_print(const lval_t *v);
static lval_t *lval_pop(lval_t *v, int i);
/* static lval_t *builtin(lval_t *a, char *func); */
static lval_t *builtin_op(lenv_t *e, lval_t *a, char *op);
static lval_t *builtin_head(lenv_t *e, lval_t *v);
static lval_t *builtin_tail(lenv_t *e, lval_t *v);
static lval_t *builtin_list(lenv_t *e, lval_t *v);
static lval_t *builtin_eval(lenv_t *e, lval_t *v);
static lval_t *builtin_add(lenv_t *e,lval_t *v);
static lval_t *builtin_sub(lenv_t *e,lval_t *v);
static lval_t *builtin_mul(lenv_t *e,lval_t *v);
static lval_t *builtin_div(lenv_t *e,lval_t *v);
static lval_t *builtin_join(lenv_t *e, lval_t *a);
static lval_t *lval_join(lval_t *x, lval_t *y);
static lval_t *lval_take(lval_t *v, int i);
static lval_t *lval_eval_sexpr(lenv_t *e, lval_t *v);

#define LASSERT(args, cond, err)      \
	if (!(cond)) {                \
		lval_del(args);       \
		return lval_err(err); \
	}

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

lval_t *lval_eval(lenv_t *e, lval_t *v)
{
	if (v->type == LVAL_SYM) {
		lval_t *x = lenv_get(e, v);
		lval_del(v);
		return x;
	}
	if (v->type == LVAL_SEXPR) {
		return lval_eval_sexpr(e, v);
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
		case LVAL_QEXPR:
		for (int i = 0; i < v->count; i++) {
			lval_del(v->cell[i]);
		}
		free(v->cell);
		break;
		case LVAL_FUN:
		break;
	}

	free(v);
}

void lenv_add_builtin(lenv_t *e, char *name, lbuiltin_t func)
{
	lval_t *k = lval_sym(name);
	lval_t *f = lval_fun(func);

	lenv_put(e, k, f);
	lval_del(k);
	lval_del(f);
}

void lenv_add_builtins(lenv_t *e)
{
	/* List Functions */
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);

	/* Mathematical Functions */
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
}

lenv_t *lenv_new(void)
{
	lenv_t *e = malloc(sizeof(*e));
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;

	return e;
}

void lenv_del(lenv_t *e)
{
	for (int i = 0; i < e->count; i++) {
		/* free the strings */
		free(e->syms[i]);
		/* delete the values */
		lval_del(e->vals[i]);
	}

	free(e->syms);
	free(e->vals);

	free(e);
}

/* static functions */

static int meowlisp_parse(mpc_result_t *r, const char *input)
{
	/* Create some parsers, yo! */
	mpc_parser_t *Number   = mpc_new("number");
	mpc_parser_t *Symbol   = mpc_new("symbol");
	mpc_parser_t *Sexpr    = mpc_new("sexpr");
	mpc_parser_t *Qexpr    = mpc_new("qexpr");
	mpc_parser_t *Expr     = mpc_new("expr");
	mpc_parser_t *Lispy    = mpc_new("lispy");

	/* Define them with the following language, yo! */
	mpca_lang(MPC_LANG_DEFAULT,
		  "number   : /-?[0-9]+/ ;                              "
		  "symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;        "
		  "sexpr    : '(' <expr>* ')' ;                         "
		  "qexpr    : '{' <expr>* '}' ;                         "
		  "expr     : <number> | <symbol> | <sexpr> | <qexpr> ; "
		  "lispy    : /^/ <expr>* /$/ ;                         ",
		  Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	int ret = mpc_parse("<stdin>", input, Lispy, r);
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

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
	if (strcmp(t->tag, ">") == 0 || strstr(t->tag, "sexpr")) {
		x = lval_sexpr();
	} else if (strstr(t->tag, "qexpr")) {
		x = lval_qexpr();
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

static lval_t *lval_sexpr(void)
{
	lval_t *v = malloc(sizeof(*v));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;

	return v;
}

static lval_t *lval_qexpr(void)
{
	lval_t *v = malloc(sizeof(*v));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;

	return v;
}

static lval_t *lval_fun(lbuiltin_t func)
{
	lval_t *v = malloc(sizeof(*v));
	v->type = LVAL_FUN;
	v->fun = func;

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

static lval_t *lval_copy(lval_t *v)
{
	lval_t *x = malloc(sizeof(*x));
	x->type = v->type;

	switch(v->type) {
		/* copy functions and numbers directly */
		case LVAL_FUN:
		x->fun = v->fun;
		break;

		case LVAL_NUM:
		x->num = v->num;
		break;

		/* copy strings using malloc and strcpy */
		case LVAL_ERR:
		x->err = malloc(strlen(v->err) + 1);
		strcpy(x->err, v->err);
		break;

		case LVAL_SYM:
		x->sym = malloc(strlen(v->sym) + 1);
		strcpy(x->sym, v->sym);
		break;

		/* copy lists by copying each sub expression */
		case LVAL_SEXPR:
		case LVAL_QEXPR:
		x->count = v->count;
		x->cell = malloc(sizeof(*x->cell) * x->count);
		for (int i = 0; i < v->count; i++) {
			x->cell[i] = lval_copy(v->cell[i]);
		}
		break;
	}

	return x;
}

static lval_t *lenv_get(lenv_t *e, lval_t *v)
{
	for (int i = 0; i < e->count; i++) {
		if (strcmp(v->sym, e->syms[i]) == 0) {
			return lval_copy(e->vals[i]);
		}
	}

	return lval_err("unbound symbol!");
}

static void lenv_put(lenv_t *e, lval_t *k, lval_t *v)
{
	for (int i = 0; i < e->count; i++) {
		if (strcmp(k->sym, e->syms[i]) == 0) {
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}

	e->count++;
	e->vals = realloc(e->vals, sizeof(*e->vals) * e->count);
	e->syms = realloc(e->syms, sizeof(*e->syms) * e->count);

	e->vals[e->count - 1] = lval_copy(v);
	e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
	strcpy(e->syms[e->count - 1], k->sym);
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
		case LVAL_FUN:
		printf("<function>");
		break;
		case LVAL_SEXPR:
		lval_expr_print(v, '(', ')');
		break;
		case LVAL_QEXPR:
		lval_expr_print(v, '{', '}');
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

/*
static lval_t *builtin(lval_t *a, char *func)
{
	if (strcmp(func, "head") == 0) {
		return builtin_head(a);
	}
	if (strcmp(func, "tail") == 0) {
		return builtin_tail(a);
	}
	if (strcmp(func, "list") == 0) {
		return builtin_list(a);
	}
	if (strcmp(func, "eval") == 0) {
		return builtin_eval(a);
	}
	if (strcmp(func, "join") == 0) {
		return builtin_join(a);
	}
	if (strstr("+-/*%", func)) {
		return builtin_op(a, func);
	}

	lval_del(a);
	return lval_err("Unknown function!");
}
*/

static lval_t *builtin_op(lenv_t *e, lval_t *a, char *op)
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
		if (strcmp(op, "%") == 0) {
			if (y->num == 0) {
				lval_del(x);
				lval_del(y);
				lval_del(a);
				return lval_err("Division (mod) by Zero!");
			}
			x->num = x->num % y->num;
		}

		lval_del(y);
	}

	lval_del(a);

	return x;
}

static lval_t *builtin_head(lenv_t *e, lval_t *a)
{
	LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments!");
	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect types!");
	LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed {}!");

	lval_t *v = lval_take(a, 0);

	while (v->count > 1) {
		lval_del(lval_pop(v, 1));
	}

	return v;
}

static lval_t *builtin_tail(lenv_t *e, lval_t *a)
{
	LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments!");
	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect types!");
	LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed {}!");

	lval_t *v = lval_take(a, 0);

	lval_del(lval_pop(v, 0));

	return v;
}

static lval_t *builtin_list(lenv_t *e, lval_t *a)
{
	a->type = LVAL_QEXPR;

	return a;
}

static lval_t *builtin_eval(lenv_t *e, lval_t *a)
{
	LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments!");
	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

	lval_t *x = lval_take(a, 0);
	x->type = LVAL_SEXPR;

	return lval_eval(e, x);
}

static lval_t *builtin_add(lenv_t *e,lval_t *v)
{
	return builtin_op(e, v, "+");
}

static lval_t *builtin_sub(lenv_t *e,lval_t *v)
{
	return builtin_op(e, v, "-");
}

static lval_t *builtin_mul(lenv_t *e,lval_t *v)
{
	return builtin_op(e, v, "*");
}

static lval_t *builtin_div(lenv_t *e,lval_t *v)
{
	return builtin_op(e, v, "/");
}

static lval_t *builtin_join(lenv_t *e, lval_t *a)
{
	for (int i = 0; i < a->count; i++) {
		LASSERT(a, (a->cell[i]->type == LVAL_QEXPR), "Function 'join' passed incorrect type!");
	}

	lval_t *x = lval_pop(a, 0);

	while (a->count) {
		x = lval_join(x, lval_pop(a, 0));
	}

	lval_del(a);
	return x;
}

static lval_t *lval_join(lval_t *x, lval_t *y)
{
	while (y->count) {
		x = lval_add(x, lval_pop(y, 0));
	}

	lval_del(y);

	return x;
}

static lval_t *lval_take(lval_t *v, int i)
{
	lval_t *x = lval_pop(v, i);
	lval_del(v);

	return x;
}

static lval_t *lval_eval_sexpr(lenv_t *e, lval_t *v)
{
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(e, v->cell[i]);
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
	if (f->type != LVAL_FUN) {
		lval_del(f);
		lval_del(v);
		return lval_err("first element is not a function!");
	}

	lval_t *res = f->fun(e, v);
	lval_del(f);

	return res;
}
