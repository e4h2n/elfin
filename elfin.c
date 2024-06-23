#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <ncurses.h>
#include "editor.h"

#define DEFAULT_PAIR 1
#define LINENUMBER_PAIR 2
#define COMMENT_PAIR 3
#define PURP_PAIR 4
#define BLUE_PAIR 5
#define TAN_PAIR 6

#define UNUSED(x) (void)(x)

// TODO rewrite scrolling!

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
  PGD = 9999,         /* Page Down */
  PGU = 99999,        /* Page Up */
};

/* debug tool */
void printRows(editor *E){
  for(int i = 0; i < E->numrows; i++){
    printf("%2d: %s\n", i, E->rowarray[i]->text);
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

/* PRINTS the editor *
 * MOVES cursor to correct position based on E->mr, E->mc */
void displayEditor(editor *E){
  int maxr = getmaxy(stdscr)-1;
  int width = getmaxx(stdscr) - 5;

  erow* curr_row;
  int currln = 0; // VISUAL line

  // for cursor position
  int dr = -1; // -1 means not yet found
  int dc; // uninitialized so it will crash if something goes wrong :D
  
  // iterate over the rows, starting at toprow
  for (int curr_row_number = E->toprow; currln <= maxr; curr_row_number++){
    move(currln, 0);
    clrtoeol();

    if(curr_row_number < E->numrows){ // valid line
      curr_row = E->rowarray[curr_row_number];

      attron(COLOR_PAIR(LINENUMBER_PAIR));
      printw("%4d ", curr_row_number + 1);
      attroff(COLOR_PAIR(LINENUMBER_PAIR));
      
      int sublines = (curr_row->len)/width + !!((curr_row->len)%width); // ceil division
      for (int j = 0; j < sublines && currln <= maxr; j++){
        // cursor finding
        // default to last subrow, rightmost col
        if (dr < 0 && curr_row_number == E->mr && (j == E->mc/width || j == sublines-1)){
          dr = currln;
          dc = min(E->mc, curr_row->len-1);
          dc %= width;
        }

        if (j){ // this means we are in a subline, clear any line number previously printed
          move(currln, 0);
          clrtoeol();
        }
        // print subrow contents
        move(currln, 5);
        colorPrint(curr_row->text + j*width, width, DEFAULT_PAIR);
        currln++;
      }
    } else { // past the end of the file
      currln ++;
      colorPrint("~", 1, LINENUMBER_PAIR);
    }
  }
  move(dr, dc+5);
}

/* PRINT the status */
void displayStatus(editor *E){
  int maxr = getmaxy(stdscr) - 1;
  move(maxr,0);
  clrtoeol();
  attron(COLOR_PAIR(DEFAULT_PAIR));
  printw("%s", E->status);
  attroff(COLOR_PAIR(DEFAULT_PAIR));
}

int findBotRow(editor *E, int* extra){ // 0-indexed
  int width = getmaxx(stdscr) - 5;
  erow* row;
  int maxr = getmaxy(stdscr)-1; // VISUAL rows
  int visualr = 0; // COUNTING visual rows needed to display editor
  int i = E->toprow-1;
  while(visualr < maxr && i < E->numrows-1){
    i++;
    row = E->rowarray[i];
    visualr += row->len/width + !!(row->len%width);
  }
  if (extra){
    *extra = visualr - maxr; // number of sublines NOT displayed
  }
  return i;
}

void repositionView(editor *E){ // for scrolling, usually
  // not keeping track of sublines makes this *really* annoying
  int width = getmaxx(stdscr) - 5;
  erow* curr_row = E->rowarray[E->mr];

  int extra; // number of sublines NOT displayed
  int total = curr_row->len/width; // number of sublines in the current row
  int at = E->mc/width; // which subline we are at currently
  int botr = findBotRow(E, &extra);

  extra = max(extra+at - total, 0);

  if (E->mr > botr || (E->mr == botr && extra > 0)) {  
    E->toprow = max(E->toprow, E->toprow + (E->mr - botr) + extra);
  }
  else if (E->mr < E->toprow){
    E->toprow = E->mr;
  }

  E->toprow = min(E->toprow, E->numrows-1); // safety first!
}


/* VIEW MODE *
 * scrolling, cursor movement, yank/paste */
Mode View(editor *E, int input){
  if (E->mode != VIEW) return E->mode;
  
  if (input == ':'){ // ENTER COMMAND MODE
    return COMMAND;
  } else if (input == 'i'){ // ENTER INSERT MODE
    return INSERT;
  } else if ((input == DOWN || input == 'j')){ // arrow keys, move cursor
    E->toprow += (E->mr == E->numrows-1); // CAN EXCEED NUMLINES, caught in repositionView
    E->mr = min(E->numrows-1, E->mr+1);

  } else if ((input == UP || input == 'k')){
    E->mr = max(0, E->mr-1);
  } else if ((input == LEFT || input == 'h') && E->mc > 0){
    E->mc = min(E->rowarray[E->mr]->len - 1, E->mc - 1);
  } else if ((input == RIGHT || input == 'l') && E->mc < E->rowarray[E->mr]->len-1){
    E->mc ++;
  } else if (input == 'd' && getch() == 'd'){
    deleteRow(E, E->mr);
    E->mr = min(E->mr, E->numrows-1);
  }

  repositionView(E);
  return VIEW; // STAY IN VIEW MODE
}

/* INSERT MODE *
 * typing, cursor movement, deletion */
Mode Insert(editor *E, int input){
  if (E->mode != INSERT) return E->mode;
  int width = getmaxx(stdscr) - 5;
  erow* curr_row = E->rowarray[E->mr];

  if (input == ESC){
    return VIEW;

  } else if (input == DOWN){
    if (curr_row->len - E->mc <= curr_row->len%width){ // change line
      if (E->mr == E->numrows-1){ // at bottom
        E->toprow ++;
      } else{
        E->mr ++;
        E->mc %= width;
      }
    } else { // change subline
      E->mc = min(E->mc + width, curr_row->len);
    }
  } else if (input == UP){
    if (E->mr > 0 && E->mc < width){
      E->mr --; // change line
      erow* dest_row = E->rowarray[E->mr];
      E->mc = E->mc%width + width*(dest_row->len/width);
    } else{ // change subline
      E->mc = max(E->mc - width, 0);
    }
  } else if (input == LEFT){
    if (E->mc > 0){
      E->mc --;
    }
  } else if (input == RIGHT){
    if (E->mc < E->rowarray[E->mr]->len-1){
      E->mc ++;
    }

  } else if (input == '\n'){
    splitRow(E, E->mr, E->mc);
    E->mc = 0;
    E->mr ++;

  } else if (input == BACKSPACE || input == DEL){
    if (E->mc == 0 && E->mr > 0){
      E->mc = E->rowarray[E->mr-1]->len - 1;
      delCatRow(E, E->mr);
      E->mr --;
    }
    else if (deleteChar(curr_row, E->mc-1)){
      E->mc --;
    }

  } else if (input == TAB){
    insertChar(curr_row, E->mc, ' ');
    insertChar(curr_row, E->mc, ' ');
    E->mc += 2;

  } else {
    if (insertChar(curr_row, E->mc, input)){
      E->mc ++;
    }
  }
  repositionView(E);
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
    if (!strcmp(currrow->text, "c")){
      for (int i = E->numrows-1; i >= 0; i--){
        deleteRow(E, i);
        E->mr = 0;
        E->mc = 1;
        E->toprow = 0;
      }
    }
    else if (!strcmp(currrow->text, "q")){
      return QUIT;
    } else if (!strcmp(currrow->text, "w")){
      saveToFile(E);
    } else if (!strcmp(currrow->text, "wq")){
      saveToFile(E);
      return QUIT;
    }

    // clear out the command line
    free(currrow->text);
    currrow->text = strdup("");
    currrow->len = 0;
    C->mcol = 0;
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

  E->mode = VIEW;
  E->mr = 0;
  E->mc = 1;

  while(1){
    displayEditor(E);
    int dmr = getcury(stdscr);
    int dmc = getcurx(stdscr);

    if (E->mode == VIEW){
      sprintf(E->status, "\"%s\" | %d lines | %d, %d", E->filename, E->numrows, E->mr+1, E->mc); 
      //sprintf(E->status, "\"%s\" | %p cmd ptr | %d, %d", E->filename, (void*)E->command->cmd, E->mr+1, E->rowarray[E->mr]->len); 

      displayStatus(E);
      move(dmr, dmc);
      E->mode = View(E, getch());
    } else if (E->mode == INSERT){
      sprintf(E->status, " INSERT | %d lines | %d, %d", E->numrows, E->mr+1, E->mc);

      displayStatus(E);
      move(dmr, dmc);
      E->mc = min(E->mc, E->rowarray[E->mr]->len-1); // make sure we are in bounds
      E->mode = Insert(E, getch());
    } else if (E->mode == COMMAND){
      int maxr = getmaxy(stdscr) -1;
      move(maxr, 0);
      clrtoeol();
      printw(":%s", E->command->cmd->text);
      move(maxr, E->command->mcol+1);

      E->mode = Command(E, getch());
    } else if (E->mode == QUIT){
      // TODO delete/free the editor
      break;
    }
  }
  endwin();
  return 0;
}
