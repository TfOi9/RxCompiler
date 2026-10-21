#include "ir/clone_helper.hpp"
#include "ir/builder.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/layout.hpp"
#include "ir/type_lowering.hpp"
#include "ir/function_lowering.hpp"
#include "semantic/struct_resolver.hpp"
#include "semantic/types.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <variant>

namespace ir {

FunctionId CloneHelper::request(semantic::TyId type) {
    if (auto it = functions_.find(type); it != functions_.end()) {
        return it->second;
    }
    FunctionId id = declarations_.addFunction(
        "__rx_clone_" + std::to_string(type),
        Linkage::Internal,
        FunctionForm::Definition,
        {
            module_.types.none(),
            {
                module_.types.ptr(),
                module_.types.ptr()
            }
        }
    );
    functions_.emplace(type, id);
    pending_.push_back(type);
    return id;
}

TypeLayout CloneHelper::objectLayout(semantic::TyId type) {
    TypeLayout result = layout_.layoutOf(types_.storageType(type));
    IrBuilder::requireIr(result.size != 0 && result.size <= std::numeric_limits<uint32_t>::max(), "clone object size is not representable on RV32");
    IrBuilder::requireIr(result.alignment == 1 || result.alignment == 4, "unsupported clone object alignment");
    return result;
}

Operand CloneHelper::emitAllocate(semantic::TyId element_type, Operand count) {
    IrBuilder::requireIr(count.type == module_.types.i32(), "clone allocation count must be usize");
    TypeLayout object = objectLayout(element_type);
    Operand stride = builder_.constantInt(module_.types.i32(), static_cast<uint32_t>(object.size));
    Operand bytes = builder_.emitBinary(OpCode::Mul, count, stride);
    Operand alignment = builder_.constantInt(module_.types.i32(), object.alignment);
    auto result = builder_.emitCall(runtime_.alloc, {bytes, alignment});
    IrBuilder::requireIr(result.has_value(), "allocator has no return value");
    return *result;
}

Operand CloneHelper::fieldAddress(semantic::TyId owner, Operand address, uint32_t ordinal) {
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

void CloneHelper::copyStorage(semantic::TyId type, Operand destination, Operand source) {
    TypeId storage = types_.storageType(type);
    TypeLayout object = objectLayout(type);
    auto alignment = static_cast<uint8_t>(object.alignment);
    Operand value = builder_.emitLoad(storage, source, alignment);
    builder_.emitStore(value, destination, alignment);
}

void CloneHelper::emitChild(semantic::TyId type, Operand destination, Operand source) {
    FunctionId function = request(type);
    builder_.emitCall(function, {destination, source});
}

void CloneHelper::cloneRange(semantic::TyId element_type, Operand destination, Operand source, Operand count) {
    TypeId i32 = module_.types.i32();
    IrBuilder::requireIr(count.type == i32, "clone range count must be usize");
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
    Operand condition = builder_.emitICmp(Predicate::Ult, index, count);
    builder_.emitCondBr(condition, body, done);
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, body);
    TypeId storage = types_.storageType(element_type);
    Operand destination_element = builder_.emitGep(storage, destination, {index});
    Operand source_element = builder_.emitGep(storage, source, {index});
    emitChild(element_type, destination_element, source_element);
    Operand next = builder_.emitBinary(OpCode::Add, index, one);
    builder_.emitStore(next, counter, 4);
    builder_.emitBr(header);
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, done);
}

void CloneHelper::emitArray(semantic::TyId owner, const semantic::ArrayTy& array, Operand destination, Operand source) {
    objectLayout(owner);
    TypeId storage = types_.storageType(owner);
    TypeId i32 = module_.types.i32();
    Operand zero = builder_.constantInt(i32, 0);
    Operand destination_begin = builder_.emitGep(
        storage,
        destination,
        {zero, zero}
    );
    Operand source_begin = builder_.emitGep(
        storage,
        source,
        {zero, zero}
    );
    Operand count = builder_.constantInt(i32, array.len);
    cloneRange(array.elem, destination_begin, source_begin, count);
}

