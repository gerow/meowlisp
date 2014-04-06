#include <stdio.h>
#include <stdlib.h>

#include <histedit.h>

char *get_prompt(EditLine *el)
{
	return "meowlisp> ";
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

	puts("Meowlisp Version 0.0.1\n");
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
		printf("No you're a %s\n", input);
	}

	el_end(el);
	history_end(h);

	return 0;
}
