#include "tokenizer.hpp"
#include <ctype.h>
#include <unordered_map>
#include <algorithm>

const Token Token::ERROR = Token();

static const std::unordered_map<std::string, Token::Type> keywords{
	{ "void", Token::Type::TYPE_VOID },
	{ "char", Token::Type::TYPE_CHAR },
	{ "short", Token::Type::TYPE_SHORT },
	{ "int", Token::Type::TYPE_INT },
	{ "long", Token::Type::TYPE_LONG },
	{ "float", Token::Type::TYPE_FLOAT },
	{ "double", Token::Type::TYPE_DOUBLE },
	{ "enum", Token::Type::TYPE_ENUM },
	{ "struct", Token::Type::TYPE_STRUCT },
	{ "if", Token::Type::IF },
	{ "else", Token::Type::ELSE },
	{ "while", Token::Type::WHILE },
	{ "return", Token::Type::RETURN },
};

void Tokenizer::AddLine(std::string line){
	while (line.find("\r\n") != std::string::npos){
    	line.erase(line.find("\r\n"), 2);
	}
	if(!line.length()){
		lines.push_back("");
		return;
	}

	while(line.length()){
		auto pos = line.find_first_of('\n');

		if(pos == std::string::npos){
			lines.push_back(line);
			line.clear();
		}
		else{
			lines.push_back(line.substr(0, pos));
			line.erase(0, pos + 1);
		}
	}
}
Token Tokenizer::NextToken(){
	if(currLine >= lines.size()) return Token(Token::Type::TEOF);

	while(currChar >= lines[currLine].size()){
		currLine++;
		currChar = 0;

		if(currLine >= lines.size()) return Token(Token::Type::TEOF);
	}

	while(std::isspace(lines[currLine][currChar])){
		currChar++;

		if(currChar >= lines[currLine].size()){
			currChar = 0;
			currLine++;

			if(currLine >= lines.size()) return Token(Token::Type::TEOF);
		}
	}

	line = currLine + 1;
	chr = currChar + 1;
	if(std::isalpha(lines[currLine][currChar])){
		std::string val{};
		while(std::isalnum(lines[currLine][currChar]) && currChar < lines[currLine].size()){
			val += lines[currLine][currChar++];
		}

		if(keywords.contains(val)) return Token(keywords.at(val), "", line);

		return Token(Token::Type::IDENT, val, line);
	}
	if(std::isdigit(lines[currLine][currChar])){
		std::string val;
		while((std::isdigit(lines[currLine][currChar]) || lines[currLine][currChar] == '.') && currChar < lines[currLine].size()){
			val += lines[currLine][currChar++];
		}

		if(std::count(val.begin(), val.end(), '.') > 1) return Token();

		if(std::count(val.begin(), val.end(), '.') == 1) return Token(Token::Type::FLOATING_NUMBER, val, line);
		return Token(Token::Type::INTEGER_NUMBER, val, line);
	}
	if(lines[currLine][currChar] == '\''){
		try{
			std::string val{lines[currLine][++currChar], 0};
			char lookahead = lines[currLine].at(currChar + 1);
			if(lookahead != '\'') return Token();
			currChar++;

			return Token(Token::Type::CHAR_LITERAL, val, line);
		}
		catch(...){
			return Token(Token::Type::ERR, "", line);
		}
	}
	if(lines[currLine][currChar] == '"'){
		currChar++;
		std::string val = "";

		while(lines[currLine][currChar] != '"'){
			val += lines[currLine][currChar++];

			if(currChar >= lines[currLine].size()) return Token(Token::Type::ERR, "", line);
		}

	}

	char lookahead = (currChar + 1 >= lines[currLine].size() ? '\0' : lines[currLine][currChar + 1]);
	switch(lines[currLine][currChar++]){
		case '+': 
			if(lookahead == '='){
				currChar++;
				return Token(Token::Type::ADDASSIGN, "", line);
			}
			return Token(Token::Type::PLUS, "", line);
		case '-': 
			if(lookahead == '='){
				currChar++;
				return Token(Token::Type::SUBASSIGN, "", line);
			}
			else if(lookahead == '>'){
				currChar++;
				return Token(Token::Type::DEREFERENCE, "", line);
			}
			return Token(Token::Type::MINUS, "", line);
		case '*': 
			if(lookahead == '='){
				currChar++;
				return Token(Token::Type::MULTASSIGN, "", line);
			}
			return Token(Token::Type::STAR, "", line);
		case '/': 
			if(lookahead == '='){
				currChar++;
				return Token(Token::Type::DIVASSIGN, "", line);
			}
			return Token(Token::Type::SLASH, "", line);

		case '=': 
			if(lookahead == '='){
				currChar++;
				return Token(Token::Type::EQ, "", line);
			}
			return Token(Token::Type::ASSIGN, "", line);
		case '!':
			if(lookahead == '='){
				currChar++;
				return Token(Token::Type::NEQ, "", line);
			}
			return Token(Token::Type::NOT, "", line);
		case '>': 
			if(lookahead == '='){
				currChar++;
				return Token(Token::Type::GEQ, "", line);
			}
			return Token(Token::Type::GREATER, "", line);
		case '<': 
			if(lookahead == '='){
				currChar++;
				return Token(Token::Type::LEQ, "", line);
			}
			return Token(Token::Type::LESS, "", line);

		case ';': return Token(Token::Type::SEMICOLON, "", line);
		case ',': return Token(Token::Type::COMMA, "", line);
		case '.': return Token(Token::Type::DOT, "", line);

		case '(': return Token(Token::Type::OPEN_PARENTH, "", line);
		case ')': return Token(Token::Type::CLOSED_PARENTH, "", line);
		case '{': return Token(Token::Type::OPEN_BRACKET, "", line);
		case '}': return Token(Token::Type::CLOSED_BRACKET, "", line);
	}

	return Token();
}