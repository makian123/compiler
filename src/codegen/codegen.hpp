#pragma once

#include <string>
#include <fstream>
#include "parser/parser.hpp"

void GenerateCode(std::ofstream &file, const Node *root);