#include "meowlisp.h"
#include "mpc.h"

static int meowlisp_parse(mpc_result_t *r, const char *input);
static lval_t *lval_read_tag(mpc_ast_t *t);
static lval_t *lval_num(long num);
static lval_t *lval_err(char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
static lval_t *lval_sym(char *m);
static lval_t *lval_sexpr(void);
static lval_t *lval_qexpr(void);
static lval_t *lval_fun(lbuiltin_t func);
static lval_t *lval_lambda(lval_t *formals, lval_t *body);
static lval_t *lval_add(lval_t *v, lval_t *x);
static lval_t *lval_read_num(mpc_ast_t *t);
static lval_t *lval_copy(lval_t *v);
static lval_t *lval_call(lenv_t *e, lval_t *f, lval_t *a);
static lval_t *lenv_get(lenv_t *e, lval_t *v);
static void lenv_put(lenv_t *e, lval_t *k, lval_t *v);
static void lenv_def(lenv_t *e, lval_t *k, lval_t *v);
static lenv_t *lenv_copy(lenv_t *e);
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
static lval_t *builtin_mod(lenv_t *e,lval_t *v);
static lval_t *builtin_join(lenv_t *e, lval_t *a);
static lval_t *builtin_def(lenv_t *e, lval_t *a);
static lval_t *builtin_put(lenv_t *e, lval_t *a);
static lval_t *builtin_lambda(lenv_t *e, lval_t *a);
static lval_t *builtin_var(lenv_t *e, lval_t *a, char *func);
static lval_t *lval_join(lval_t *x, lval_t *y);
static lval_t *lval_take(lval_t *v, int i);
static lval_t *lval_eval_sexpr(lenv_t *e, lval_t *v);
static char *ltype_name(int t);

#define LASSERT(args, cond, fmt, ...)                       \
	if (!(cond)) {                                      \
		lval_t *err = lval_err(fmt, ##__VA_ARGS__); \
		lval_del(args);                             \
		return err;                                 \
	}

#define LASSERT_TYPE(args, function, got, expected)              \
	LASSERT(args, ((got) == (expected)), "Function '%s' passed incorrect types! Got %s, Expected %s.", function, ltype_name(got), ltype_name(expected));

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
		if (!v->builtin) {
			lenv_del(v->env);
			lval_del(v->formals);
			lval_del(v->body);
		}
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
	lenv_add_builtin(e, "%", builtin_mod);

	/* Variable Definitions */
	lenv_add_builtin(e, "def", builtin_def);
	lenv_add_builtin(e, "=", builtin_put);

	/* Lambdas */
	lenv_add_builtin(e, "\\", builtin_lambda);
}

lenv_t *lenv_new(void)
{
	lenv_t *e = malloc(sizeof(*e));
	e->par = NULL;
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
		  "symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%]+/ ;        "
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

static lval_t *lval_err(char *fmt, ...)
{
	lval_t *v = malloc(sizeof(*v));
	v->type  = LVAL_ERR;

	va_list va;
	va_start(va, fmt);
	vasprintf(&v->err, fmt, va);
	va_end(va);

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
	v->builtin = func;

	return v;
}

static lval_t *lval_lambda(lval_t *formals, lval_t *body)
{
	lval_t *v = malloc(sizeof(*v));
	v->type = LVAL_FUN;

	v->builtin = NULL;

	v->env = lenv_new();

	v->formals = formals;
	v->body = body;

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

	return errno != ERANGE ? lval_num(x) : lval_err("'%s' is an invalid number", t->contents);
}

static lval_t *lval_copy(lval_t *v)
{
	lval_t *x = malloc(sizeof(*x));
	x->type = v->type;

	switch(v->type) {
		/* copy functions and numbers directly */
		case LVAL_FUN:
		if (v->builtin) {
			x->builtin = v->builtin;
		} else {
			x->builtin = NULL;
			x->env = lenv_copy(v->env);
			x->formals = lval_copy(v->formals);
			x->body = lval_copy(v->body);
		}
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

static lval_t *lval_call(lenv_t *e, lval_t *f, lval_t *a)
{
	/* if this is a builtin just do that! */
	if (f->builtin) {
		return f->builtin(e, a);
	}

	int given = a->count;
	int total = f->formals->count;

	/* while we still have arguments to bind */
	while (a->count) {
		/* if we've run out of formals to bind.. oh noes! */
		if (f->formals->count == 0) {
			lval_del(a);
			return lval_err("Function passed too many arguments. Got %i, Expected %i.", given, total);
		}

		/* Pop the first symbol from the formals */
		lval_t *sym = lval_pop(f->formals, 0);

		if (strcmp(sym->sym, "&") == 0) {
			if (f->formals->count != 1) {
				lval_del(a);
				return lval_err("Function format invalid. Symbol '&' not followed by single symbol.");
			}

			/* next formal should be bound to remaining arguments */
			lval_t *nsym = lval_pop(f->formals, 0);
			lenv_put(f->env, nsym, builtin_list(e, a));
			lval_del(sym);
			lval_del(nsym);
			break;
		}

		/* Pop the next argument from the list */
		lval_t *val = lval_pop(a, 0);

		/* Bind a copy into the function's environment */
		lenv_put(f->env, sym, val);

		/* Delete the symbol and the value */
		lval_del(sym);
		lval_del(val);
	}

	/* we've completely bound the argument list, so we can clean it up */
	lval_del(a);

	/*
	 * check to see if only a varargs remains. If so, consider the function
	 * resolved and don't do any currying. Just call it with an empty list
	 */
	if (f->formals->count > 0 &&
	    strcmp(f->formals->cell[0]->sym, "&") == 0) {
	    	if (f->formals->count != 2) {
	    		return lval_err("Function format invalid. Symbol '&' not followed by a single symbol.");
	    	}
	    	/* Pop and delete '&' symbol */
	    	lval_del(lval_pop(f->formals, 0));

	    	/* Pop next symbol and create empty list */
	    	lval_t *sym = lval_pop(f->formals, 0);
	    	lval_t *val = lval_qexpr();

	    	/* Bind to environment and delete */
	    	lenv_put(f->env, sym, val);
	    	lval_del(sym);
	    	lval_del(val);
	}

	/* If all formals have been evaluated */
	if (f->formals->count == 0) {
		/* Set Function Environment parent to current evaluation Environment */
		f->env->par = e;

		/* Evaluate and return */
		return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
	}

	/* Otherwise return partially evaluated function */
	return lval_copy(f);
}

static lval_t *lenv_get(lenv_t *e, lval_t *v)
{
	for (int i = 0; i < e->count; i++) {
		if (strcmp(v->sym, e->syms[i]) == 0) {
			return lval_copy(e->vals[i]);
		}
	}

	if (e->par) {
		return lenv_get(e->par, v);
	}

	return lval_err("unbound symbol '%s'", v->sym);
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

/* define in the top environment */
static void lenv_def(lenv_t *e, lval_t *k, lval_t *v)
{
	while (e->par) {
		e = e->par;
	}

	lenv_put(e, k, v);
}

static lenv_t *lenv_copy(lenv_t *e)
{
	lenv_t *n = malloc(sizeof(*n));

	n->par = e->par;
	n->count = e->count;

	n->syms = malloc(sizeof(*n->syms) * n->count);
	n->vals = malloc(sizeof(*n->vals) * n->count);

	for (int i = 0; i < n->count; i++) {
		n->syms[i] = malloc(strlen(e->syms[i] + 1));
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = lval_copy(e->vals[i]);
	}

	return n;
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
		if (v->builtin) {
			printf("<function>");
		} else {
			printf("\\ ");
			lval_print(v->formals);
			putchar(' ');
			lval_print(v->body);
			putchar(')');
		}
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
			return lval_err("Cannot operator on non-number! Got %s, Expected %s",
					 ltype_name(a->cell[i]->type),
					 ltype_name(LVAL_NUM));
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
	LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments! Got %i, Expected %i.", a->count, 1);
	LASSERT_TYPE(a, "head", a->cell[0]->type, LVAL_QEXPR);
	LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed {}!");

	lval_t *v = lval_take(a, 0);

	while (v->count > 1) {
		lval_del(lval_pop(v, 1));
	}

	return v;
}

static lval_t *builtin_tail(lenv_t *e, lval_t *a)
{
	LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments! Got %i, Expected %i", a->count, 1);
	LASSERT_TYPE(a, "tail", a->cell[0]->type, LVAL_QEXPR);
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
	LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments! Got %i, Expected %i", a->count, 1);
	LASSERT_TYPE(a, "eval", a->cell[0]->type, LVAL_QEXPR);

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

static lval_t *builtin_mod(lenv_t *e,lval_t *v)
{
	return builtin_op(e, v, "%");
}

static lval_t *builtin_join(lenv_t *e, lval_t *a)
{
	for (int i = 0; i < a->count; i++) {
		LASSERT_TYPE(a, "join", a->cell[i]->type, LVAL_QEXPR);
	}

	lval_t *x = lval_pop(a, 0);

	while (a->count) {
		x = lval_join(x, lval_pop(a, 0));
	}

	lval_del(a);
	return x;
}

static lval_t *builtin_def(lenv_t *e, lval_t *a)
{
	return builtin_var(e, a, "def");
}

static lval_t *builtin_put(lenv_t *e, lval_t *a)
{
	return builtin_var(e, a, "=");
}

static lval_t *builtin_var(lenv_t *e, lval_t *a, char *func)
{
	LASSERT_TYPE(a, "def", a->cell[0]->type, LVAL_QEXPR);
	lval_t *syms = a->cell[0];

	for (int i = 0; i < syms->count; i++) {
		LASSERT_TYPE(a, "def arg0", syms->cell[i]->type, LVAL_SYM);
	}

	LASSERT(a, (syms->count == a->count - 1), "Function 'def' cannot define number of values to symbols");

	for (int i = 0; i < syms->count; i++) {
		if (strcmp(func, "def") == 0) {
			lenv_def(e, syms->cell[i], a->cell[i + 1]);
		}
		if (strcmp(func, "=") == 0) {
			lenv_put(e, syms->cell[i], a->cell[i + 1]);
		}
	}

	lval_del(a);

	return lval_sexpr();
}

static lval_t *builtin_lambda(lenv_t *e, lval_t *a)
{
	LASSERT(a, (a->count == 2), "Function '\\' passed invalid number of arguments. Got %i, Expected 2", a->count);
	LASSERT_TYPE(a, "\\", a->cell[0]->type, LVAL_QEXPR);
	LASSERT_TYPE(a, "\\", a->cell[1]->type, LVAL_QEXPR);

	for (int i = 0; i < a->cell[0]->count; i++) {
		LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
			"Cannot define non-symbol. Got %s Expected %s.",
			ltype_name(a->cell[0]->cell[i]->type),
			ltype_name(LVAL_SYM));
	}

	lval_t *formals = lval_pop(a, 0);
	lval_t *body = lval_pop(a, 0);

	lval_del(a);

	return lval_lambda(formals, body);
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
		lval_t *err =  lval_err("first element is not a function! Got %s, Expected %s",
				ltype_name(f->type),
				ltype_name(LVAL_FUN));
		lval_del(f);
		lval_del(v);

		return err;
	}

	lval_t *res = lval_call(e, f, v);
	lval_del(f);

	return res;
}

static char *ltype_name(int t)
{
	switch(t) {
		case LVAL_FUN:
		return "Function";
		case LVAL_NUM:
		return "Number";
		case LVAL_ERR:
		return "Error";
		case LVAL_SYM:
		return "Symbol";
		case LVAL_SEXPR:
		return "S-Expression";
		case LVAL_QEXPR:
		return "Q-Expression";
		default:
		return "Unknown";
	}
}
