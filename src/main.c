/* 
main.c: main program for bc calling yacc

Dries Van Dyck
Department WNI
Theoretical Computer Science Group
Hasselt University 

januari 2010

History:
- 11/01/2010  Initial draft
Bugs:

*/

#include "lexer.h"
#include "bc.h"
#include <stdlib.h>

/* The main program for bc. */
int main()
{
	//yydebug = 1;

	/* Do the parse. */
	yyparse();

	exit(0);
}
