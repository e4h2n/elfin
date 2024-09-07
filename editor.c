#include "editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#define UNUSED(x) (void)(x)

int min(int a, int b) { return ((a < b) ? (a) : (b)); }
int max(int a, int b) { return ((a > b) ? (a) : (b)); }

/* ======= POINT UTILS ======= */
bool pointEqual(point p1, point p2) { return (p1.r == p2.r && p1.c == p2.c); }

bool pointGreater(point p1, point p2) {
    if (p1.r > p2.r) {
        return true;
    } else if (p1.r < p2.r) {
        return false;
    } else {
        return (p1.c > p2.c);
    }
}

bool pointLess(point p1, point p2) {
    if (p1.r < p2.r) {
        return true;
    } else if (p1.r > p2.r) {
        return false;
    } else {
        return (p1.c < p2.c);
    }
}

point maxPoint(point p1, point p2) {
    if (pointGreater(p1, p2)) {
        return p1;
    } else {
        return p2;
    }
}

point minPoint(point p1, point p2) {
    if (pointLess(p1, p2)) {
        return p1;
    } else {
        return p2;
    }
}

void freeRow(struct erow **ptr) {
    struct erow *row = *ptr;
    free(row->text);
    free(row);
    *ptr = NULL;
}

/* insert a character into a line at the specified position *
 * must be a valid position and row */
void insertChar(struct erow *row, int pos, char c) {
    assert(row);
    assert(pos >= 0 && pos <= row->len);

    row->len++;
    row->text = realloc(row->text, row->len + 1);

    memmove(row->text + pos + 1, row->text + pos, row->len - pos);

    row->text[pos] = c;
}

/* insert a string into a line at the specified position *
 * must be a valid position and row */
void insertString(struct erow *row, int pos, char *str, int len) {
    assert(row);
    assert(pos >= 0 && pos <= row->len);
    if (len == 0)
        return;

    row->len += len;
    row->text = realloc(row->text, row->len + 1);
    // previous len = row->len - len
    memmove(row->text + pos + len, row->text + pos, row->len - len - pos);
    strncpy(row->text + pos, str, len);
}

/* delete a character from a line at the specified position *
 * must be a valid position and row */
void deleteChar(struct erow *row, int pos) {
    assert(row);
    assert(pos >= 0 && pos < row->len);

    memmove(row->text + pos, row->text + pos + 1, row->len - pos);
    // realloc?
    row->len--;
}

/* insert a new row in the editor *
 * must be possible (ex. cannot insert row 13 in a 5-row editor) *
 * CAN insert ONE row past the last row(ex. new row 5 into a 5-row editor) */
void newRow(struct editor *E, int rownum) {
    assert(rownum <= E->numrows);

    E->rowarray =
        realloc(E->rowarray, (E->numrows + 1) * sizeof(struct erow *));
    E->rowarray[E->numrows] = NULL;
    int shift = E->numrows - rownum;
    memmove(E->rowarray + rownum + 1, E->rowarray + rownum,
            shift * sizeof(struct erow *));

    struct erow *new_row = malloc(sizeof(struct erow));
    new_row->len = 0;
    new_row->text = malloc(sizeof(char));
    new_row->text[0] = '\0';

    E->rowarray[rownum] = new_row;
    E->numrows++;
}

/* insert \n at the specified postion *
 * moves everything past the \n to a new row */
void insertNewline(struct editor *E, int row, int col) {
    assert(row < E->numrows);
    assert(col <= E->rowarray[row]->len);

    newRow(E, row + 1);
    struct erow *curr_row = E->rowarray[row];
    struct erow *new_row = E->rowarray[row + 1];

    new_row->text = realloc(new_row->text, curr_row->len - col + 1);
    strncpy(new_row->text, curr_row->text + col, curr_row->len - col + 1);
    new_row->len = curr_row->len - col;

    curr_row->text[col] = '\0';
    curr_row->len = col;
}

void deleteRow(struct editor *E, int rownum) {
    assert(rownum >= 0);
    assert(E->numrows > rownum);
    if (E->numrows == 1)
        return;
    // realloc?
    freeRow(E->rowarray + rownum);
    int shift = E->numrows - 1 - rownum;
    memmove(E->rowarray + rownum, E->rowarray + rownum + 1,
            shift * sizeof(struct erow *));
    E->numrows--;
}

void freeRowarr(struct erow **rowarr, int len) {
    for (int i = 0; i < len; i++) {
        freeRow(rowarr + i);
    }
}

