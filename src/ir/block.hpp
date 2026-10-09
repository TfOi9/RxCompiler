#pragma once
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include <optional>
#include <variant>

namespace ir {

struct Br {
    BlockId target;
};
struct CondBr {
    Operand condition;
    BlockId yes;
    BlockId no;
};
struct Ret {
    std::optional<Operand> value;
};
struct Unreachable {};

using Terminator = std::variant<Br, CondBr, Ret, Unreachable>;

struct BasicBlock {
    BlockId id;
    std::vector<Instruction> instructions;
    std::optional<Terminator> terminator;
};

} // namespace ir