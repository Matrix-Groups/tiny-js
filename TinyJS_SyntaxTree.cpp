#include "TinyJS_SyntaxTree.h"
#include <string.h>
#include <assert.h>

#define ASSERT(X) assert(X)

CScriptSyntaxTree::CScriptSyntaxTree(CScriptLex* lexer)
{ 
	this->lexer = lexer;
	lexerOwned = false;
	root = 0;
}

CScriptSyntaxTree::CScriptSyntaxTree(const std::string& buffer)
{
	this->lexer = new CScriptLex(buffer);
	lexerOwned = true;
	root = 0;
}

CScriptSyntaxTree::~CScriptSyntaxTree()
{
	if(root)
		delete root;
	if(lexerOwned)
		delete lexer;
}

void CScriptSyntaxTree::parse()
{
	CSyntaxSequence* stmts = 0;
	while(lexer->tk)
		stmts = new CSyntaxSequence(stmts, statement());
	lexer->match(LEX_EOF);
	if(stmts && !stmts->first())
		root = stmts->second();
	else
		root = stmts;
}

std::vector<CSyntaxExpression*> CScriptSyntaxTree::functionCall()
{
	std::vector<CSyntaxExpression*> args;
	lexer->match('(');
	while(lexer->tk != ')')
	{
		args.push_back(base());
		if(lexer->tk != ')') lexer->match(',');
	}
	lexer->match(')');
	return args;
}

CSyntaxExpression* CScriptSyntaxTree::factor()
{
	if(lexer->tk == '(')
	{
		lexer->match('(');
		CSyntaxExpression* a = base();
		lexer->match(')');
		return a;
	}
	if(lexer->tk == LEX_R_TRUE)
	{
		lexer->match(LEX_R_TRUE);
		return new CSyntaxFactor("1");
	}
	if(lexer->tk == LEX_R_FALSE)
	{
		lexer->match(LEX_R_FALSE);
		return new CSyntaxFactor("0");
	}
	if(lexer->tk == LEX_R_NULL)
	{
		lexer->match(LEX_R_NULL);
		return new CSyntaxFactor("null");
	}
	if(lexer->tk == LEX_R_UNDEFINED)
	{
		lexer->match(LEX_R_UNDEFINED);
		return new CSyntaxFactor("undefined");
	}
	if(lexer->tk == LEX_ID)
	{
		std::string tokenName = lexer->tkStr;
		lexer->match(LEX_ID);
		CSyntaxExpression* a = 0;
		while(lexer->tk == '(' || lexer->tk == '.' || lexer->tk == '[')
		{
			if(lexer->tk == '(')
			{ 
				a = new CSyntaxFunctionCall(a ? a : new CSyntaxID(tokenName), functionCall());
			}
			else if(lexer->tk == '.')
			{ 
				lexer->match('.');
				const std::string &name = lexer->tkStr;
				a = new CSyntaxBinaryOperator('.', a ? a : new CSyntaxID(tokenName), new CSyntaxID(name));
				lexer->match(LEX_ID);
			}
			else if(lexer->tk == '[')
			{ 
				lexer->match('[');
				CSyntaxExpression* index = base();
				lexer->match(']');
				a = new CSyntaxBinaryOperator('[', a ? a : new CSyntaxID(tokenName), index);
			}
			else ASSERT(0);
		}
		// likely the lhs of an assignment (or rhs, really)
		if(!a)
			a = new CSyntaxID(tokenName);
		return a;
	}
	if(lexer->tk == LEX_INT || lexer->tk == LEX_FLOAT)
	{
		CSyntaxFactor* a = new CSyntaxFactor(lexer->tkStr);
		lexer->match(lexer->tk);
		return a;
	}
	if(lexer->tk == LEX_STR)
	{
		CSyntaxFactor* a = new CSyntaxFactor(lexer->tkStr);
		lexer->match(LEX_STR);
		return a;
	}
	if(lexer->tk == '{')
	{
		/* JSON-style object definition */
		std::string arg = "{";
		lexer->match('{');
		// compile string
		while(lexer->tk != '}')
		{
			arg += lexer->tkStr;
			lexer->match(lexer->tk);
		}
		arg += "}";
		lexer->match('}');
		// "__obj_()" is a special native function that constructs an object from a string
		return new CSyntaxFunctionCall(new CSyntaxID("__obj_"), std::vector<CSyntaxExpression*>(1,
			new CSyntaxFactor(arg.c_str())));
	}
	if(lexer->tk == '[')
	{
		/* JSON-style array definition */
		lexer->match('[');
		std::string arg = "[";
		while(lexer->tk != ']')
		{
			arg += lexer->tkStr;
			lexer->match(lexer->tk);
		}
		lexer->match(']');
		arg += ']';
		return new CSyntaxFunctionCall(new CSyntaxID("__array_"), std::vector<CSyntaxExpression*>(1,
			new CSyntaxFactor(arg.c_str())));
	}
	if(lexer->tk == LEX_R_FUNCTION)
	{
		return parseFunctionDefinition();
	}
	if(lexer->tk == LEX_R_NEW)
	{
		// new -> create a new object
		// this means that if the id referenced is a function,
		// call the function with a new object "this" in scope,
		// otherwise create a new object and set its .prototype attribute
		// to the thing specified.
		lexer->match(LEX_R_NEW);
		const std::string &className = lexer->tkStr;
		lexer->match(LEX_ID);
		std::string arg = className + "(";
		while(lexer->tk != ')')
		{
			arg += lexer->tkStr;
			lexer->match(lexer->tk);
		}
		lexer->match(')');
		arg += ')';
		return new CSyntaxFunctionCall(new CSyntaxID("__new_"), std::vector<CSyntaxExpression*>(1,
			new CSyntaxFactor(arg.c_str())));
	}
	// Nothing we can do here... just hope it's the end...
	lexer->match(LEX_EOF);
	return NULL;
}

