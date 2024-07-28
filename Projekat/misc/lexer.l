%{
  #include "../inc/parser.tab.h"
  extern "C" int yylex();
  int line_num = 1;
%}

%option outfile="lexer.c" header-file="lexer.h"

%%
"#"[^\n\r]*               { /*printf("Lexer found comment: %s\n", yytext);*/ }
0[xX][0-9a-fA-F]+         {
                            sscanf(yytext, "%x", &yylval.number);
			                      return NUMBER;
                          }
[0-9]+                    {
                            sscanf(yytext, "%d", &yylval.number);
			                      return NUMBER;
                          }
[_a-zA-Z][_a-zA-Z0-9]*    { 
                            yylval.identifier = strdup(yytext);
                            return IDENTIFIER; 
                          }
"\.end"                   { return END_ASM; }
[ \t]                     { }
\n                        { line_num++; return ENDL; }
"."                       { return DOT; }
":"                       { return COLON; }
","                       { return COMMA; }
"$"                       { return DOLLAR; }
"%"                       { return PERCENT; }
"["                       { return OPEN; }
"]"                       { return CLOSE; }
"+"                       { return PLUS; }
.                         { printf("Lexer ignored character: %s\n", yytext); }

%%