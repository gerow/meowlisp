#include <stdio.h>
#include <stdlib.h>
#include <histedit.h>
#include "mpc.h"

char *get_prompt(EditLine *el)
{
	return "meowlisp> ";
}

long eval_op(long x, char *op, long y)
{
	if (strcmp(op, "+") == 0) {
		return x + y;
	}
	if (strcmp(op, "-") == 0) {
		return x - y;
	}
	if (strcmp(op, "*") == 0) {
		return x * y;
	}
	if (strcmp(op, "/") == 0) {
		return x / y;
	}

	return 0;
}

long eval(mpc_ast_t *t)
{
	if(strstr(t->tag, "number")) {
		return atoi(t->contents);
	}

	char *op = t->children[1]->contents;
	long x = eval(t->children[2]);

	int i = 3;
	while (strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}

	return x;
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
			mpc_ast_print(r.output);
			ast = r.output;
			long result = eval(ast->children[1]);

			printf("%li\n", result);

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
