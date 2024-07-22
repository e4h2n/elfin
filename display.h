#pragma once
#include <sys/ioctl.h>
#include <stdbool.h>

#include "editor.h"

// 256 Color
// color;style
#define LINENUM_FG "140;2"

typedef enum Mode{
  VIEW,
  INSERT,
  COMMAND,
  QUIT
} Mode;

typedef struct point{
  int r, c;
} point;

struct commandRow{ // for selecting
  int mcol;
  struct erow msg;
};

struct abuf{
  char* buf;
  int size;
};

struct editorInterface{
  char* filename;
  struct editor* E;

  int toprow;
  Mode mode;
  int coloff;

  struct winsize ws;

  // line, pos coords
  point cursor;
  point anchor;

  struct commandRow cmd;
  struct abuf status;
};

int min(int a, int b);
int max(int a, int b);

void abAppend(struct abuf* ab, char* s, int len);
void abFree(struct abuf* ab);

point search(point start, char* needle);

void adjustToprow();
void clearScreen();
void printEditorContents();
void statusPrintMode();
void printEditorStatus();
void resize(int _);
