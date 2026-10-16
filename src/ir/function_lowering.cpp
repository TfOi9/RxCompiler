#include "ir/function_lowering.hpp"
#include "ir/builder.hpp"
#include "ir/instruction.hpp"
#include "ir/layout.hpp"
#include "ir/module_lowering.hpp"
#include "semantic/semantic_ids.hpp"
#include <limits>
#include <optional>

namespace ir {

LocalSlot& FunctionLowering::ensureLocalSlot(semantic::LocalId id) {
    IrBuilder::requireIr(id < locals_.size(), "local ID out of range");
    if (locals_[id].has_value()) {
        return *locals_[id];
    }
    const auto& info = body_.locals.at(id);
    TypeId storage = types_.storageType(info.type);
    TypeLayout object_layout = layout_.layoutOf(storage);
    IrBuilder::requireIr(object_layout.alignment <= std::numeric_limits<uint8_t>::max(), "local alignment exceeds IR representation");
    auto alignment = static_cast<uint8_t>(object_layout.alignment);
    Operand address = builder_.emitEntryAlloca(storage, alignment);
    locals_[id].emplace(LocalSlot{address, storage, alignment});
    return *locals_[id];
}

const LocalSlot& FunctionLowering::localSlot(semantic::LocalId id) const {
    IrBuilder::requireIr(id < locals_.size(), "local ID out of range");
    IrBuilder::requireIr(locals_[id].has_value(), "local storage has not been generated");
    return *locals_[id];
}

Operand FunctionLowering::loadLocal(semantic::LocalId id) {
    const LocalSlot& slot = localSlot(id);
    TypeId value_type = types_.valueType(body_.locals.at(id).type);
    Operand value = builder_.emitLoad(slot.storage_type, slot.address, slot.alignment);
    if (slot.storage_type == value_type) {
        return value;
    }
    IrBuilder::requireIr(slot.storage_type == module_.types.i8() && value_type == module_.types.i1(), "unsupported local value conversion");
    return builder_.emitTrunc(value, value_type);
}

void FunctionLowering::storeLocal(semantic::LocalId id, Operand value) {
    const LocalSlot& slot = localSlot(id);
    TypeId value_type = types_.valueType(body_.locals.at(id).type);
    IrBuilder::requireIr(value_type == value.type, "local storage type mismatch");
    if (slot.storage_type != value_type) {
        IrBuilder::requireIr(value_type == module_.types.i1() && slot.storage_type == module_.types.i8(), "unsupported local store conversion");
        value = builder_.emitZExt(value, slot.storage_type);
    }
    builder_.emitStore(value, slot.address, slot.alignment);
}

void FunctionLowering::bindParameters() {
    const auto& params = function_.signature.parameters;
    IrBuilder::requireIr(body_.parameter_locals.size() == params.size(), "parameter local count mismatch");
    for (size_t i = 0; i < params.size(); i++) {
        semantic::LocalId local = body_.parameter_locals[i];
        ensureLocalSlot(local);
        Operand incoming = builder_.parameter(ir_function_, i);
        storeLocal(local, incoming);
    }
}

void FunctionLowering::finishReturn(const BodyResult& result) {
    if (!result.reaches_end) {
        IrBuilder::requireIr(!result.value.has_value(), "non-fallthrough body has a value");
        IrBuilder::requireIr(!builder_.hasInsertionPoint(), "non-fallthrough body has an open block");
        return;
    }
    IrBuilder::requireIr(builder_.hasInsertionPoint(), "fallthrough body has no insertion point");
    TypeId return_type = types_.returnType(function_.signature.return_type);
    if (return_type == module_.types.none()) {
        IrBuilder::requireIr(!result.value.has_value(), "unit function body has an IR value");
        builder_.emitRet(std::nullopt);
        return;    
    }
    IrBuilder::requireIr(result.value.has_value(), "non-unit function body has no return value");
    builder_.emitRet(result.value);
}

} // namespace ir