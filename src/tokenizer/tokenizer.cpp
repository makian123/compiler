#include "tokenizer.hpp"
#include <ctype.h>
#include <unordered_map>
#include <algorithm>

static const std::unordered_map<std::string, Token::Type> keywords{
	{ "char", Token::Type::TYPE_CHAR },
	{ "short", Token::Type::TYPE_SHORT },
	{ "int", Token::Type::TYPE_INT },
	{ "long", Token::Type::TYPE_LONG },
	{ "float", Token::Type::TYPE_FLOAT },
	{ "double", Token::Type::TYPE_DOUBLE },
	{ "enum", Token::Type::TYPE_ENUM },
	{ "struct", Token::Type::TYPE_STRUCT }
};

void Tokenizer::AddLine(std::string line){
	while (line.find("\r\n") != std::string::npos){
    	line.erase(line.find("\r\n"), 2);
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

	if(std::isalpha(lines[currLine][currChar])){
		std::string val{};
		while(std::isalnum(lines[currLine][currChar]) && currChar < lines[currLine].size()){
			val += lines[currLine][currChar++];
		}

		if(keywords.contains(val)) return Token(keywords.at(val), val, currLine);

		return Token(Token::Type::IDENT, val, currLine);
	}
	if(std::isdigit(lines[currLine][currChar])){
		std::string val;
		while((std::isdigit(lines[currLine][currChar]) || lines[currLine][currChar] == '.') && currChar < lines[currLine].size()){
			val += lines[currLine][currChar++];
		}

		if(std::count(val.begin(), val.end(), '.') > 1) return Token(Token::Type::ERR, "", currLine);

		if(std::count(val.begin(), val.end(), '.') == 1) return Token(Token::Type::FLOATING_NUMBER, val, currLine);
		return Token(Token::Type::INTEGER_NUMBER, val, currLine);
	}
	if(lines[currLine][currChar] == '\''){
		try{
			std::string val{lines[currLine][++currChar], 0};
			char lookahead = lines[currLine].at(currChar + 1);
			if(lookahead != '\'') return Token(Token::Type::ERR, "", currLine);
			currChar++;

			return Token(Token::Type::CHAR_LITERAL, val, currLine);
		}
		catch(...){
			return Token(Token::Type::ERR, "", currLine);
		}
	}
	if(lines[currLine][currChar] == '"'){
		currChar++;
		std::string val = "";

		while(lines[currLine][currChar] != '"'){
			val += lines[currLine][currChar++];

			if(currChar >= lines[currLine].size()) return Token(Token::Type::ERR, "", currLine);
		}

	}

	std::string currCh(1, lines[currLine][currChar++]);
	switch(currCh[0]){
		case '+': return Token(Token::Type::PLUS, currCh, currLine);
		case '-': return Token(Token::Type::MINUS, currCh, currLine);
		case '*': return Token(Token::Type::STAR, currCh, currLine);
		case '/': return Token(Token::Type::SLASH, currCh, currLine);

		case '=': return Token(Token::Type::EQ, currCh, currLine);

		case ';': return Token(Token::Type::SEMICOLON, currCh, currLine);
		case ',': return Token(Token::Type::COMMA, currCh, currLine);

		case '(': return Token(Token::Type::OPEN_PARENTH, currCh, currLine);
		case ')': return Token(Token::Type::CLOSED_PARENTH, currCh, currLine);
		case '{': return Token(Token::Type::OPEN_BRACKET, currCh, currLine);
		case '}': return Token(Token::Type::CLOSED_BRACKET, currCh, currLine);
	}

	return Token(Token::Type::ERR, "", currLine);
}