#include "codegen.hpp"

static int stackDepth = 0;

static void ParseFunction(std::ofstream &file, const FuncDeclNode *node);
static void ParseBlock(std::ofstream &file, const BlockNode *node);

static void PrintLine(std::ofstream &fp, const std::string &str){
	fp << str << "\n";
}

static void Push(std::ofstream &fp, const std::string &str = "rax"){
	PrintLine(fp, std::string("\tpush ") + str);
	stackDepth++;
}
static void Pop(std::ofstream &fp, const std::string &str){
	PrintLine(fp, std::string("\tpop ") + str);
	stackDepth--;
}

static void ParseFunction(std::ofstream &file, const FuncDeclNode *node){
	PrintLine(file, node->ident.val + ":");
	Push(file, "rbp");
	PrintLine(file, "\tmov rbp, rsp");

	PrintLine(file, "\t//Code");

	Pop(file, "rbp");
	PrintLine(file, "\tret");
}
static void ParseBlock(std::ofstream &file, const BlockNode *node){
	for(auto &stmt: node->stmts){
		switch(stmt->type){
			case NodeType::BLOCK:
				stackDepth++;
				ParseBlock(file, dynamic_cast<const BlockNode*>(stmt.get()));
				break;
			case NodeType::FUNCDECL:
				if(stackDepth) break;
				ParseFunction(file, dynamic_cast<const FuncDeclNode*>(stmt.get()));
				break;
		}
	}

	if(stackDepth)
		stackDepth--;
}

void GenerateCode(std::ofstream &file, const Node *root){
	stackDepth = 0;
	ParseBlock(file, dynamic_cast<const BlockNode*>(root));
}