#pragma once
#include "ir/builder.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/layout.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/semantic_model.hpp"
#include "semantic/types.hpp"
#include <map>
#include <vector>

namespace ir {

class TypeLowering;
struct RuntimeFunctions;

class CloneHelper {
public:
    CloneHelper(const semantic::SemanticModel& model, Module& module, TypeLowering& types, TargetLayout& layout, const RuntimeFunctions& runtime):
        model_(model), module_(module), types_(types), layout_(layout), runtime_(runtime), declarations_(module), builder_(module) {}
    FunctionId request(semantic::TyId type);
    void emitPending();
private:
    const semantic::SemanticModel& model_;
    Module& module_;
    TypeLowering& types_;
    TargetLayout& layout_;
    const RuntimeFunctions& runtime_;
    IrBuilder declarations_;
    IrBuilder builder_;
    std::map<semantic::TyId, FunctionId> functions_;
    std::vector<semantic::TyId> pending_;
    size_t next_pending_ = 0;
    FunctionId helper_function_{};

    TypeLayout objectLayout(semantic::TyId type);
    Operand emitAllocate(semantic::TyId element_type, Operand count);
    Operand fieldAddress(semantic::TyId owner, Operand address, uint32_t ordinal);
    void copyStorage(semantic::TyId type, Operand destination, Operand source);
    void emitChild(semantic::TyId type, Operand destination, Operand source);
    void cloneRange(semantic::TyId element_type, Operand destination, Operand source, Operand count);
    void emitArray(semantic::TyId owner, const semantic::ArrayTy& array, Operand destination, Operand source);
    void emitStruct(semantic::TyId owner, const semantic::StructTy& structure, Operand destination, Operand source);
    void emitBox(const semantic::BoxTy& box, Operand destination, Operand source);
    void emitVec(semantic::TyId owner, const semantic::VecTy& vec, Operand destination, Operand source);
    void emitOne(semantic::TyId type, FunctionId function);
};

} // namespace ir