#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <ncurses.h>
#include <regex.h>
#include "editor.h"

#define DEFAULT_PAIR 1
#define LINENUMBER_PAIR 2
#define COMMENT_PAIR 3
#define PURP_PAIR 4
#define BLUE_PAIR 5
#define TAN_PAIR 6

#define UNUSED(x) (void)(x)

enum KEY_PRESS{
  KEY_NULL = 0,       /* NULL */
  TAB = 9,            /* Tab */
  ESC = 27,           /* Escape */
  BACKSPACE =  127,   /* Backspace */
  DEL = 8,            /* Delete */
  DOWN = 258,         /* Down Arrow */ 
  UP = 259,           /* Up Arrow */
  LEFT = 260,         /* Left Arrow */
  RIGHT = 261,        /* Right Arrow */
};

/* debug tool */
void printRows(editor *E){
  for(int i = 0; i < E->numrows; i++){
    printf("%2d: %s\n", i, E->rowarray[i].text);
  }
  printf("\n");
}

void colorPrint(char* s, int len, int COLOR){
  attron(COLOR_PAIR(COLOR));
  printw("%.*s", len, s);
  attroff(COLOR_PAIR(COLOR));
}

int min(int a, int b){
  if (a > b) {
    return b;
  }
  else {
    return a;
  }
}

int max(int a, int b){
  if (a > b) {
    return a;
  }
  else {
    return b;
  }
}

/* PRINT the editor */
void displayEditor(editor *E){
  int maxr = getmaxy(stdscr);
  erow* currrow;

  for (int i = 0; i < maxr - 1; i++){
    move(i, 0);
    clrtoeol();

    if(i+E->toprow < E->numrows){
      currrow = E->rowarray+i+E->toprow;

      attron(COLOR_PAIR(LINENUMBER_PAIR));
      printw("%4d ", i+E->toprow+1);
      attroff(COLOR_PAIR(LINENUMBER_PAIR));

      colorPrint(currrow->text, currrow->len, DEFAULT_PAIR);
    } else {
      colorPrint("~", 1, DEFAULT_PAIR);
    }
  }
}

/* PRINT the status */
void displayStatus(editor *E){
  int maxr = getmaxy(stdscr);
  move(maxr-1,0);
  clrtoeol();
  attron(COLOR_PAIR(DEFAULT_PAIR));
  printw("%s", E->status);
  attroff(COLOR_PAIR(DEFAULT_PAIR));
}

/* VIEW MODE *
 * scrolling, cursor movement, yank/paste */
Mode View(editor *E, int input){
  if (E->mode != VIEW) return E->mode;
  int s = 0; // 0(default): page scroll; 1: cursor scroll
             //strcpy(E->status, E->filename);
  if (input == ':'){ // ENTER COMMAND MODE
    return COMMAND;
  } else if (input == 'i'){ // ENTER INSERT MODE
    return INSERT;
  } else if(input == 'k' && E->toprow > 0){ // scroll page up
    E->toprow --;
  } else if (input == 'j' && E->toprow < E->numrows-1){ // scroll page down
    E->toprow ++;
  } else if (input == DOWN && E->mr < E->numrows-1){ // arrow keys, move cursor
    E->mr ++;
    s = 1;
  } else if (input == UP && E->mr > 0){
    E->mr --;
    s = 1;
  } else if ((input == LEFT || input == 'h') && E->mc > 0){
    E->mc --;
  } else if ((input == RIGHT || input == 'l') && E->mc < E->rowarray[E->mr].len-1){
    E->mc ++;
  }

  // SCROLL LOGIC
  int maxr = getmaxy(stdscr);
  if (E->mr < E->toprow){
    if (s){ // cursor moved, scroll the page
      E->toprow = E->mr;
    } else { // page moved, move the cursor
      E->mr = E->toprow;
    }
  }
  else if (E->mr > E->toprow+maxr-2){
    if (s){ // cursor moved, scroll the page
      E->toprow = E->mr - maxr + 2;
    } else { // page moved, move the cursor
      E->mr = E->toprow + maxr - 2;
    }
  }
  return VIEW; // STAY IN VIEW MODE
}

/* INSERT MODE *
 * typing, cursor movement, deletion */
