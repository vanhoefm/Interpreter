%{
/*
bc.y: yacc-file for bc

Dries Van Dyck
Department WNI
Theoretical Computer Science Group
Hasselt University

januari 2010

History:
- 31/01/10 Initial draft
- 20/03/10 Grammar for bc is finished
- 21/03/10 Added code to construct abstract syntax treeReducing stack by rule 18 (line 194):
- 10/04/10 Abstract syntax tree has been updated: Now internally uses std::list instead of Pair/First-classes
- 30/04/10 Mathy Vanhoef: Finished interpret function
- 01/04/10 Mathy Vanhoef: Added Memory Stack

Bugs:
*/

#include <assert.h>
#include <string.h>
#include "lexer.h"
#include "absyn.h"

/* line and col numbers from the lexer */
extern int line_nr;
extern int col_nr;

/**
 * Routine called by yyparse whenever it detects an error in its input
 */
void yyerror(const char* s);
char * strclone(const char *string);

/**
 * Keeps track of the semantic context. This is used so certain statements like break, continue and return are
 * correctly used.
 *
 * Duplicate arguments and/or auto variables will be printed multiple times.
 */
class SemanticContext
{
public:
	SemanticContext() { reset(); }
	~SemanticContext() { reset(); }
	void reset();
	void enterFunction() { m_inFunction = true; }
	void setFunctionArguments(const list<char*> *funcArgs);
	void exitFunction() { m_inFunction = false; m_funcArgs = NULL; }
	void enterWhile() { m_while++; }
	void exitWhile() { m_while--; }

	void checkBreak(int line_nr, int col_nr);
	void checkContinue(int line_nr, int col_nr);
	void checkReturn(int line_nr, int col_nr);
	void checkFunctionVariable(const list<char*> *variables, const char *nextVar, int line_nr, int col_nr);

	bool detectedError() const { return m_error; }
	

private:
	static bool isStringInList(const list<char*> *stringlist, const char *string);
	bool m_inFunction;
	/** Function arguments of the current function we are in */
	const list<char*> *m_funcArgs;
	int m_while;
	bool m_error;
	bool isInWhile() const { return m_while > 0; }
	bool isInFunction() const { return m_inFunction; }
	void printError(const char *error, int line_nr, int col_nr) const;
};

static SemanticContext semContext;
static RuntimeContext runContext;
/** Memory manager - is also accessed by our lexer */
MemoryStack memStack;

%}

%union {
	double number;
	char *string;

	AbsStatement *statement;
	AbsExpression *expression;
	FunctionDefinition *funcDefCmd;
	StatementList *statementList;
	list<char*> *variableList;
	list<AbsExpression*> *expressionList;
}

%type <statement> statement;
%type <statementList> statementline;
%type <statementList> statementmultiline;
%type <expression> expression;
%type <funcDefCmd> funcdef;
%type <variableList> optvariablelist;
%type <variableList> variablelist;
%type <variableList> optautolist;
%type <expressionList> optexpressionlist;
%type <expressionList> expressionlist;
%type <number> NUMBER;
%type <string> NAME;

%token NEWLINE LPAREN RPAREN LBRACE RBRACE SEMICOLON NAME NUMBER AND OR NOT EQ LE GE NE
%token LT GT PLUS MINUS MUL DIV MOD POW ASSIGN PLUSASSIGN MINUSASSIGN MULASSIGN DIVASSIGN
%token MODASSIGN POWASSIGN INCR DECR IF ELSE WHILE BREAK CONTINUE DEFINE COMMA AUTO RETURN HALT

%left OR
%left AND
%left NOT
%left LT LE GT GE EQ NE
%right ASSIGN PLUSASSIGN MINUSASSIGN MULASSIGN DIVASSIGN MODASSIGN POWASSIGN
%left PLUS MINUS
%left MUL DIV MOD
%left POW
%left UMINUS
%left INCR DECR

%%

%start prog ;

prog : /* empty */
	| prog command		{ semContext.reset(); }
	;

