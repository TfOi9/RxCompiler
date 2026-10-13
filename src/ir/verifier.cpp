#include "ir/verifier.hpp"
#include "ir/block.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/types.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <functional>

namespace ir {

std::vector<IrError> IrVerifier::verify() const {
    auto loop_err = checkStructLoop();
    if (!loop_err.empty()) {
        return loop_err;
    }
    auto comp_err = checkFunctionCompleteness();
    if (!comp_err.empty()) {
        return comp_err;
    }
    std::vector<IrError> errors;
    for (const auto& func: module_.functions) {
        if (func.form == FunctionForm::Declaration) {
            continue;
        }
        auto func_err = checkFunction(func);
        for (const auto& err: func_err) {
            errors.push_back(err);
        }
    }
    return errors;
}

std::vector<IrError> IrVerifier::checkStructLoop() const {
    enum class StructStatus {
        Unknown,
        Visiting,
        Visited
    };
    std::unordered_map<StructId, StructStatus> status;
    std::function<bool(StructId)> visit = [&](StructId pos) -> bool {
        if (!status.count(pos)) {
            status[pos] = StructStatus::Visiting;
        } else return true;
        const auto& def = module_.structs.at(pos);
        if (!def.fields) {
            status[pos] = StructStatus::Visited;
            return true;
        }
        bool ok = true;
        for (const auto& field: *def.fields) {
            Type type = module_.types.get(field);
            while (const auto* arr = std::get_if<ArrayType>(&type)) {
                const TypeId element = arr->element;
                type = module_.types.get(element);
            }
            const auto* named = std::get_if<NamedStructType>(&type);
            if (!named) {
                continue;
            }
            const StructId id = named->id;
            if (status.count(id)) {
                if (status.at(id) == StructStatus::Visiting) {
                    return false;
                }
            } else {
                ok = visit(id) && ok;
            }
        }
        if (!ok) {
            return false;
        }
        status[pos] = StructStatus::Visited;
        return true;
    };
    std::vector<IrError> errors;
    for (size_t i = 0; i < module_.structs.size(); i++) {
        if (status.count(i) && status.at(i) == StructStatus::Visited) {
            continue;
        }
        if ((status.count(i) && status.at(i) == StructStatus::Visiting) || (!status.count(i) && !visit(i))) {
            errors.push_back(
                IrError{
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    i,
                    "struct " + module_.structs.at(i).name + " has infinite layout"
                }
            );
        }
    }
    return errors;
}

std::vector<IrError> IrVerifier::checkFunctionCompleteness() const {
    std::vector<IrError> errors;
    for (const auto& func: module_.functions) {
        if (func.form == FunctionForm::Declaration) {
            continue;
        }
        if (!func.blocks.size()) {
            errors.push_back(
                IrError{
                    func.id,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    "function declaration " + func.symbol_name + " has no blocks"
                }
            );
        }
        for (const auto& block: func.blocks) {
            const auto err = checkBlockTerminator(func.id, block);
            if (err.has_value()) {
                errors.push_back(*err);
            }
        }
    }
    return errors;
}

std::optional<IrError> IrVerifier::checkBlockTerminator(FunctionId func, const BasicBlock& block) const {
    if (!block.terminator.has_value()) {
        return IrError{
            func,
            block.id,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            "basic block in func ID=" + std::to_string(func) + "block ID=" + std::to_string(block.id) + " has no terminator"
        };
    }
    return std::nullopt;
}

FunctionGraph IrVerifier::buildFunctionGraph(const Function& func) const {
    FunctionGraph g{
        std::vector<std::vector<BlockId>>(func.blocks.size(), std::vector<BlockId>()),
        std::vector<std::vector<BlockId>>(func.blocks.size(), std::vector<BlockId>()),
        std::vector<bool>(func.blocks.size(), false)
    };
    for (const auto& block: func.blocks) {
        if (const auto* br = std::get_if<Br>(&*block.terminator)) {
            BlockId to = br->target;
            g.predecessor[to].push_back(block.id);
            g.successor[block.id].push_back(to);
        } else if (const auto* condbr = std::get_if<CondBr>(&*block.terminator)) {
            BlockId yes = condbr->yes, no = condbr->no;
            g.predecessor[yes].push_back(block.id);
            g.predecessor[no].push_back(block.id);
            g.successor[block.id].push_back(yes);
            g.successor[block.id].push_back(no);
        }
    }
    std::vector<bool>& visited = g.reachable;
    std::function<void(BlockId)> visit = [&](BlockId pos) -> void {
        visited[pos] = true;
        for (const auto& to: g.successor[pos]) {
            if (visited[to]) {
                continue;
            }
            visit(to);
        }
    };
    visit(func.blocks.front().id);
    return g;
}

std::vector<IrVerifier::DefinitionPosition> IrVerifier::collectDefinitionPositions(const Function& func) const {
    std::vector<DefinitionPosition> defs(func.value_types.size());
    for (const auto& block: func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); i++) {
            const auto& inst = block.instructions[i];
            if (inst.result.has_value()) {
                defs[*inst.result] = {block.id, i};
            }
        }
    }
    return defs;
}

std::vector<std::vector<bool>> IrVerifier::computeDominators(const FunctionGraph& graph) const {
    size_t size = graph.predecessor.size();
    auto dom = std::vector<std::vector<bool>>(size, std::vector<bool>(size, false));
    for (size_t i = 0; i < size; i++) {
        if (graph.reachable[i]) {
            dom[i] = graph.reachable;
        }
    }
    dom[0].assign(size, false);
    dom[0][0] = true;
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 1; i < size; i++) {
            if (!graph.reachable[i]) {
                continue;
            }
            std::vector<bool> next = graph.reachable;
            for (BlockId block: graph.predecessor[i]) {
                if (!graph.reachable[block]) {
                    continue;
                }
                for (size_t j = 0; j < size; j++) {
                    next[j] = next[j] && dom[block][j];
                }
            }
            next[i] = true;
            if (next != dom[i]) {
                changed = true;
                dom[i] = std::move(next);
            }
        }
    }
    return dom;
}