CSyntaxExpression* CScriptSyntaxTree::unary()
{
	if(lexer->tk == '!')
	{
		lexer->match('!'); // binary not
		return new CSyntaxUnaryOperator('!', factor());
	}
	return factor();
}

CSyntaxExpression* CScriptSyntaxTree::term()
{
	CSyntaxExpression* a = unary();
	while(lexer->tk == '*' || lexer->tk == '/' || lexer->tk == '%')
	{
		int op = lexer->tk;
		lexer->match(lexer->tk);
		return new CSyntaxBinaryOperator(op, a, unary());
	}
	return a;
}

CSyntaxExpression* CScriptSyntaxTree::expression()
{
	bool negate = false;
	if(lexer->tk == '-')
	{
		lexer->match('-');
		negate = true;
	}
	CSyntaxExpression* a = term();
	if(negate)
	{
		a = new CSyntaxBinaryOperator('-', new CSyntaxFactor("0"), a);
	}

	while(lexer->tk == '+' || lexer->tk == '-' ||
		lexer->tk == LEX_PLUSPLUS || lexer->tk == LEX_MINUSMINUS)
	{
		int op = lexer->tk;
		lexer->match(lexer->tk);
		if(op == LEX_PLUSPLUS || op == LEX_MINUSMINUS)
		{
			a = new CSyntaxUnaryOperator(op, a);
		}
		else
		{
			a = new CSyntaxBinaryOperator(op, a, term());
		}
	}
	return a;
}

CSyntaxExpression* CScriptSyntaxTree::shift()
{
	CSyntaxExpression* a = expression();
	if(lexer->tk == LEX_LSHIFT || lexer->tk == LEX_RSHIFT || lexer->tk == LEX_RSHIFTUNSIGNED)
	{
		int op = lexer->tk;
		lexer->match(op);
		return new CSyntaxBinaryOperator(op, a, base());
	}
	return a;
}

CSyntaxExpression* CScriptSyntaxTree::condition()
{
	CSyntaxExpression* a = shift();
	while(lexer->tk == LEX_EQUAL || lexer->tk == LEX_NEQUAL ||
		lexer->tk == LEX_TYPEEQUAL || lexer->tk == LEX_NTYPEEQUAL ||
		lexer->tk == LEX_LEQUAL || lexer->tk == LEX_GEQUAL ||
		lexer->tk == '<' || lexer->tk == '>')
	{
		int op = lexer->tk;
		lexer->match(lexer->tk);
		a = new CSyntaxRelation(op, a, shift());
	}
	return a;
}

