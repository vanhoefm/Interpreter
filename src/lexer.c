/* 
lexer.c: main method used by the lexer

Student Project Compilers, 2009-2010

Dries Van Dyck
Department WNI
Theoretical Computer Science Group
Hasselt University 

januari 2010


History:
- 29/01/2010  Initial draft
- 25/02/2010  adapted code to use "y.tab.h" instead of "tokens.h"
Bugs:

*/

#include "lexer.h"
#include "y.tab.h"

/* Array with tokens such that index = tokenid - 257*/
char *tokens[] = 
  {"NEWLINE", "LPAREN", "RPAREN", "LBRACE", "RBRACE", /*"LBRACK", "RBRACK", */
   "SEMICOLON", "NAME", "NUMBER", "AND", "OR", "NOT", "EQ", "LE", "GE", 
   "NE", "LT", "GT", "PLUS", "MINUS", "MUL", "DIV", "MOD", "POW", 
   "ASSIGN", "PLUSASSIGN", "MINUSASSIGN", "MULASSIGN", "DIVASSIGN", 
   "MODASSIGN", "POWASSIGN", "INCR", "DECR", "IF", "ELSE", "WHILE", "BREAK", 
   "CONTINUE", "DEFINE", "COMMA", "AUTO", "RETURN", "HALT"}; 

/* main function reading from stdin */
int main()
{
  int tokenid;
  
  /* If we de not explicitly bind yyin to a file, stdin is assumed. */
  while (tokenid=yylex())
    {
      /* Token codes start from 257 (default chosen by Yacc) */ 
      printf(" %s", tokens[tokenid-257]);

      /* In case of a NAME or FLOAT append its value*/
      if ( (tokenid == NAME) || (tokenid == NUMBER) ) 
	printf("=\"%s\"", yytext);
    }
    printf("\n");
  return 0;
}
