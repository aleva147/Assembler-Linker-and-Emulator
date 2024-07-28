%{
  #include "../inc/parserHelper.hpp"
  #include <iostream>
  #include <cstring>
  using namespace std;

  extern "C" int yylex();
  extern int yyparse();
  extern FILE *yyin;
  extern int line_num;

  arg* listOfArgsHead = NULL, *listOfArgsCur = listOfArgsHead;
  lab* listOfLabsHead = NULL, *listOfLabsCur = listOfLabsHead;
 
  void yyerror(const char* s);
%}

%union {
  int number;
  char* identifier;
  struct arg* arg;
}

%token DOT COLON COMMA
%token DOLLAR PERCENT OPEN CLOSE PLUS 
%token ENDL END_ASM

%token <number> NUMBER
%token <identifier> IDENTIFIER

%type <arg> arg;
%type <identifier> reg;
%type <number> literal;
%type <identifier> symbol;


%%


input:
  lines;

lines:
  lines line
  | ENDLS line
  | line;

line:
  label directiveOrInstruction ENDLS
  | label ENDLS
  | directiveOrInstruction ENDLS
  | END_ASM {
    //cout << "Parser reached '.end'. Ignoring the rest the file." << endl;
    YYACCEPT;
  };

label:
  IDENTIFIER COLON {
    //cout << "Parser found label: " << $1 << endl;

    lab* l = (lab*)malloc(sizeof(lab));
    l->name = $1;
    l->next = NULL;

    if (!listOfLabsHead) {
      listOfLabsHead = l;
      listOfLabsCur = listOfLabsHead;
    }
    else {
      listOfLabsCur->next = l;
      listOfLabsCur = listOfLabsCur->next;
    }
  };

directiveOrInstruction:
  directive
  | instruction;

directive:
  DOT IDENTIFIER listOfIdentifiers {
    //cout << "Parser found directive: " << $2 << endl;
    
    if (listOfLabsHead) {
      createCommand($2, listOfArgsHead, true, listOfLabsHead);
      listOfLabsHead = NULL;
      listOfLabsCur = listOfLabsHead;
    }
    else {
      createCommand($2, listOfArgsHead, true);
    }

    listOfArgsHead = NULL;
    listOfArgsCur = listOfArgsHead;
  };

listOfIdentifiers:
  listOfIdentifiers COMMA IDENTIFIER {
    if (!listOfArgsHead) {
        listOfArgsHead = createArg(NULL, $3, 0, 5);
        listOfArgsCur = listOfArgsHead;
      }
      else {
        listOfArgsCur->next = createArg(NULL, $3, 0, 5);
        listOfArgsCur = listOfArgsCur->next;
      }
  }
  | listOfIdentifiers COMMA NUMBER { 
      if (!listOfArgsHead) {
        listOfArgsHead = createArg(NULL, NULL, $3, 4);
        listOfArgsCur = listOfArgsHead;
      }
      else {
        listOfArgsCur->next = createArg(NULL, NULL, $3, 4);
        listOfArgsCur = listOfArgsCur->next;
      }
    }
  | IDENTIFIER { 
      if (!listOfArgsHead) {
        listOfArgsHead = createArg(NULL, $1, 0, 5);
        listOfArgsCur = listOfArgsHead;
      }
      else {
        listOfArgsCur->next = createArg(NULL, $1, 0, 5);
        listOfArgsCur = listOfArgsCur->next;
      }
    }
  | NUMBER { 
      if (!listOfArgsHead) {
        listOfArgsHead = createArg(NULL, NULL, $1, 4);
        listOfArgsCur = listOfArgsHead;
      }
      else {
        listOfArgsCur->next = createArg(NULL, NULL, $1, 4);
        listOfArgsCur = listOfArgsCur->next;
      }
    }
  | ;

instruction:
  IDENTIFIER {
    //cout << "Parser found instruction: " << $1 << endl;
    createCommand($1, NULL, false, listOfLabsHead);
    listOfLabsHead = NULL; listOfLabsCur = listOfLabsHead;
  }
  | IDENTIFIER arg {
    //cout << "Parser found instruction with one arg: " << $1 << endl;
    createCommand($1, $2, false, listOfLabsHead);
    listOfLabsHead = NULL; listOfLabsCur = listOfLabsHead;
  }
  | IDENTIFIER arg COMMA arg {
    //cout << "Parser found instruction with two args: " << $1 << endl;
    struct arg *first_arg = $2;
	  first_arg->next = $4;
	  createCommand($1, $2, false, listOfLabsHead);
    listOfLabsHead = NULL; listOfLabsCur = listOfLabsHead;
  }
  | IDENTIFIER arg COMMA arg COMMA arg {
    //cout << "Parser found instruction with three args: " << $1 << endl;
    struct arg *first_arg = $2;
	  first_arg->next = $4;
    first_arg->next->next = $6;
	  createCommand($1, $2, false, listOfLabsHead);
    listOfLabsHead = NULL; listOfLabsCur = listOfLabsHead;
  };

arg:
  PERCENT reg 
  { $$ = createArg($2, NULL, 0, 0); }
  | OPEN PERCENT reg CLOSE
  { $$ = createArg($3, NULL, 0, 1); }
  | OPEN PERCENT reg PLUS literal CLOSE
  { $$ = createArg($3, NULL, $5, 2); }
  | OPEN PERCENT reg PLUS symbol CLOSE
  { $$ = createArg($3, $5, 0, 3); }
  | DOLLAR literal
  { $$ = createArg(NULL, NULL, $2, 4); }
  | DOLLAR symbol
  { $$ = createArg(NULL, $2, 0, 5); }
  | literal
  { $$ = createArg(NULL, NULL, $1, 6); }
  | symbol
  { $$ = createArg(NULL, $1, 0, 7); }

reg:
  IDENTIFIER;

literal:
  NUMBER;

symbol:
  IDENTIFIER;

ENDLS:
  ENDLS ENDL
  | ENDL ;


%%


void yyerror(const char *s) {
  cout << "Parser ERROR on line " << line_num << "!  Message: " << s << endl;
  exit(-1);
}