void CloneHelper::emitStruct(semantic::TyId owner, const semantic::StructTy& structure, Operand destination, Operand source) {
    objectLayout(owner);
    const auto* info = model_.findStruct(structure.def);
    IrBuilder::requireIr(info != nullptr, "clone struct has no semantic definition");
    IrBuilder::requireIr(info->derives.valid.has(semantic::DeriveKind::Clone), "struct does not support derived Clone");
    for (const auto& field: info->fields) {
        IrBuilder::requireIr(field.ordinal <= std::numeric_limits<uint32_t>::max(), "clone field ordinal exceeds IR representation");
        auto ordinal = static_cast<uint32_t>(field.ordinal);
        Operand destination_field = fieldAddress(owner, destination, ordinal);
        Operand source_field = fieldAddress(owner, source, ordinal);
        emitChild(field.type, destination_field, source_field);
    }
}

void CloneHelper::emitBox(const semantic::BoxTy& box, Operand destination, Operand source) {
    Operand old_data = builder_.emitLoad(module_.types.ptr(), source, 4);
    Operand one = builder_.constantInt(module_.types.i32(), 1);
    Operand new_data = emitAllocate(box.elem, one);
    emitChild(box.elem, new_data, old_data);
    builder_.emitStore(new_data, destination, 4);
}

void CloneHelper::emitVec(semantic::TyId owner, const semantic::VecTy& vec, Operand destination, Operand source) {
    Operand destination_data = fieldAddress(owner, destination, 0);
    Operand destination_length = fieldAddress(owner, destination, 1);
    Operand destination_capacity = fieldAddress(owner, destination, 2);
    Operand source_data = fieldAddress(owner, source, 0);
    Operand source_length = fieldAddress(owner, source, 1);
    TypeId i32 = module_.types.i32();
    Operand zero = builder_.constantInt(i32, 0);
    builder_.emitStore(builder_.nullPointer(), destination_data, 4);
    builder_.emitStore(zero, destination_length, 4);
    builder_.emitStore(zero, destination_capacity, 4);
    Operand length = builder_.emitLoad(i32, source_length, 4);
    BlockId nonempty = builder_.createBlock(helper_function_);
    BlockId done = builder_.createBlock(helper_function_);
    Operand has_elements = builder_.emitICmp(Predicate::Ne, length, zero);
    builder_.emitCondBr(has_elements, nonempty, done);
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, nonempty);
    Operand old_data = builder_.emitLoad(module_.types.ptr(), source_data, 4);
    Operand new_data = emitAllocate(vec.elem, length);
    cloneRange(vec.elem, new_data, old_data, length);
    builder_.emitStore(new_data, destination_data, 4);
    builder_.emitStore(length, destination_length, 4);
    builder_.emitStore(length, destination_capacity, 4);
    builder_.emitBr(done);
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(helper_function_, done);
}

void CloneHelper::emitOne(semantic::TyId type, FunctionId function) {
    helper_function_ = function;
    IrBuilder::requireIr(module_.functions.at(function).blocks.empty(), "clone helper body already exists");
    BlockId entry = builder_.appendBlock(function);
    builder_.positionAtEnd(function, entry);
    Operand destination = builder_.parameter(function, 0);
    Operand source = builder_.parameter(function, 1);
    semantic::TyInfo info = model_.typeContext().get(type);
    if (std::holds_alternative<semantic::PrimaryTy>(info)) {
        copyStorage(type, destination, source);
    } else if (const auto* reference = std::get_if<semantic::RefTy>(&info)) {
        IrBuilder::requireIr(!reference->is_mut, "mutable reference does not support Clone");
        copyStorage(type, destination, source);
    } else if (const auto* array = std::get_if<semantic::ArrayTy>(&info)) {
        emitArray(type, *array, destination, source);
    } else if (const auto* structure = std::get_if<semantic::StructTy>(&info)) {
        emitStruct(type, *structure, destination, source);
    } else if (const auto* box = std::get_if<semantic::BoxTy>(&info)) {
        emitBox(*box, destination, source);
    } else if (const auto* vec = std::get_if<semantic::VecTy>(&info)) {
        emitVec(type, *vec, destination, source);
    } else {
        throw std::logic_error("semantic type cannot be lowered to a Clone helper");
    }
    builder_.emitRet(std::nullopt);
    builder_.clearInsertionPoint();
}

void CloneHelper::emitPending() {
    while (next_pending_ < pending_.size()) {
        semantic::TyId type = pending_[next_pending_];
        FunctionId function = functions_.at(type);
        emitOne(type, function);
        next_pending_++;
    }
}

} // namespace ir
