#include "TinyJS.h"
#include <string.h>
#include <vector>
#include <cstdlib>

#pragma once

// designed to be abstract, but not enforced
class CSyntaxNode
{
public:
	CSyntaxNode();
	~CSyntaxNode();

protected:
	CSyntaxNode* node;
};						   

// designed to be abstract except when used as a block, but not enforced
class CSyntaxStatement : public CSyntaxNode { };

// designed to be abstract, but not enforced
class CSyntaxExpression : public CSyntaxNode { };

class CSyntaxSequence : public CSyntaxStatement
{
public:
	CSyntaxSequence(CSyntaxNode* front, CSyntaxNode* last);
	~CSyntaxSequence();

	CSyntaxNode* first() { return node; }
	CSyntaxNode* second() { return last; }

private:
	CSyntaxNode* last;
};

class CSyntaxEval : public CSyntaxExpression
{
public:
	CSyntaxEval(CSyntaxExpression* expr);
};

class CSyntaxIf : public CSyntaxStatement
{
public:
	CSyntaxIf(CSyntaxExpression* expr, CSyntaxNode* body, CSyntaxNode* else_);
	CSyntaxIf(CSyntaxExpression* expr, CSyntaxNode* body);
	~CSyntaxIf();

private:
	CSyntaxExpression* expr;
	CSyntaxNode* else_;
};

class CSyntaxWhile : public CSyntaxStatement
{
public:
	CSyntaxWhile(CSyntaxExpression* expr, CSyntaxNode* body);
	~CSyntaxWhile();

private:
	CSyntaxExpression* expr;
};

class CSyntaxFor : public CSyntaxStatement
{
public:
	CSyntaxFor(CSyntaxNode* init, CSyntaxExpression* cond, CSyntaxExpression* update, CSyntaxNode* body);
	~CSyntaxFor();

private:
	CSyntaxNode* init;
	CSyntaxExpression* cond;
	CSyntaxExpression* update;
};

class CSyntaxFactor : public CSyntaxExpression
{
public:
	enum FACTOR_TYPES
	{
		F_TYPE_INT = 1,
		F_TYPE_DOUBLE = 2,
		F_TYPE_STRING = 4,
		F_TYPE_IDENTIFIER = 8
	};
	CSyntaxFactor(std::string val);

	bool isValueType() { return factorType & (F_TYPE_INT | F_TYPE_DOUBLE | F_TYPE_STRING); }
	std::string getRawValue() { return value; }
	double getDouble() { if(factorType != F_TYPE_DOUBLE) return getInt(); return std::strtod(value.c_str(), 0); }
	int getInt() { if(factorType != F_TYPE_INT) return 0; return std::strtol(value.c_str(), 0, 0); }

protected:
	std::string value;
	int factorType;
};

class CSyntaxID	: public CSyntaxFactor
{
public:
	CSyntaxID(std::string id);
};

class CSyntaxFunction : public CSyntaxExpression
{
public:
	CSyntaxFunction(CSyntaxID* name, std::vector<CSyntaxID*>& arguments, CSyntaxStatement* body);
	~CSyntaxFunction();

private:
	CSyntaxID* name;
	std::vector<CSyntaxID*> arguments;
};

class CSyntaxFunctionCall : public CSyntaxExpression
{
public:
	CSyntaxFunctionCall(CSyntaxExpression* name, std::vector<CSyntaxExpression*> arguments);
	~CSyntaxFunctionCall();

protected:
	std::vector<CSyntaxExpression*> actuals;
};

class CSyntaxNew : public CSyntaxFunctionCall
{
public:
	CSyntaxNew(CSyntaxExpression* name, std::vector<CSyntaxExpression*>& arguments);
};

class CSyntaxReturn : public CSyntaxStatement
{
public:
	CSyntaxReturn(CSyntaxExpression* value);
};

class CSyntaxAssign : public CSyntaxExpression
{
public:
	// this has to be an expression on the rhs to deal with things like
	//     var some.path.to.thing = something;
	// or things like 
	//     a["myfavoriteindex"] = something;
	// the thing on the lhs is an l-value, but it is an expression that evaluates
	// to one. 
	CSyntaxAssign(int op, CSyntaxExpression* lvalue, CSyntaxExpression* rvalue);
	~CSyntaxAssign();

private:
	int op;
	CSyntaxExpression* lval;
};

class CSyntaxTernaryOperator : public CSyntaxExpression
{
public:
	CSyntaxTernaryOperator(int op, CSyntaxExpression* cond, CSyntaxExpression* b1, CSyntaxExpression* b2);
	~CSyntaxTernaryOperator();

private:
	int op;
	CSyntaxExpression* b1;
	CSyntaxExpression* b2;
};

class CSyntaxBinaryOperator : public CSyntaxExpression
{
public:
	CSyntaxBinaryOperator(int op, CSyntaxExpression* left, CSyntaxExpression* right);
	~CSyntaxBinaryOperator();

	bool canBeLval() { return op == '.' || op == '['; }

private:
	int op;
	CSyntaxExpression* right;
};

class CSyntaxCondition : public CSyntaxBinaryOperator
{
public:
	CSyntaxCondition(int op, CSyntaxExpression* left, CSyntaxExpression* right);
};

class CSyntaxRelation : public CSyntaxBinaryOperator
{
public:
	CSyntaxRelation(int rel, CSyntaxExpression* left, CSyntaxExpression* right);
};

class CSyntaxUnaryOperator : public CSyntaxExpression
{
public:
	CSyntaxUnaryOperator(int op, CSyntaxExpression* expr);

private:
	int op;
};

class CScriptSyntaxTree
{
public:
	CScriptSyntaxTree(CScriptLex* lexer);
	CScriptSyntaxTree(const std::string& buffer);
	~CScriptSyntaxTree();

	void parse();

protected:
	CScriptLex* lexer;
	bool lexerOwned;
	CSyntaxNode* root;

	// taken from TinyJS.h
	// parsing - in order of precedence
	std::vector<CSyntaxExpression*> functionCall();
	CSyntaxExpression* factor();
	CSyntaxExpression* unary();
	CSyntaxExpression* term();
	CSyntaxExpression* expression();
	CSyntaxExpression* shift();
	CSyntaxExpression* condition();
	CSyntaxExpression* logic();
	CSyntaxExpression* ternary();
	CSyntaxExpression* base();
	// can return null for blocks like "{ }" or possibly "{ ;* }"
	CSyntaxStatement* block();
	CSyntaxNode* statement();
	// parsing utility functions
	CSyntaxFunction* parseFunctionDefinition();
	std::vector<CSyntaxID*> parseFunctionArguments();
};