void deleteRange(struct editor *E, point start, point end) {
    struct erow *start_row = E->rowarray[start.r];
    if (pointEqual(start, end)) {
        if (start_row->len == 0) {
            deleteRow(E, start.r);
        } else {
            deleteChar(E->rowarray[start.r], start.c);
        }
        return;
    }
    int shifted = E->numrows - end.r;
    int deleted = end.r - start.r;

    // update start row
    if (end.r == start.r) {
        memmove(start_row->text + start.c, start_row->text + end.c + 1,
                (start_row->len - end.c - 1) * sizeof(char));
        start_row->len += start.c - end.c - 1;
        return;
    }
    start_row->len = start.c;

    // update end row
    struct erow *end_row = E->rowarray[end.r];
    memmove(end_row->text, end_row->text + end.c + 1,
            max(0, end_row->len - end.c - 1) * sizeof(char));
    end_row->len -= end.c + 1;
    end_row->len = max(0, end_row->len);

    // append start_row to the front of end_row
    insertString(end_row, 0, start_row->text, start_row->len);

    // free/delete the locations that will be overwritten
    freeRowarr(E->rowarray + start.r, deleted);
    E->numrows -= deleted;

    // shift everything
    memmove(E->rowarray + start.r, E->rowarray + end.r,
            shifted * sizeof(struct erow *));
}

void copyToClipboard(struct editor *E, point start, point end) {
    freeRowarr(E->clipboard, E->clipboard_len);

    E->clipboard_len = end.r - start.r + 1;
    E->clipboard =
        realloc(E->clipboard, sizeof(struct erow *) * E->clipboard_len);

    E->clipboard[0] = malloc(sizeof(struct erow));
    E->clipboard[0]->len = 0;
    insertString(E->clipboard[0], 0, E->rowarray[start.r]->text + start.c,
                 E->rowarray[start.r]->len - start.c);
    for (int i = 1; i < E->clipboard_len - 1; i++) {
        E->clipboard[i] = malloc(sizeof(struct erow));
        E->clipboard[i]->len = 0;
        insertString(E->clipboard[i], 0, E->rowarray[start.r + i]->text,
                     E->rowarray[start.r + i]->len);
    }
    E->clipboard[E->clipboard_len - 1] = malloc(sizeof(struct erow));
    E->clipboard[E->clipboard_len - 1]->len = 0;
    insertString(E->clipboard[end.r - start.r], 0, E->rowarray[end.r]->text,
                 end.c + 1);
}

void pasteClipboard(struct editor *E, point at) {
    assert(at.r >= 0 && at.r < E->numrows);

    insertString(E->rowarray[at.r], at.c, E->clipboard[0]->text,
                 E->clipboard[0]->len);

    int shifted = E->numrows - at.r - 1;
    E->numrows += E->clipboard_len - 1;
    E->rowarray = realloc(E->rowarray, E->numrows * sizeof(struct erow *));
    memmove(E->rowarray + at.r + E->clipboard_len, E->rowarray + at.r + 1,
            shifted * sizeof(struct erow *));

    for (int i = 1; i < E->clipboard_len; i++) {
        E->rowarray[at.r + i] = malloc(sizeof(struct erow));
        E->rowarray[at.r + i]->len = 0;
        insertString(E->rowarray[at.r + i], 0, E->clipboard[i]->text,
                     E->clipboard[i]->len);
    }
}

struct editor *editorFromFile(char *filename) {
    struct editor *E = malloc(sizeof(struct editor));
    E->numrows = 0;
    E->rowarray = NULL;
    FILE *fp = fopen(filename, "r");
    newRow(E, 0);
    if (!fp) { // NEW FILE
        return E;
    }

    int c;
    while ((c = fgetc(fp)) != EOF) {
        struct erow *curr_row = E->rowarray[E->numrows - 1];
        if (c == '\n') {
            newRow(E, E->numrows);
        } else if (c != '\r') {
            insertChar(curr_row, curr_row->len, c);
        }
    }
    // don't create a new line for the last line terminator
    // for a non-empty file
    if (E->numrows > 1 && E->rowarray[E->numrows - 1]->len == 0) {
        deleteRow(E, E->numrows - 1);
    }
    fclose(fp);
    return E;
}

void editorSaveFile(struct editor *E, char *filename) {
    FILE *fp = fopen(filename, "w");
    for (int i = 0; i < E->numrows; i++) {
        fprintf(fp, "%s\n", E->rowarray[i]->text);
    }
    fclose(fp);
}

void destroyEditor(struct editor **ptr) {
    struct editor *E = *ptr;
    freeRowarr(E->rowarray, E->numrows);
    freeRowarr(E->clipboard, E->clipboard_len);

    free(E->rowarray);
    free(E->clipboard);

    free(E);
    *ptr = NULL;
}
