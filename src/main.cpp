#include "frontend/parser.hpp"
#include <iostream>
#include <string>

int main() {
    std::string code;
    std::cin >> code;
    frontend::dump(frontend::parseToAst(code), std::cout);
    return 0;
}