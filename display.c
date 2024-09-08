#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "display.h"

#define szstr(str) str, sizeof(str)

extern struct editorInterface *I;

/* ======= ESC SEQUENCE UTILS ======= */
void abAppend(struct abuf *ab, char *s, int len) {
    char *new = realloc(ab->buf, ab->size + len);
    if (new == NULL)
        return; // :(
    memcpy(new + ab->size, s, len);
    ab->buf = new;
    ab->size += len;
}

// append the MOVE esc sequence to a string
void move(struct abuf *ab, int r, int c) {
    char *buf;
    int len = asprintf(&buf, "\x1b[%d;%dH", r, c);
    if (len < 0)
        return;
    abAppend(ab, buf, len);
    free(buf);
}

void abFree(struct abuf *ab) {
    free(ab->buf);
    free(ab);
}

/* ======= COMMAND STUFF ======= */
point search(point start, char *needle) {
    start.c++;
    // search from start
    for (int i = start.r; i < I->E->numrows; i++) {
        struct erow *curr_row = I->E->rowarray[i];
        int start_c = i == start.r ? start.c : 0;
        char *loc =
            strnstr(curr_row->text + start_c, needle, curr_row->len - start_c);
        if (loc) {
            point ret = {i, loc - curr_row->text};
            return ret;
        }
    }
    // search from beginning
    for (int i = 0; i < I->E->numrows; i++) {
        struct erow *curr_row = I->E->rowarray[i];
        char *loc = strnstr(curr_row->text, needle, curr_row->len);
        if (loc) {
            point ret = {i, loc - curr_row->text};
            return ret;
        }
    }
    start.c--;
    return start;
}

/* ======= DISPLAY UTILS ======= */
point getBoundedCursor(void) {
    point out = I->cursor;
    out.c = min(out.c, I->E->rowarray[out.r]->len);
    return out;
}

point getLargestDisplayedPoint(
    void) { // TODO technically doesn't work because of wide chars
    int maxr = I->ws.ws_row;
    int maxc = I->ws.ws_col - I->coloff - 1;
    struct editor *E = I->E;

    int visual_r = 1;

    int r;
    int len;
    for (r = I->toprow; r < E->numrows && visual_r < maxr; r++) {
        len = E->rowarray[r]->len;
        visual_r += len / maxc + (len % maxc != 0) + (len == 0);
    }
    int extra = max(visual_r - maxr, 0);
    if (extra != 0) {
        len -= len % maxc - (extra - 1) * maxc;
    }
    point out = {r - 1, len};
    return out;
}

void adjustToprow(void) {
    if (I->cursor.r < I->toprow) {
        I->toprow = I->cursor.r;
        return;
    }

    while (pointGreater(getBoundedCursor(), getLargestDisplayedPoint())) {
        I->toprow++;
    }
}

/* ======= DISPLAY ======= */
void clearScreen(void) { write(STDIN_FILENO, szstr("\x1b[2J")); }

