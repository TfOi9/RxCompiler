#include "ir/equality_helper.hpp"
#include "ir/type_lowering.hpp"
#include "semantic/struct_resolver.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>

namespace ir {

FunctionId EqualityHelper::request(semantic::TyId type) {
    if (auto it = functions_.find(type); it != functions_.end()) {
        return it->second;
    }
    FunctionId function = declarations_.addFunction(
        "__rx_equal_" + std::to_string(type),
        Linkage::Internal,
        FunctionForm::Definition,
        {
            module_.types.i1(),
            {
                module_.types.ptr(),
                module_.types.ptr()
            }
        }
    );
    functions_.emplace(type, function);
    pending_.push_back(type);
    return function;
}

Operand EqualityHelper::fieldAddress(semantic::TyId owner, Operand address, uint32_t ordinal) {
    TypeId i32 = module_.types.i32();
    return builder_.emitGep(
        types_.storageType(owner),
        address,
        {
            builder_.constantInt(i32, 0),
            builder_.constantInt(i32, ordinal)
        }
    );
}

Operand EqualityHelper::emitChild(semantic::TyId type, Operand left, Operand right) {
    FunctionId function = request(type);
    auto result = builder_.emitCall(function, {left, right});
    IrBuilder::requireIr(result.has_value(), "equality helper has no return value");
    return *result;
}

void EqualityHelper::returnFalseUnless(Operand condition) {
    BlockId next = builder_.createBlock(helper_function_);
    BlockId failed = builder_.createBlock(helper_function_);
    builder_.emitCondBr(condition, next, failed);
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, failed);
    builder_.emitRet(builder_.constantInt(module_.types.i1(), 0));
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, next);
}

Operand EqualityHelper::compareScalar(semantic::TyId type, Operand left, Operand right) {
    TypeId storage = types_.storageType(type);
    TypeLayout object = layout_.layoutOf(storage);
    IrBuilder::requireIr(object.alignment == 1 || object.alignment == 4, "unsupported equality scalar alignment");
    auto alignment = static_cast<uint8_t>(object.alignment);
    Operand lhs = builder_.emitLoad(storage, left, alignment);
    Operand rhs = builder_.emitLoad(storage, right, alignment);
    return builder_.emitICmp(Predicate::Eq, lhs, rhs);
}

Operand EqualityHelper::compareIndirect(semantic::TyId target, Operand left, Operand right) {
    Operand lhs = builder_.emitLoad(module_.types.ptr(), left, 4);
    Operand rhs = builder_.emitLoad(module_.types.ptr(), right, 4);
    return emitChild(target, lhs, rhs);
}

Operand EqualityHelper::compareRange(semantic::TyId element_type, Operand left, Operand right, Operand count) {
    TypeId i32 = module_.types.i32();
    IrBuilder::requireIr(count.type == i32, "equality range count must be usize");
    Operand zero = builder_.constantInt(i32, 0);
    Operand one = builder_.constantInt(i32, 1);
    Operand counter = builder_.emitEntryAlloca(i32, 4);
    builder_.emitStore(zero, counter, 4);
    BlockId header = builder_.createBlock(helper_function_);
    BlockId body = builder_.createBlock(helper_function_);
    BlockId done = builder_.createBlock(helper_function_);
    builder_.emitBr(header);
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, header);
    Operand index = builder_.emitLoad(i32, counter, 4);
    Operand in_range = builder_.emitICmp(Predicate::Ult, index, count);
    builder_.emitCondBr(in_range, body, done);
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, body);
    TypeId storage = types_.storageType(element_type);
    Operand lhs = builder_.emitGep(storage, left, {index});
    Operand rhs = builder_.emitGep(storage, right, {index});
    Operand equal = emitChild(element_type, lhs, rhs);
    returnFalseUnless(equal);
    Operand next = builder_.emitBinary(OpCode::Add, index, one);
    builder_.emitStore(next, counter, 4);
    builder_.emitBr(header);
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, done);
    return builder_.constantInt(module_.types.i1(), 1);
}

