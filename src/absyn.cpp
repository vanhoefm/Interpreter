/*
absyn.cpp: Class implementations for the abstract syntax tree

Dries Van Dyck
Department WNI
Theoretical Computer Science Group
Hasselt University 

januari 2010

History:
- 31/01/10 Initial draft
- 10/04/10 Finished classes to construct Abstract Syntax Tree
- 30/04/10 Mathy Vanhoef: Finished interpret function
- 01/04/10 Mathy Vanhoef: Added Memory Stack

Bugs:
*/
#include <math.h>
#include <algorithm>
#include <assert.h>
#include "absyn.h"




void MemoryStack::push(Absyn *absynPointer)
{
	m_absynPointers.push(absynPointer);
	m_pointerTypes.push(AbsynPointer);
}

void MemoryStack::push(list<char*> *charListPointer)
{
	m_charListPointers.push(charListPointer);
	m_pointerTypes.push(CharListPointer);
}

void MemoryStack::push(list<AbsExpression*> *absExprPointerPointer)
{
	m_absExprListPointers.push(absExprPointerPointer);
	m_pointerTypes.push(AbsExprListPointer);
}

void MemoryStack::pop()
{
	switch(m_pointerTypes.top())
	{
		case AbsynPointer:
			m_absynPointers.pop();
			break;

		case CharListPointer:
			m_charListPointers.pop();
			break;

		case AbsExprListPointer:
			m_absExprListPointers.pop();
			break;
	}

	m_pointerTypes.pop();
}

void MemoryStack::pop(int num)
{
	for(int i = 0; i < num; ++i)
		pop();
}

void MemoryStack::addString(char *charPointer)
{
	m_charPointers.push_front(charPointer);
}

/**
 * Since strings are being added to the front of the list, in the worst case the first element is the lookahead
 * and the second element will be the string that is being used (= the argument). So when properly used 'charPointer'
 * will always be found as the 1st or 2nd element.
 *
 * We search through the whole list anyway, it shouldn't have an impact on performance and is more robust.
 */
void MemoryStack::useString(char *charPointer)
{
	for(list<char*>::iterator it = m_charPointers.begin(); it != m_charPointers.end(); ++it)
	{
		if(*it == charPointer)
		{
			m_charPointers.erase(it);
			break;
		}
	}
}

void MemoryStack::freeAll()
{
	// Free all Absyn pointers
	while(!m_absynPointers.empty())
	{
		delete m_absynPointers.top();
		m_absynPointers.pop();
	}

	// Free all char list pointers
	while(!m_charListPointers.empty())
	{
		for(list<char*>::iterator it = m_charListPointers.top()->begin(); it != m_charListPointers.top()->end(); ++it)
			delete[] *it;

		delete m_charListPointers.top();
		m_charListPointers.pop();
	}

	// Free all AbsExpression list pointers
	while(!m_absExprListPointers.empty())
	{
		for(list<AbsExpression*>::iterator it = m_absExprListPointers.top()->begin(); it != m_absExprListPointers.top()->end(); ++it)
			delete *it;

		delete m_absExprListPointers.top();
		m_absExprListPointers.pop();
	}

	// Free all char pointers
	for(list<char*>::iterator it = m_charPointers.begin(); it != m_charPointers.end(); ++it)
	{
		delete[] *it;
	}
	m_charPointers.clear();
}




RuntimeContext::~RuntimeContext()
{
	reset();
}

void RuntimeContext::reset()
{
	while(!m_callStack.empty())
		m_callStack.pop();

	for(map<string, const FunctionDefinition*>::iterator it = m_functionTable.begin(); it != m_functionTable.end(); ++it)
	{
		delete it->second;
	}
	m_functionTable.clear();

	m_variables.clear();
}

double RuntimeContext::getVariable(const string &name) const
{
	map<string, stack<double> >::const_iterator it = m_variables.find(name);
	
	if(it != m_variables.end())
	{
		return it->second.top();
	}
	else
		return 0.0;
}

void RuntimeContext::setVariable(const string &name, double value)
{
	stack<double> &variable = m_variables[name];

	if(variable.empty())
		variable.push(value);
	else
		variable.top() = value;
}

