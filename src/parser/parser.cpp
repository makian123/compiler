#include "parser.hpp"
#include <algorithm>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include "util/logger.hpp"

const VarType VarType::ERROR = VarType();
static std::pair<Token, VarType> EmptyName;
static bool parsingParams = false;

static int Precedence(const Token &tok){
	switch(tok.type){
		case Token::Type::DOT:
			return 16;
		case Token::Type::NOT:
			return 15;
		case Token::Type::STAR:
		case Token::Type::SLASH:
			return 13;
		case Token::Type::PLUS:
		case Token::Type::MINUS:
			return 12;
		case Token::Type::GREATER:
		case Token::Type::GEQ:
		case Token::Type::LESS:
		case Token::Type::LEQ:
			return 9;
		case Token::Type::EQ:
		case Token::Type::NEQ:
			return 8;
		case Token::Type::ASSIGN:
		case Token::Type::ADDASSIGN:
		case Token::Type::SUBASSIGN:
		case Token::Type::MULTASSIGN:
		case Token::Type::DIVASSIGN:
			return 2;
		case Token::Type::COMMA:
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
		case NodeType::MEMBER:
			ret += Indent(depth) + "MEMBER:\n";
			ret += Indent(depth + 1) + static_cast<MemberNode&>(node).member.name;
			break;
		case NodeType::RETURN:
			ret += Indent(depth) + "RETURN:\n";
			if(static_cast<ReturnNode&>(node).expr->type == NodeType::ERR)
				ret += Indent(depth + 1) + "VOID";
			else
				ret += GetCode(*static_cast<ReturnNode&>(node).expr.get(), depth + 1);
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
			if(static_cast<VarDeclNode&>(node).initial->type == NodeType::ERR)
				ret +=  Indent(depth + 2) + "VOID";
			else
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
		case NodeType::IF:
			ret += Indent(depth) + "IF:\n";
			ret += Indent(depth + 1) + "Cond:\n";
			ret += GetCode(*static_cast<IfNode&>(node).cond.get(), depth + 2) + "\n";
			ret += Indent(depth + 1) + "Then:\n";
			ret += GetCode(*static_cast<IfNode&>(node).then.get(), depth + 2);

			if(static_cast<IfNode&>(node).elseBody->type != NodeType::ERR){
				ret += "\n" + Indent(depth + 1) + "Else:\n";
				ret += GetCode(*static_cast<IfNode&>(node).elseBody.get(), depth + 2);
			}
			break;
	}

	return ret;
}
std::string Parser::GenerateCode() const{
	return GetCode(*rootNode.get());
}

std::pair<Token, VarType> &Parser::FindIdent(const Token &name) const{
	std::shared_ptr<Scope> currentScope = currScope;
	while(currentScope){
		auto foundPos = std::find_if(
			currentScope->identifiers.begin(), 
			currentScope->identifiers.end(), 
			[name](const auto &pair) {
				return pair.first.val == name.val;
			}
		);
		if(foundPos != currentScope->identifiers.end())
			return *foundPos;

		currentScope = currentScope->parent;
	}

	return EmptyName;
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
		if(currTok.type == Token::Type::TEOF || currTok.type == Token::Type::ERR) break;
		
		auto node = ParseStmt();
		if(node->type != NodeType::ERR)
			rootNode->AddStmt(node);
	}
}

