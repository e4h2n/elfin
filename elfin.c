#include "display.h"
#include "editor.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define szstr(str) str, sizeof(str)
#define UNUSED(x) (void)(x)

enum KEY_ACTION {
    KEY_NULL = 0,
    TAB = 9,
    ENTER = 13,
    ESC = 27,
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

struct editorInterface *I;          // THE editor used across all files
static struct termios orig_termios; // restore at exit

/* ======= terminal setup ======= */

int countDigits(int n) {
    // technically this says 0 has 0 digits
    // but that's not a case we worry about
    // we will always have at least one line
    int out = 0;
    while (n != 0) {
        n /= 10;
        out++;
    }
    return out;
}

void init_I(char* filename) {
	I = malloc(sizeof(struct editorInterface));
    I->filename = strdup(filename);
    I->E = editorFromFile(I->filename);
    I->toprow = 0;
    I->mode = VIEW;
    I->coloff = max(4, countDigits(I->E->numrows) + 2);
    I->cursor.r = 0;
    I->cursor.c = 0;
    I->anchor.r = -1;
    I->cmd.msg.text = strdup("");
    I->cmdStack = NULL;
    resize(0);
}

void destroy_I(void) {
    destroyEditor(&I->E);
    free(I->cmd.msg.text);
    free(I->status.buf);
	free(I->filename);
    while (I->cmdStack != NULL) {
        I->cmdStack = remove_node(I->cmdStack);
    }
    free(I);
}

void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int readKey(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("readKey()");
    }
    if (c == ESC) {
        char seq[3];
        if (!read(STDIN_FILENO, &seq[0], 1) || !read(STDIN_FILENO, &seq[1], 1))
            return c;

        if (seq[0] == '[') {
            switch (seq[1]) {
            case 'A':
                return ARROW_UP;
            case 'B':
                return ARROW_DOWN;
            case 'C':
                return ARROW_RIGHT;
            case 'D':
                return ARROW_LEFT;
            }
        }
    }
    return c;
}

void cleanup(void) {
	destroy_I();
    disableRawMode();
}

/* ======= IO utils ======= */
void View(int c);
void Insert(int c);
void Command(int c);
void doUserCommand(struct erow cmd);

