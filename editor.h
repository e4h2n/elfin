#pragma once

typedef struct point {
  int r, c;
} point;

struct erow {
  int len;
  char *text;
};

struct editor {
  int numrows;
  struct erow **rowarray;
  int clipboard_len; // # of rows in the clipboard
  struct erow **clipboard;
};

void deleteChar(struct erow *row, int pos);
void insertChar(struct erow *row, int pos, char c);
void insertString(struct erow *row, int pos, char *str, int len);

void deleteRow(struct editor *E, int rownum);
void insertNewline(struct editor *E, int row, int col);

void copyToClipboard(struct editor *E, point start, point end);
void pasteClipboard(struct editor *E, point at);

struct editor *editorFromFile(char *filename);
void editorSaveFile(struct editor *E, char *filename);
void destroyEditor(struct editor **ptr);
