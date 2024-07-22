#pragma once

struct erow {
  int len;
  char* text;
} erow;

struct editor {
  int numrows;
  struct erow** rowarray;
} editor;

void deleteChar(struct erow* row, int pos);
void insertChar(struct erow* row, int pos, char c);
void insertString(struct erow* row, int pos, char* str, int len);

void deleteRow(struct editor* E, int rownum);
void insertNewline(struct editor *E, int row, int col);

struct editor* editorFromFile(char* filename);
void editorSaveFile(struct editor* E, char* filename);
void destroyEditor(struct editor** ptr);
