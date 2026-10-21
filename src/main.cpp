#include "frontend/parser.hpp"
#include "ir/module_lowering.hpp"
#include "ir/printer.hpp"
#include "ir/verifier.hpp"
#include "semantic/semantic.hpp"
#include <exception>
#include <iostream>
#include <string>

int main() {
    std::string code, line;
    while (std::getline(std::cin, line)) {
        code += line + '\n';
    }
    std::cerr << code << std::endl;
    auto ast = frontend::parseToAst(code);
    if (!ast.success) {
        frontend::dump(ast, std::cerr);
        return 1;
    }
    auto result = semantic::analyze(*ast.crate);
    semantic::dump(result, std::cerr);
    if (!result.success) {
        return 1;
    }
    try {
        ir::IrBuilder::requireIr(result.model != nullptr, "semantic analysis produced no model");
        ir::Module module;
        ir::ModuleLowering lowering(*result.model, module);
        lowering.lower();
        ir::IrVerifier verifier(module);
        auto errors = verifier.verify();
        if (!errors.empty()) {
            for (const auto& error: errors) {
                std::cerr << "IR verification error";
                if (error.func) {
                    std::cerr << " in function " << *error.func;
                }
                if (error.block) {
                    std::cerr << ", block " << *error.block;
                }
                std::cerr << ": " << error.message << '\n';
            }
            return 1;
        }
        ir::IrPrinter(module, std::cout).print();
    } catch (const std::exception& error) {
        std::cerr << "IR lowering error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
