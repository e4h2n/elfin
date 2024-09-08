#include "command.h"
#include "editor.h"

#include <stdlib.h>

void doCommand(struct editor *E, struct command *cmd) {
	if (cmd->type == ADD) {
		insertRange(E, cmd->at, cmd->rows, cmd->numrows);
	} else if (cmd->type == DELETE) {
		int last_c = cmd->rows[cmd->numrows - 1]->len - 1;
		if (cmd->numrows == 1) {
			last_c += cmd->at.c;
		}
		point end = { cmd->numrows - 1 + cmd->at.r, last_c };
		deleteRange(E, cmd->at, end);
	}
}

void undoCommand(struct editor *E, struct command *cmd) {
	struct command inv_cmd = { cmd->at, cmd->rows, cmd->numrows, cmd->type };
	if (inv_cmd.type == ADD) {
		inv_cmd.type = DELETE;
	} else if (inv_cmd.type == DELETE) {
		inv_cmd.type = ADD;
	}
	doCommand(E, &inv_cmd);
}

void freeCommand(struct command *cmd) {
	freeRowarr(cmd->rows, cmd->numrows);
	free(cmd);
}

// removes(frees) the input commandStack (and associated command)
// returns the next node
struct commandStack *remove_node(struct commandStack *node) {
	if (node == NULL) return node;
	struct commandStack *ret = node->next;
	freeCommand(node->command);
	free(node);
	return ret;
}

struct commandStack *push(struct command *pushed, struct commandStack *stack) {
	// TODO merge commands of the same type, if continguous
	struct commandStack *pushedNode = malloc(sizeof(struct commandStack));
	pushedNode->command = pushed;
	pushedNode->next = stack;
	return pushedNode;
}
