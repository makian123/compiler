#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <iostream>
#include "tokenizer/tokenizer.hpp"

struct VarType;
struct Member{
	VarType *type;
	std::string name;
	size_t offset;

	Member(VarType *type_, const std::string &name_, size_t offset_):type(type_), name(name_), offset(offset_) {}
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
	VarType(Type type_, std::string name_, size_t typeSz_, VarType *baseType_, const std::vector<Member> &members_, bool isUnsigned_, bool isArray_, size_t arrSize_)
		:type(type_), name(name_), typeSz(typeSz_), baseType(baseType_), members(members_), isUnsigned(isUnsigned_), isArray(isArray_), arrSize(arrSize_){}
	VarType(const VarType &other)
		:type(other.type), name(other.name), typeSz(other.typeSz), baseType(other.baseType), members(other.members), isUnsigned(other.isUnsigned), isArray(other.isArray), arrSize(other.arrSize){}

	static VarType ERROR;
};

enum class NodeType{
	ERR,

	VAL,
	BINARY,
	VARDECL,
	BLOCK
};
struct Node{
	NodeType type;

	explicit Node(NodeType type = NodeType::ERR): type(type) {}
	virtual ~Node() = default;
};
struct ValNode: public Node {
	Token val;

	ValNode(const Token &tok): val(tok), Node(NodeType::VAL) {}
};
struct BinaryNode: public Node {
	std::shared_ptr<Node> lhs, rhs;
	Token operand;

	BinaryNode(std::shared_ptr<Node> lhs_, const Token &operand_, std::shared_ptr<Node> rhs_)
		: lhs(lhs_), operand(operand_), rhs(rhs_), Node(NodeType::BINARY) {}
};
struct VarDeclNode: public Node {
	VarType varType;
	Token ident;
	std::shared_ptr<Node> initial;

	VarDeclNode(VarType varType_, Token ident_, std::shared_ptr<Node> init): varType(varType_), ident(ident_), initial(init), Node(NodeType::VARDECL) {}
};
struct BlockNode: public Node {
	std::vector<std::shared_ptr<Node>> stmts;

	BlockNode(const std::vector<std::shared_ptr<Node>> &stmts_): stmts(stmts_), Node(NodeType::BLOCK) {}
	void AddStmt(std::shared_ptr<Node> stmt){
		stmts.push_back(stmt);
	}
};

class Parser{
	private:
	std::vector<VarType> types;
	std::vector<Token> identifiers;
	std::shared_ptr<BlockNode> rootNode;

	Tokenizer &tokenizer;
	Token currTok;

	Token NextToken(){
		Token ret = currTok;
		currTok = tokenizer.NextToken();
		return ret;
	}

	std::shared_ptr<Node> ParsePrimary();
	std::shared_ptr<Node> ParseVarDecl();
	std::shared_ptr<Node> ParseExpr(int parentPrecedence = 0);
	std::shared_ptr<Node> ParseStmt();

	void ParseStructdecl();
	VarType &FindType(Token name);

	public:
	Parser(Tokenizer &tok);
	void Parse();
	std::string GenerateCode();
};