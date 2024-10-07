#pragma once
#include <stdbool.h>
#include <sys/ioctl.h>

#include "command.h"
#include "editor.h"

// RGB
#define FG "224;222;244"
#define BG "35;33;54"

#define CURSORLINE_FG "246;193;119"
#define LINENUM_FG "144;140;170"

#define STATUSLINE_A_BG "246;193;119"
#define STATUSLINE_A_FG "42;40;62"
#define STATUSLINE_BG "57;53;82"
#define STATUSLINE_FG "246;193;119"

#define SELECT_BG "86;82;110"

typedef enum Mode { VIEW, INSERT, COMMAND, QUIT } Mode;

struct commandRow { // for selecting
    int mcol;
    struct erow msg;
};

struct abuf {
    char *buf;
    int size;
};

struct editorInterface {
    char *filename;
    struct editor *E;

    int toprow;
    Mode mode;
    int coloff;

    struct winsize ws;

    // line, pos coords
    point cursor;
    point anchor;

    struct commandRow cmd;
    struct abuf status;

    struct commandStack *cmdStack;
};

int min(int a, int b);
int max(int a, int b);

void abAppend(struct abuf *ab, char *s, int len);
void abFree(struct abuf *ab);

point search(point start, char *needle);

void adjustToprow(void);
void clearScreen(void);
void printEditorContents(void);
void statusPrintMode(void);
void printEditorStatus(void);
void resize(int _);
