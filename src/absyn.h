/*
absyn.h: Class definitions for the abstract syntax tree

An abstract syntax tree comes in two variants: 
1) Code for execution -> deleted after execution
2) Function definitions -> stay alive until function is redefined

Dries Van Dyck
Department WNI
Theoretical Computer Science Group
Hasselt University 

januari 2010

History:
- 31/01/10 Initial draft
- 10/04/10 Mathy Vanhoef: Finished classes to construct Abstract Syntax Tree
- 30/04/10 Mathy Vanhoef: Finished interpret function
- 01/04/10 Mathy Vanhoef: Added Memory Stack

Bugs:
*/

#ifndef ABSYN_H
#define ABSYN_H

#include <ostream>
#include <iostream>
#include <list>
#include <stack>
#include <map>
#include <string>

using namespace std;

/**
 * A bc program is a sequence of commands. The program itself needs not to be stored since a previous command is either a
 * function defintion, which is stored seperately or a semicolon-list of statements, which are deleted after execution.
 *
 * For the interpreter we use C++ exceptions to implement statements like break, continue and return. While technicly
 * encoutering such a statement is not "unexpected" and not an "exception", the advantages of using C++ exceptions are
 * greater than the technical "disadvantage" that these statements are "not really" exceptions. The advantage is
 * that only the revelent statements will need to be modified to implement such statements. Eg. to implement break we only need to
 * add code to BreakStatement and WhileStatement.
 *
 * Another option would be to use global flags. These flags are set if a break/continue/.. statement is executed. In this
 * case we would have to check the global flags each time we interpet an abstract statement. This results in adding code
 * to unrelevant classes that shouldn't need to know that a break/continue/.. statement has been executed. This makes
 * developing harder since adding new code (and forgetting to check the global flags) can cause problems to the seemingly
 * unrelated statements break/continue/..
 */

class Absyn;
class AbsExpression;
class FunctionDefinition;
class StatementList;


/**
 * Keeps track of all the memory that is being used by yacc and lex for constructing the Abstact Syntax Tree (AST). The AST
 * is constructed in post-order by yacc. That is, first the children are allocated and then the parent will be allocated.
 * We use this to keep track of memory. In general we will do the following:
 * 	- When a node is allocated, it is pushed on the stack.
 *	- When a node with n children is allocated, we pop the stack n times, allocate the new node, and push it on the stack.
 *	  Note that it is now the responsibility of the new node to free its children.
 *	- When there is an error, we will free all the pointers that are currently on the stack.
 *
 * Because yacc can use a lookahead we have to treat strings (the token NAME) as a special case. The problem is that, if we
 * use a stack, the last element pushed on the stack can actually be the lookahead and not the string we actually want! This
 * is solved by using the function 'useString' that needs the pointer to the string we want to use, this will ensure the
 * correct pointer is 'popped'.
 */
class MemoryStack
{
public:
	void push(Absyn *absynPointer);
	void push(list<char *> *charListPointer);
	void push(list<AbsExpression *> *absExprPointerPointer);
	void pop();
	void pop(int num);
	void popAndPush(int num, Absyn *absynPointer) { pop(num); push(absynPointer); }
	void popAndPush(int num, list<char *> *charListPointer) { pop(num); push(charListPointer); }
	void popAndPush(int num, list<AbsExpression *> *absExprPointerPointer) { pop(num); push(absExprPointerPointer); }
	/**
	 * Adds a char pointer. Must be allocated using 'new char[]' and will be freed using 'delete[]'.
	 */
	void addString(char *charPointer);
	/**
	 * This tells the Memory Stack that it is no longer responsible for freeing the char pointer. Call this function once you
	 * will delete the char pointer yourself.
	 */
	void useString(char *charPointer);
	/**
	 * Frees all the memory that is currently unused
	 */
	void freeAll();

private:
	enum PointerType { AbsynPointer, CharListPointer, AbsExprListPointer };
	/**
	 * This stack contains the type of the pointers that have been pushed. This is used so that pop() will pop a value from
	 * the correct stack. We couldn't use once single stack to store the pointers because we need to keep track of multiple
	 * types of pointers. Using this 'type stack' we are emulating the behaviour of a "single stack with multiple pointer types".
	 */
	stack<PointerType> m_pointerTypes;
	stack<Absyn*> m_absynPointers;
	stack<list<char *> *> m_charListPointers;
	stack<list<AbsExpression *> *> m_absExprListPointers;

