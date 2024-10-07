#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <assert.h>
#include "display.h"

#define szstr(str) str, sizeof(str)
#define TAB_WIDTH 4
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

void writeCharToBuffer(struct abuf* ab, char* to_add, int visual_c) {
	if (*to_add == '\t') { // tab handling
		int num_spaces = TAB_WIDTH - (visual_c % TAB_WIDTH);
		char* tab_string;
		asprintf(&tab_string, "%*s", num_spaces, ""); // left pads the empty string
		abAppend(ab, tab_string, num_spaces);
		free(tab_string);
	} else {
		abAppend(ab, to_add, 1);
	}
}

void setDefaultFG(struct abuf* ab) {
	abAppend(ab, szstr("\x1b[38;2;" FG "m"));
}

void setDefaultBG(struct abuf* ab) {
	abAppend(ab, szstr("\x1b[48;2;" BG "m"));
}

/* ======= DISPLAY ======= */
void clearScreen(void) {
	write(STDIN_FILENO, szstr("\x1b[2J"));
}

void printEditorContents(void) {
    int maxr = I->ws.ws_row;                 // height
    int maxc = I->ws.ws_col - I->coloff - 1; // width

    struct editor *E = I->E;
    int visual_r = 1;

    point save_cursor;
    save_cursor.r = -1; // default not found

    struct abuf ab = {NULL, 0};        // buffer for the WHOLE screen
	if (I->mode == VIEW || I->mode == COMMAND) { // cursor type
		abAppend(&ab, szstr("\x1b[2 q"));
	} else if (I->mode == INSERT) {
		abAppend(&ab, szstr("\x1b[5 q"));
	}
    abAppend(&ab, szstr("\x1b[?25l")); // hide cursor

    point startSel;
    point endSel;
    bool select = false;
    if (I->anchor.r != -1) { // selection mode
        startSel = minPoint(I->cursor, I->anchor);
        startSel.c = min(E->rowarray[startSel.r]->len - 1, startSel.c);
        endSel = maxPoint(I->cursor, I->anchor);
        endSel.c = min(E->rowarray[endSel.r]->len - 1, endSel.c);
        select = startSel.r < I->toprow;
    }

    // ITER OVER THE ROWS
    for (int r = I->toprow; visual_r < maxr && r < E->numrows; r++) {
        struct erow *curr_row = E->rowarray[r];
        int visual_c = 0; // same as displayed col (starting at coloff)

        /* LINENUM DISPLAY */
        move(&ab, visual_r, 0);
        char linenum[I->coloff];
        sprintf(linenum, "%*d ", I->coloff - 1, r + 1);
		if (I->cursor.r == r) { // set linenum fg color
			abAppend(&ab, szstr("\x1b[1m")); // bold
			abAppend(&ab, szstr("\x1b[38;2;" CURSORLINE_FG "m"));
		} else {
        	abAppend(&ab, szstr("\x1b[38;2;" LINENUM_FG "m"));
		}
        abAppend(&ab, linenum, I->coloff);
        abAppend(&ab, szstr("\x1b[m")); // reset all formatting
		setDefaultFG(&ab);
		setDefaultBG(&ab);

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
				setDefaultBG(&ab);
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

            char to_add = curr_row->text[c];
            int cwidth = 1;
            if (to_add == '\t') { // tabs are special
                cwidth = TAB_WIDTH - (visual_c % TAB_WIDTH);
            }

            /* SUBLINE HANDLING */
            if (visual_c + cwidth >= maxc) { // new subline?
                abAppend(&ab, szstr("\x1b[m"));       // reset all formatting
                if (++visual_r >= maxr) break;
                visual_c = 0;
				// erase to EOL
                abAppend(&ab, szstr("\x1b[0K"));
                move(&ab, visual_r, I->coloff + 1); // move to upcoming subline
                abAppend(&ab, szstr("\x1b[1K"));    // erase to start of line
                if (select) {
					// start sel
                    abAppend(&ab, szstr("\x1b[48;2;" SELECT_BG "m"));
                }
            }

			visual_c += cwidth;

            /* CURSOR FINDING LOGIC */
            if (save_cursor.r == -1 && I->cursor.r == r) {
                if (c == I->cursor.c) {
                    save_cursor.r = visual_r;
                    save_cursor.c = visual_c - 1 + I->coloff + 1;
                } else if (c == curr_row->len - 1) {
                    save_cursor.r = visual_r;
                    save_cursor.c = visual_c + I->coloff + 1;
                }
            }	

            /* WRITING CHARACTER */
			writeCharToBuffer(&ab, &to_add, visual_c - cwidth);

            /* END SELECTION HIGHLIGHTING */
            if (!select) {
				setDefaultBG(&ab);
            }
        }
        // prepare to start a new row
		setDefaultBG(&ab);
        abAppend(&ab, szstr("\x1b[0K"));  // erase to end of line
        visual_r++;
    }

    // clear all displayed rows past the end of the file
    for (int r = visual_r; r < maxr; r++) {
        move(&ab, r, 0);
        abAppend(&ab, szstr("\x1b[0K")); // erase to end of line
    }

    abAppend(&ab, szstr("\x1b[48;2;" BG "m")); // end selection, in case it was enabled

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
	abAppend(&I->status, szstr("\x1b[38;2;" STATUSLINE_A_BG "m"));
	abAppend(&I->status, szstr(""));
	abAppend(&I->status, szstr("\x1b[48;2;" STATUSLINE_A_BG "m"));
	abAppend(&I->status, szstr("\x1b[38;2;" STATUSLINE_A_FG "m"));
	abAppend(&I->status, szstr("\x1b[1m"));
    switch (I->mode) { // MODE
    case VIEW:
        abAppend(&I->status, szstr("VIEW"));
        break;
    case INSERT:
        abAppend(&I->status, szstr("INSERT"));
        break;
    default:
        break;
    }
	abAppend(&I->status, szstr("\x1b[48;2;" STATUSLINE_BG "m"));
	abAppend(&I->status, szstr("\x1b[38;2;" STATUSLINE_A_BG "m"));
	abAppend(&I->status, szstr(" "));
	abAppend(&I->status, szstr("\x1b[0m"));
	abAppend(&I->status, szstr("\x1b[48;2;" STATUSLINE_BG "m"));
	abAppend(&I->status, szstr("\x1b[38;2;" STATUSLINE_FG "m"));
    // filename
    abAppend(&I->status, I->filename, strlen(I->filename));
    // number of lines, number of bytes
    char *buf;
    int len;
    asprintf(&buf, " %dL", I->E->numrows);
    len = strlen(buf);
    abAppend(&I->status, buf, len);
	abAppend(&I->status, szstr("\x1b[48;2;" BG" m"));
	abAppend(&I->status, szstr("\x1b[38;2;" STATUSLINE_BG "m"));
	abAppend(&I->status, szstr(" "));
    abAppend(&I->status, szstr("\x1b[0K")); // erase to end of line
    free(buf);
    // cursor coordinates
    asprintf(&buf, "%d:%d", I->cursor.r + 1, I->cursor.c + 1);
    len = strlen(buf) + 2;
    move(&I->status, I->ws.ws_row, I->ws.ws_col - len);
	abAppend(&I->status, szstr("\x1b[1m"));
	abAppend(&I->status, szstr("\x1b[38;2;" STATUSLINE_A_BG "m"));
	abAppend(&I->status, szstr(""));
	abAppend(&I->status, szstr("\x1b[48;2;" STATUSLINE_A_BG "m"));
	abAppend(&I->status, szstr("\x1b[38;2;" STATUSLINE_A_FG "m"));
    abAppend(&I->status, buf, len);
	abAppend(&I->status, szstr("\x1b[48;2;" BG "m"));
	abAppend(&I->status, szstr("\x1b[38;2;" STATUSLINE_A_BG "m"));
	abAppend(&I->status, szstr(""));
	abAppend(&I->status, szstr("\x1b[m")); // reset all formatting
    free(buf);
}

void printEditorStatus(void) {
    struct abuf ab = {NULL, 0};
    abAppend(&ab, szstr("\x1b[?25l")); // hide cursor
    move(&ab, I->ws.ws_row, 0);
    abAppend(&ab, I->status.buf, I->status.size); // write the status
    abAppend(&ab, szstr("\x1b[0K"));              // erase to end of line
    abAppend(&ab, szstr("\x1b[m"));               // reset all formatting
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
    printEditorStatus();
    printEditorContents();
}
