#pragma once

#include <string>
#include <vector>

struct Token{
	enum class Type: int {
		ERR = -2,
		TEOF = -1,

		VALUES_BEGIN,
		IDENT = VALUES_BEGIN,
		INTEGER_NUMBER,
		FLOATING_NUMBER,
		CHAR_LITERAL,
		STRING_LITERAL,
		VALUES_END = STRING_LITERAL,

		TYPES_BEGIN,
		TYPE_CHAR = TYPES_BEGIN,
		TYPE_SHORT,
		TYPE_INT,
		TYPE_LONG,
		TYPE_FLOAT,
		TYPE_DOUBLE,
		TYPE_ENUM,
		TYPE_STRUCT,
		TYPES_END = TYPE_STRUCT,

		PLUS,
		MINUS,
		STAR,
		SLASH,
		
		EQ,

		SEMICOLON,
		COMMA,
		
		OPEN_PARENTH,
		CLOSED_PARENTH,
		OPEN_BRACKET,
		CLOSED_BRACKET,
	};
	
	Type type;
	std::string val;
	size_t line;

	explicit Token(): type(Type::ERR), val(""), line(0) {}
	Token(Type type_, const std::string &val_ = "", size_t line_ = 0): type(type_), val(val_), line(line_) {}
	Token(const Token &tok):type(tok.type), val(tok.val), line(tok.line) {}
	Token(const Token &&tok):type(tok.type), val(tok.val), line(tok.line) {}

	Token &operator=(const Token &other){
		type = other.type;
		val = other.val;
		line = other.line;

		return *this;
	}
};

class Tokenizer{
	private:
	std::vector<std::string> lines {};
	size_t currLine = 0, currChar = 0;

	public:
	Tokenizer() = default;

	void AddLine(std::string line);

	Token NextToken();
};