CSyntaxExpression* CScriptSyntaxTree::logic()
{
	CSyntaxExpression* a = condition();
	CSyntaxExpression* b;
	while(lexer->tk == '&' || lexer->tk == '|' || lexer->tk == '^' || lexer->tk == LEX_ANDAND || lexer->tk == LEX_OROR)
	{
		int op = lexer->tk;
		lexer->match(lexer->tk);
		b = condition();
		if(op == LEX_ANDAND || op == LEX_OROR)
			a = new CSyntaxCondition(op, a, b);
		else
			a = new CSyntaxBinaryOperator(op, a, b);
	}
	return a;
}

CSyntaxExpression* CScriptSyntaxTree::ternary()
{
	CSyntaxExpression* lhs = logic();
	if(lexer->tk == '?')
	{
		CSyntaxExpression* b1 = base();
		lexer->match(':');
		lhs = new CSyntaxTernaryOperator('?', lhs, b1, base());
	}

	return lhs;
}

CSyntaxExpression* CScriptSyntaxTree::base()
{
	CSyntaxExpression* lhs = ternary();
	if(lexer->tk == '=' || lexer->tk == LEX_PLUSEQUAL || lexer->tk == LEX_MINUSEQUAL)
	{					
		int op = lexer->tk;
		lexer->match(lexer->tk);
		CSyntaxExpression* rhs = base();
		if(op == '=')
		{
			lhs = new CSyntaxAssign('=', lhs, rhs);
		}
		// hey look, syntactic desugaring
		else if(op == LEX_PLUSEQUAL)
		{
			lhs = new CSyntaxAssign('=', lhs, new CSyntaxBinaryOperator('+', lhs, rhs));
		}
		else if(op == LEX_MINUSEQUAL)
		{
			lhs = new CSyntaxAssign('=', lhs, new CSyntaxBinaryOperator('-', lhs, rhs));
		}
		else ASSERT(0);
	}
	return lhs;
}

CSyntaxStatement* CScriptSyntaxTree::block()
{
	lexer->match('{');
	CSyntaxSequence* stmts = 0;
	while(lexer->tk && lexer->tk != '}')
		stmts = new CSyntaxSequence(stmts, statement());
	lexer->match('}');
	return stmts;
}

