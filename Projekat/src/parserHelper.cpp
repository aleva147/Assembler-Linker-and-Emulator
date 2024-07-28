#include "../inc/parserHelper.hpp"
#include <stdlib.h>
#include <stdio.h>
// #include <string.h>


command *commandsHead = NULL, *commandsCur = commandsHead;


arg* createArg(char* reg, char* sym, int lit, int type)
{
	arg* a = (arg*)malloc(sizeof(arg));
	a->reg = reg;
  a->sym = sym;
  a->lit = lit;
	a->type  = type;
	a->next = NULL;
	return a;
}

command* createCommand(char *name, arg* args, bool isDirective, lab* labs)
{
	command* cmnd = (command*)malloc(sizeof(command));
	cmnd->name = name;
	cmnd->args = args;
	cmnd->next = NULL;
  cmnd->isDirective = isDirective;
  cmnd->labs = labs;

  if (!commandsHead) {
    commandsHead = cmnd;
    commandsCur = commandsHead;
  }
  else {
    commandsCur->next = cmnd;
    commandsCur = commandsCur->next;
  }

	return cmnd;
}


void printArgs(arg* arg){
  switch(arg->type) {
    case 0:
      printf("%s", arg->reg);
      break;
    case 1:
      printf("(%s)", arg->reg);
      break;
    case 2:
      printf("(%s+0x%x)", arg->reg, arg->lit);
      break;
    case 3:
      printf("(%s+%s)", arg->reg, arg->sym);
      break;
    case 4:
      printf("%d", arg->lit);
      break;
    case 5:
      printf("%s", arg->sym);
      break;
    case 6:
      printf("(%d)", arg->lit);
      break;
    case 7:
      printf("(%s)", arg->sym);
      break;
    default:
      printf("No matching case in the switch component in print_arg");
      break;
  }
}

void printCommands(command* cmndsHead) {
	command* cmnd = cmndsHead;
  while (cmnd) {
    // Print labels which are infront of this command:
    if (cmnd->labs) {
      printf("Labels: ");
      lab* curLab = cmnd->labs;
      while (curLab) {
        printf("%s", curLab->name);
        if (curLab->next) printf(", ");
        curLab = curLab->next;
      }
    }
    printf("  ");

    // Print command type:
    if (cmnd->isDirective) {
      printf("Directive: ");
    }
    else {
      printf("Instruction: ");
    }

    // Print command identifier:
    printf("%s ", cmnd->name);

    // Print command arguments:
    arg* curArg = cmnd->args;
    while(curArg) {
      printArgs(curArg);
      if (curArg->next) printf(", ");
      curArg = curArg->next;
    }
    printf("\n");

    // Iterate to the next command in line:
    cmnd = cmnd->next;
  }
}


void freeLabs(lab* labsHead) {
  while(labsHead) {
      lab* tmp = labsHead;
      labsHead = labsHead->next;
      free(tmp->name);
      free(tmp);
  }
}

void freeArgs(arg* args) {
  while (args) {
    arg* tmp = args;
    args = args->next;
    free(tmp->reg);
    free(tmp->sym);
    free(tmp);
  }
}

void freeCommands(command* cmndsHead) {
  while (cmndsHead) {
    command* tmp = cmndsHead;
    cmndsHead = cmndsHead->next;
    freeLabs(tmp->labs);
	  free(tmp->name); 
    freeArgs(tmp->args);
    free(tmp);
  }
}