std::optional<IrError> IrVerifier::checkUse(const Operand& operand, BlockId block, size_t inst_index, const FunctionGraph& graph, const std::vector<DefinitionPosition>& defs, const std::vector<std::vector<bool>>& dom) const{
    const auto* local = std::get_if<LocalValue>(&operand.data);
    if (!local) {
        return std::nullopt;
    }
    const auto& func = module_.functions[local->func];
    if (local->id < func.signature.parameters.size()) {
        return std::nullopt;
    }
    if (!graph.reachable[block]) {
        return std::nullopt;
    }
    const auto& definition = defs[local->id];
    const auto def_block = definition.block;
    if (!graph.reachable[def_block]) {
        return IrError{
            func.id,
            block,
            inst_index,
            local->id,
            std::nullopt,
            "local value has no reachable definition"
        };
    }
    if (def_block == block && definition.inst >= inst_index) {
        return IrError{
            func.id,
            block,
            inst_index,
            local->id,
            std::nullopt,
            "local value definition does not precede its use"
        };
    }
    if (!dom[block][def_block]) {
        return IrError{
            func.id,
            block,
            inst_index,
            local->id,
            std::nullopt,
            "local value may not have definition"
        };
    }
    return std::nullopt;
}

std::vector<IrError> IrVerifier::checkFunction(const Function& func) const {
    std::vector<IrError> errors;
    auto graph = buildFunctionGraph(func);
    if (!graph.predecessor[0].empty()) {
        return {
            IrError{
                func.id,
                func.blocks.front().id,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                "function entrance block has unexpected predecessor"
            }
        };
    }
    auto defs = collectDefinitionPositions(func);
    auto dom = computeDominators(graph);
    for (const auto& block: func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); i++) {
            const auto& inst = block.instructions[i];
            if (const auto* load = std::get_if<Load>(&inst.data)) {
                auto addr_err = checkUse(load->address, block.id, i, graph, defs, dom);
                if (addr_err.has_value()) {
                    errors.push_back(*addr_err);
                }
            } else if (const auto* store = std::get_if<Store>(&inst.data)) {
                auto addr_err = checkUse(store->address, block.id, i, graph, defs, dom);
                auto value_err = checkUse(store->value, block.id, i, graph, defs, dom);
                if (addr_err.has_value()) {
                    errors.push_back(*addr_err);
                }
                if (value_err.has_value()) {
                    errors.push_back(*value_err);
                }
            } else if (const auto* binary = std::get_if<Binary>(&inst.data)) {
                auto lhs_err = checkUse(binary->lhs, block.id, i, graph, defs, dom);
                auto rhs_err = checkUse(binary->rhs, block.id, i, graph, defs, dom);
                if (lhs_err.has_value()) {
                    errors.push_back(*lhs_err);
                }
                if (rhs_err.has_value()) {
                    errors.push_back(*rhs_err);
                }
            } else if (const auto* icmp = std::get_if<ICmp>(&inst.data)) {
                auto lhs_err = checkUse(icmp->lhs, block.id, i, graph, defs, dom);
                auto rhs_err = checkUse(icmp->rhs, block.id, i, graph, defs, dom);
                if (lhs_err.has_value()) {
                    errors.push_back(*lhs_err);
                }
                if (rhs_err.has_value()) {
                    errors.push_back(*rhs_err);
                }
            } else if (const auto* zext = std::get_if<ZExt>(&inst.data)) {
                auto value_err = checkUse(zext->value, block.id, i, graph, defs, dom);
                if (value_err.has_value()) {
                    errors.push_back(*value_err);
                }
            } else if (const auto* trunc = std::get_if<Trunc>(&inst.data)) {
                auto value_err = checkUse(trunc->value, block.id, i, graph, defs, dom);
                if (value_err.has_value()) {
                    errors.push_back(*value_err);
                }
            } else if (const auto* gep = std::get_if<Gep>(&inst.data)) {
                auto base_err = checkUse(gep->base, block.id, i, graph, defs, dom);
                if (base_err.has_value()) {
                    errors.push_back(*base_err);
                }
                for (const auto& idx: gep->indices) {
                    auto idx_err = checkUse(idx, block.id, i, graph, defs, dom);
                    if (idx_err.has_value()) {
                        errors.push_back(*idx_err);
                    }
                }
            } else if (const auto* call = std::get_if<Call>(&inst.data)) {
                for (const auto& arg: call->args) {
                    auto arg_err = checkUse(arg, block.id, i, graph, defs, dom);
                    if (arg_err.has_value()) {
                        errors.push_back(*arg_err);
                    }
                }
            }
        }
        const auto& term = block.terminator;
        if (const auto* condbr = std::get_if<CondBr>(&*term)) {
            auto cond_err = checkUse(condbr->condition, block.id, block.instructions.size(), graph, defs, dom);
            if (cond_err.has_value()) {
                errors.push_back(*cond_err);
            }
        } else if (const auto* ret = std::get_if<Ret>(&*term); ret && ret->value.has_value()) {
            auto ret_err = checkUse(*ret->value, block.id, block.instructions.size(), graph, defs, dom);
            if (ret_err.has_value()) {
                errors.push_back(*ret_err);
            }
        }
    }
    return errors;
}

} // namespace ir