	list<char*> m_charPointers;
};





/**
 * Contains the function definitions, global variables and local variables during the execution of the programm.
 * Has function to get/set variables, get/set functions and enter/exit functions.
 */
class RuntimeContext
{
public:
	~RuntimeContext();
	void reset();

	/**
	 * Get's the value of a variable. It will search for local variables first, and then check global variables.
	 * If the variable doesn't exists it returns 0.
	 */
	double getVariable(const string &name) const;
	/**
	 * Set the value of a variable. It will search for local variables first, and else set it as a global variable.
	 * If the variable doesn't exist it will be added as a global variable.
	 */
	void setVariable(const string &name, double value);

	/**
	 * Add the function to the runtime context If a function with the same name already exists, it will be removed and replaced by the new one.
	 */
	void addFunctionDefinition(const FunctionDefinition *definition);
	/**
	 * Returns the function definition of the function. Returns NULL when the function wasn't found.
	 */
	const FunctionDefinition * getFunctionDefinition(const char *name) const;

	/**
	 * Prepares the runtime context to execute a function body. It will add the parameters and auto variables as local variables,
	 * and will also update the call stack.
	 * @param	function		Function that is being called
	 * @param	argValues		Values of the arguments of the function. Size must match the number of arguments of the function!
	 */
	void enterFunction(const FunctionDefinition *function, const list<double> &argValues);
	/**
	 * Cleans up the runtime context after executing a function body. It will remove the parameters and auto variables.
	 */
	void exitFunction();

	/**
	 * Throws a RuntimeException with the given message
	 */
	void throwError(const string &errmessage) const;


private:
	/**
	 * Contains all the function definitions. The name of the function is used as the key.
	 */
	map<string, const FunctionDefinition*> m_functionTable;
	/**
	 * Contains all the variables. Name of the variable is used as the key. The top element of the stack contains the
	 * current value of the (local or global) variable. Arguments and local variables are added by pushing it's value
	 * on this stack (see enter/exit function).
	 */
	map<string, stack<double> > m_variables;
	/**
	 * Callstack to keep track in which function we currently are
	 */
	stack<const FunctionDefinition*> m_callStack;
	/**
	 * Adds a local variable
	 */
	void addVariable(const string &name, double value);
	/**
	 * Removes a local variable
	 */
	void delVariable(const string &name);
};


class RuntimeException : public exception
{
public:
	RuntimeException(const string &message) : m_message(message)
	{}
	~RuntimeException() throw()
	{}
	const string & getMessage() const { return m_message; }

private:
	string m_message;
};

class ContinueFlowException : public exception
{
};

class BreakFlowException : public exception
{
};

class HaltFlowException : public exception
{
};

class ReturnFlowException : public exception
{
public:
	ReturnFlowException(double value) : m_value(value)
	{}
	double getValue() const { return m_value; }

private:
	double m_value;
};


/**
 * Abstract base class for all classes in the syntax tree.
 * Provides utility methods and declares abstract methods
 * which must be implemented by each concrete derived class.
 */
class Absyn
{
public: 
	virtual ~Absyn() {}
  
	/**
	 * Prints the abstract syntax tree to outputstream o and indents each line corresponding
	 * to the depth of the node printed, starting with depth spaces. Prefix follows
	 * the identation immediately. 
	 */
	virtual void print(int depth = 0, const char* prefix = "", ostream& o = cout) const = 0;

	/**
 	 * Interpret the AST. Will output results to cout. If the current node is an expression
	 * its value will be returned. If it is not an expression 0.0 will be returned.
	 */
	virtual double interpret(RuntimeContext *context, ostream &o = cout) const = 0;

