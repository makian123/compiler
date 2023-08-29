#pragma once

#include <memory>
#include <vector>
#include <utility>
#include <iostream>
#include <functional>
#include <unordered_map>

#include <llvm/IR/Value.h>

#include "tokenizer/tokenizer.hpp"

struct VarType;
struct Member{
	const VarType *type = nullptr;
	std::string name = "";
	size_t offset = 0;

	explicit Member() = default;
	Member(const VarType *type_, const std::string &name_, size_t offset_):type(type_), name(name_), offset(offset_) {}
	Member(const Member &other): type(other.type), name(other.name), offset(other.offset) {}
};

struct VarType{
	enum class Type{
		ERR,
		VOID,
		CHAR,
		SHORT,
		INT,
		LONG,
		FLOAT,
		DOUBLE,
		STRUCT,

		PTR
	};

	Type type;
	std::string name;
	size_t typeSz;

	//For pointers
	VarType *baseType;

	//For objects
	std::vector<Member> members;

	bool isUnsigned, isArray;
	size_t arrSize;

	explicit VarType(): type(Type::ERR), name(), baseType(), members(), isUnsigned(false), isArray(false), arrSize(0), typeSz(0) {}
	VarType(Type type_, std::string name_, size_t typeSz_, VarType *baseType_, const std::vector<Member> &members_, bool isUnsigned_, bool isArray_, size_t arrSize_);
	VarType(const VarType &other);

	static const VarType ERROR;

	llvm::Type *Codegen() const;
};

struct Scope{
	struct Variable{
		Token ident;
		VarType type;
		llvm::Value *val = nullptr;

		explicit Variable() = default;
		Variable(Token ident_, VarType type_, llvm::Value *val_): ident(ident_), type(type_), val(val_) {}
		Variable(const Variable &other): ident(other.ident), type(other.type), val(other.val) {}
	};
	
	std::vector<VarType> types;
	std::vector<Variable> identifiers;
	std::vector<std::shared_ptr<Scope>> scopes;
	std::shared_ptr<Scope> parent;
	Scope() = default;
};

enum class NodeType{
	ERR,

	VAL,
	BINARY,
	VARDECL,
	BLOCK,
	FUNCDECL,
	IF,
	WHILE,
	RETURN,
	FUNCTIONCALL,
	VARASSIGN,
	MEMBER
};
struct Node{
	NodeType type;

	explicit Node(NodeType type = NodeType::ERR): type(type) {}
	virtual ~Node() = default;
	
	virtual llvm::Value *Codegen() { return nullptr; }
};
struct ValNode: public Node{
	Token val;

	ValNode(const Token &tok): val(tok), Node(NodeType::VAL) {}

	llvm::Value *Codegen() override;
};
struct BinaryNode: public Node{
	std::shared_ptr<Node> lhs, rhs;
	Token operand;

	BinaryNode(std::shared_ptr<Node> lhs_, const Token &operand_, std::shared_ptr<Node> rhs_)
		: lhs(lhs_), operand(operand_), rhs(rhs_), Node(NodeType::BINARY) {}

	llvm::Value *Codegen() override;
};
struct VarDeclNode: public Node{
	const VarType *varType;
	Token ident;
	std::shared_ptr<Node> initial;

	VarDeclNode(const VarType *varType_, Token ident_, std::shared_ptr<Node> init): varType(varType_), ident(ident_), initial(init), Node(NodeType::VARDECL) {}

	llvm::Value *Codegen() override;
};
struct BlockNode: public Node{
	std::vector<std::shared_ptr<Node>> stmts;
	std::shared_ptr<Scope> myScope;

	explicit BlockNode(): stmts(), myScope(nullptr), Node(NodeType::BLOCK) {}
	BlockNode(const std::vector<std::shared_ptr<Node>> &stmts_, std::shared_ptr<Scope> scope): stmts(stmts_), myScope(scope), Node(NodeType::BLOCK) {}
	BlockNode(const BlockNode &other): stmts(other.stmts), myScope(other.myScope), Node(NodeType::BLOCK) {}

