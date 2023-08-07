#include "parser.hpp"
#include <algorithm>
#include <unordered_map>
#include <stdexcept>
#include <iostream>

VarType VarType::ERROR = VarType();

static int Precedence(const Token &tok){
	switch(tok.type){
		case Token::Type::STAR:
		case Token::Type::SLASH:
			return 2;
		case Token::Type::PLUS:
		case Token::Type::MINUS:
			return 1;
		default:
			return 0;
	}
}

static std::unordered_map<Token::Type, VarType> primitives;

std::string Parser::GetCode(Node &node){
	std::string ret = "";

	switch(node.type){
		case NodeType::BLOCK:
			for(auto &node_: static_cast<BlockNode&>(node).stmts)
				ret += GetCode(*node_.get()) + "\n";
			break;
	}

	return ret;
}
std::string Parser::GenerateCode(){
	return GetCode(*rootNode.get());
}

VarType &Parser::FindType(Token toFind){
	if(primitives.contains(toFind.type)){
		return primitives.at(toFind.type);
	}

	auto foundPos = std::find_if(types.begin(), types.end(), [toFind](VarType &type) {
		return type.name == toFind.val;
	});

	if(foundPos == types.end()) return VarType::ERROR;
	return *foundPos;
}

void Parser::Parse(){
	while(true){
		if(currTok.type == Token::Type::TEOF) break;
		if(currTok.type == Token::Type::TYPE_STRUCT) {
			ParseStructdecl();
			continue;
		}
		
		rootNode->AddStmt(ParseStmt());
	}
}

void Parser::ParseStructdecl(){
	if(currTok.type != Token::Type::TYPE_STRUCT) return;
	NextToken();

	Token structName = NextToken();

	Token tmp = NextToken();
	if(tmp.type != Token::Type::OPEN_BRACKET) return;
	
	std::vector<Member> members;
	size_t offset = 0;
	while(true){
		auto &currType = FindType(NextToken());
		if(currType.type == VarType::Type::ERR) break;
		
		while(true){
			Token name = NextToken();
			auto delimiter = NextToken();

			members.emplace_back(currType, name.val, offset);
			offset += currType.typeSz;
			
			if(delimiter.type == Token::Type::SEMICOLON) break;
		}
	}
	NextToken();	//Semicolon

	types.push_back(VarType(VarType::Type::STRUCT, structName.val, offset, nullptr, members, false, false, 0));
}
std::shared_ptr<Node> Parser::ParseStmt(){
	if(FindType(currTok).type != VarType::Type::ERR){
		return ParseVarDecl();
	}

	return std::make_shared<Node>();
}
std::shared_ptr<Node> Parser::ParseExpr(int parentPrecedence){
	auto left = ParsePrimary();

	while(true){
		auto precedence = Precedence(currTok);
		if(precedence == 0 || precedence <= parentPrecedence)
			break;

		Token operand = NextToken();
		auto right = ParseExpr();
		left = std::make_shared<BinaryNode>(left, operand, right);
	}

	return left;
}
std::shared_ptr<Node> Parser::ParsePrimary(){
	std::shared_ptr<Node> ret = std::make_shared<Node>();
	if((int)currTok.type >= (int)Token::Type::VALUES_BEGIN && (int)currTok.type <= (int)Token::Type::VALUES_END) {
		ret = std::make_shared<ValNode>(NextToken());
	}
	else if(currTok.type == Token::Type::OPEN_PARENTH){
		NextToken();
		ret = ParseExpr();
	}
	return ret;
}
std::shared_ptr<Node> Parser::ParseVarDecl(){
	Token typeName = NextToken();

	auto found = FindType(typeName);
	if(found.type != VarType::Type::ERR){
		Token varName = NextToken();
		identifiers.push_back(varName);

		if(currTok.type == Token::Type::SEMICOLON)
			return std::make_shared<VarDeclNode>(found, varName, std::make_shared<Node>());
		
		return std::make_shared<VarDeclNode>(found, varName, ParseExpr());
	}
	return std::make_shared<Node>();
}

Parser::Parser(Tokenizer &tok): tokenizer(tok) {
	primitives[Token::Type::TYPE_CHAR] = VarType(VarType::Type::CHAR, std::string(), 1, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_SHORT] = VarType(VarType::Type::SHORT, std::string(), 2, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_INT] = VarType(VarType::Type::INT, std::string(), 4, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_LONG] = VarType(VarType::Type::LONG, std::string(), 8, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_FLOAT] = VarType(VarType::Type::FLOAT, std::string(), 4, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_DOUBLE] = VarType(VarType::Type::DOUBLE, std::string(), 8, nullptr, std::vector<Member>(), false, false, 0);

	currTok = tokenizer.NextToken();
	rootNode = std::make_shared<BlockNode>(std::vector<std::shared_ptr<Node>>());
}