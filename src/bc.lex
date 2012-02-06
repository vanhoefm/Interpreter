%{
/* 
bc.lex: lex-file for bc

Dries Van Dyck
Department WNI
Theoretical Computer Science Group
Hasselt University 

januari 2010

History:
- 15/01/10 Initial draft
- 26/02/10 Mathy Vanhoef: Added regular expressions and line/column counting
- 04/03/10 Mathy Vanhoef: Adjusted for use with bison
- 21/03/10 Mathy Vanhoef: Strings and numbers are now saved in yylval
- 28/03/10 Mathy Vanhoef: Now uses yylloc so yacc knows the line and column number of each token
- 01/04/10 Mathy Vanhoef: Allocated string for NAME is now registered in the Memory Stack

Bugs:

*/

#include "absyn.h"
#include "y.tab.h"

/**
 * Memory manager (defined in bc.y):
 *	The lexer will allocate a char array for the token NAME. It is allocated using 'new char[]' and thus must be freed using 'delete[]'.
 *	This allocation is registered to the Memory Stack using addString(pointer). Once the string is being used by yacc (and the Memory
 *	Stack is no longer responsible to free the memory) you have to call useString(pointer). See the function 'copystring()'.
 */
extern MemoryStack memStack;

/**
 * Keep track of current position of lex for error messages, i.e. 
 * the position just *after* the last token read.
 */
int line_nr = 1;
int col_nr = 1;

/**
 * Update position when there are no newlines
 */
void adjust()
{
	yylloc.first_column = col_nr;
	yylloc.first_line = line_nr;
	col_nr += yyleng;
}

/**
 * Update position when there could be a random number of newlines
 */
void count()
{
	int i;

	// Also update yylloc so position of NEWLINE is correct
	yylloc.first_column = col_nr;
	yylloc.first_line = line_nr;

	for(i = 0; yytext[i] != '\0'; i++)
	{
		if(yytext[i] == '\n')
		{
			line_nr++;
			col_nr = 1;
		}
		else
			col_nr++;
	}
}

void printerr()
{
	fprintf(stderr, "\n");

	if (yytext[0] < ' ')
	{
		/* non-printable char */
		fprintf(stderr, "illegal character: ^%c", yytext[0] + '@'); 
	}
	else
	{
		if (yytext[0] > '~')
		{
			/* non-printable char printed as octal int padded with zeros, eg \012*/
			fprintf(stderr, "illegal character: \\%03o", (int) yytext[0]);
		}
		else
		{
			fprintf(stderr, "illegal character: %s", yytext);
		}
	}

	/* lex read exactly one char; the illegal one */
	fprintf(stderr, " at line %d column %d\n", line_nr, (col_nr-1));
}

void copystring()
{
	yylval.string = new char[yyleng + 1];
	strcpy(yylval.string, yytext);
	memStack.addString(yylval.string);
}

void copynumber()
{
	yylval.number = atof(yytext);
}

%}
name          [a-z]([a-z]|[0-9]|_)*
number        (([0-9]+)|([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+))
whitespace    (" "|\t)+
commentline   "#".*
commentblock  "/*"([^*]|("*"+([^*/])))*"*"+"/"
%%
"\n"       { count(); return NEWLINE; }
"("        { adjust(); return LPAREN; }
")"        { adjust(); return RPAREN; }
"{"        { adjust(); return LBRACE; }
"}"        { adjust(); return RBRACE; }
";"        { adjust(); return SEMICOLON; }
"&&"       { adjust(); return AND; }
"||"       { adjust(); return OR; }
"!"        { adjust(); return NOT; }
"=="       { adjust(); return EQ; }
"<="       { adjust(); return LE; }
">="       { adjust(); return GE; }
"!="       { adjust(); return NE; }
"<"        { adjust(); return LT; }
">"        { adjust(); return GT; }
"+"        { adjust(); return PLUS; }
"-"        { adjust(); return MINUS; }
"*"        { adjust(); return MUL; }
"/"        { adjust(); return DIV; }
"%"        { adjust(); return MOD; }
"^"        { adjust(); return POW; }
"="        { adjust(); return ASSIGN; }
"+="       { adjust(); return PLUSASSIGN; }
"-="       { adjust(); return MINUSASSIGN; }
"*="       { adjust(); return MULASSIGN; }
"/="       { adjust(); return DIVASSIGN; }
"%="       { adjust(); return MODASSIGN; }
"^="       { adjust(); return POWASSIGN; }
"++"       { adjust(); return INCR; }
"--"       { adjust(); return DECR; }
if         { adjust(); return IF; }
else       { adjust(); return ELSE; }
while      { adjust(); return WHILE; }
break      { adjust(); return BREAK; }
continue   { adjust(); return CONTINUE; }
define     { adjust(); return DEFINE; }
,          { adjust(); return COMMA; }
auto       { adjust(); return AUTO; }
return     { adjust(); return RETURN; }
halt       { adjust(); return HALT; }
{name}     { adjust(); copystring(); return NAME; }
{number}   { adjust(); copynumber(); return NUMBER; }
{whitespace}	 { adjust(); }
{commentline}   { adjust(); }
{commentblock}  { count(); }
.               { adjust(); printerr(); }
%%

/* Function called by (f)lex when EOF is read. If yywrap returns a
   true (non-zero) (f)lex will terminate and continue otherwise.*/
int yywrap()
{
	return 1;
}
