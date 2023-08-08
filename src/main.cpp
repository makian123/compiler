#include <iostream>
#include <fstream>
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"

int main(int argc, char **argv){
	Tokenizer tokenizer;
	tokenizer.AddLine("int lol = 12 + 4; char asd = 124 * 4 + lol;");

	Parser parser(tokenizer);
	parser.Parse();
	
	std::ofstream output("test.txt");
	output << parser.GenerateCode();
	output.close();
	
	return 0;
}