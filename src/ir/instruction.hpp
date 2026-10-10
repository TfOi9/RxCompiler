#pragma once
#include "ir/ir_ids.hpp"
#include <vector>
#include <optional>

namespace ir {

struct Immediate {
    uint32_t bits;
};
struct LocalValue {
    ValueId id;
    FunctionId func;
};
struct NullPointer {};

using OperandData = std::variant<Immediate, LocalValue, NullPointer>;

struct Operand {
    TypeId type;
    OperandData data;
};

enum class OpCode {
    Add, Sub, Mul, Udiv, Sdiv, Urem, Srem,
    Shl, Lshr, Ashr, And, Or, Xor
};

enum class Predicate {
    Eq, Ne, Ugt, Uge, Ult, Ule, Sgt, Sge, Slt, Sle
};

struct Alloca {
    TypeId type;
    uint8_t align;
};
struct Load {
    TypeId type;
    Operand address;
    uint8_t align;
};
struct Store {
    Operand value;
    Operand address;
    uint8_t align;
};
struct Binary {
    OpCode opcode;
    Operand lhs;
    Operand rhs;
};
struct ICmp {
    Predicate predicate;
    Operand lhs;
    Operand rhs;
};
struct ZExt {
    Operand value;
    TypeId type;
};
struct Trunc {
    Operand value;
    TypeId type;
};
struct Gep {
    TypeId type;
    Operand base;
    std::vector<Operand> indices;
};
struct Call {
    FunctionId func;
    std::vector<Operand> args;
};

using InstructionData = std::variant<Alloca, Load, Store, Binary, ICmp, ZExt, Trunc, Gep, Call>;

struct Instruction {
    std::optional<ValueId> result;
    InstructionData data;
};

} // namespace ir