#include "parser.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

#include "util/logger.hpp"

static std::unique_ptr<llvm::LLVMContext> context;
static std::unique_ptr<llvm::IRBuilder<>> builder;
static std::unique_ptr<llvm::Module> module;
static llvm::BasicBlock *currentScope = nullptr;
static llvm::Function *currFunc = nullptr;
static Parser *currParser = nullptr;

const VarType VarType::ERROR = VarType();
static Scope::Variable EmptyName;
static bool parsingParams = false;
static bool parsingCond = false;

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
		case NodeType::VARASSIGN:
			ret += Indent(depth) + "ASSIGN:\n";
			ret += Indent(depth + 1) + static_cast<VarAssignNode&>(node).varName.val + "\n";
			ret += Indent(depth + 1) + "VALUE:\n";
			ret += GetCode(*static_cast<VarAssignNode&>(node).expression.get(), depth + 2) + "\n";
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
		case NodeType::WHILE:
			ret += Indent(depth) + "WHILE:\n";
			ret += Indent(depth + 1) + "COND:\n";
			ret += GetCode(*static_cast<WhileNode&>(node).cond.get(), depth + 2) + "\n";
			ret += Indent(depth + 1) + "THEN:\n";
			ret += GetCode(*static_cast<WhileNode&>(node).then.get(), depth + 2);
			break;
	}

	return ret;
}
std::string Parser::GenerateCode() const{
	return GetCode(*rootNode.get());
}