void printEditorContents(void) {
    int maxr = I->ws.ws_row;                 // height
    int maxc = I->ws.ws_col - I->coloff - 1; // width

    struct editor *E = I->E;
    int visual_r = 1;

    point save_cursor;
    save_cursor.r = -1; // default not found

    struct abuf ab = {NULL, 0};        // buffer for the WHOLE screen
    abAppend(&ab, szstr("\x1b[?25l")); // hide cursor

    point startSel;
    point endSel;
    bool select = false;
    if (I->anchor.r != -1) {
        startSel = minPoint(I->cursor, I->anchor);
        startSel.c = min(E->rowarray[startSel.r]->len - 1, startSel.c);
        endSel = maxPoint(I->cursor, I->anchor);
        endSel.c = min(E->rowarray[endSel.r]->len - 1, endSel.c);
        select = startSel.r < I->toprow;
    }

    // ITER OVER THE ROWS
    for (int r = I->toprow; visual_r < maxr && r < E->numrows; r++) {
        struct erow *curr_row = E->rowarray[r];
        int visual_c = -1; // same as displayed col when modded by maxc and
                           // adjusted by coloff

        /* LINENUM DISPLAY */
        move(&ab, visual_r, 0);
        char linenum[I->coloff];
        sprintf(linenum, "%*d ", I->coloff - 1, r + 1);
        abAppend(&ab, szstr("\x1b[38;5;" LINENUM_FG "m")); // set FG color
        abAppend(&ab, linenum, I->coloff);
        abAppend(&ab, szstr("\x1b[m")); // reset all formatting
        if (select) {
            abAppend(&ab, szstr("\x1b[48;2;" SELECT_BG "m")); // start sel
        }

        if (curr_row->len == 0) {  // empty line
            if (startSel.r == r) { // start selection here
                abAppend(&ab, szstr("\x1b[48;2;" SELECT_BG "m")); // start sel
                select = true;
            }
            abAppend(&ab, szstr(" "));
            if (endSel.r == r) {                  // end selection here
                abAppend(&ab, szstr("\x1b[49m")); // end sel
                select = false;
            }
            if (I->cursor.r == r) { // cursor here
                save_cursor.r = visual_r;
                save_cursor.c = I->coloff + 1;
            }
        }
        // ITER OVER EACH CHAR (0-indexed)
        for (int c = 0; c < curr_row->len && visual_r < maxr; c++) {
            point curr = {r, c};
            /* START SELECTION HIGHLIGHTING */
            if (pointEqual(curr, startSel)) {
                abAppend(&ab, szstr("\x1b[48;2;" SELECT_BG "m")); // start sel
                select = true;
            }
            if (pointEqual(curr, endSel)) {
                select = false;
            }

            char *to_add = curr_row->text + c;
            int cwidth = 1;
            if (*to_add == '\t') { // TODO abstract
                cwidth = 4;
            }
            visual_c += cwidth;

            /* SUBLINE HANDLING */
            if (c != 0 && visual_c % maxc < cwidth) { // new subline?
                visual_c +=
                    cwidth - 1 -
                    (visual_c % maxc); // (wide only) how many 'slots' skipped?
                if (++visual_r >= maxr)
                    break;
                abAppend(
                    &ab,
                    szstr(
                        "\x1b[0K")); // erase to end of line (needed for resize)
                move(&ab, visual_r, I->coloff + 1); // move to upcoming subline
                abAppend(&ab, szstr("\x1b[1K"));    // erase to start of line
            }

            /* CURSOR FINDING LOGIC */
            if (save_cursor.r == -1 && I->cursor.r == r) {
                if (c == I->cursor.c) {
                    save_cursor.r = visual_r;
                    save_cursor.c =
                        (visual_c - cwidth + 1) % maxc + I->coloff + 1;
                } else if (c == curr_row->len - 1) {
                    save_cursor.r = visual_r;
                    save_cursor.c = (visual_c + 1) % maxc + I->coloff + 1;
                }
            }

            /* WRITING CHARACTER */
            // TODO abstract character substitution
            if (*to_add == '\t') { // replace tabs with 4x spaces
                abAppend(&ab, "    ", 4);
            } else {
                abAppend(&ab, to_add, 1);
            }

            /* END SELECTION HIGHLIGHTING */
            if (!select) {
                abAppend(&ab, szstr("\x1b[49m")); // end sel
            }
        }
        // prepare to start a new row
        abAppend(&ab, szstr("\x1b[49m")); // end sel
        abAppend(&ab, szstr("\x1b[0K"));  // erase to end of line
        visual_r++;
    }

    abAppend(&ab, szstr("\x1b[49m")); // end selection, in case it was enabled

    // draw blank lines past the last row
    for (int r = visual_r; r < maxr; r++) {
        move(&ab, r, 0);
        abAppend(&ab, szstr("\x1b[0K")); // erase to end of line
        abAppend(&ab, szstr("~"));
    }
    // move the cursor to display position
    if (I->mode == COMMAND) {
        move(&ab, maxr, I->cmd.mcol + 1);
    } else {
        move(&ab, save_cursor.r, save_cursor.c);
    }
    abAppend(&ab, szstr("\x1b[?25h")); // show cursor

    write(STDIN_FILENO, ab.buf, ab.size);
    free(ab.buf);
}

void statusPrintMode(void) { // TODO rename this lol
    if (I->mode == COMMAND) {
        abAppend(&I->status, I->cmd.msg.text, I->cmd.msg.len);
        return;
    }
    switch (I->mode) { // MODE
    case VIEW:
        abAppend(&I->status, szstr("VIEW — "));
        break;
    case INSERT:
        abAppend(&I->status, szstr("INSERT — "));
        break;
    default:
        break;
    }
    // filename
    abAppend(&I->status, I->filename, strlen(I->filename));
    // number of lines, number of bytes
    char *buf;
    int len;
    asprintf(&buf, " %dL", I->E->numrows);
    len = strlen(buf);
    abAppend(&I->status, buf, len);
    abAppend(&I->status, szstr("\x1b[0K")); // erase to end of line
    free(buf);
    // cursor coordinates
	point at = { -314, -42 };
	int nr = -42;
	int type = -1;
	char* str = "_";
	if (I->cmdStack != NULL) {
		at = I->cmdStack->command->at;
		nr = I->cmdStack->command->rows[0]->len;
		type = I->cmdStack->command->type == INSERT ? 1 : 0;
		str = I->cmdStack->command->rows[0]->text;
	}
    asprintf(&buf, "%d, %d | (%d, %d), %d, type: %d, contents: %s", I->cursor.r + 1,
             I->cursor.c + 1, at.r, at.c, nr, type, str); // asprintf my beloved
    len = strlen(buf);
    move(&I->status, I->ws.ws_row, I->ws.ws_col - len);
    abAppend(&I->status, buf, len);
    free(buf);
}

void printEditorStatus(void) {
    struct abuf ab = {NULL, 0};
    abAppend(&ab, szstr("\x1b[?25l")); // hide cursor
    move(&ab, I->ws.ws_row, 0);
    abAppend(&ab, I->status.buf, I->status.size); // write the status
    abAppend(&ab, szstr("\x1b[0K"));              // erase to end of line
    write(STDIN_FILENO, ab.buf, ab.size);
    free(ab.buf);
}

void resize(int _ __attribute__((unused))) {
    ioctl(1, TIOCGWINSZ, &I->ws);
    point max = {I->ws.ws_row, I->ws.ws_col};
    point min = {0, 0};
    I->cursor = maxPoint(minPoint(I->cursor, max), min);
    I->status.size = 0;
    statusPrintMode();
    adjustToprow();
    printEditorContents();
    printEditorStatus();
}
