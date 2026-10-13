#pragma once

#include "ir/builder.hpp"
#include "ir/printer.hpp"
#include "ir/verifier.hpp"

// Each example still constructs its own module with IrBuilder.
inline ir::Module makeModule() {
    ir::Module module;
    module.target_triple = "riscv32-unknown-elf";
    module.data_layout = "e-m:e-p:32:32-i64:64-n32-S128";
    return module;
}

inline int verifyAndPrint(const ir::Module& module, std::ostream& output = std::cout,
                          std::ostream& diagnostics = std::cerr) {
    const auto errors = ir::IrVerifier(module).verify();
    for (const auto& error : errors) {
        diagnostics << "Project verifier: " << error.message << '\n';
    }
    if (!errors.empty()) return 1;
    ir::IrPrinter(module, output).print();
    return 0;
}
