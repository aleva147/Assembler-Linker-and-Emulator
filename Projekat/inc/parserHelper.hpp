#ifndef _parser_helper_h_
#define _parser_helper_h_


/*
  type: 0 - %reg
        1 - [%reg]
        2 - [%reg + literal]
        3 - [%reg + symbol]
        4 - $literal
        5 - $symbol
        6 - literal
        7 - symbol
*/
struct arg {
	char* reg;
  char* sym;
	int lit;
  int type;
	arg* next;
};

struct lab {
  char* name;
  lab* next;
};

struct command {
  bool isDirective;
  lab* labs;
	char* name;
	arg* args;
	command* next;
};


extern command* commandsHead;


arg* createArg(char*, char*, int, int);
command* createCommand(char*, arg*, bool = false, lab* = nullptr);

// For debugging:
void printArgs(arg*);
void printCommands(command*);

void freeLabs(const lab*);
void freeArgs(const arg*);
void freeCommands(command*);


#endif