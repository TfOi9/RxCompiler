#include "frontend/parser.hpp"
#include <iostream>
#include <string>

int main() {
    std::string code, line;
    while (std::getline(std::cin, line)) {
        code += line + '\n';
    }
    std::cerr << code << std::endl;
    frontend::dump(frontend::parseToAst(code), std::cout);
    return 0;
}