#include "ir/builder.hpp"
#include "ir/block.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/types.hpp"
#include <algorithm>
#include <optional>
#include <cassert>
#include <variant>
#include <stdexcept>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace ir {

void IrBuilder::requireIr(bool condition, const char *message) {
    if (!condition) {
        throw std::logic_error(message);
    }
}

bool IrBuilder::isValidAlign(uint8_t align) {
    return align != 0 && (align & (align - 1)) == 0;
}

bool IrBuilder::isValidTypeRef(TypeId id) {
    std::unordered_set<TypeId> seen;
    while (true) {
        if (!module_.types.contains(id) || !seen.insert(id).second) {
            return false;
        }
        Type type = module_.types.get(id);
        if (std::holds_alternative<VoidType>(type)) {
            return false;
        }
        if (const auto* array = std::get_if<ArrayType>(&type)) {
            id = array->element;
            continue;
        }
        if (const auto* named = std::get_if<NamedStructType>(&type)) {
            return named->id < module_.structs.size();
        }
        return !std::holds_alternative<IntegerType>(type) || module_.types.isInteger(id);
    }
}

bool IrBuilder::isSized(TypeId id) const {
    enum class Mark { Active, Sized, Unsized };
    std::unordered_map<TypeId, Mark> marks;
    std::function<bool(TypeId)> visit = [&](TypeId id) -> bool {
        if (!module_.types.contains(id)) {
            return false;
        }
        if (auto it = marks.find(id); it != marks.end()) {
            return it->second == Mark::Sized;
        }
        marks[id] = Mark::Active;
        Type type = module_.types.get(id);
        bool ok = false;
        if (std::holds_alternative<PointerType>(type)) {
            ok = true;
        } else if (std::holds_alternative<IntegerType>(type)) {
            ok = module_.types.isInteger(id);
        } else if (const auto* arr = std::get_if<ArrayType>(&type)) {
            ok = visit(arr->element);
        } else if (const auto* named = std::get_if<NamedStructType>(&type)) {
            if (named->id < module_.structs.size()) {
                const auto& fields = module_.structs[named->id].fields;
                ok = fields.has_value() && std::all_of(fields->begin(), fields->end(), visit);
            }
        }
        marks[id] = ok ? Mark::Sized : Mark::Unsized;
        return ok;
    };
    return visit(id);
}

void IrBuilder::validateOperand(const Operand& operand, const Function& func) const {
    requireIr(module_.types.contains(operand.type), "invalid operand type");
    if (const auto* local = std::get_if<LocalValue>(&operand.data)) {
        requireIr(local->func == func.id, "value belongs to another function");
        requireIr(local->id < func.value_types.size(), "local ID out of range");
        requireIr(func.value_types[local->id] == operand.type, "local value type mismatch");
    } else if (const auto* imm = std::get_if<Immediate>(&operand.data)) {
        requireIr(module_.types.isInteger(operand.type), "integer constant has non-integer type");
        if (operand.type == module_.types.i1()) {
            requireIr(imm->bits <= 1, "i1 constant is out of range");
        } else if (operand.type == module_.types.i8()) {
            requireIr(imm->bits <= 0xFFu, "i8 constant is out of range");
        }
    } else {
        requireIr(operand.type == module_.types.ptr(), "null has non-pointer type");
    }
}

std::pair<ValueId, Operand> IrBuilder::newValue(Function& func, TypeId type) {
    ValueId id = func.value_types.size();
    func.value_types.push_back(type);
    return {
        id,
        Operand{
            type,
            LocalValue{
                id,
                func.id
            }
        }
    };
}

std::pair<Function&, BasicBlock&> IrBuilder::get() {
    requireIr(current_function_.has_value(), "current function is empty");
    requireIr(current_block_.has_value(), "current block is empty");
    requireIr(*current_function_ < module_.functions.size(), "function ID out of range");
    auto& func = module_.functions[*current_function_];
    requireIr(func.form == FunctionForm::Definition, "get requires a definition");
    requireIr(*current_block_ < func.blocks.size(), "block ID out of range");
    auto& block = func.blocks[*current_block_];
    requireIr(!block.terminator.has_value(), "block already terminated");
    return {func, block};
}

StructId IrBuilder::declareStruct(const std::string& name) {
    requireIr(
        std::none_of(module_.structs.begin(), module_.structs.end(),
                    [&](const StructDef& s) { return s.name == name; }),
        "duplicate struct name"
    );
    StructId id = module_.structs.size();
    module_.structs.push_back(
        StructDef{
            name,
            std::nullopt
        }
    );
    return id;
}

