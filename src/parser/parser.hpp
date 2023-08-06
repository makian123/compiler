#pragma once

#include <memory>
#include "tokenizer/tokenizer.hpp"

enum class NodeType{
	ERR,

	VAL,
	BINARY,
	VARDECL
};
struct Node{
	NodeType type;

	Node(NodeType type = NodeType::ERR): type(type) {}
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

class Parser{
	private:
	Tokenizer &tokenizer;
	Token currTok;

	Token NextToken(){
		currTok = tokenizer.NextToken();
		
		return currTok;
	}

	std::shared_ptr<Node> ParseVal();
	std::shared_ptr<Node> ParseBinary();
	std::shared_ptr<Node> ParseVarDecl();
	public:
	Parser(Tokenizer &tok): tokenizer(tok) {}
	std::shared_ptr<Node> Parse();
};