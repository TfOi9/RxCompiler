#pragma once
#include "ir/block.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include <optional>
#include <string>
#include <utility>

namespace ir {

class IrBuilder {
public:
    explicit IrBuilder(Module& module): module_(module) {}

    static void requireIr(bool condition, const char *message);
    static bool isValidAlign(uint8_t align);
    
    bool isValidTypeRef(TypeId id);
    bool isSized(TypeId id) const;

    StructId declareStruct(const std::string& name);
    void defineStruct(StructId id, std::vector<TypeId> fields);
    FunctionId addFunction(const std::string& name, Linkage linkage, FunctionForm form, FunctionSignature signature);

    BlockId appendBlock(FunctionId function);
    void positionAtEnd(FunctionId function, BlockId block);
    void clearInsertionPoint();
    bool hasInsertionPoint() const;
    Operand parameter(FunctionId function, size_t index) const;

    Operand constantInt(TypeId type, uint32_t bits) const;
    Operand nullPointer() const;

    Operand emitAlloca(TypeId type, uint8_t align);
    Operand emitEntryAlloca(TypeId type, uint8_t align);
    Operand emitLoad(TypeId type, Operand address, uint8_t align);
    void emitStore(Operand value, Operand address, uint8_t align);
    Operand emitBinary(OpCode op, Operand lhs, Operand rhs);
    Operand emitICmp(Predicate pred, Operand lhs, Operand rhs);
    Operand emitZExt(Operand value, TypeId target);
    Operand emitTrunc(Operand value, TypeId target);
    Operand emitGep(TypeId src, Operand base, const std::vector<Operand>& indices);
    std::optional<Operand> emitCall(FunctionId callee, const std::vector<Operand>& args);

    void emitBr(BlockId target);
    void emitCondBr(Operand condition, BlockId yes, BlockId no);
    void emitRet(std::optional<Operand> value);
    void emitUnreachable();

private:
    Module& module_;
    std::optional<FunctionId> current_function_;
    std::optional<BlockId> current_block_;

    std::pair<Function&, BasicBlock&> get();
    void validateOperand(const Operand& operand, const Function& func) const;
    std::pair<ValueId, Operand> newValue(Function& func, TypeId type);
};

} // namespace ir