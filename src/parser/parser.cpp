#include "parser.hpp"
#include <algorithm>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <sstream>

const VarType VarType::ERROR = VarType();
static bool parsingParams = false;

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

static inline std::string Indent(int depth){
	std::stringstream ss;
	for(int i = 0; i < depth; ++i)
		ss << "\t";

	return ss.str();
}

static std::string GetCode(Node &node, int depth = 0){
	std::string ret = "";

	switch(node.type){
		case NodeType::BLOCK:
			ret += Indent(depth) + "BLOCK:\n";
			for(auto &node_: static_cast<BlockNode&>(node).stmts)
				ret += GetCode(*node_.get(), depth + 1) + "\n";
			if(static_cast<BlockNode&>(node).stmts.size() > 0) ret.erase(ret.end() - 1);
			break;
		case NodeType::VAL:
			ret += Indent(depth) + static_cast<ValNode&>(node).val.val;
			break;
		case NodeType::BINARY:
			ret += Indent(depth) + "BINARY:\n";
			ret += Indent(depth + 1) + "LHS:\n" + GetCode(*static_cast<BinaryNode&>(node).lhs.get(), depth + 2) + "\n";
			ret += Indent(depth + 1) + "OPERAND: " + static_cast<BinaryNode&>(node).operand.val + "\n";
			ret += Indent(depth + 1) + "RHS:\n" + GetCode(*static_cast<BinaryNode&>(node).rhs.get(), depth + 2);
			break;
		case NodeType::VARDECL:
			ret += Indent(depth) + "VAR:\n";
			ret += Indent(depth + 1) + "Name: " + static_cast<VarDeclNode&>(node).ident.val + "\n";
			ret += Indent(depth + 1) + "Val:\n";
			ret += GetCode(*static_cast<VarDeclNode&>(node).initial.get(), depth + 2);
			break;
		case NodeType::FUNCDECL:
			ret += Indent(depth) + "FUNC:\n";
			ret += Indent(depth + 1) + "Name: " + static_cast<FuncDeclNode&>(node).ident.val + "\n";
			ret += Indent(depth + 1) + "Params:\n";
			for(auto &param:  static_cast<FuncDeclNode&>(node).params){
				ret += GetCode(*param.get(), depth + 2) + "\n";
			}
			
			ret += Indent(depth + 1) + "Body:\n";
			ret += GetCode(*static_cast<FuncDeclNode&>(node).block.get(), depth + 2);
			break;
	}

	return ret;
}
std::string Parser::GenerateCode() const{
	return GetCode(*rootNode.get());
}