std::shared_ptr<Node> Parser::ParseBlock(){
	if(currTok.type != Token::Type::OPEN_BRACKET){
		auto stmt = ParseStmt();
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

	currScope->types.push_back(VarType(VarType::Type::STRUCT, structName.val, offset, nullptr, members, false, false, 0));
}
std::shared_ptr<Node> Parser::ParseStmt(){
	std::shared_ptr<Node> ret = std::make_shared<Node>();

	//Variable or function decl
	if(FindType(currTok).type != VarType::Type::ERR){
		ret = ParseVarDecl();
		NextToken();	//Semicolon
	}
	else if(currTok.type == Token::Type::TYPE_STRUCT){
		ParseStructdecl();
		NextToken();	//Semicolon
	}
	//Variable stmt
	else if(currTok.type == Token::Type::IF){
		ret = ParseIf();
	}
	else if(currTok.type == Token::Type::RETURN){
		NextToken();
		ret = std::make_shared<ReturnNode>(ParseExpr());
		NextToken();	//Semicolon
	}
	else if(currTok.type == Token::Type::IDENT){
		//TODO: make it work
	}
	
	return ret;
}
std::shared_ptr<Node> Parser::ParseFuncDecl(const VarType &funcType, const Token &name){
	if(currTok.type != Token::Type::OPEN_PARENTH || funcType.type == VarType::Type::ERR || name.type == Token::Type::ERR) return std::make_shared<Node>();
	NextToken();

	std::vector<std::shared_ptr<VarDeclNode>> params;
	std::shared_ptr<Node> block;
	
	currScope->scopes.push_back(std::make_shared<Scope>());
	currScope->scopes.back()->parent = currScope;
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
		currScope->identifiers.emplace_back(varName, found);

		if(currTok.type == Token::Type::COMMA || currTok.type == Token::Type::CLOSED_PARENTH)
			return std::make_shared<VarDeclNode>(&found, varName, std::make_shared<Node>());
		
		if(currTok.type == Token::Type::EQ){
			NextToken();
			return std::make_shared<VarDeclNode>(&found, varName, ParseExpr());
		}
	}

	return std::make_shared<Node>();
}
std::shared_ptr<Node> Parser::ParseIf(){
	std::shared_ptr<Node> ret = std::make_shared<Node>();
	if(currTok.type == Token::Type::IF){
		NextToken();

		if(currTok.type != Token::Type::OPEN_PARENTH) return ret;
		NextToken();

		auto cond = ParseExpr();
		if(cond->type == NodeType::ERR) return ret;

		NextToken();
	
		currScope->scopes.push_back(std::make_shared<Scope>());
		currScope->scopes.back()->parent = currScope;
		currScope = currScope->scopes.back();
		auto then = ParseBlock();
		currScope = currScope->parent;
		if(then->type == NodeType::ERR) {
			currScope->scopes.pop_back();
			return ret;
		}

		std::shared_ptr<Node> elseBody = std::make_shared<Node>();
		if(currTok.type == Token::Type::ELSE){
			NextToken();

			currScope->scopes.push_back(std::make_shared<Scope>());
			currScope->scopes.back()->parent = currScope;
			currScope = currScope->scopes.back();
			elseBody = ParseBlock();
			currScope = currScope->parent;

			if(elseBody->type == NodeType::ERR) {
				currScope->scopes.pop_back();
			}
		}

		ret = std::make_shared<IfNode>(cond, then, elseBody);
	}

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
	else if(currTok.type == Token::Type::IDENT){
		auto tmpName = NextToken();
		auto type = FindIdent(tmpName).second;
		if(type.type == VarType::Type::ERR){
			Log::Error(*this, "Variable '", tmpName.val, "' not found\n");
		}

		while(true){
			if(currTok.type != Token::Type::DOT && currTok.type != Token::Type::DEREFERENCE) break;

			NextToken();
			if(currTok.type != Token::Type::IDENT){
				Log::Error(*this, "Invalid member specified\n");
			}
			auto &tok = currTok;

			auto member = std::find_if(type.members.begin(), type.members.end(), 
				[&tok](const Member &memb){
					return memb.name == tok.val;
				}
			);
			if(member == type.members.end()){
				Log::Error(*this, "Member '", currTok.val, "' not found\n");
			}

			NextToken();
			if(currTok.type != Token::Type::DOT && currTok.type != Token::Type::DEREFERENCE) 
				return std::make_shared<MemberNode>(*member);

			type = *member->type;
		}

		return std::make_shared<ValNode>(tmpName);
	}
	else if(currTok.type == Token::Type::OPEN_PARENTH){
		NextToken();
		ret = ParseExpr();
	}
	return ret;
}
std::shared_ptr<Node> Parser::ParseVarDecl(){
	Token typeName = currTok;
	auto &found = FindType(typeName);

	if(found.type != VarType::Type::ERR){
		NextToken();

		Token varName = NextToken();
		currScope->identifiers.emplace_back(varName, found);

		if(currTok.type == Token::Type::SEMICOLON)
			return std::make_shared<VarDeclNode>(&found, varName, std::make_shared<Node>());
		
		if(currTok.type == Token::Type::ASSIGN){
			NextToken();
			return std::make_shared<VarDeclNode>(&found, varName, ParseExpr());
		}

		if(currTok.type == Token::Type::OPEN_PARENTH){
			return ParseFuncDecl(found, varName);
		}
	}
	Log::Error(*this, "Type ", currTok.val, " not found");
	return std::make_shared<Node>();
}

Parser::Parser(Tokenizer &tok): tokenizer(tok) {
	primitives[Token::Type::TYPE_VOID] = VarType(VarType::Type::VOID, std::string(), 0, nullptr, std::vector<Member>(), false, false, 0);
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