command : funcdef NEWLINE
		{
			if(!semContext.detectedError())
			{
				runContext.addFunctionDefinition($1);
			}
			else
			{
				delete $1;
			}

			memStack.pop();
		}
	| statementline NEWLINE
		{
			if(!semContext.detectedError())
			{
				try
				{
					$1->interpret(&runContext);
				}
				catch(const RuntimeException &runtimeEx)
				{
					cerr << endl << runtimeEx.getMessage() << endl;
				}
				catch(const HaltFlowException &haltFlow)
				{
					// Clean up
					delete $1;
					memStack.pop();

					// Then immediate return from yyparse, returning 0 to report success
					YYACCEPT;
				}
			}

			delete $1;
			memStack.pop();
		}
	| error NEWLINE
		{
			memStack.freeAll();
			yyerrok;
		}
	;

optnewline : /* empty */
	| NEWLINE
	;

scheidingsteken : NEWLINE
	| SEMICOLON
	;

statementline : /* empty */				{ $$ = new StatementList(); memStack.push($$); }
	| statement						{ $$ = new StatementList($1); memStack.popAndPush(1, $$); }
	| statementline SEMICOLON			{ $$ = $1; }
	| statementline SEMICOLON statement	{ $$ = $1; $$->addStatement($3); memStack.pop(); }
	;

statementmultiline : /* empty */					{ $$ = new StatementList(); memStack.push($$); }
	| statement								{ $$ = new StatementList($1); memStack.popAndPush(1, $$); }
	| statementmultiline scheidingsteken			{ $$ = $1; }
	| statementmultiline scheidingsteken statement	{ $$ = $1; $$->addStatement($3); memStack.pop(); }
	;

statement : expression		{ $$ = new ExpressionStatement($1); memStack.popAndPush(1, $$); }
	| LBRACE optnewline statementmultiline RBRACE			{ $$ = $3; }
	| IF LPAREN expression RPAREN optnewline statement		{ $$ = new IfStatement($3, $6); memStack.popAndPush(2, $$);  }
	| IF LPAREN expression RPAREN optnewline statement ELSE optnewline statement	{ $$ = new IfStatement($3, $6, $9); memStack.popAndPush(3, $$);  }
	| WHILE
			{ semContext.enterWhile(); }
		LPAREN expression RPAREN optnewline statement
			{ semContext.exitWhile(); $$ = new WhileStatement($4, $7); memStack.popAndPush(2, $$);  }
	| HALT				{ $$ = new HaltStatement(); memStack.push($$); }
	| BREAK				{ semContext.checkBreak(@1.first_line, @1.first_column); $$ = new BreakStatement(); memStack.push($$); }
	| CONTINUE			{ semContext.checkContinue(@1.first_line, @1.first_column); $$ = new ContinueStatement(); memStack.push($$); }
	| RETURN				{ semContext.checkReturn(@1.first_line, @1.first_column); $$ = new ReturnStatement(); memStack.push($$); }
	| RETURN expression		{ semContext.checkReturn(@1.first_line, @1.first_column); $$ = new ReturnStatement($2); memStack.popAndPush(1, $$); }
	;

