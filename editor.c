#include "editor.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <assert.h>

#define UNUSED(x) (void)(x)

void freeRow(struct erow** ptr){
  struct erow* row = *ptr;
  free(row->text);
  free(row);
  *ptr = NULL;
}

/* insert a character into a line at the specified position *
 * must be a valid position and row */
void insertChar(struct erow* row, int pos, char c){
  assert(row);
  assert(pos >= 0 && pos <= row->len);

  row->len ++;
  row->text = realloc(row->text, row->len+1);

  memmove(row->text + pos + 1, row->text + pos, row->len - pos);

  row->text[pos] = c;
}

/* insert a string into a line at the specified position *
 * must be a valid position and row */
void insertString(struct erow* row, int pos, char* str, int len){
  assert(row);
  assert(pos >= 0 && pos <= row->len);

  row->len += len;
  row->text = realloc(row->text, row->len+1);
  // previous len = row->len - len
  memmove(row->text + pos + len, row->text + pos, row->len - len - pos);
  strncpy(row->text+pos, str, len);
}

/* delete a  character from a line at the specified position *
 * must be a valid position and row */
void deleteChar(struct erow* row, int pos){
  assert(row);
  assert(pos >= 0 && pos < row->len);

  memmove(row->text + pos, row->text + pos + 1, row->len - pos);
  // realloc?
  row->len --;
}

/* insert a new row in the editor *
 * must be possible (ex. cannot insert row 13 in a 5-row editor) *
 * CAN insert ONE row past the last row(ex. new row 5 into a 5-row editor) */
void newRow(struct editor *E, int rownum){
  assert(rownum <= E->numrows);

  E->rowarray = realloc(E->rowarray, (E->numrows+1)*sizeof(struct erow*));
  E->rowarray[E->numrows] = NULL;
  int shift = E->numrows - rownum;
  memmove(E->rowarray + rownum+1, E->rowarray + rownum, shift*sizeof(struct erow*));

  struct erow* new_row = malloc(sizeof(struct erow));
  new_row->len = 0;
  new_row->text = malloc(sizeof(char));
  new_row->text[0] = '\0';

  E->rowarray[rownum] = new_row;
  E->numrows++;
}

/* insert \n at the specified postion *
 * moves everything past the \n to a new row */
void insertNewline(struct editor *E, int row, int col){
  assert(row < E->numrows);
  assert(col <= E->rowarray[row]->len);

  newRow(E, row+1);
  struct erow* curr_row = E->rowarray[row];
  struct erow* new_row = E->rowarray[row+1];

  new_row->text = realloc(new_row->text, curr_row->len - col + 1);
  strncpy(new_row->text, curr_row->text + col, curr_row->len - col + 1);
  new_row->len = curr_row->len - col;

  curr_row->text[col] = '\0';
  curr_row->len = col;
}

void deleteRow(struct editor *E, int rownum){
  assert(rownum >= 0);
  assert(E->numrows > rownum);
  // realloc?
  freeRow(E->rowarray + rownum);
  int shift = E->numrows-1 - rownum;
  memmove(E->rowarray + rownum, E->rowarray + rownum+1, shift*sizeof(struct erow*));
  E->numrows --;
}

struct editor* editorFromFile(char* filename){
  struct editor* E = malloc(sizeof(editor));
  E->numrows = 0;
  E->rowarray = NULL;
  FILE* fp = fopen(filename, "r");
  newRow(E, 0);
  if (!fp){ // NEW FILE
    return E;
  }

  int c;
  while((c = fgetc(fp)) != EOF){
    struct erow* curr_row = E->rowarray[E->numrows - 1];
    if (c == '\n'){
      newRow(E, E->numrows);
    } else if (c != '\r'){
      insertChar(curr_row, curr_row->len, c);
    }
  }
  // don't create a new line for the last line terminator
  // for a non-empty file
  if(E->numrows > 1 && E->rowarray[E->numrows-1]->len == 0){
    deleteRow(E, E->numrows-1);
  }
  fclose(fp);
  return E;
}

void editorSaveFile(struct editor* E, char* filename){
  FILE* fp = fopen(filename, "w");
  for(int i = 0; i < E->numrows; i++){
    fprintf(fp, "%s\n", E->rowarray[i]->text);
  }
  fclose(fp);
}

void destroyEditor(struct editor** ptr){
  struct editor* E = *ptr;
  for(int i = 0; i < E->numrows; i++){
    freeRow(E->rowarray+i);
  }
  free(E->rowarray);
  free(E);
  *ptr = NULL;
}
