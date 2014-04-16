#include <stdio.h>
#include <stdlib.h>
#include <histedit.h>

#include "mpc.h"
#include "meowlisp.h"

char *get_prompt(EditLine *el)
{
	return "meowlisp> ";
}

int main(int argc, char **argv)
{
	int ret;

	lenv_t *e = lenv_new();
	lenv_add_builtins(e);

	History *h =  history_init();
	HistEvent ev;

	EditLine *el = el_init(argv[0], stdin, stdout, stderr);
	el_set(el, EL_PROMPT, get_prompt);
	el_set(el, EL_EDITOR, "emacs");
	el_set(el, EL_HIST, history, h);
	history(h, &ev, H_SETSIZE, 32);

	puts("Meowlisp Version 0.0.1");
	puts(" \\    /\\ \n"
	     "  )  ( ')\n"
	     " (  /  ) \n"
	     "  \\(__)| \n");
	puts("Press Ctrl+c to Exit\n");

	for (;;) {
		int count = 0;

		const char *input = el_gets(el, &count);
		if (input == NULL) {
			puts("\n");
			break;
		}
		ret = history(h, &ev, H_ENTER, input);
		if (ret == -1) {
			abort();
		}

		lval_t *v = lval_read(input);
		v = lval_eval(e, v);
		lval_println(v);
		lval_del(v);
	}

	el_end(el);
	history_end(h);

	return 0;
}
