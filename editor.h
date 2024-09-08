#pragma once

#include <stdbool.h>

typedef struct point {
    int r, c;
} point;

int max(int a, int b);
int min(int a, int b);

bool pointEqual(point p1, point p2);

bool pointGreater(point p1, point p2);
bool pointLess(point p1, point p2);

point maxPoint(point p1, point p2);
point minPoint(point p1, point p2);

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

void freeRowarr(struct erow **rowarr, int len);

void deleteChar(struct erow *row, int pos);
void insertChar(struct erow *row, int pos, char c);
void insertString(struct erow *row, int pos, char *str, int len);

void deleteRow(struct editor *E, int rownum);
void insertNewline(struct editor *E, int row, int col);

struct erow **copyRange(struct erow **rows, point start, point end);

void deleteRange(struct editor *E, point start, point end);
void insertRange(struct editor *E, point at, struct erow **rows, int numrows);

void copyToClipboard(struct editor *E, point start, point end);

struct editor *editorFromFile(char *filename);
void editorSaveFile(struct editor *E, char *filename);
void destroyEditor(struct editor **ptr);