	/**
	 * Prints 2*depth spaces followed by prefix to outputstream o.
	 */
	void indent(int depth = 0, const char* prefix = "", ostream& o = cout) const;

protected:
	/**
	 * Utility function to free a list of char* pointers. Frees both the char* pointers in the list aswell as the
	 * list<char*>* pointer.
	 */
	static void freeCharList(list<char *> *charList);
	/**
	 * Utility function to free a list of AbsExpression* pointers. Frees both the AbsExpression* pointers in the
	 * list aswell as the list<AbsExpression*>* pointer.
	 */
	static void freeExprList(list<AbsExpression *> *exprList);
};

// ------------------------------------------------------------------------------

class AbsCommand : public Absyn
{
public:
	virtual ~AbsCommand() {}

protected:
	AbsCommand() {}
};


class FunctionDefinition : public AbsCommand
{
public:
	FunctionDefinition(char *name, list<char *> *args, list<char *> *autos, StatementList *commands)
	: m_name(name), m_args(args), m_autos(autos), m_commands(commands)
	{}
	~FunctionDefinition();
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	const char * getName() const { return m_name; }
	const list<char *> * getArguments() const { return m_args; }
	const list<char *> * getAutoVariables() const { return m_autos; }
	const StatementList * getBody() const { return m_commands; }

protected:
	char *m_name;
	list<char *> *m_args;
	list<char *> *m_autos;
	StatementList *m_commands;
	void printVariableList(list<char *> *list, ostream& o = cout) const;
};


class StatementListCommand : public AbsCommand
{
public:
	StatementListCommand(StatementList *list) : m_list(list) {}
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;

protected:
	StatementList *m_list;
};

// ------------------------------------------------------------------------------


class AbsExpression : public Absyn
{
public:
	/**
	 * Returns true if the result of the expression should be printed (if used as a statement). False if the
	 * result shouldn't be printed.
	 */
	virtual bool shouldDisplayResult() const = 0;

protected:
	AbsExpression() {}
};

class ConstantExpression : public AbsExpression
{
public:
	ConstantExpression(double constant) : m_constant(constant) {}
	void setConstant(double constant) { m_constant = constant; }
	double getConstant() const { return m_constant; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }

protected:
	double m_constant;
};

class VariableExpression : public AbsExpression
{
public:
	VariableExpression(char *name) : m_name(name)
	{}
	~VariableExpression() { delete[] m_name; m_name = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }

protected:
	char *m_name;
};

class PrefixOpExpression : public AbsExpression
{
public:
	enum OpType { OP_INCR, OP_DECR };
	static const char * stringOpType(OpType type);
	PrefixOpExpression(char *name, OpType type) : m_name(name), m_type(type) {}
	~PrefixOpExpression() { delete[] m_name; m_name = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }

protected:
	char *m_name;
	OpType m_type;
};

class PostfixOpExpression : public AbsExpression
{
public:
	enum OpType { OP_INCR, OP_DECR };
	static const char * stringOpType(OpType type);
	PostfixOpExpression(char *name, OpType type) : m_name(name), m_type(type) {}
	~PostfixOpExpression() { delete[] m_name; m_name = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }

protected:
	char *m_name;
	OpType m_type;
};

class ArithmeticExpression : public AbsExpression
{
public:
	enum OpType { OP_PLUS, OP_MINUS, OP_MUL, OP_DIV, OP_MOD, OP_POW };
	static const char * stringOpType(OpType type);
	ArithmeticExpression(AbsExpression *lhs, AbsExpression *rhs, OpType type) : m_lhs(lhs), m_rhs(rhs), m_type(type) {}
	~ArithmeticExpression() { delete m_lhs; m_lhs = NULL; delete m_rhs; m_rhs = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }

protected:
	AbsExpression *m_lhs;
	AbsExpression *m_rhs;
	OpType m_type;
};

