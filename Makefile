CC      = cc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c99
LDFLAGS = -ledit

OBJECTS = repl.o

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

meowlisp: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o meowlisp

all: meowlisp

clean:
	-rm -f *.o
	-rm -f meowlisp