void RuntimeContext::addFunctionDefinition(const FunctionDefinition *definition)
{
	string name = definition->getName();
	
	map<string, const FunctionDefinition*>::iterator it = m_functionTable.find(name);
	if(it != m_functionTable.end())
	{
		// Delete old function
		if(it->second)
			delete it->second;
		
		// Replace with old function
		it->second = definition;
	}
	else
	{
		m_functionTable.insert(pair<string, const FunctionDefinition*>(name, definition));
	}
}

const FunctionDefinition * RuntimeContext::getFunctionDefinition(const char *name) const
{
	map<string, const FunctionDefinition*>::const_iterator it = m_functionTable.find(name);

	if(it != m_functionTable.end())
		return it->second;
	else
		return NULL;
}

void RuntimeContext::enterFunction(const FunctionDefinition *function, const list<double> &argValues)
{
	assert(function->getArguments()->size() == argValues.size());

	// Add parameters - iterate through both lists (names and values) at the same time
	const list<char*> *arguments = function->getArguments();

	list<char*>::const_iterator itParam = arguments->begin();
	list<double>::const_iterator itValue = argValues.begin();

	while(itParam != arguments->end() && itValue != argValues.end())
	{
		addVariable(*itParam, *itValue);

		++itParam;
		++itValue;
	}
	
	// Add auto variables
	const list<char*> *autovars = function->getAutoVariables();
	for(list<char*>::const_iterator itAutovar = autovars->begin(); itAutovar != autovars->end(); ++itAutovar)
	{
		addVariable(*itAutovar, 0);
	}

	// Add it to the callstack
	m_callStack.push(function);
}

void RuntimeContext::exitFunction()
{
	assert(!m_callStack.empty());

	const FunctionDefinition *function = m_callStack.top();
	m_callStack.pop();

	// Remove auto variables
	const list<char*> *autovars = function->getAutoVariables();
	for(list<char*>::const_iterator itAutovar = autovars->begin(); itAutovar != autovars->end(); ++itAutovar)
	{
		delVariable(*itAutovar);
	}

	// Remove parameters
	const list<char*> *arguments = function->getArguments();
	for(list<char*>::const_iterator itParam = arguments->begin(); itParam != arguments->end(); ++itParam)
	{
		delVariable(*itParam);
	}
}

void RuntimeContext::throwError(const string &errmessage) const
{
	string currFunc;

	if(m_callStack.empty())
		currFunc = "(main)";
	else
		currFunc = m_callStack.top()->getName();

	string message = string("runtime error in function ") + currFunc + ": " + errmessage + ".";
	throw RuntimeException(message);
}

void RuntimeContext::addVariable(const string &name, double value)
{
	m_variables[name].push(value);
}

void RuntimeContext::delVariable(const string &name)
{
	m_variables[name].pop();
}




void Absyn::indent(int depth, const char *prefix, ostream &o) const
{
	for (int i = 0; i < depth; i++)
		o << "  ";

	o << prefix;
}

/*static*/ void Absyn::freeCharList(list<char *> *charList)
{
	for(list<char *>::iterator it = charList->begin(); it != charList->end(); ++it)
	{
		delete[] *it;
	}

	delete charList;
}

/*static*/ void Absyn::freeExprList(list<AbsExpression *> *exprList)
{
	for(list<AbsExpression *>::iterator it = exprList->begin(); it != exprList->end(); ++it)
	{
		delete *it;
	}

	delete exprList;
}

FunctionDefinition::~FunctionDefinition()
{
	delete[] m_name;
	m_name = NULL;
	freeCharList(m_args);
	m_args = NULL;
	freeCharList(m_autos);
	m_autos = NULL;
	delete m_commands;
	m_commands = NULL;
}

void FunctionDefinition::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "FunctionDefinition: " << m_name << "\n";

	indent(depth + 1, "Arguments: ", o);
	printVariableList(m_args, o);

	indent(depth + 1, "Auto variables: ", o);
	printVariableList(m_autos, o);

	m_commands->print(depth + 1, "Body: ", o);
}

double FunctionDefinition::interpret(RuntimeContext *context, ostream &o) const
{
	assert(0);
}

