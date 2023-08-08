#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include "tokenizer/tokenizer.hpp"
#include "parser/parser.hpp"

enum class Flags: char{
	OUTPUT_FILE = 1 << 0,
	ARCHITECTURE = 1 << 1
};

int main(int argc, char **argv){
	if(argc < 2){
		std::cout << "Input file not specified";
		return 1;
	}
	std::vector<std::string> inFilePaths;
	std::string outFilePath;
	char flagActive = 0;

	for(int i = 1; i < argc; ++i){
		if(flagActive){
			if(flagActive & (char)Flags::OUTPUT_FILE){
				outFilePath = argv[i];
			}
			flagActive = 0;
			continue;
		}

		if(!std::strcmp(argv[i], "-o")){
			flagActive |= (char)Flags::OUTPUT_FILE;
			continue;
		}

		inFilePaths.push_back(argv[i]);
	}
	if(!inFilePaths.size()){
		std::cout << "Input file not specified";
		return 1;
	}
	if(!outFilePath.length()){
		outFilePath = "a.out";
	}

	std::ifstream inFile(inFilePaths[0]);
	Tokenizer tokenizer;

	std::string line;
	while(std::getline(inFile, line)){
		tokenizer.AddLine(line);
	}
	inFile.close();

	Parser parser(tokenizer);
	parser.Parse();
	
	std::ofstream output(outFilePath);
	output << parser.GenerateCode();
	output.close();
	
	return 0;
}