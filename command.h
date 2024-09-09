#pragma once

#include "editor.h"

typedef enum cmdType { ADD, DELETE, NEWROW, DELROW } cmdType;

struct command {
	point at;
	struct erow **rows;
	int numrows;
	cmdType type;
};

struct commandStack {
	struct command *command;
	struct commandStack *next;
};

void doCommand(struct editor *E, struct command *cmd);
void undoCommand(struct editor *E, struct command *cmd);

void freeCommand(struct command *cmd);

struct commandStack *remove_node(struct commandStack *node);
struct commandStack *push(struct command *pushed, struct commandStack *stack);