void FunctionDefinition::printVariableList(list<char *> *varlist, ostream& o) const
{
	if(varlist->empty())
	{
		o << "(none)\n";
	}
	else
	{
		// Get first variable
		char *previous = NULL;
		list<char*>::const_iterator it = varlist->begin();
		previous = *it;

		// As long as there are variables, print the previous variable followed by a comma
		++it;
		while(it != varlist->end())
		{
			o << previous << ", ";
			previous = *it;
			++it;
		}

		// Print last variable without comma
		o << previous << "\n";
	}
}


void StatementListCommand::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "StatementListCommand\n";

	m_list->print(depth + 1, "", o);
}


void ConstantExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "ConstantExpression: " << m_constant << "\n";
}

double ConstantExpression::interpret(RuntimeContext *context, ostream &o) const
{
	return m_constant;
}


void VariableExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "VariableExpression: " << m_name << "\n";
}

double VariableExpression::interpret(RuntimeContext *context, ostream &o) const
{
	return context->getVariable(m_name);
}


const char * PrefixOpExpression::stringOpType(OpType type)
{
	switch(type)
	{
		case OP_INCR:
			return "increment";
		case OP_DECR:
			return "decrement";
		default:
			return "unkown";
	}
}

void PrefixOpExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "PrefixOpExpression:\n";

	indent(depth + 1, "", o);
	o << "Variable: " << m_name << "\n";
	indent(depth + 1, "", o);
	o << "Operation type: " << stringOpType(m_type) << "\n";
}

double PrefixOpExpression::interpret(RuntimeContext *context, ostream &o) const
{
	double value = context->getVariable(m_name);

	switch(m_type)
	{
		case OP_INCR:
			++value;
			break;
		case OP_DECR:
			--value;
			break;
	}

	context->setVariable(m_name, value);
	return value;
}


const char * PostfixOpExpression::stringOpType(OpType type)
{
	switch(type)
	{
		case OP_INCR: return "increment";
		case OP_DECR: return "decrement";
		default: return "unkown";
	}
}

void PostfixOpExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "PostfixOpExpression:\n";

	indent(depth + 1, "", o);
	o << "Variable: " << m_name << "\n";
	indent(depth + 1, "", o);
	o << "Operation type: " << stringOpType(m_type) << "\n";
}

double PostfixOpExpression::interpret(RuntimeContext *context, ostream &o) const
{
	double oldvalue = context->getVariable(m_name);
	double newvalue;

	switch(m_type)
	{
		case OP_INCR:
			newvalue = oldvalue + 1;
			break;
		case OP_DECR:
			newvalue = oldvalue - 1;
			break;
	}

	context->setVariable(m_name, newvalue);
	return oldvalue;
}


const char * ArithmeticExpression::stringOpType(OpType type)
{
	switch(type)
	{
		case OP_PLUS: return "plus";
		case OP_MINUS: return "minus";
		case OP_MUL: return "multiply";
		case OP_DIV: return "divide";
		case OP_MOD: return "modulo";
		case OP_POW: return "power";
		default: return "unknown";
	}
}

void ArithmeticExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "ArithmeticExpression\n";

	m_lhs->print(depth + 1, "Left-hand side: ", o);
	indent(depth + 1, "", o);
	o << "Operator: " << stringOpType(m_type) << "\n";
	m_rhs->print(depth + 1, "Right-hand side: ", o);
}

double ArithmeticExpression::interpret(RuntimeContext *context, ostream &o) const
{
	double lhs = m_lhs->interpret(context, o);
	double rhs = m_rhs->interpret(context, o);
	double rval = 0;

	switch(m_type)
	{
		case OP_PLUS:
			rval = lhs + rhs;
			break;
		case OP_MINUS:
			rval = lhs - rhs;
			break;
		case OP_MUL:
			rval = lhs * rhs;
			break;
		case OP_DIV:
			if(rhs == 0)
				context->throwError("division by zero");
			rval = lhs / rhs;
			break;
		case OP_MOD:
			if(rhs == 0)
				context->throwError("modulo zero");
			rval = lhs - floor(lhs / rhs) * rhs;
			break;
		case OP_POW:
			rval = pow(lhs, std::max(0.0, floor(rhs)));
			break;
	}

	return rval;
}


