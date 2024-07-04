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
#define INV_PAIR 7

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
  // TODO
  PGD = 9999,         /* Page Down */
  PGU = 99999,        /* Page Up */
};

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
 * MOVES cursor to correct position based on E->mr, E->mc *
 * ASSUMES cursor is in view */
void displayEditor(editor *E){
  int maxr = getmaxy(stdscr)-1;
  int width = getmaxx(stdscr) - 5;

  erow* curr_row;
  int currln = 0; // VISUAL line

  // for cursor position
  int dr = -1; // -1 means not yet found
  int dc; // uninitialized so it will crash if something goes wrong :D

  bool selected = false;
  
  // iterate over the rows, starting at toprow
  for (int curr_row_number = E->toprow; currln <= maxr; curr_row_number++){
    move(currln, 0);
    clrtoeol();

    if(curr_row_number < E->numrows){ // valid line
      curr_row = E->rowarray[curr_row_number];

      attron(COLOR_PAIR(LINENUMBER_PAIR));
      printw("%4d ", curr_row_number + 1);
      attroff(COLOR_PAIR(LINENUMBER_PAIR));

      for(int col = 0; col < curr_row->len; col++){
        if (col/width && (col%width == 0)){ // new subrow
          currln++;
          move(currln, 0);
          clrtoeol();
        }        
        if (currln > maxr){
          break;
        }

        // cursor finding
        if(dr == -1 && curr_row_number == E->mr && (col == E->mc || col == curr_row->len-1)){
          dr = currln;
          dc = col%width + 5;
          selected = !selected;
        }
        if (curr_row_number == E->ar && col == E->ac){
          selected = !selected;
        }

        move(currln, col%width + 5);
        colorPrint(curr_row->text + col, 1, (selected && E->ar >= 0) ? INV_PAIR : DEFAULT_PAIR); // meh

      }
    } else { // past the end of the file
      colorPrint("~", 1, LINENUMBER_PAIR);
    }
    currln++;
  }
  move(dr, dc);
}

/* PRINT the status */
void displayStatus(editor *E){
  int maxr = getmaxy(stdscr) - 1;
  move(maxr,0);
  clrtoeol();
  colorPrint(E->status, 80, DEFAULT_PAIR);
}

/* returns the number of the lowest (greatest number) row in view *
 * sets extra to the number of sublines out of view */
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
    *extra = max(visualr - maxr, 0); // number of sublines NOT displayed
  }
  return i;
}

/* modifies topline to ensure the cursor is on the screen */
void repositionView(editor *E){ // for scrolling, usually
  // not keeping track of sublines makes this *really* annoying
  int width = getmaxx(stdscr) - 5;
  erow* curr_row = E->rowarray[E->mr];
  erow* top_row = E->rowarray[E->toprow];

  int extra; // number of sublines NOT displayed
  int total = curr_row->len/width; // number of sublines in the current row
  int at = min(E->mc, curr_row->len)/width; // which subline we are at currently
  int botr = findBotRow(E, &extra);

  while(E->mr > botr) { // cursor past bottom row
    // could be more efficient, but be careful with sublines!
    E->toprow++;
    botr = findBotRow(E, &extra);
  }

  extra = at - (total - extra); // how many sublines we need to reposition by
  while(E->mr == botr && extra > 0) { // cursor on bottom row, on a subline out of view
    E->toprow ++;
    extra -= (top_row->len/width + !!(top_row->len%width));

    top_row = E->rowarray[E->toprow];
    botr = findBotRow(E, NULL);
  }

  if (E->mr < E->toprow){
    E->toprow = E->mr;
  }

  E->toprow = min(E->toprow, E->numrows-1); // safety first!
}

int charsBetween(editor* E, int r1, int c1, int r2, int c2){
  int out = 0;
  for (int i = min(r1, r2); i < max(r1, r2); i++){
    out += E->rowarray[i]->len;
  }
  if (r1 < r2){
    out -= c1;
    out += c2;
  } else if (r2 < r1){
    out -= c2;
    out += c1;
  } else{
    out -= min(c1, c2);
    out += max(c1, c2);
  }
  return out;
}

/* COMMAND MODE *
 * save, quit, search, clear */
