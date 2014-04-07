#include <stdio.h>
#include <stdlib.h>
#include <histedit.h>
#include "mpc.h"

#define TYPE_NUMBER 0
#define TYPE_ERROR 1

#define LERROR_DIV_ZERO 0
#define LERROR_BAD_OP 1
#define LERROR_BAD_NUM 2

typedef struct {
	int type;
	union {
		long number;
		int error;
	};
} lval_t;

void lval_print(lval_t *v)
{
	switch (v->type) {
		case TYPE_NUMBER:
		printf("number: %li\n", v->number);
		break;
		case TYPE_ERROR:
		switch (v->error) {
			case LERROR_DIV_ZERO:
			printf("error: divide by zero\n");
			break;
			case LERROR_BAD_OP:
			printf("error: bad operator\n");
			break;
			case LERROR_BAD_NUM:
			printf("error: bad number\n");
			break;
			default:
			printf("error: unknown error\n");
		}
		break;
		default:
		printf("Invalid lval\n");
	}
}

void lval_cpy(lval_t *dst, const lval_t *src)
{
	memcpy(dst, src, sizeof(*dst));
}

void lval_num(lval_t *dst, long num)
{
	dst->type   = TYPE_NUMBER;
	dst->number = num;
}

void lval_err(lval_t *dst, int err)
{
	dst->type  = TYPE_ERROR;
	dst->error = err;
}

char *get_prompt(EditLine *el)
{
	return "meowlisp> ";
}

void eval_op(lval_t *r, const lval_t *x, const char *op, const lval_t *y)
{
	if (x->type == TYPE_ERROR) {
		lval_cpy(r, x);
		return;
	}

	if (y->type == TYPE_ERROR) {
		lval_cpy(r, y);
		return;
	}

	if (strcmp(op, "+") == 0) {
		lval_num(r, x->number + y->number);
		return;
	}
	if (strcmp(op, "-") == 0) {
		lval_num(r, x->number - y->number);
		return;
	}
	if (strcmp(op, "*") == 0) {
		lval_num(r, x->number * y->number);
		return;
	}
	if (strcmp(op, "/") == 0) {
		if (y->number == 0) {
			lval_err(r, LERROR_DIV_ZERO);
			return;
		} else {
			lval_num(r, x->number / y->number);
			return;
		}
	}

	lval_err(r, LERROR_BAD_OP);
	return;
}

void eval(lval_t *r, const mpc_ast_t *t)
{
	if(strstr(t->tag, "number")) {
		r->type = TYPE_NUMBER;
		r->number = atoi(t->contents);
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
	mpc_parser_t *String   = mpc_new("string");
	mpc_parser_t *Literal  = mpc_new("literal");
	mpc_parser_t *Operator = mpc_new("operator"); 
	mpc_parser_t *Expr     = mpc_new("expr");
	mpc_parser_t *Lispy    = mpc_new("lispy");

	/* Define them with the following language, yo! */
	mpca_lang(MPC_LANG_DEFAULT,
		  "number   : /-?[0-9]+/ ;                           "
		  "string   : '\"' /[^\"]*/ '\"' ;                   "
		  "literal  : <string> | <number> ;                  "
		  "operator : /[a-zA-Z-]+/ | '+' | '/' | '*';        "
		  "expr     : <literal> | '(' <operator> <expr>+ ')' ;"
		  "lispy    : /^/ <expr>+ /$/ ;           ",
		  Number, String, Literal, Operator, Expr, Lispy);

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
			lval_t rval;
			eval(&rval, ast->children[1]);

			/* printf("%li\n", result); */
			lval_print(&rval);

			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
	}

	el_end(el);
	history_end(h);
	mpc_cleanup(6, Number, String, Literal, Operator, Expr, Lispy);

	return 0;
}
