CC = clang
CFLAGS = -Wall -Wpedantic -Werror -Wextra

all: elfin

elfin: elfin.o display.o editor.o command.o
	$(CC) $(CFLAGS) $(LDLIBS) elfin.o display.o editor.o command.o -o elfin

elfin.o: elfin.c editor.h
	$(CC) $(CFLAGS) -c elfin.c

display.o: display.c display.h
	$(CC) $(CFLAGS) -c display.c

editor.o: editor.c editor.h
	$(CC) $(CFLAGS) -c editor.c

command.o: command.c command.h
	$(CC) $(CFLAGS) -c command.c

clean:
	rm -f elfin elfin.o display.o editor.o command.o