void View(int c) {
    struct erow *curr_row = I->E->rowarray[I->cursor.r];
    switch (c) {
    case ESC:
        I->anchor.r = -1;
        break;
    case '.':
        doUserCommand(I->cmd.msg);
        break;
    case '/':
    case ':':
        I->mode = COMMAND;
        I->cmd.msg.len = 0;
        I->cmd.mcol = 0;
        I->cmd.msg.text[0] = '\0';
        Command(c);
        break;
    case 'i':
        I->mode = INSERT;
        break;
    case 'I':
        I->cursor.c = 0; // TODO make this ignore leading whitespace
        I->mode = INSERT;
        break;
    case 'a':
        I->cursor.c = I->cursor.c + (I->cursor.c < curr_row->len ? 1 : 0);
        I->mode = INSERT;
        break;
    case 'A':
        I->cursor.c = curr_row->len;
        I->mode = INSERT;
        break;
    case 'o':
        I->cursor.c = curr_row->len;
        Insert(ENTER);
        I->mode = INSERT;
        break;
    case 'O':
        I->cursor.c = 0;
        Insert(ENTER);
        View('k');
        I->mode = INSERT;
        break;
    case ARROW_DOWN:
    case 'j':
        I->cursor.r = min(I->E->numrows - 1, I->cursor.r + 1);
        break;
    case ARROW_UP:
    case 'k':
        I->cursor.r = max(0, I->cursor.r - 1);
        break;
    case ARROW_LEFT:
    case 'h':
        I->cursor.c = max(0, min(I->cursor.c - 1, curr_row->len - 1));
        break;
    case ARROW_RIGHT:
    case 'l':
        I->cursor.c = I->cursor.c + (I->cursor.c < curr_row->len ? 1 : 0);
        break;
    case '0':
        I->cursor.c = 0;
        break;
    case '$':
        I->cursor.c = curr_row->len;
        break;
    case 'v':
        if (I->anchor.r == -1) {
            I->anchor = I->cursor;
        } else {
            I->anchor.r = -1;
        }
        break;
    case 'y': {
        point start = {I->cursor.r, 0};
        point end = {I->cursor.r, max(0, curr_row->len - 1)};
        if (I->anchor.r != -1) {
            start = minPoint(I->cursor, I->anchor);
            end = maxPoint(I->cursor, I->anchor);
            start.c = min(start.c, I->E->rowarray[start.r]->len - 1);
            end.c = min(end.c, I->E->rowarray[end.r]->len - 1);
        }
        copyToClipboard(I->E, start, end);
    } break;
    case 'p':
        if (I->E->clipboard_len > 0) {
            struct command *cmd = malloc(sizeof(struct command));
            cmd->at = I->cursor;
            cmd->at.c = min(cmd->at.c, max(0, I->E->rowarray[cmd->at.r]->len));
            point cb_start = {0, 0};
            point cb_end = {I->E->clipboard_len - 1,
                            I->E->clipboard[I->E->clipboard_len - 1]->len - 1};
            cmd->rows = copyRange(I->E->clipboard, cb_start, cb_end);
            cmd->numrows = cb_end.r + 1;
            cmd->type = ADD;

            I->cmdStack = push(cmd, I->cmdStack);
            doCommand(I->E, cmd);
        }
        break;
    case 'd':
        if (I->anchor.r != -1) {
            point start = minPoint(I->cursor, I->anchor);
            point end = maxPoint(I->cursor, I->anchor);
            start.c = min(start.c, I->E->rowarray[start.r]->len - 1);
            start.c = max(0, start.c);
            end.c = min(end.c, I->E->rowarray[end.r]->len - 1);
            end.c = max(0, end.c);

            struct command *cmd = malloc(sizeof(struct command));
            cmd->at = start;
            cmd->rows = copyRange(I->E->rowarray, start, end);
            cmd->numrows = end.r - start.r + 1;
            cmd->type = DELETE;

            I->cmdStack = push(cmd, I->cmdStack);
            doCommand(I->E, cmd);

            I->cursor = minPoint(I->anchor, I->cursor);
            I->anchor.r = -1;
        }
        break;
    case 'G':
        I->cursor.r = I->E->numrows - 1;
        I->cursor.c = max(0, I->E->rowarray[I->cursor.r]->len - 1);
        break;
    case 'g':
        if (readKey() == 'g') {
            I->cursor.r = 0;
            I->cursor.c = 0;
        }
        break;
    case 'u':
        if (I->cmdStack != NULL) {
            struct command *cmd = I->cmdStack->command;
            undoCommand(I->E, cmd);

            I->cursor = cmd->at;
            if (cmd->type == DELROW) {
                I->cursor.c = 0;
            }

            I->cmdStack = remove_node(I->cmdStack);
        }
        break;
    }
}

void Insert(int c) {
    struct erow *curr_row = I->E->rowarray[I->cursor.r];
    I->anchor.r = -1;
    switch (c) {
    case ESC:
        I->mode = VIEW;
        break;
    case ARROW_DOWN:
        I->cursor.r = min(I->E->numrows - 1, I->cursor.r + 1);
        break;
    case ARROW_UP:
        I->cursor.r = max(0, I->cursor.r - 1);
        break;
    case ARROW_LEFT:
        I->cursor.c = max(0, min(I->cursor.c - 1, curr_row->len - 1));
        break;
    case ARROW_RIGHT:
        I->cursor.c = I->cursor.c + (I->cursor.c < curr_row->len ? 1 : 0);
        break;
    case ENTER:
        I->cursor.c = min(I->cursor.c, curr_row->len);
        {
            struct command *cmd = malloc(sizeof(struct command));
            cmd->type = NEWROW;
            cmd->at = I->cursor;

            I->cmdStack = push(cmd, I->cmdStack);
            doCommand(I->E, cmd);
        }

        I->cursor.c = 0;
        I->cursor.r++;
        break;
    case BACKSPACE:
        I->cursor.c = min(I->cursor.c, curr_row->len);
        {
            struct command *cmd = malloc(sizeof(struct command));
            cmd->at = I->cursor;
            if (I->cursor.c == 0 && I->cursor.r > 0) {
                cmd->type = DELROW;
                cmd->at.c = curr_row->len;
                I->cursor.r--;
                I->cursor.c = I->E->rowarray[I->cursor.r]->len;
            } else if (I->cursor.c > 0) {
                cmd->type = DELETE;
                cmd->at.c--;
                cmd->rows = malloc(sizeof(struct erow *));
                cmd->rows[0] = malloc(sizeof(struct erow));
                cmd->rows[0]->text = malloc(sizeof(char));
                cmd->rows[0]->text[0] =
                    I->E->rowarray[cmd->at.r]->text[cmd->at.c];
                cmd->rows[0]->len = 1;
                cmd->numrows = 1;
                I->cursor.c--;
            } else
                break;
            I->cmdStack = push(cmd, I->cmdStack);
            doCommand(I->E, cmd);
        }
        break;

    default: // add character
        I->cursor.c = min(I->cursor.c, curr_row->len);
        {
            struct command *cmd = malloc(sizeof(struct command));
            cmd->at = I->cursor;
            cmd->type = ADD;
            cmd->rows = malloc(sizeof(struct erow *));
            cmd->rows[0] = malloc(sizeof(struct erow));
            cmd->rows[0]->text = malloc(sizeof(char));
            cmd->rows[0]->len = 1;
            cmd->rows[0]->text[0] = c;
            cmd->numrows = 1;

            I->cursor.c++;
            I->cmdStack = push(cmd, I->cmdStack);
            doCommand(I->E, cmd);
        }
    }
}