Scope::Variable &Parser::FindIdent(const Token &name) const{
	std::shared_ptr<Scope> currentScope = currScope;
	while(currentScope){
		auto foundPos = std::find_if(
			currentScope->identifiers.begin(), 
			currentScope->identifiers.end(), 
			[name](const Scope::Variable &var) {
				return var.ident.val == name.val;
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

static std::string GetIR() {
	std::string module_str;
	llvm::raw_string_ostream ostream{module_str};
	module->print(ostream, nullptr, false);
	return module_str;
}
void Parser::Parse(){
	context = std::make_unique<llvm::LLVMContext>();
	module = std::make_unique<llvm::Module>(fileName, *context);
	builder = std::make_unique<llvm::IRBuilder<>>(*context);

	currParser = this;

	while(true){
		if(currTok.type == Token::Type::TEOF || currTok.type == Token::Type::ERR) break;
		
		auto node = ParseStmt();
		if(node->type != NodeType::ERR)
			rootNode->AddStmt(node);
	}

	auto tmp = rootNode->Codegen();

	module->print(llvm::errs(), nullptr);
	currParser = nullptr;
}

std::shared_ptr<Node> Parser::ParseBlock(){
	if(currTok.type != Token::Type::OPEN_BRACKET){
		auto stmt = ParseStmt();
		return stmt;
	}
	
	auto ret = std::make_shared<BlockNode>(std::vector<std::shared_ptr<Node>>(), currScope);
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
	if(tmp.type != Token::Type::OPEN_BRACKET){
		Log::Error(*this, "Missing (");
	}
	
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

	if(FindType(currTok).type != VarType::Type::ERR){
		ret = ParseVarDecl();
		NextToken();
		return ret;
	}
	else if(currTok.type == Token::Type::TYPE_STRUCT){
		ParseStructdecl();
		NextToken();	//Semicolon
	}
	else if(currTok.type == Token::Type::IF){
		return ParseIf();
	}
	else if(currTok.type == Token::Type::RETURN){
		NextToken();
		ret = std::make_shared<ReturnNode>(ParseExpr());
		NextToken();	//Semicolon

		return ret;
	}
	else if(currTok.type == Token::Type::WHILE){
		NextToken();
		if(currTok.type != Token::Type::OPEN_PARENTH){
			Log::Error(*this, "Missing (");
		}

		NextToken();
		auto cond = ParseExpr();
		NextToken();

		std::shared_ptr<Node> then = std::make_shared<Node>();
		if(currTok.type != Token::Type::OPEN_BRACKET){
			then = ParseStmt();
		}
		else{
			currScope->scopes.push_back(std::make_shared<Scope>());
			currScope->scopes.back()->parent = currScope;
			currScope = currScope->scopes.back();

			then = ParseBlock();
			
			currScope = currScope->parent;
		}
		
		return std::make_shared<WhileNode>(cond, then);
	}
	else if(currTok.type == Token::Type::IDENT){
		auto varName = NextToken();
		auto expr = std::make_shared<Node>();
		if(currTok.type == Token::Type::ASSIGN){
			NextToken();
			expr = ParseExpr();
		}
		NextToken();

		return std::make_shared<VarAssignNode>(varName, expr);
	}
	
	NextToken();
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
		currScope->identifiers.emplace_back(varName, found, nullptr);

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

		if(currTok.type != Token::Type::OPEN_PARENTH) {
			Log::Error(*this, "Missing (");
		}
		NextToken();

		auto cond = ParseExpr();
		if(cond->type == NodeType::ERR) {
			Log::Error(*this, "Invalid expression");
		}

		NextToken();
	
		currScope->scopes.push_back(std::make_shared<Scope>());
		currScope->scopes.back()->parent = currScope;
		currScope = currScope->scopes.back();
		auto then = ParseBlock();
		currScope = currScope->parent;
		if(then->type == NodeType::ERR) {
			currScope->scopes.pop_back();
			Log::Error(*this, "Invalid block");
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
std::shared_ptr<Node> Parser::ParseVarDecl(){
	Token typeName = currTok;
	auto &found = FindType(typeName);

	if(found.type != VarType::Type::ERR){
		NextToken();

		Token varName = NextToken();
		currScope->identifiers.emplace_back(varName, found, nullptr);

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
		auto type = FindIdent(tmpName).type;
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

Parser::Parser(Tokenizer &tok, const std::string &fileName_): tokenizer(tok), fileName(fileName_) {
	primitives[Token::Type::TYPE_VOID] = VarType(VarType::Type::VOID, std::string(), 0, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_CHAR] = VarType(VarType::Type::CHAR, std::string(), 1, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_SHORT] = VarType(VarType::Type::SHORT, std::string(), 2, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_INT] = VarType(VarType::Type::INT, std::string(), 4, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_LONG] = VarType(VarType::Type::LONG, std::string(), 8, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_FLOAT] = VarType(VarType::Type::FLOAT, std::string(), 4, nullptr, std::vector<Member>(), false, false, 0);
	primitives[Token::Type::TYPE_DOUBLE] = VarType(VarType::Type::DOUBLE, std::string(), 8, nullptr, std::vector<Member>(), false, false, 0);

	currTok = tokenizer.NextToken();
	currScope = std::make_shared<Scope>();
	rootNode = std::make_shared<BlockNode>(std::vector<std::shared_ptr<Node>>(), currScope);
}

VarType::VarType(
	Type type_, 
	std::string name_, 
	size_t typeSz_, 
	VarType *baseType_, 
	const std::vector<Member> &members_, 
	bool isUnsigned_,
	bool isArray_, 
	size_t arrSize_)
	:type(type_), name(name_), typeSz(typeSz_),
	baseType(baseType_), members(members_), isUnsigned(isUnsigned_), 
	isArray(isArray_), arrSize(arrSize_){}
VarType::VarType(const VarType &other)
		:type(other.type), name(other.name), typeSz(other.typeSz), 
		baseType(other.baseType), members(other.members), isUnsigned(other.isUnsigned), 
		isArray(other.isArray), arrSize(other.arrSize){}

llvm::Type *VarType::Codegen() const {
	if(type == Type::PTR){
		switch(type){
			case Type::VOID:
				return llvm::Type::getInt64Ty(*context);
			case Type::CHAR:
				return llvm::Type::getInt8PtrTy(*context);
			case Type::SHORT:
				return llvm::Type::getInt16PtrTy(*context);
			case Type::INT:
				return llvm::Type::getInt32PtrTy(*context);
			case Type::LONG:
				return llvm::Type::getInt64PtrTy(*context);
			case Type::FLOAT:
				return llvm::Type::getFloatPtrTy(*context);
			case Type::DOUBLE:
				return llvm::Type::getDoublePtrTy(*context);
		}
	}
	else {
		switch(type){
			case Type::VOID:
				return llvm::Type::getVoidTy(*context);
			case Type::CHAR:
				return llvm::Type::getInt8Ty(*context);
			case Type::SHORT:
				return llvm::Type::getInt16Ty(*context);
			case Type::INT:
				return llvm::Type::getInt32Ty(*context);
			case Type::LONG:
				return llvm::Type::getInt64Ty(*context);
			case Type::FLOAT:
				return llvm::Type::getFloatTy(*context);
			case Type::DOUBLE:
				return llvm::Type::getDoubleTy(*context);
		}
	}

	std::cerr << "Type not found\n";
	return nullptr;
}
llvm::Value *ValNode::Codegen() {
	if((int)val.type >= (int)Token::Type::VALUES_BEGIN && (int)val.type <= (int)Token::Type::VALUES_END){
		switch(val.type){
			case Token::Type::INTEGER_NUMBER:
				return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), val.val, 10);
			case Token::Type::FLOATING_NUMBER:
				return llvm::ConstantFP::get(*context, llvm::APFloat(std::stod(val.val)));
			default:
				return nullptr;
		}
	}

	auto ident = currParser->FindIdent(val);
	if(ident.type.type == VarType::Type::ERR) 
		std::cerr << "Invalid variable referenced\n";
	return ident.val;
}
llvm::Value *BinaryNode::Codegen() {
	auto l = lhs->Codegen();
	auto r = rhs->Codegen();

	if(!l || !r) return nullptr;

	switch(operand.type){
		case Token::Type::PLUS:
			return builder->CreateFAdd(l, r, "addtmp");
		case Token::Type::MINUS:
			return builder->CreateFSub(l, r, "subtemp");
		case Token::Type::STAR:
			return builder->CreateFMul(l, r, "multemp");
		case Token::Type::SLASH:
			return builder->CreateFDiv(l, r, "divtemp");
		case Token::Type::GREATER:
		case Token::Type::GEQ:
		case Token::Type::LESS:
		case Token::Type::LEQ:
			return builder->CreateUIToFP(l, llvm::Type::getDoubleTy(*context), "booltmp");
		default:
			std::cerr << "Invalid operantor\n";
			return nullptr;
	}
}
llvm::Value *VarDeclNode::Codegen() {
	auto &ident = currParser->FindIdent(this->ident);
	llvm::Value *toRet = nullptr;
	if(varType->isArray){
		toRet = builder->CreateAlloca(
			varType->Codegen(),
			0, 
			llvm::ConstantInt::get(*context, llvm::APInt(64, varType->arrSize, false)),
			this->ident.val
		);

		if(initial){ builder->CreateStore(toRet, initial->Codegen(), false); }
		ident.val = toRet;

		return toRet;
	}

	toRet = builder->CreateAlloca(varType->Codegen(), 0, nullptr, this->ident.val);
	
	if(initial->type != NodeType::ERR){ builder->CreateStore(toRet, initial->Codegen(), false); }
	ident.val = toRet;

	return toRet;
}
llvm::Value *BlockNode::Codegen() {
	auto parentScope = currParser->GetScope();
	currParser->SetScope(myScope);

	for(auto &node: stmts){
		llvm::Value *ret = node->Codegen();
		if(false){
			std::cerr << "Node: ";
			ret->print(llvm::errs());
			std::cerr << "\n";
		}

		std::string error_str;
  		llvm::raw_string_ostream ostream{error_str};
		if(llvm::verifyModule(*module, &ostream)){
			std::cout << "Error in IR: " << GetIR() << "\nERROR: " << error_str << "\n\n";
		}
	}

	currParser->SetScope(parentScope);

	return nullptr;
}
llvm::Value *MemberNode::Codegen() {
	return nullptr;
}
llvm::Value *FuncDeclNode::Codegen() {
	auto func = llvm::Function::Create(
		llvm::FunctionType::get(funcType->Codegen(), false), 
		llvm::Function::ExternalLinkage, 
		ident.val, 
		*module
	);

	auto body = llvm::BasicBlock::Create(*context, "entry", func);
	builder->SetInsertPoint(body);
	auto lastScope = currentScope;

	currentScope = body;
	currFunc = func;
	if(block){
		block->Codegen();
	}
	currFunc = nullptr;
	currentScope = lastScope;

	//llvm::verifyFunction(*func);

	return func;
}
llvm::Value *VarAssignNode::Codegen() {
	const auto &varFind = currParser->FindIdent(varName);
	if(varFind.type.type != VarType::Type::ERR){
		return builder->CreateStore(varFind.val, expression->Codegen(), false);
	}

	std::cerr << "Invalid type of variable " << varFind.ident.val << "\n";
	return nullptr;
}
llvm::Value *WhileNode::Codegen() {
	return nullptr;
}
llvm::Value *IfNode::Codegen() {
	auto condVal = cond->Codegen();
	condVal = builder->CreateFCmpONE(condVal, llvm::ConstantFP::get(*context, llvm::APFloat(0.0f)), "ifcond");
	auto func = builder->GetInsertBlock()->getParent();

	auto thenBB = llvm::BasicBlock::Create(*context, "then", func);
	auto elseBB = llvm::BasicBlock::Create(*context, "else");
	auto mergeBB = llvm::BasicBlock::Create(*context, "ifcond");

	builder->CreateCondBr(condVal, thenBB, elseBB);

	builder->SetInsertPoint(thenBB);
	auto thenVal = then->Codegen();
	if(!thenVal) return nullptr;

	builder->CreateBr(mergeBB);

	thenBB = builder->GetInsertBlock();

	llvm::Value *elseVal = nullptr;

	func->getBasicBlockList().push_back(elseBB);
	builder->SetInsertPoint(elseBB);

	if(elseBody){
		elseVal = elseBody->Codegen();

		if(!elseVal) return nullptr;
	}
	builder->CreateBr(mergeBB);

	elseBB = builder->GetInsertBlock();

	func->getBasicBlockList().push_back(mergeBB);
	builder->SetInsertPoint(mergeBB);
	auto pn = builder->CreatePHI(llvm::Type::getDoubleTy(*context), (bool)then + (bool)elseBody, "iftmp");

	pn->addIncoming(thenVal, thenBB);
	if(elseBody){
		pn->addIncoming(elseVal, elseBB);
	}

	return pn;
}
llvm::Value *ReturnNode::Codegen() {
	llvm::Value *ret = nullptr;
	if(expr->type == NodeType::ERR){ ret = builder->CreateRetVoid(); }
	else { ret = builder->CreateRet(expr->Codegen()); }

	return ret;
}