	void AddStmt(std::shared_ptr<Node> stmt){ stmts.push_back(stmt); }
	llvm::Value *Codegen() override;
};
struct FuncDeclNode: public Node{
	const VarType *funcType;
	Token ident;
	std::vector<std::shared_ptr<VarDeclNode>> params;
	std::shared_ptr<Node> block;

	FuncDeclNode(const VarType *funcType_, const Token &ident_, const std::vector<std::shared_ptr<VarDeclNode>> &params_, std::shared_ptr<Node> block_)
		:funcType(funcType_), ident(ident_), params(params_), block(block_), Node(NodeType::FUNCDECL) {}

	llvm::Value *Codegen() override;
};
struct ReturnNode: public Node{
	std::shared_ptr<Node> expr;

	ReturnNode(std::shared_ptr<Node> expr_): expr(expr_), Node(NodeType::RETURN) {}

	llvm::Value *Codegen() override;
};
struct IfNode: public Node{
	std::shared_ptr<Node> cond;
	std::shared_ptr<Node> then;
	std::shared_ptr<Node> elseBody;

	explicit IfNode(): Node(NodeType::IF) {}
	IfNode(std::shared_ptr<Node> cond_, std::shared_ptr<Node> then_, std::shared_ptr<Node> elseBody_)
		:cond(cond_), then(then_), elseBody(elseBody_), Node(NodeType::IF) {}

	llvm::Value *Codegen() override;
};
struct WhileNode: public Node{
	std::shared_ptr<Node> cond, then;

	WhileNode(std::shared_ptr<Node> cond_, std::shared_ptr<Node> then_)
		:cond(cond_), then(then_), Node(NodeType::WHILE) {}

	llvm::Value *Codegen() override;
};
struct VarAssignNode: public Node{
	Token varName;
	std::shared_ptr<Node> expression;

	VarAssignNode(const Token &varName_, std::shared_ptr<Node> expression_)
		:varName(varName_), expression(expression_), Node(NodeType::VARASSIGN) {}

	llvm::Value *Codegen() override;
};
struct FuncCallNode: public Node{
	Token funcName;
	std::vector<std::shared_ptr<Node>> params;

	FuncCallNode(const Token &funcName_, const std::vector<std::shared_ptr<Node>> &params_)
		:funcName(funcName_), params(params_), Node(NodeType::FUNCTIONCALL) {}

	llvm::Value *Codegen() override;
};
struct MemberNode: public Node{
	Member member;
	MemberNode(Member member_):member(member_), Node(NodeType::MEMBER) {}

	llvm::Value *Codegen() override;
};

class Parser{
	public:
	private:
	friend class Log;
	std::shared_ptr<BlockNode> rootNode;

	Tokenizer &tokenizer;
	Token currTok;
	std::shared_ptr<Scope> currScope;

	Token NextToken(){
		Token ret = currTok;
		currTok = tokenizer.NextToken();
		return ret;
	}

	std::shared_ptr<Node> ParseIf();
	std::shared_ptr<Node> ParseBlock();
	std::shared_ptr<Node> ParsePrimary();
	std::shared_ptr<Node> ParseFuncDecl(const VarType &type, const Token &name);
	std::shared_ptr<Node> ParseParam();
	std::shared_ptr<Node> ParseVarDecl();
	std::shared_ptr<Node> ParseExpr(int parentPrecedence = 0);
	std::shared_ptr<Node> ParseStmt();

	std::string fileName;

	void ParseStructdecl();
	public:
	Parser(Tokenizer &tok, const std::string &fileName_);

	void Parse();
	std::string GenerateCode() const;
	const Node *GetRoot() { return rootNode.get(); }
	
	Scope::Variable &FindIdent(const Token &name) const;
	const VarType &FindType(const Token &name) const;
	const std::shared_ptr<Scope> &GetScope() const { return currScope; }
	void SetScope(std::shared_ptr<Scope> scope) { currScope = scope; }
};