Operand EqualityHelper::compareArray(semantic::TyId owner, const semantic::ArrayTy& array, Operand left, Operand right) {
    TypeId storage = types_.storageType(owner);
    TypeId i32 = module_.types.i32();
    Operand zero = builder_.constantInt(i32, 0);
    Operand lhs = builder_.emitGep(storage, left, {zero, zero});
    Operand rhs = builder_.emitGep(storage, right, {zero, zero});
    Operand count = builder_.constantInt(i32, array.len);
    return compareRange(array.elem, lhs, rhs, count);
}

Operand EqualityHelper::compareStruct(semantic::TyId owner, const semantic::StructTy& structure, Operand left, Operand right) {
    const auto* info = model_.findStruct(structure.def);
    IrBuilder::requireIr(info != nullptr, "equality struct has no semantic definition");
    IrBuilder::requireIr(info->derives.valid.has(semantic::DeriveKind::PartialEq), "struct does not support PartialEq");
    for (const auto& field: info->fields) {
        IrBuilder::requireIr(field.ordinal <= std::numeric_limits<uint32_t>::max(), "equality field ordinal exceeds IR representation");
        auto ordinal = static_cast<uint32_t>(field.ordinal);
        Operand lhs = fieldAddress(owner, left, ordinal);
        Operand rhs = fieldAddress(owner, right, ordinal);
        Operand equal = emitChild(field.type, lhs, rhs);
        returnFalseUnless(equal);
    }
    return builder_.constantInt(module_.types.i1(), 1);
}

Operand EqualityHelper::compareVec(semantic::TyId owner, const semantic::VecTy& vec, Operand left, Operand right) {
    Operand left_length_address = fieldAddress(owner, left, 1);
    Operand right_length_address = fieldAddress(owner, right, 1);
    Operand left_length = builder_.emitLoad(module_.types.i32(), left_length_address, 4);
    Operand right_length = builder_.emitLoad(module_.types.i32(), right_length_address, 4);
    Operand same_length = builder_.emitICmp(Predicate::Eq, left_length, right_length);
    returnFalseUnless(same_length);
    Operand left_data_address = fieldAddress(owner, left, 0);
    Operand right_data_address = fieldAddress(owner, right, 0);
    Operand left_data = builder_.emitLoad(module_.types.ptr(), left_data_address, 4);
    Operand right_data = builder_.emitLoad(module_.types.ptr(), right_data_address, 4);
    return compareRange(vec.elem, left_data, right_data, left_length);
}

void EqualityHelper::emitOne(semantic::TyId type, FunctionId function) {
    helper_function_ = function;
    IrBuilder::requireIr(module_.functions.at(function).blocks.empty(), "equality helper body already exists");
    BlockId entry = builder_.appendBlock(function);
    builder_.positionAtEnd(function, entry);
    Operand left = builder_.parameter(function, 0);
    Operand right = builder_.parameter(function, 1);
    semantic::TyInfo info = model_.typeContext().get(type);
    Operand result;
    if (std::holds_alternative<semantic::PrimaryTy>(info)) {
        result = compareScalar(type, left, right);
    } else if (const auto* reference = std::get_if<semantic::RefTy>(&info)) {
        result = compareIndirect(reference->target, left, right);
    } else if (const auto* array = std::get_if<semantic::ArrayTy>(&info)) {
        result = compareArray(type, *array, left, right);
    } else if (const auto* structure = std::get_if<semantic::StructTy>(&info)) {
        result = compareStruct(type, *structure, left, right);
    } else if (const auto* box = std::get_if<semantic::BoxTy>(&info)) {
        result = compareIndirect(box->elem, left, right);
    } else if (const auto* vec = std::get_if<semantic::VecTy>(&info)) {
        result = compareVec(type, *vec, left, right);
    } else {
        throw std::logic_error("semantic type cannot be lowered to equality");
    }
    builder_.emitRet(result);
    builder_.clearInsertionPoint();
}

void EqualityHelper::emitPending() {
    while (next_pending_ < pending_.size()) {
        semantic::TyId type = pending_[next_pending_];
        FunctionId function = functions_.at(type);
        emitOne(type, function);
        next_pending_++;
    }
}

} // namespace ir
