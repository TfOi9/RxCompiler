#pragma once
#include "ir/function.hpp"
#include "ir/ir_ids.hpp"
#include <optional>
#include <string>
#include <vector>

namespace ir {

struct IrError {
    std::optional<FunctionId> func;
    std::optional<BlockId> block;
    std::optional<size_t> inst;
    std::optional<ValueId> value;
    std::optional<StructId> named;
    std::string message;
};

class IrVerifier {
public:
    explicit IrVerifier(const Module& module): module_(module) {}
    std::vector<IrError> verify() const;
private:
    const Module& module_;

    std::vector<IrError> checkStructLoop() const;
};

} // namespace ir