const Token &Parser::FindIdent(const Token &name) const{
	std::shared_ptr<Scope> currentScope = currScope;
	while(currentScope){
		auto foundPos = std::find_if(
			currentScope->identifiers.begin(), 
			currentScope->identifiers.end(), 
			[name](const Token &type) {
				return type.val == name.val;
			}
		);
		if(foundPos != currentScope->identifiers.end())
			return *foundPos;

		currentScope = currentScope->parent;
	}

	return Token::ERROR;
}
const VarType &Parser::FindType(const Token &toFind) const{
	if(primitives.contains(toFind.type)){
		return primitives.at(toFind.type);
	}

	std::shared_ptr<Scope> currentScope = currScope;
	while(currentScope){
		auto foundPos = std::find_if(
			currentScope->types.begin(), 
			currentScope->types.end(), 
			[toFind](const VarType &type) {
				return type.name == toFind.val;
			}
		);
		if(foundPos != currentScope->types.end())
			return *foundPos;

		currentScope = currentScope->parent;
	}

	return VarType::ERROR;
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

std::shared_ptr<Node> Parser::ParseBlock(){
	if(currTok.type != Token::Type::OPEN_BRACKET){
		auto stmt = ParseStmt();
		NextToken(); //Semicolon
		return stmt;
	}
	
	std::shared_ptr<BlockNode> ret = std::make_shared<BlockNode>();
	NextToken();

	while(true){
		auto tmp = ParseStmt();
		if(tmp->type == NodeType::ERR) break;

		ret->AddStmt(tmp);
		if(currTok.type == Token::Type::CLOSED_BRACKET){
			NextToken();
			return ret;
		}
	}

	return std::make_shared<Node>();
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

			members.emplace_back(&currType, name.val, offset);
			offset += currType.typeSz;
			
			if(delimiter.type == Token::Type::SEMICOLON) break;
		}
	}
	NextToken();	//Semicolon

	currScope->types.push_back(VarType(VarType::Type::STRUCT, structName.val, offset, nullptr, members, false, false, 0));
}
std::shared_ptr<Node> Parser::ParseStmt(){
	std::shared_ptr<Node> ret = std::make_shared<Node>();
	if(FindType(currTok).type != VarType::Type::ERR){
		ret = ParseVarDecl();
	}
	NextToken();	//Semicolon
	return ret;
}
std::shared_ptr<Node> Parser::ParseExpr(int parentPrecedence){
	auto left = ParsePrimary();

	while(true){
		auto precedence = Precedence(currTok);
		if(precedence == 0 || precedence <= parentPrecedence)
			break;

		Token operand = NextToken();
		auto right = ParseExpr(precedence);
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
std::shared_ptr<Node> Parser::ParseFuncDecl(const VarType &funcType, const Token &name){
	if(currTok.type != Token::Type::OPEN_PARENTH || funcType.type == VarType::Type::ERR || name.type == Token::Type::ERR) return std::make_shared<Node>();
	NextToken();

	std::vector<std::shared_ptr<VarDeclNode>> params;
	std::shared_ptr<Node> block;
	
	currScope->scopes.push_back(std::make_shared<Scope>());
	currScope = currScope->scopes.back();
	while(true){
		auto param = ParseParam();
		if(param->type != NodeType::VARDECL) break;

		params.push_back(std::dynamic_pointer_cast<VarDeclNode>(param));
		NextToken();
	}
	if(!params.size()) NextToken(); //For case when ) is left
	block = ParseBlock();
	currScope = currScope->parent;

	if(block->type == NodeType::ERR)
		return std::make_shared<Node>();
	
	return std::make_shared<FuncDeclNode>(&funcType, name, params, block);
}
std::shared_ptr<Node> Parser::ParseParam(){
	Token typeName = currTok;
	auto &found = FindType(typeName);

	if(found.type != VarType::Type::ERR){
		NextToken();
		Token varName = NextToken();
		currScope->identifiers.push_back(varName);

		if(currTok.type == Token::Type::COMMA || currTok.type == Token::Type::CLOSED_PARENTH)
			return std::make_shared<VarDeclNode>(&found, varName, std::make_shared<Node>());
		
		if(currTok.type == Token::Type::EQ){
			NextToken();
			return std::make_shared<VarDeclNode>(&found, varName, ParseExpr());
		}
	}

	return std::make_shared<Node>();
}
std::shared_ptr<Node> Parser::ParseVarDecl(){
	Token typeName = currTok;
	auto &found = FindType(typeName);

	if(found.type != VarType::Type::ERR){
		NextToken();

		Token varName = NextToken();
		currScope->identifiers.push_back(varName);

		if(currTok.type == Token::Type::SEMICOLON)
			return std::make_shared<VarDeclNode>(&found, varName, std::make_shared<Node>());
		
		if(currTok.type == Token::Type::EQ){
			NextToken();
			return std::make_shared<VarDeclNode>(&found, varName, ParseExpr());
		}

		if(currTok.type == Token::Type::OPEN_PARENTH){
			return ParseFuncDecl(found, varName);
		}
	}
	return std::make_shared<Node>();
}

Parser::Parser(Tokenizer &tok): tokenizer(tok) {
	primitives[Token::Type::TYPE_VOID] = VarType(VarType::Type::VOID, std::string(), 1, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_CHAR] = VarType(VarType::Type::CHAR, std::string(), 1, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_SHORT] = VarType(VarType::Type::SHORT, std::string(), 2, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_INT] = VarType(VarType::Type::INT, std::string(), 4, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_LONG] = VarType(VarType::Type::LONG, std::string(), 8, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_FLOAT] = VarType(VarType::Type::FLOAT, std::string(), 4, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_DOUBLE] = VarType(VarType::Type::DOUBLE, std::string(), 8, nullptr, std::vector<Member>(), false, false, 0);

	currTok = tokenizer.NextToken();
	rootNode = std::make_shared<BlockNode>(std::vector<std::shared_ptr<Node>>());
	currScope = std::make_shared<Scope>();
}