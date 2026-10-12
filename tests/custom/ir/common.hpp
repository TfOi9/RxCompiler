#pragma once

#include "ir/builder.hpp"
#include "ir/printer.hpp"

// Only target configuration is shared; each example calls IrBuilder directly.
inline ir::Module makeModule() {
    ir::Module module;
    module.target_triple = "riscv32-unknown-elf";
    module.data_layout = "e-m:e-p:32:32-i64:64-n32-S128";
    return module;
}
