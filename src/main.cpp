#include "frontend/parser.hpp"
#include "semantic/semantic.hpp"
#include <iostream>
#include <string>

int main() {
    std::string code, line;
    while (std::getline(std::cin, line)) {
        code += line + '\n';
    }
    std::cerr << code << std::endl;
    auto ast = frontend::parseToAst(code);
    frontend::dump(ast, std::cout);
    if (!ast.success) {
        return 1;
    }
    auto result = semantic::analyze(*ast.crate);
    semantic::dump(result, std::cout);
    if (!result.success) {
        return 1;
    }
    return 0;
}