CSyntaxNode* CScriptSyntaxTree::statement()
{
	if(lexer->tk == LEX_ID ||
		lexer->tk == LEX_INT ||
		lexer->tk == LEX_FLOAT ||
		lexer->tk == LEX_STR ||
		lexer->tk == '-')
	{
		/* Execute a simple statement that only contains basic arithmetic... */
		CSyntaxNode* out = base();
		lexer->match(';');
		return out;
	}
	else if(lexer->tk == '{')
	{
		/* A block of code */
		return block();
	}
	else if(lexer->tk == ';')
	{
		/* Empty statement - to allow things like ;;; */
		lexer->match(';');
		return statement();
	}
	else if(lexer->tk == LEX_R_VAR)
	{
		lexer->match(LEX_R_VAR);
		CSyntaxSequence* stmts = 0;
		CSyntaxAssign* stmt = 0;
		while(lexer->tk != ';')
		{
			CSyntaxExpression* lhs = new CSyntaxID(lexer->tkStr);
			lexer->match(LEX_ID);
			// now do stuff defined with dots
			while(lexer->tk == '.')
			{
				lexer->match('.');
				lhs = new CSyntaxBinaryOperator('.', lhs, new CSyntaxID(lexer->tkStr));
				lexer->match(LEX_ID);
			}
			// sort out initialiser
			if(lexer->tk == '=')
			{
				lexer->match('=');
				stmt = new CSyntaxAssign('=', lhs, base());
			}
			if(lexer->tk != ';')
			{
				lexer->match(',');
				stmts = new CSyntaxSequence(stmts, stmt ? lhs : stmt);
				stmt = 0;
			}
		}
		lexer->match(';');
		return stmts ? stmts : (CSyntaxNode*)stmt;
	}
	else if(lexer->tk == LEX_R_IF)
	{
		lexer->match(LEX_R_IF);
		lexer->match('(');
		CSyntaxExpression* cond = base();
		lexer->match(')');
		CSyntaxNode* body = statement();
		CSyntaxNode* else_ = 0;
		if(lexer->tk == LEX_R_ELSE)
		{
			lexer->match(LEX_R_ELSE);
			else_ = statement();
		}
		return new CSyntaxIf(cond, body, else_);
	}
	else if(lexer->tk == LEX_R_WHILE)
	{
		lexer->match(LEX_R_WHILE);
		lexer->match('(');
		CSyntaxExpression* cond = base();
		lexer->match(')');
		CSyntaxNode* body = statement();
		return new CSyntaxWhile(cond, body);
	}
	else if(lexer->tk == LEX_R_FOR)
	{
		lexer->match(LEX_R_FOR);
		lexer->match('(');
		CSyntaxNode* init = statement(); // initialisation
		CSyntaxExpression* cond = base(); // condition
		lexer->match(';');
		CSyntaxExpression* iter = base(); // iterator
		lexer->match(')');
		CSyntaxNode* body = statement();
		return new CSyntaxFor(init, cond, iter, body);
	}
	else if(lexer->tk == LEX_R_RETURN)
	{
		lexer->match(LEX_R_RETURN);
		CSyntaxExpression* value = base();
		lexer->match(';');
		return new CSyntaxReturn(value);
	}
	else if(lexer->tk == LEX_R_FUNCTION)
	{
		return parseFunctionDefinition();
	}
	else
	{
		lexer->match(LEX_EOF);
		return NULL;
	}
}

CSyntaxFunction* CScriptSyntaxTree::parseFunctionDefinition()
{
	lexer->match(LEX_R_FUNCTION);
	std::string funcName = TINYJS_TEMP_NAME;
	/* we can have functions without names */
	if(lexer->tk == LEX_ID)
	{
		funcName = lexer->tkStr;
		lexer->match(LEX_ID);
	}
	std::vector<CSyntaxID*> args = parseFunctionArguments();
	CSyntaxStatement* body = block();
	return new CSyntaxFunction(funcName == TINYJS_TEMP_NAME ? 0 : new CSyntaxID(funcName), args, body);
}

std::vector<CSyntaxID*> CScriptSyntaxTree::parseFunctionArguments()
{
	std::vector<CSyntaxID*> out;
	lexer->match('(');
	while(lexer->tk != ')')
	{
		out.push_back(new CSyntaxID(lexer->tkStr));
		lexer->match(LEX_ID);
		if(lexer->tk != ')') lexer->match(',');
	}
	lexer->match(')');
	return out;
}

CSyntaxNode::CSyntaxNode()
{ 
	node = 0;
}

CSyntaxNode::~CSyntaxNode()
{ 
	if(node)
		delete node;
}

CSyntaxSequence::CSyntaxSequence(CSyntaxNode* front, CSyntaxNode* last)
{
	node = front;
	this->last = last;
}

CSyntaxSequence::~CSyntaxSequence()
{
	if(last)
		delete last;
}

CSyntaxEval::CSyntaxEval(CSyntaxExpression* expr)
{
	ASSERT(expr);
	node = expr;
}

CSyntaxIf::CSyntaxIf(CSyntaxExpression* expr, CSyntaxNode* body, CSyntaxNode* else_)
{
	ASSERT(expr);
	ASSERT(body);
	node = body;
	this->else_ = else_;
	this->expr = expr;
}

CSyntaxIf::CSyntaxIf(CSyntaxExpression* expr, CSyntaxNode* body) : CSyntaxIf(expr, body, 0) { }

CSyntaxIf::~CSyntaxIf()
{
	delete expr;
	if(else_)
		delete else_;
}