Mode Command(editor* E, int input){
  commandrow* C = E->command;
  erow* curr_row = C->cmd;

  if (input == LEFT){
    if (C->mcol > 0){
      C->mcol --;
    }
  } else if (input == RIGHT){
    if (C->mcol < C->cmd->len-1){
      C->mcol ++;
    }

  } else if (input == '\n'){
    if (!strcmp(curr_row->text, "c")){
      for (int i = E->numrows-1; i >= 0; i--){
        deleteRow(E, i);
        E->mr = 0;
        E->mc = 1;
        E->toprow = 0;
      }
    }
    else if (!strcmp(curr_row->text, "q")){
      return QUIT;
    } else if (!strcmp(curr_row->text, "w")){
      saveToFile(E);
    } else if (!strcmp(curr_row->text, "wq")){
      saveToFile(E);
      return QUIT;
    } else if (!strncmp(curr_row->text, "f ", 2) && curr_row->len > 2){
      findNext(E, &E->mr, &E->mc, curr_row->text + 2);
      return VIEW;
    }
    return VIEW;

  } else if (input == BACKSPACE || input == DEL){
    if (C->mcol <= 0){
      return VIEW;
    }
    else if (deleteChar(curr_row, C->mcol-1)){
      C->mcol --;
    }

  } else if (input == TAB){
    insertChar(curr_row, C->mcol, ' ');
    insertChar(curr_row, C->mcol, ' ');
    C->mcol += 2;

  } else if (input == ESC){
    return VIEW;

  } else {
    if (insertChar(curr_row, C->mcol, input)){
      C->mcol ++;
    }
  }
  return COMMAND;
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
  return INSERT;
}

void delSelected(editor* E){
  if (E->ar == -1) return; // nothing selected

  int bigr, bigc;
  bigr = max(E->ar, E->mr);
  bigc = E->ar > E->mr ? E->ac : E->mc;
  if(E->ar == E->mr) bigc = max(E->ac, E->mc);

  int counter = charsBetween(E, E->mr, E->mc, E->ar, E->ac);

  E->mr = bigr;
  E->mc = min(bigc, E->rowarray[E->mr]->len);

  E->mode = INSERT;

  for(; counter > 0; counter--){
    Insert(E, BACKSPACE);
  }

  E->ar = -1;

  E->mode = VIEW;
}

/* VIEW MODE *
 * scrolling, cursor movement, yank/paste */
Mode View(editor *E, int input){
  if (E->mode != VIEW) return E->mode;
  
  if (input == ':'){ // ENTER COMMAND MODE    
    // clear out the command line first
    // TODO make a function for clearing the cmd line
    commandrow* C = E->command;
    erow* curr_row = C->cmd;
    if (curr_row->len != 0){
      free(curr_row->text);
      curr_row->text = strdup("");
      curr_row->len = 0;
      C->mcol = 0;
    }
    return COMMAND;
  } else if (input == 'i'){ // ENTER INSERT MODE
    return INSERT;
  } else if (input == 'v'){
    if (E->ar != -1) { E->ar = -1; }
    else {
    E->ar = E->mr;
    E->ac = min(E->mc, E->rowarray[E->mr]->len-1);
    }
  } else if ((input == DOWN || input == 'j')){ // arrow keys, move cursor
    E->toprow += (E->mr == E->numrows-1); // CAN EXCEED NUMLINES, caught in repositionView
    E->mr = min(E->numrows-1, E->mr+1);

  } else if ((input == UP || input == 'k')){
    E->mr = max(0, E->mr-1);
  } else if ((input == LEFT || input == 'h') && E->mc > 0){
    E->mc = min(E->rowarray[E->mr]->len - 1, E->mc - 1);
  } else if ((input == RIGHT || input == 'l') && E->mc < E->rowarray[E->mr]->len-1){
    E->mc ++;
  } else if (input == 'd'){
    if (E->ar != -1){
      delSelected(E);
    } else if (getch() == 'd'){
      deleteRow(E, E->mr);
      E->mr = min(E->mr, E->numrows-1);
    }
  }  else if (input == '.'){
    return Command(E, '\n');
  }

  return VIEW; // STAY IN VIEW MODE
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
  init_pair(LINENUMBER_PAIR, COLOR_YELLOW, -1);
  init_pair(INV_PAIR, COLOR_BLACK, COLOR_WHITE);

  editor *E = editorFromFile(argv[1]);

  E->mode = VIEW;
  E->mr = 0;
  E->mc = 1;

  while(1){
    if (E->mode != VIEW){
      E->ar = -1;
    }
    repositionView(E);
    displayEditor(E);
    int dmr = getcury(stdscr); // yeah this is a little awkward
    int dmc = getcurx(stdscr);

    if (E->mode == VIEW){
      //sprintf(E->status, "%s", E->command->cmd->text);
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
      colorPrint(":", 1, DEFAULT_PAIR);
      colorPrint(E->command->cmd->text, E->command->cmd->len, DEFAULT_PAIR);
      move(maxr, E->command->mcol+1);

      E->mode = Command(E, getch());
    } else if (E->mode == QUIT){
      // TODO delete/free the editor
      deleteEditor(&E);
      break;
    }
  }
  endwin();
  return 0;
}