const char * BooleanExpression::stringOpType(OpType type)
{
	switch(type)
	{
		case OP_LT: return "less than";
		case OP_LE: return "less than or equal";
		case OP_GT: return "greater than";
		case OP_GE: return "greater than or equal";
		case OP_EQ: return "equal";
		case OP_NE: return "not equal";
		case OP_AND: return "and";
		case OP_OR: return "or";
	}
}

void BooleanExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "BooleanExpression\n";

	m_lhs->print(depth + 1, "Left-hand side: ", o);
	indent(depth + 1, "", o);
	o << "Operator: " << stringOpType(m_type) << "\n";
	m_rhs->print(depth + 1, "Right-hand side: ", o);
}

double BooleanExpression::interpret(RuntimeContext *context, ostream &o) const
{
	double lhs = m_lhs->interpret(context, o);
	double rhs = m_rhs->interpret(context, o);
	double rval;

	switch(m_type)
	{
		case OP_LT:
			rval = lhs < rhs;
			break;
		case OP_LE:
			rval = lhs <= rhs;
			break;
		case OP_GT:
			rval = lhs > rhs;
			break;
		case OP_GE:
			rval = lhs >= rhs;
			break;
		case OP_EQ:
			rval = lhs == rhs;
			break;
		case OP_NE:
			rval = lhs != rhs;
			break;
		case OP_AND:
			rval = (lhs != 0) && (rhs != 0);
			break;
		case OP_OR:
			rval = (lhs != 0) || (rhs != 0);
			break;
	}

	return rval;
}


void NegationExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "NegationExpression\n";
	
	m_expr->print(depth + 1, "", o);
}

double NegationExpression::interpret(RuntimeContext *context, ostream &o) const
{
	double value = m_expr->interpret(context, o);
	return !(value != 0);
}


void AssignExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "AssignExpression\n";

	indent(depth + 1, "", o);
	o << "Variable: " << m_name << "\n";
	m_expr->print(depth + 1, "Expression: ", o);
}

double AssignExpression::interpret(RuntimeContext *context, ostream &o) const
{
	double value = m_expr->interpret(context, o);
	context->setVariable(m_name, value);

	return value;
}


void FunctionCallExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "FunctionCallExpression\n";

	
	// Print function name
	indent(depth + 1, "", o);
	o << "Name: " << m_name << "\n";


	// Print arguments
	indent(depth + 1, "", o);
	o << "Arguments: ";

	if(m_argList->empty())
	{
		o << "ExpressionList: Empty\n";
	}
	else
	{
		o << "ExpressionList:\n";
		for(list<AbsExpression*>::const_iterator it = m_argList->begin(); it != m_argList->end(); ++it)
		{
			(*it)->print(depth + 2, "", o);
		}
	}
}

double FunctionCallExpression::interpret(RuntimeContext *context, ostream &o) const
{
	// Get the function definition and check if the function exists
	const FunctionDefinition *functionDef = context->getFunctionDefinition(m_name);

	if(functionDef == NULL)
	{
		string foutmelding = string("function '") + m_name + "' not defined";
		context->throwError(foutmelding);
	}

	// Get argument names and check if we have the same number of argument to call it
	const list<char *> *argumentNames = functionDef->getArguments();

	if(argumentNames->size() !=m_argList->size())
	{
		string foutmelding = string("wrong number of arguments for function '") + m_name + "'";
		context->throwError(foutmelding);
	}

	// Get the values of all the arguments
	list<double> argumentValues;

	for(list<AbsExpression*>::const_iterator it = m_argList->begin(); it != m_argList->end(); ++it)
	{
		argumentValues.push_back((*it)->interpret(context, o));
	}

	// Prepare runtime context
	context->enterFunction(functionDef, argumentValues);
	
	// Call the function - return 0 when there is no return statement
	double rval = 0;
	try
	{
		functionDef->getBody()->interpret(context, o);
	}
	catch(const ReturnFlowException &returnFlow)
	{
		rval = returnFlow.getValue();
	}
	catch(...)
	{
		// Keep RuntimeContext consistent, then rethrow the exception. A common example where this is needed
		// is when the function we are calling throws a RuntimeException.
		context->exitFunction();
		throw;
	}

	// Restore the original runtime context
	context->exitFunction();

	return rval;
}


