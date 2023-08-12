#pragma once

#include <iostream>
#include <utility>
#include "parser/parser.hpp"

class Log{
	public:
	template<typename Arg, typename ...Args>
	static inline void Info(const Parser &parser, Arg&& arg, Args&& ...args){
		std::cout << "[INFO] Line " << parser.currTok.line << " " << std::forward<Arg>(arg);
		((std::cout << std::forward<Args>(args)), ...);
	}

	template<typename Arg, typename ...Args>
	static inline void Warn(const Parser &parser, Arg&& arg, Args&& ...args){
		std::cout << "[WARNING] Line " << parser.currTok.line << " " << std::forward<Arg>(arg);
		((std::cout << std::forward<Args>(args)), ...);
	}

	template<typename Arg, typename ...Args>
	static inline void Error(const Parser &parser, Arg&& arg, Args&& ...args){
		std::cout << "[ERROR] Line " << parser.currTok.line << " " << std::forward<Arg>(arg);
		((std::cout << std::forward<Args>(args)), ...);
		exit(1);
	}
};