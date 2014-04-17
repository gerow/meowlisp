CC       = cc

CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -g
CFLAGS  += -Iinclude -Wno-c11-extensions
CFLAGS  += -Wno-gnu-zero-variadic-macro-arguments

LDFLAGS  = -ledit

OBJECTS  = repl.o
OBJECTS += mpc.o
OBJECTS += meowlisp.o

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

meowlisp: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o meowlisp

all: meowlisp

clean:
	-rm -f *.o
	-rm -f meowlisp
