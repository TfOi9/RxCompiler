#pragma once
#include "ir/block.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
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

struct FunctionGraph {
    std::vector<std::vector<BlockId>> predecessor;
    std::vector<std::vector<BlockId>> successor;
    std::vector<bool> reachable;
};

class IrVerifier {
public:
    explicit IrVerifier(const Module& module): module_(module) {}
    std::vector<IrError> verify() const;
private:
    struct DefinitionPosition {
        BlockId block;
        size_t inst;
    };

    const Module& module_;

    std::vector<IrError> checkStructLoop() const;
    std::vector<IrError> checkFunctionCompleteness() const;
    std::optional<IrError> checkBlockTerminator(FunctionId func, const BasicBlock& block) const;
    FunctionGraph buildFunctionGraph(const Function& func) const;
    std::vector<DefinitionPosition> collectDefinitionPositions(const Function& func) const;
    std::vector<std::vector<bool>> computeDominators(const FunctionGraph& graph) const;
    std::optional<IrError> checkUse(const Operand& operand, BlockId block, size_t inst_index, const FunctionGraph& graph, const std::vector<DefinitionPosition>& defs, const std::vector<std::vector<bool>>& dom) const;
    std::vector<IrError> checkFunction(const Function& func) const;
};

} // namespace ir