void IrBuilder::defineStruct(StructId id, std::vector<TypeId> fields) {
    requireIr(id < module_.structs.size(), "struct ID out of range");
    requireIr(!module_.structs.at(id).fields.has_value(), "struct already defined");
    for (TypeId field : fields) {
        requireIr(isValidTypeRef(field), "invalid struct field type");
        requireIr(field != module_.types.none(), "void struct field");
    }
    module_.structs[id].fields = std::move(fields);
}

FunctionId IrBuilder::addFunction(const std::string& name, Linkage linkage, FunctionForm form, FunctionSignature signature) {
    requireIr(
        std::none_of(module_.functions.begin(), module_.functions.end(),
                    [&](const Function& f) { return f.symbol_name == name; }),
        "duplicate function symbol"
    );
    requireIr(module_.types.contains(signature.return_type), "invalid return type");
    requireIr(signature.return_type == module_.types.none() || isSized(signature.return_type), "return type is not sized");
    for (TypeId param : signature.parameters) {
        requireIr(module_.types.contains(param), "invalid parameter type");
        requireIr(param != module_.types.none(), "void parameter");
        requireIr(isSized(param), "parameter type is not sized");
    }
    FunctionId id = module_.functions.size();
    Function func = {
        id,
        name,
        linkage,
        signature,
        form,
        {},
        {},
        {}
    };
    for (auto param_type: signature.parameters) {
        ValueId id = func.value_types.size();
        Parameter parameter{id, param_type};
        func.parameters.push_back(parameter);
        func.value_types.push_back(param_type);
    }
    module_.functions.push_back(func);
    current_function_ = id;
    current_block_ = std::nullopt;
    return id;
}

BlockId IrBuilder::appendBlock(FunctionId function) {
    requireIr(function < module_.functions.size(), "function ID out of range");
    auto& func = module_.functions[function];
    requireIr(func.form == FunctionForm::Definition, "appending block after a declaration");
    BlockId id = func.blocks.size();
    func.blocks.push_back(BasicBlock{id, {}, std::nullopt});
    current_function_ = function;
    current_block_ = id;
    return id;
}

void IrBuilder::positionAtEnd(FunctionId function, BlockId block) {
    requireIr(function < module_.functions.size(), "function ID out of range");
    auto& func = module_.functions[function];
    requireIr(func.form == FunctionForm::Definition, "positioning a block in a declaration");
    requireIr(block < func.blocks.size(), "block ID out of range");
    requireIr(!func.blocks[block].terminator.has_value(), "block already terminated");
    current_function_ = function;
    current_block_ = block;
}

void IrBuilder::clearInsertionPoint() {
    current_block_ = std::nullopt;
}

bool IrBuilder::hasInsertionPoint() const {
    if (current_function_ == std::nullopt || current_block_ == std::nullopt) {
        return false;
    }
    requireIr(current_function_ < module_.functions.size(), "current function out of range");
    const auto& func = module_.functions[*current_function_];
    if (func.form == FunctionForm::Declaration) {
        return false;
    }
    requireIr(current_block_ < func.blocks.size(), "current block out of range");
    return !func.blocks[*current_block_].terminator.has_value();
}

Operand IrBuilder::parameter(FunctionId function, size_t index) const {
    requireIr(function < module_.functions.size(), "current function out of range");
    const auto& func = module_.functions[function];
    requireIr(index < func.parameters.size(), "parameter index out of range");
    const auto& param = func.parameters[index];
    return Operand{
        param.type,
        LocalValue{param.value, function}
    };
}

Operand IrBuilder::constantInt(TypeId type, uint32_t bits) const {
    requireIr(module_.types.isInteger(type), "constant integer must be integer typed");
    if (type == module_.types.i1()) {
        bits = bits & 1u;
    } else if (type == module_.types.i8()) {
        bits = bits & 0xFF;
    }
    return Operand{
        type,
        Immediate{bits}
    };
}

Operand IrBuilder::nullPointer() const {
    return Operand{
        module_.types.ptr(),
        NullPointer{}
    };
}

Operand IrBuilder::emitAlloca(TypeId type, uint8_t align) {
    requireIr(isValidAlign(align), "align has invalid value");
    requireIr(isSized(type), "alloca requires a sized type");
    auto [func, block] = get();
    auto [id, result] = newValue(func, module_.types.ptr());
    Instruction inst = {
        id,
        Alloca{
            type,
            align
        }
    };
    block.instructions.push_back(inst);
    return result;
}

