#pragma once
#include "ir/block.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include <iostream>

namespace ir {

class IrPrinter {
public:
    IrPrinter(const Module& module, std::ostream& os): module_(module), os_(os) {}
    void print();
private:
    const Module& module_;
    std::ostream& os_;

    void printStructDefinition(StructId id);
    void printFunction(const Function& func);
    void printBlock(const BasicBlock& block);

    void printType(TypeId type);
    void printOperandValue(const Operand& op);
    void printTypedOperand(const Operand& op);

    void printFunctionRef(FunctionId id);
    void printStructureRef(StructId id);
    void printValueRef(ValueId id);
    void printBlockName(BlockId id);
    void printBlockRef(BlockId id);
    void printQuotedString(const std::string& text);
    void printUnsigned(uint32_t value);

    void printInstruction(const Instruction& inst);
    void printAlloca(const Alloca& inst);
    void printLoad(const Load& inst);
    void printStore(const Store& inst);
    void printBinary(const Binary& inst);
    void printICmp(const ICmp& inst);
    void printZExt(const ZExt& inst);
    void printTrunc(const Trunc& inst);
    void printGep(const Gep& inst);
    void printCall(const Call& inst);

    void printTerminator(const Terminator& term);
    void printBr(const Br& term);
    void printCondBr(const CondBr& term);
    void printRet(const Ret& term);
    void printUnreachable(const Unreachable& term);
};

} // namespace ir