void MinusExpression::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "MinusExpression\n";
	
	m_expr->print(depth + 1, "", o);
}

double MinusExpression::interpret(RuntimeContext *context, ostream &o) const
{
	return -m_expr->interpret(context, o);
}


void ExpressionStatement::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "ExpressionStatement\n";
	
	m_expr->print(depth + 1, "", o);
}

double ExpressionStatement::interpret(RuntimeContext *context, ostream &o) const
{
	double result = m_expr->interpret(context, o);

	if(m_expr->shouldDisplayResult())
		o << result << std::endl;

	return 0;
}


IfStatement::~IfStatement()
{
	delete m_condition;
	m_condition = NULL;
	delete m_trueStmt;
	m_trueStmt = NULL;
	delete m_falseStmt;
	m_falseStmt = NULL;
}

void IfStatement::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "IfStatement\n";
	
	m_condition->print(depth + 1, "Condition: ", o);
	m_trueStmt->print(depth + 1, "If clause: ", o);
	if(m_falseStmt)
		m_falseStmt->print(depth + 1, "Else clause: ", o);
}

double IfStatement::interpret(RuntimeContext *context, ostream &o) const
{
	double condition = m_condition->interpret(context, o);
	
	if(condition != 0)
		m_trueStmt->interpret(context, o);
	else if(m_falseStmt)
		m_falseStmt->interpret(context, o);

	return 0;
}


void WhileStatement::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "WhileStatement\n";
	
	m_condition->print(depth + 1, "Condition: ", o);
	m_statement->print(depth + 1, "Body: ", o);
}

double WhileStatement::interpret(RuntimeContext *context, ostream &o) const
{
	double condition = m_condition->interpret(context, o);

	while(condition != 0)
	{
		try
		{
			m_statement->interpret(context, o);
		}
		catch(const ContinueFlowException &continueFlow)
		{
			condition = m_condition->interpret(context, o);
			continue;
		}
		catch(const BreakFlowException &breakFlow)
		{
			break;
		}

		condition = m_condition->interpret(context, o);
	}

	return 0;
}


void BreakStatement::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "BreakStatement\n";
}

double BreakStatement::interpret(RuntimeContext *context, ostream &o) const
{
	throw BreakFlowException();
}


void ContinueStatement::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "ContinueStatement\n";
}

double ContinueStatement::interpret(RuntimeContext *context, ostream &o) const
{
	throw ContinueFlowException();
}


void HaltStatement::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "HaltStatement\n";
}

double HaltStatement::interpret(RuntimeContext *context, ostream &o) const
{
	throw HaltFlowException();
}


void ReturnStatement::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);
	o << "ReturnStatement\n";
	if(m_expr)
		m_expr->print(depth + 1, "", o);
}

double ReturnStatement::interpret(RuntimeContext *context, ostream &o) const
{
	double value = 0;

	if(m_expr)
		value = m_expr->interpret(context, o);
		
	throw ReturnFlowException(value);
}


StatementList::~StatementList()
{
	for(list<AbsStatement*>::const_iterator it = m_statements.begin(); it != m_statements.end(); ++it)
	{
		delete *it;
	}

	m_statements.clear();
}

void StatementList::print(int depth, const char* prefix, ostream& o) const
{
	indent(depth, prefix, o);

	if(m_statements.empty())
	{
		o << "StatementList: Empty\n";
	}
	else
	{
		o << "StatementList\n";
		for(list<AbsStatement*>::const_iterator it = m_statements.begin(); it != m_statements.end(); ++it)
		{
			(*it)->print(depth + 1, "", o);
		}
	}
}

double StatementList::interpret(RuntimeContext *context, ostream &o) const
{
	for(list<AbsStatement*>::const_iterator it = m_statements.begin(); it != m_statements.end(); ++it)
	{
		(*it)->interpret(context, o);
	}

	return 0;
}

