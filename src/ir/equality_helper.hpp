#pragma once
#include "ir/builder.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/layout.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/semantic_model.hpp"
#include "semantic/types.hpp"
#include <cstddef>
#include <map>
#include <vector>

namespace ir {

class TypeLowering;

class EqualityHelper {
public:
    EqualityHelper(const semantic::SemanticModel& model, Module& module, TypeLowering& types, TargetLayout& layout):
        model_(model), module_(module), types_(types), layout_(layout), declarations_(module), builder_(module) {}
    FunctionId request(semantic::TyId type);
    void emitPending();
private:
    const semantic::SemanticModel& model_;
    Module& module_;
    TypeLowering& types_;
    TargetLayout& layout_;
    IrBuilder declarations_;
    IrBuilder builder_;
    std::map<semantic::TyId, FunctionId> functions_;
    std::vector<semantic::TyId> pending_;
    size_t next_pending_ = 0;
    FunctionId helper_function_{};

    Operand fieldAddress(semantic::TyId owner, Operand address, uint32_t ordinal);
    Operand emitChild(semantic::TyId type, Operand left, Operand right);
    void returnFalseUnless(Operand condition);
    Operand compareScalar(semantic::TyId type, Operand left, Operand right);
    Operand compareIndirect(semantic::TyId target, Operand left, Operand right);
    Operand compareRange(semantic::TyId element_type, Operand left, Operand right, Operand count);
    Operand compareArray(semantic::TyId owner, const semantic::ArrayTy& array, Operand left, Operand right);
    Operand compareStruct(semantic::TyId owner, const semantic::StructTy& structure, Operand left, Operand right);
    Operand compareVec(semantic::TyId owner, const semantic::VecTy& vec, Operand left, Operand right);
    void emitOne(semantic::TyId type, FunctionId function);
};

} // namespace ir