Operand IrBuilder::emitEntryAlloca(TypeId type, uint8_t align) {
    requireIr(isValidAlign(align), "align has invalid value");
    requireIr(isSized(type), "alloca requires a sized type");
    requireIr(current_function_.has_value(), "current function is empty");
    requireIr(*current_function_ < module_.functions.size(), "function ID out of range");
    auto& func = module_.functions[*current_function_];
    requireIr(func.form == FunctionForm::Definition, "cannot emit instructions into a declaration");
    requireIr(!func.blocks.empty(), "function has no blocks to enter");
    auto& block = func.blocks.front();
    auto pos = block.instructions.begin();
    while (pos != block.instructions.end() && std::holds_alternative<Alloca>(pos->data)) {
        pos++;
    }
    auto [id, result] = newValue(func, module_.types.ptr());
    Instruction inst = {
        id,
        Alloca{
            type,
            align
        }
    };
    block.instructions.insert(pos, inst);
    return result;
}

Operand IrBuilder::emitLoad(TypeId type, Operand address, uint8_t align) {
    requireIr(isValidAlign(align), "align has invalid value");
    requireIr(isSized(type), "memory value must be sized");
    requireIr(module_.types.isPointer(address.type), "address is not a pointer");
    auto [func, block] = get();
    validateOperand(address, func);
    auto [id, result] = newValue(func, type);
    Instruction inst = {
        id,
        Load{
            type,
            address,
            align
        }
    };
    block.instructions.push_back(inst);
    return result;
}

void IrBuilder::emitStore(Operand value, Operand address, uint8_t align) {
    requireIr(isValidAlign(align), "align has invalid value");
    requireIr(isSized(value.type), "memory value must be sized");
    requireIr(module_.types.isPointer(address.type), "address is not a pointer");
    auto [func, block] = get();
    validateOperand(value, func);
    validateOperand(address, func);
    Instruction inst = {
        std::nullopt,
        Store{
            value,
            address,
            align
        }
    };
    block.instructions.push_back(inst);
}

Operand IrBuilder::emitBinary(OpCode op, Operand lhs, Operand rhs) {
    auto [func, block] = get();
    validateOperand(lhs, func);
    validateOperand(rhs, func);
    requireIr(lhs.type == rhs.type, "binary requires same operand types");
    requireIr(module_.types.isInteger(lhs.type), "binary requires integer operands");
    auto [id, result] = newValue(func, lhs.type);
    Instruction inst = {
        id,
        Binary{
            op,
            lhs,
            rhs
        }
    };
    block.instructions.push_back(inst);
    return result;
}

Operand IrBuilder::emitICmp(Predicate pred, Operand lhs, Operand rhs) {
    auto [func, block] = get();
    validateOperand(lhs, func);
    validateOperand(rhs, func);
    requireIr(lhs.type == rhs.type, "icmp requires same operand types");
    requireIr(module_.types.isInteger(lhs.type), "icmp requires integer operand type");
    auto [id, result] = newValue(func, module_.types.i1());
    Instruction inst = {
        id,
        ICmp{
            pred,
            lhs,
            rhs
        }
    };
    block.instructions.push_back(inst);
    return result;
}

Operand IrBuilder::emitZExt(Operand value, TypeId target) {
    auto [func, block] = get();
    validateOperand(value, func);
    requireIr(module_.types.isInteger(value.type), "zext requires integer operand type");
    requireIr(module_.types.isInteger(target), "zext requires integer target type");
    requireIr(module_.types.integerSize(target) > module_.types.integerSize(value.type), "zext requires an extended integer type");
    auto [id, result] = newValue(func, target);
    Instruction inst = {
        id,
        ZExt{
            value,
            target
        }
    };
    block.instructions.push_back(inst);
    return result;
}

Operand IrBuilder::emitTrunc(Operand value, TypeId target) {
    auto [func, block] = get();
    validateOperand(value, func);
    requireIr(module_.types.isInteger(value.type), "trunc requires integer operand type");
    requireIr(module_.types.isInteger(target), "trunc requires integer target type");
    requireIr(module_.types.integerSize(target) < module_.types.integerSize(value.type), "trunc requires a shrunk integer type");
    auto [id, result] = newValue(func, target);
    Instruction inst = {
        id,
        Trunc{
            value,
            target
        }
    };
    block.instructions.push_back(inst);
    return result;
}

