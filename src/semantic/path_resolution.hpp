#pragma once

#include "semantic_ids.hpp"
#include <variant>

namespace semantic {

struct LocalTarget {
    LocalId id;
};

struct ConstantTarget {
    ConstId id;
};

struct FunctionTarget {
    FunctionId id;
};

enum class BuiltinOp {
    BoxNew,
    VecNew,
    VecLen,
    VecIsEmpty,
    VecPush,
    VecRemove,
    Clone,
    ArrayLen
};

struct BuiltinTarget {
    BuiltinOp operation;
    TyId owner_type;
};

using ResolvedValue = std::variant<LocalTarget, ConstantTarget, FunctionTarget, BuiltinTarget>;

} // namespace semantic