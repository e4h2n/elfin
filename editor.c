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
    assert(pos >= 0);
    assert(pos <= row->len);
    if (len == 0) {
        return;
    }

    // whether str overlaps with row->text, if so we need to update the str
    // pointer after realloc'ing row->text -2 hours :)
    bool overlaps = str >= row->text && str < row->text + row->len;
    int diff = str - row->text;

    row->len += len;
    row->text = realloc(row->text, row->len);
    if (overlaps) {
        str = row->text + diff;
    }
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
        freeRow(&rowarr[i]);
    }
}

struct erow **copyRange(struct erow **rows, point start, point end) {
    int numrows = end.r - start.r + 1;
    struct erow **ret = calloc(sizeof(struct erow *), numrows);
    if (numrows == 1) {
        ret[0] = malloc(sizeof(struct erow));
        ret[0]->len = end.c - start.c + 1;
        ret[0]->text = calloc(sizeof(char), ret[0]->len);
        strncpy(ret[0]->text, rows[start.r]->text + start.c, ret[0]->len);
        return ret;
    }
    // copy first row starting from start.c
    ret[0] = malloc(sizeof(struct erow));
    ret[0]->len = rows[start.r]->len - start.c;
    ret[0]->text = calloc(sizeof(char), ret[0]->len);
    strncpy(ret[0]->text, rows[start.r]->text + start.c, ret[0]->len);
    // copy middle rows
    for (int i = 1; i < numrows - 1; i++) {
        ret[i] = malloc(sizeof(struct erow));
        ret[i]->len = rows[start.r + i]->len;
        ret[i]->text = calloc(sizeof(char), ret[i]->len);
        strncpy(ret[i]->text, rows[start.r + i]->text, ret[i]->len);
    }
    // copy last row ending at end.c
    ret[numrows - 1] = malloc(sizeof(struct erow));
    ret[numrows - 1]->len = end.c + 1;
    ret[numrows - 1]->text = calloc(sizeof(char), ret[numrows - 1]->len);
    strncpy(ret[numrows - 1]->text, rows[end.r]->text, ret[numrows - 1]->len);

    return ret;
}

void deleteRange(struct editor *E, point start, point end) {
    struct erow *start_row = E->rowarray[start.r];
    struct erow *end_row = E->rowarray[end.r];

    int shifted = E->numrows - end.r;
    int deleted = end.r - start.r;

    // append start_row to the front of end_row
    int insert_pos = min(end_row->len, end.c + 1);
    insertString(end_row, insert_pos, start_row->text, start.c);

    memmove(end_row->text, end_row->text + insert_pos,
            max(0, end_row->len - insert_pos) * sizeof(char));
    end_row->len -= insert_pos;
    assert(end_row->len >= 0);

    // free/delete the locations that will be overwritten
    freeRowarr(E->rowarray + start.r, deleted);
    E->numrows -= deleted;

    // shift rows to fill the freed entries
    memmove(E->rowarray + start.r, E->rowarray + end.r,
            shifted * sizeof(struct erow *));
}

void insertRange(struct editor *E, point at, struct erow **rows, int numrows) {
    assert(at.r >= 0 && at.r < E->numrows);

    if (numrows == 1) {
        insertString(E->rowarray[at.r], at.c, rows[0]->text, rows[0]->len);
        return;
    }

    insertNewline(E, at.r, at.c);
    insertString(E->rowarray[at.r], at.c, rows[0]->text, rows[0]->len);

    int shifted = E->numrows - at.r;
    E->numrows += numrows - 2;
    E->rowarray = realloc(E->rowarray, E->numrows * sizeof(struct erow *));
    memmove(E->rowarray + at.r + numrows - 1, E->rowarray + at.r + 1,
            shifted * sizeof(struct erow *));

    for (int i = 1; i < numrows - 1; i++) {
        E->rowarray[at.r + i] = malloc(sizeof(struct erow));
        E->rowarray[at.r + i]->len = 0;
        insertString(E->rowarray[at.r + i], 0, rows[i]->text, rows[i]->len);
    }

    insertString(E->rowarray[at.r + numrows - 1], 0, rows[numrows - 1]->text,
                 rows[numrows - 1]->len);
}

void copyToClipboard(struct editor *E, point start, point end) {
    freeRowarr(E->clipboard, E->clipboard_len);
    free(E->clipboard);

    E->clipboard_len = end.r - start.r + 1;
    E->clipboard = copyRange(E->rowarray, start, end);
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