/* ======= USER COMMANDS ======= */
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

void doUserCommand(struct erow cmd) {
    if (cmd.len <= 1)
        return;

    if (cmd.text[0] == '/') {
        I->cursor = search(I->cursor, cmd.text + 1);
        I->anchor = I->cursor;
        I->anchor.c += cmd.len - 2;
	} else if (!strncmp(cmd.text, ":e ", 3)) {
		if (cmd.len > 3) {
			char* text = strndup(cmd.text+3, cmd.len - 3);
			destroy_I();
			init_I(text);
			free(text);
		}
    } else if (!strncmp(cmd.text, ":w", cmd.len)) {
        editorSaveFile(I->E, I->filename);
    } else if (!strncmp(cmd.text, ":q", cmd.len)) {
        I->mode = QUIT;
    } else if (!strncmp(cmd.text, ":wq", cmd.len)) {
        editorSaveFile(I->E, I->filename);
        I->mode = QUIT;
    }
}

void Command(int c) {
    switch (c) {
    case ESC:
        I->mode = VIEW;
        break;
    case BACKSPACE:
        if (I->cmd.mcol > 1) {
            deleteChar(&I->cmd.msg, I->cmd.mcol - 1);
            I->cmd.mcol--;
        } else {
            I->mode = VIEW;
        }
        break;
    case ARROW_LEFT:
        if (I->cmd.mcol > 1) {
            I->cmd.mcol--;
        }
        break;
    case ARROW_RIGHT:
        if (I->cmd.mcol < I->cmd.msg.len) {
            I->cmd.mcol++;
        }
        break;
    case ENTER:
        I->mode = VIEW;
        doUserCommand(I->cmd.msg);
        break;

    default:
        insertChar(&I->cmd.msg, I->cmd.mcol, c);
        I->cmd.mcol++;
    }
}

void editorProcessKey(int c) {
    if (I->mode == VIEW) {
        View(c);
    } else if (I->mode == INSERT) {
        Insert(c);
    } else if (I->mode == COMMAND) {
        Command(c);
    }
}

/* ======= main ======= */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("USAGE: elfin <filename>\n");
        return 0;
    }
	
    /* terminal setup */
	write(STDIN_FILENO, szstr("\x1b[?1049h")); // start new buffer
    enableRawMode();

    /* signal stuff */
    signal(SIGWINCH, resize);

	/* editor init */
	init_I(argv[1]);

    /* main IO loop */
    while (I->mode != QUIT) {
        I->status.size = 0; // this "clears" the status
        I->coloff = max(4, countDigits(I->E->numrows) + 2);
        statusPrintMode();
        adjustToprow();
        printEditorStatus();
        printEditorContents();
        editorProcessKey(readKey());
    }

	write(STDIN_FILENO, szstr("\x1b[?1049l")); // restore old buffer
	cleanup();
    return 0;
}
