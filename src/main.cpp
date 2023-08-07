#include <iostream>
#include <fstream>
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"

int main(){
	Tokenizer tokenizer;
	tokenizer.AddLine("struct testStruct{ int var1; char var2; double var3; };");

	Parser parser(tokenizer);
	parser.Parse();
	
	std::ofstream output("test.txt");
	output << parser.GenerateCode();
	output.close();
	
	return 0;
}