#pragma once
#include "ir/ir_ids.hpp"
#include "ir/types.hpp"
#include "ir/block.hpp"
#include <vector>
#include <string>

namespace ir {

struct FunctionSignature {
    TypeId return_type;
    std::vector<TypeId> parameters;
};

struct Parameter {
    ValueId value;
    TypeId type;
};

enum class Linkage {
    External,
    Internal
};

struct Function {
    FunctionId id;
    std::string symbol_name;
    Linkage linkage;
    FunctionSignature signature;
    std::vector<Parameter> parameters;
    std::vector<TypeId> value_types;
    std::vector<BasicBlock> blocks;
};

struct Module {
    std::string target_triple;
    std::string data_layout;
    TypeTable types;
    std::vector<StructDef> structs;
    std::vector<Function> functions;
};

} // namespace ir