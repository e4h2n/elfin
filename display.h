#pragma once
#include <stdbool.h>
#include <sys/ioctl.h>

#include "command.h"
#include "editor.h"

// RGB
// color;style
#define LINENUM_FG "140;2;1"
#define SELECT_BG "42;64;121"

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