expression : NUMBER			{ $$ = new ConstantExpression($1); memStack.push($$); }
	| NAME				{ $$ = new VariableExpression($1); memStack.useString($1); memStack.push($$); }
	| INCR NAME			{ $$ = new PrefixOpExpression($2, PrefixOpExpression::OP_INCR); memStack.useString($2); memStack.push($$); }
	| DECR NAME			{ $$ = new PrefixOpExpression($2, PrefixOpExpression::OP_DECR); memStack.useString($2); memStack.push($$); }
	| NAME INCR			{ $$ = new PostfixOpExpression($1, PostfixOpExpression::OP_INCR); memStack.useString($1); memStack.push($$); }
	| NAME DECR			{ $$ = new PostfixOpExpression($1, PostfixOpExpression::OP_DECR); memStack.useString($1); memStack.push($$); }
	| expression PLUS expression		{ $$ = new ArithmeticExpression($1, $3, ArithmeticExpression::OP_PLUS); memStack.popAndPush(2, $$); }
	| expression MINUS expression		{ $$ = new ArithmeticExpression($1, $3, ArithmeticExpression::OP_MINUS); memStack.popAndPush(2, $$); }
	| expression MUL expression		{ $$ = new ArithmeticExpression($1, $3, ArithmeticExpression::OP_MUL); memStack.popAndPush(2, $$); }
	| expression DIV expression		{ $$ = new ArithmeticExpression($1, $3, ArithmeticExpression::OP_DIV); memStack.popAndPush(2, $$); }
	| expression MOD expression		{ $$ = new ArithmeticExpression($1, $3, ArithmeticExpression::OP_MOD); memStack.popAndPush(2, $$); }
	| expression POW expression		{ $$ = new ArithmeticExpression($1, $3, ArithmeticExpression::OP_POW); memStack.popAndPush(2, $$); }
	| expression LT expression		{ $$ = new BooleanExpression($1, $3, BooleanExpression::OP_LT); memStack.popAndPush(2, $$); }
	| expression LE expression		{ $$ = new BooleanExpression($1, $3, BooleanExpression::OP_LE); memStack.popAndPush(2, $$); }
	| expression GT expression		{ $$ = new BooleanExpression($1, $3, BooleanExpression::OP_GT); memStack.popAndPush(2, $$); }
	| expression GE expression		{ $$ = new BooleanExpression($1, $3, BooleanExpression::OP_GE); memStack.popAndPush(2, $$); }
	| expression EQ expression		{ $$ = new BooleanExpression($1, $3, BooleanExpression::OP_EQ); memStack.popAndPush(2, $$); }
	| expression NE expression		{ $$ = new BooleanExpression($1, $3, BooleanExpression::OP_NE); memStack.popAndPush(2, $$); }
	| expression AND expression		{ $$ = new BooleanExpression($1, $3, BooleanExpression::OP_AND); memStack.popAndPush(2, $$); }
	| expression OR expression		{ $$ = new BooleanExpression($1, $3, BooleanExpression::OP_OR); memStack.popAndPush(2, $$); }
	| NOT expression				{ $$ = new NegationExpression($2); memStack.popAndPush(1, $$);  }
	| LPAREN expression RPAREN		{ $$ = $2; }
	| NAME ASSIGN expression			{ $$ = new AssignExpression($1, $3); memStack.useString($1); memStack.popAndPush(1, $$); }
	| NAME PLUSASSIGN expression
		{
			$$ = new AssignExpression($1, new ArithmeticExpression(new VariableExpression(strclone($1)), $3, ArithmeticExpression::OP_PLUS));
			memStack.useString($1);
			memStack.popAndPush(1, $$);
		}
	| NAME MINUSASSIGN expression
		{
			$$ = new AssignExpression($1, new ArithmeticExpression(new VariableExpression(strclone($1)), $3, ArithmeticExpression::OP_MINUS));
			memStack.useString($1);
			memStack.popAndPush(1, $$);
		}
	| NAME MULASSIGN expression
		{
			$$ = new AssignExpression($1, new ArithmeticExpression(new VariableExpression(strclone($1)), $3, ArithmeticExpression::OP_MUL));
			memStack.useString($1);
			memStack.popAndPush(1, $$);
		}
	| NAME DIVASSIGN expression
		{
			$$ = new AssignExpression($1, new ArithmeticExpression(new VariableExpression(strclone($1)), $3, ArithmeticExpression::OP_DIV));
			memStack.useString($1);
			memStack.popAndPush(1, $$);
		}
	| NAME MODASSIGN expression
		{
			$$ = new AssignExpression($1, new ArithmeticExpression(new VariableExpression(strclone($1)), $3, ArithmeticExpression::OP_MOD));
			memStack.useString($1);
			memStack.popAndPush(1, $$);
		}
	| NAME POWASSIGN expression
		{
			$$ = new AssignExpression($1, new ArithmeticExpression(new VariableExpression(strclone($1)), $3, ArithmeticExpression::OP_POW));
			memStack.useString($1);
			memStack.popAndPush(1, $$);
		}
	| NAME LPAREN optexpressionlist RPAREN { $$ = new FunctionCallExpression($1, $3); memStack.useString($1); memStack.popAndPush(1, $$); }
	| MINUS expression %prec UMINUS	{ $$ = new MinusExpression($2); }
	;