Mode Insert(editor *E, int input){
  if (E->mode != INSERT) return E->mode;
  erow *currrow = E->rowarray+ E->mr;

  if (input == ESC){
    return VIEW;

  } else if (input == DOWN){
    if (E->mr < E->numrows-1){
      E->mr ++;
    }

  } else if (input == UP){
    if (E->mr > 0){
      E->mr --;
    }
  } else if (input == LEFT){
    if (E->mc > 0){
      E->mc --;
    }
  } else if (input == RIGHT){
    if (E->mc < E->rowarray[E->mr].len-1){
      E->mc ++;
    }

  } else if (input == '\n'){
    splitRow(E, E->mr, E->mc);
    E->mc = 0;
    E->mr ++;

  } else if (input == BACKSPACE || input == DEL){
    if (E->mc == 0 && E->mr > 0){
      E->mc = E->rowarray[E->mr-1].len - 1;
      delCatRow(E, E->mr);
      E->mr --;
    }
    else if (deleteChar(currrow, E->mc-1)){
      E->mc --;
    }

  } else if (input == TAB){
    insertChar(currrow, E->mc, ' ');
    insertChar(currrow, E->mc, ' ');
    E->mc += 2;

  } else {
    if (insertChar(currrow, E->mc, input)){
      E->mc ++;
    }
  }
  // SCROLL LOGIC (cursor only)
  int maxr = getmaxy(stdscr);
  if (E->mr < E->toprow){
    E->toprow = E->mr;
  }
  else if (E->mr > E->toprow+maxr-2){
    E->toprow = E->mr - maxr + 2;
  }

  return INSERT;
}

/* COMMAND MODE *
 * save, quit, search(eventually :P) */
Mode Command(editor* E, int input){
  if (E->mode != COMMAND) return E->mode;

  commandrow* C = E->command;
  erow* currrow = C->cmd;

  if (input == LEFT){
    if (C->mcol > 0){
      C->mcol --;
    }
  } else if (input == RIGHT){
    if (C->mcol < C->cmd->len-1){
      C->mcol ++;
    }

  } else if (input == '\n'){
    // TODO
    if (!strcmp(currrow->text, "q")){
      return QUIT;
    } else if (!strcmp(currrow->text, "w")){
      saveToFile(E);
    } else if (!strcmp(currrow->text, "wq")){
      saveToFile(E);
      return QUIT;
    }
    //C->mcol = 0;
    return VIEW;

  } else if (input == BACKSPACE || input == DEL){
    if (C->mcol > 0 && deleteChar(currrow, C->mcol-1)){
      C->mcol --;
    }

  } else if (input == TAB){
    insertChar(currrow, C->mcol, ' ');
    insertChar(currrow, C->mcol, ' ');
    C->mcol += 2;

  } else {
    if (insertChar(currrow, C->mcol, input)){
      C->mcol ++;
    }
  }
  return COMMAND;
}

int main(int argc, char *argv[]){
  if (argc != 2){
    printf("USAGE: elfin <filename>\n");
    return 0;
  }

  initscr();
  keypad(stdscr, TRUE);
  cbreak();
  noecho();
  start_color();
  use_default_colors();
  
  init_pair(DEFAULT_PAIR, COLOR_WHITE, -1);
  init_pair(LINENUMBER_PAIR, COLOR_RED, -1);

  editor *E = editorFromFile(argv[1]);
  //strcpy(E->status, E->filename);
  //editor *E = calloc(sizeof(editor), 1);
  E->mode = VIEW;

  //newRow(E, 7);
  //newRow(E, 4);
  //displayEditor(E);
  //getch();

  while(1){
    displayEditor(E);
    //int maxc = getmaxx(stdscr);
    if (E->mode == VIEW){
      sprintf(E->status, "\"%s\" | %d lines | %d, %d", E->filename, E->numrows, E->mr+1, E->mc); 
      displayStatus(E);

      int dmr = max(0, min(E->mr, E->numrows-1)) - E->toprow;

      int dmc = E->mc + 5;
      int rowlen = E->rowarray[dmr + E->toprow].len-1 + 5;
      dmc = dmc < rowlen ? dmc : rowlen; // TODO CLEAN THIS UP

      move(dmr, dmc);
      E->mode = View(E, getch()); // consider moving the getch()
    } else if (E->mode == INSERT){
      sprintf(E->status, " INSERT | %d lines | %d, %d", E->numrows, E->mr+1, E->mc); 
      displayStatus(E);

      int dmr = max(0, min(E->mr, E->numrows-1)) - E->toprow;

      E->mc = max(0, min(E->rowarray[E->mr].len-1, E->mc));
      int dmc = 5+E->mc;

      move(dmr, dmc);
      E->mode = Insert(E, getch());
    } else if (E->mode == COMMAND){
      int maxr = getmaxy(stdscr) -1;
      move(maxr, 0);
      clrtoeol();
      printw(":%s", E->command->cmd->text);
      move(maxr, E->command->mcol+1);

      E->mode = Command(E, getch());
    } else if (E->mode == QUIT){
      break;
    }
  }
  endwin();
  return 0;
}