class BooleanExpression : public AbsExpression
{
public:
	enum OpType { OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE, OP_AND, OP_OR };
	static const char * stringOpType(OpType type);
	BooleanExpression(AbsExpression *lhs, AbsExpression *rhs, OpType type) : m_lhs(lhs), m_rhs(rhs), m_type(type) {}
	~BooleanExpression() { delete m_lhs; m_lhs = NULL; delete m_rhs; m_rhs = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }

protected:
	AbsExpression *m_lhs;
	AbsExpression *m_rhs;
	OpType m_type;
};

class NegationExpression : public AbsExpression
{
public:
	NegationExpression(AbsExpression *expr) : m_expr(expr) {}
	~NegationExpression() { delete m_expr; m_expr = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }

protected:
	AbsExpression *m_expr;
};

class AssignExpression : public AbsExpression
{
public:
	AssignExpression(char *name, AbsExpression *expr) : m_name(name), m_expr(expr) {}
	~AssignExpression() {  delete[] m_name; m_name = NULL; delete m_expr; m_expr = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return false; }

protected:
	char *m_name;
	AbsExpression *m_expr;
};

class FunctionCallExpression : public AbsExpression
{
public:
	FunctionCallExpression(char *name, list<AbsExpression*> *argList) : m_name(name), m_argList(argList) {}
	~FunctionCallExpression() { delete[] m_name; m_name = NULL; freeExprList(m_argList); m_argList = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }


protected:
	char *m_name;
	list<AbsExpression*> *m_argList;
};

class MinusExpression : public AbsExpression
{
public:
	MinusExpression(AbsExpression *expr) : m_expr(expr) {}
	~MinusExpression() { delete m_expr; m_expr = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
	/*virtual*/ bool shouldDisplayResult() const { return true; }

protected:
	AbsExpression *m_expr;
};

// ------------------------------------------------------------------------------

class AbsStatement : public Absyn
{
public:
	virtual ~AbsStatement() {}

protected:
	AbsStatement() {}
};

class ExpressionStatement : public AbsStatement
{
public:
	ExpressionStatement(AbsExpression *expr) : m_expr(expr) {}
	~ExpressionStatement() { delete m_expr; m_expr = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;

protected:
	AbsExpression *m_expr;
};


class IfStatement : public AbsStatement
{
public:
	IfStatement(AbsExpression *condition, AbsStatement *trueStmt, AbsStatement *falseStmt = 0) : m_condition(condition), m_trueStmt(trueStmt), m_falseStmt(falseStmt)
	{}
	~IfStatement();
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;

protected:
	AbsExpression *m_condition;
	AbsStatement *m_trueStmt;
	AbsStatement *m_falseStmt;
};

class WhileStatement : public AbsStatement
{
public:
	WhileStatement(AbsExpression *condition, AbsStatement *statement) : m_condition(condition), m_statement(statement)
	{}
	~WhileStatement() { delete m_condition; m_condition = NULL; delete m_statement; m_statement = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;

protected:
	AbsExpression *m_condition;
	AbsStatement *m_statement;
};

class BreakStatement : public AbsStatement
{
public:
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
};

class ContinueStatement : public AbsStatement
{
public:
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
};

class HaltStatement : public AbsStatement
{
public:
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;
};

class ReturnStatement : public AbsStatement
{
public:
	ReturnStatement(AbsExpression *expr = NULL) : m_expr(expr)
	{}
	~ReturnStatement() { delete m_expr; m_expr = NULL; }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;

private:
	AbsExpression *m_expr;
};


// ------------------------------------------------------------------------------

class StatementList : public AbsStatement
{
public:
	StatementList() {}
	StatementList(AbsStatement *statement) { m_statements.push_back(statement); }
	~StatementList();
	void addStatement(AbsStatement *statement) { m_statements.push_back(statement); }
	/*virtual*/ void print(int depth = 0, const char* prefix = "", ostream& o = cout) const;
	/*virtual*/ double interpret(RuntimeContext *context, ostream &o = cout) const;

private:
	list<AbsStatement*> m_statements;
};

#endif // ABSYN_H