funcdef : DEFINE
			{ semContext.enterFunction(); }
		NAME	LPAREN optvariablelist RPAREN
			{ semContext.setFunctionArguments($5); }
		optnewline LBRACE optnewline optautolist statementmultiline
			{ semContext.exitFunction(); }
		RBRACE
			{ $$ = new FunctionDefinition($3, $5, $11, $12); memStack.useString($3); memStack.popAndPush(3, $$); }
	;

optautolist : /* empty */				{ $$ = new list<char*>(); memStack.push($$); }
	| AUTO variablelist scheidingsteken	{ $$ = $2; }
	;

optvariablelist : /* empty */				{ $$ = new list<char*>(); memStack.push($$); }
	| variablelist						{ $$ = $1; }
	;

variablelist : NAME
		{
			$$ = new list<char*>();
			memStack.push($$);

			semContext.checkFunctionVariable($$, $1, @1.first_line, @1.first_column);

			$$->push_back($1);
			memStack.useString($1);
		}
	| variablelist COMMA NAME
		{
			$$ = $1;
			
			semContext.checkFunctionVariable($1, $3, @3.first_line, @3.first_column);

			$$->push_back($3);
			memStack.useString($3);
		}
	;

optexpressionlist : /* empty */			{ $$ = new list<AbsExpression*>(); memStack.push($$); }
	| expressionlist					{ $$ = $1; }
	;

expressionlist : expression				{ $$ = new list<AbsExpression*>(); $$->push_back($1); memStack.popAndPush(1, $$); }
	| expressionlist COMMA expression		{ $$ = $1; $$->push_back($3); memStack.pop(); }
	;

%%


void yyerror(const char* s)
{
	int col_nr_error = col_nr - yyleng;
	fprintf(stderr, "\n%s \"%s\" at line %d, column %d\n", s, yytext, line_nr, col_nr_error);
}

char * strclone(const char *string)
{
	char *copy = new char[strlen(string) + 1];
	return strcpy(copy, string);
}



bool SemanticContext::isStringInList(const list<char*> *stringlist, const char *string)
{
	for(list<char*>::const_iterator it = stringlist->begin(); it != stringlist->end(); ++it)
	{
		if(strcmp(*it, string) == 0)
			return true;
	}

	return false;
}

void SemanticContext::reset()
{
	m_inFunction = false;
	m_funcArgs = NULL;
	m_while = 0;
	m_error =  false;
}

void SemanticContext::setFunctionArguments(const list<char*> *funcArgs)
{
	assert(isInFunction());

	m_funcArgs = funcArgs;
}

void SemanticContext::checkBreak(int line_nr, int col_nr)
{
	if(!isInWhile())
	{
		m_error = true;
		printError("break outside while", line_nr, col_nr);
	}
}

void SemanticContext::checkContinue(int line_nr, int col_nr)
{
	if(!isInWhile())
	{
		m_error = true;
		printError("continue outside while", line_nr, col_nr);
	}
}

void SemanticContext::checkReturn(int line_nr, int col_nr)
{
	if(!isInFunction())
	{
		m_error = true;
		printError("return outside function definition", line_nr, col_nr);
	}
}

void SemanticContext::checkFunctionVariable(const list<char*> *currVariableList, const char *nextVar, int line_nr, int col_nr)
{
	assert(isInFunction());

	// Always loop through currVariableList
	if(isStringInList(currVariableList, nextVar))
	{
		m_error = true;
		printError("duplicate name in parameter or auto variable list", line_nr, col_nr);
	}
	else if(m_funcArgs)
	{
		// m_funcArgs contains the arguments of the functions (so currVariableList contains the auto variables)
		if(isStringInList(m_funcArgs, nextVar))
		{
			m_error = true;
			printError("duplicate name in parameter or auto variable list", line_nr, col_nr);
		}
	}
}

void SemanticContext::printError(const char *error, int line_nr, int col_nr) const
{
	cerr << "\nsemantic error: " << error << " at line " << line_nr << ", column " << col_nr << ".\n";
}