CSyntaxWhile::CSyntaxWhile(CSyntaxExpression* expr, CSyntaxNode* body)
{
	ASSERT(body);
	ASSERT(expr);
	node = body;
	this->expr = expr;
}

CSyntaxWhile::~CSyntaxWhile()
{
	delete expr;
}

CSyntaxFor::CSyntaxFor(CSyntaxNode* init, CSyntaxExpression* expr, CSyntaxExpression* update, CSyntaxNode* body)
{
	ASSERT(body);
	node = body;
	this->init = init;
	this->update = update;
	cond = expr;
}

CSyntaxFor::~CSyntaxFor()
{
	if(init)
		delete init;
	if(update)
		delete update;
	if(cond)
		delete cond;
}

CSyntaxFactor::CSyntaxFactor(std::string val)
{
	value = val;
	node = 0;

	char f = value.front();
	if(f == '"')
		factorType = F_TYPE_STRING;
	else if(isdigit(f))
	{
		if(value.find('.') != std::string::npos)
			factorType = F_TYPE_DOUBLE;
		else
			factorType = F_TYPE_INT;
	}
	else
		factorType = F_TYPE_IDENTIFIER;
}

CSyntaxID::CSyntaxID(std::string id) : CSyntaxFactor(id) { }

CSyntaxFunction::CSyntaxFunction(CSyntaxID* name, std::vector<CSyntaxID*>& arguments, CSyntaxStatement* body)
{
	ASSERT(body);
	this->name = name;
	this->arguments = arguments;
	node = body;
}

CSyntaxFunction::~CSyntaxFunction()
{
	if(name)
		delete name;
	for(CSyntaxID* arg : arguments)
		if(arg)
			delete arg;
}

CSyntaxAssign::CSyntaxAssign(int op, CSyntaxExpression* lvalue, CSyntaxExpression* rvalue)
{
	ASSERT(lvalue);
	ASSERT(rvalue);
	this->op = op;
	lval = lvalue;
	node = rvalue;
}

CSyntaxAssign::~CSyntaxAssign()
{
	delete lval;
}

CSyntaxTernaryOperator::CSyntaxTernaryOperator(int op, CSyntaxExpression* cond, CSyntaxExpression* b1, CSyntaxExpression* b2)
{
	ASSERT(cond);
	ASSERT(b1);
	ASSERT(b2);
	this->op = op;
	node = cond;
	this->b1 = b1;
	this->b2 = b2;
}

CSyntaxTernaryOperator::~CSyntaxTernaryOperator()
{
	delete b1;
	delete b2;
}

CSyntaxRelation::CSyntaxRelation(int rel, CSyntaxExpression* left, CSyntaxExpression* right)
	: CSyntaxBinaryOperator(rel, left, right) { }

CSyntaxBinaryOperator::CSyntaxBinaryOperator(int op, CSyntaxExpression* left, CSyntaxExpression* right)
{
	ASSERT(left);
	ASSERT(right);
	this->op = op;
	node = left;
	this->right = right;
}

CSyntaxBinaryOperator::~CSyntaxBinaryOperator()
{
	delete right;
}

CSyntaxUnaryOperator::CSyntaxUnaryOperator(int op, CSyntaxExpression* expr)
{
	this->op = op;
	node = expr;
}

CSyntaxReturn::CSyntaxReturn(CSyntaxExpression* value)
{
	node = value;
}

CSyntaxCondition::CSyntaxCondition(int op, CSyntaxExpression* left, CSyntaxExpression* right)
	: CSyntaxBinaryOperator(op, left, right) { }

CSyntaxNew::CSyntaxNew(CSyntaxExpression* name, std::vector<CSyntaxExpression*>& arguments)
	: CSyntaxFunctionCall(name, arguments) { }

CSyntaxFunctionCall::CSyntaxFunctionCall(CSyntaxExpression* name, std::vector<CSyntaxExpression*> arguments)
{
	node = name;
	actuals = arguments;
}

CSyntaxFunctionCall::~CSyntaxFunctionCall()
{
	for(CSyntaxExpression* arg : actuals)
		delete arg;
}