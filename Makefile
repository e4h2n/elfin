CC = clang
CFLAGS = -Wall -Wpedantic -Werror -Wextra
LDLIBS = -lncurses

all: elfin

elfin: elfin.o editor.o
	$(CC) $(CFLAGS) $(LDLIBS) elfin.o editor.o -o elfin

elfin.o: elfin.c editor.h
	$(CC) $(CFLAGS) -c elfin.c

editor.o: editor.c editor.h
	$(CC) $(CFLAGS) -c editor.c

clean:
	rm -f elfin elfin.o editor.o
