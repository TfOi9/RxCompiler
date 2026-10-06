#pragma once
#include "semantic_ids.hpp"
#include <variant>

namespace semantic {

using ConstantValue = std::variant<bool, int32_t, uint32_t, int64_t, uint64_t>;

struct EvaluatedConst {
    TyId type;
    ConstantValue value;
};

} // namespace semantic