Operand IrBuilder::emitGep(TypeId src, Operand base, const std::vector<Operand>& indices) {
    auto [func, block] = get();
    validateOperand(base, func);
    for (const auto& op: indices) {
        validateOperand(op, func);
    }
    requireIr(module_.types.contains(src), "invalid gep source type");
    requireIr(isSized(src), "gep source type is not sized");
    requireIr(module_.types.isPointer(base.type), "gep requires pointer base type");
    requireIr(src != module_.types.none(), "gep cannot get a void type");
    if (!indices.empty()) {
        requireIr(module_.types.isInteger(indices.front().type), "gep requires first index to be an integer type");
    }
    if (indices.size() >= 2) {
        TypeId cur_typeid = src;
        for (size_t i = 1; i < indices.size(); i++) {
            Type cur_type = module_.types.get(cur_typeid);
            if (const auto* arr = std::get_if<ArrayType>(&cur_type)) {
                requireIr(module_.types.isInteger(indices[i].type), "array index must be an integer");
                cur_typeid = arr->element;
            } else if (const auto* stru = std::get_if<NamedStructType>(&cur_type)) {
                requireIr(indices[i].type == module_.types.i32(), "struct index must be an i32 type");
                const auto* imm = std::get_if<Immediate>(&indices[i].data);
                requireIr(stru->id < module_.structs.size(), "struct ID out of range");
                const auto& str = module_.structs[stru->id];
                requireIr(imm != nullptr && str.fields.has_value() && imm->bits < str.fields->size(), "struct field index out of range");
                const auto& sel = str.fields->at(imm->bits);
                cur_typeid = sel;
            } else {
                requireIr(false, "invalid indexed type");
            }
        }
    }
    auto [id, result] = newValue(func, module_.types.ptr());
    Instruction inst = {
        id,
        Gep{
            src,
            base,
            indices
        }
    };
    block.instructions.push_back(inst);
    return result;
}

std::optional<Operand> IrBuilder::emitCall(FunctionId callee, const std::vector<Operand>& args) {
    auto [func, block] = get();
    for (const auto& op: args) {
        validateOperand(op, func);
    }
    requireIr(callee < module_.functions.size(), "callee ID out of range");
    auto callee_func = module_.functions[callee];
    requireIr(callee_func.signature.parameters.size() == args.size(), "argument count mismatch");
    for (size_t i = 0; i < args.size(); i++) {
        requireIr(callee_func.signature.parameters[i] == args[i].type, "argument type mismatch");
    }
    bool has_return = callee_func.signature.return_type != module_.types.none();
    if (has_return) {
        auto [id, result] = newValue(func, callee_func.signature.return_type);
        Instruction inst = {
            std::optional<ValueId>(id),
            Call{
                callee,
                args
            }
        };
        block.instructions.push_back(inst);
        return result;
    } else {
        Instruction inst = {
            std::nullopt,
            Call{
                callee,
                args
            }
        };
        block.instructions.push_back(inst);
        return std::nullopt;
    }
}

void IrBuilder::emitBr(BlockId target) {
    auto [func, block] = get();
    requireIr(!block.terminator.has_value(), "block already terminated");
    requireIr(target < func.blocks.size(), "target ID out of range");
    block.terminator = Br{target};
}

void IrBuilder::emitCondBr(Operand condition, BlockId yes, BlockId no) {
    auto [func, block] = get();
    validateOperand(condition, func);
    requireIr(!block.terminator.has_value(), "block already terminated");
    requireIr(condition.type == module_.types.i1(), "condbr must have i1-typed condition");
    requireIr(yes < func.blocks.size(), "yes block ID out of range");
    requireIr(no < func.blocks.size(), "no block ID out of range");
    block.terminator = CondBr{condition, yes, no};
}

void IrBuilder::emitRet(std::optional<Operand> value) {
    auto [func, block] = get();
    requireIr(!block.terminator.has_value(), "block already terminated");
    requireIr((!value.has_value() && func.signature.return_type == module_.types.none()) ||
              (value.has_value() && value->type == func.signature.return_type),
              "ret has invalid return value");
    if (value.has_value()) {
        validateOperand(*value, func);
    }
    block.terminator = Ret{value};
}

void IrBuilder::emitUnreachable() {
    auto [funn, block] = get();
    requireIr(!block.terminator.has_value(), "block already terminated");
    block.terminator = Unreachable{};
}

} // namespace ir
