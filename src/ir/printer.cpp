#include "printer.hpp"
#include "ir/block.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/types.hpp"
#include <variant>

namespace ir {

void IrPrinter::print() {
    os_ << "target triple = \"" << module_.target_triple << "\"\n";
    os_ << "target datalayout = \"" << module_.data_layout << "\"\n";
    os_ << '\n';
    for (size_t i = 0; i < module_.structs.size(); i++) {
        printStructDefinition(i);
    }
    if (module_.structs.size()) {
        os_ << '\n';
    }
    bool printed = false;
    for (size_t i = 0; i < module_.functions.size(); i++) {
        if (module_.functions[i].form == FunctionForm::Declaration) {
            printFunction(module_.functions[i]);
            printed = true;
        }
    }
    if (printed) {
        os_ << '\n';
    }
    for (size_t i = 0; i < module_.functions.size(); i++) {
        if (module_.functions[i].form == FunctionForm::Definition) {
            printFunction(module_.functions[i]);
        }
    }
}

void IrPrinter::printStructDefinition(StructId id) {
    const StructDef& def = module_.structs[id];
    printStructureRef(id);
    os_ << " = type ";
    if (!def.fields.has_value()) {
        os_ << "opaque";
        return;
    }
    os_ << "{ ";
    for (size_t i = 0; i < def.fields->size(); i++) {
        if (i) {
            os_ << ", ";
        }
        printType(def.fields->at(i));
    }
    os_ << " }\n";
}

void IrPrinter::printFunction(const Function& func) {
    if (func.form == FunctionForm::Declaration) {
        os_ << "declare ";
        printType(func.signature.return_type);
        os_ << ' ';
        printFunctionRef(func.id);
        os_ << '(';
        for (size_t i = 0; i < func.signature.parameters.size(); i++) {
            if (i) {
                os_ << ", ";
            }
            printType(func.signature.parameters.at(i));
        }
        os_ << ")\n";
    } else if (func.form == FunctionForm::Definition) {
        os_ << "define ";
        if (func.linkage == Linkage::Internal) {
            os_ << "internal ";
        } else {
            os_ << "external ";
        }
        printType(func.signature.return_type);
        os_ << ' ';
        printFunctionRef(func.id);
        os_ << '(';
        for (size_t i = 0; i < func.signature.parameters.size(); i++) {
            if (i) {
                os_ << ", ";
            }
            printTypedOperand(
                Operand{
                    func.parameters.at(i).type,
                    LocalValue{
                        func.parameters.at(i).value,
                        func.id
                    }
                }
            );
        }
        os_ << ") {\n";
        for (const auto& bb: func.blocks) {
            printBlock(bb);
            os_ << '\n';
        }
        os_ << "}\n";
    }
}

void IrPrinter::printBlock(const BasicBlock& block) {
    printBlockName(block.id);
    os_ << ":\n";
    for (const auto& inst: block.instructions) {
        os_ << '\t';
        printInstruction(inst);
    }
    os_ << '\t';
    if (block.terminator.has_value()) {
        printTerminator(*block.terminator);
    }
}

void IrPrinter::printType(TypeId type) {
    const Type& ty = module_.types.get(type);
    if (std::holds_alternative<VoidType>(ty)) {
        os_ << "void";
    } else if (const auto* integer = std::get_if<IntegerType>(&ty)) {
        os_ << 'i' << static_cast<int>(integer->bits);
    } else if (std::holds_alternative<PointerType>(ty)) {
        os_ << "ptr";
    } else if (const auto* arr = std::get_if<ArrayType>(&ty)) {
        os_ << '[' << arr->length << " x ";
        printType(arr->element);
        os_ << ']';
    } else if (const auto* named = std::get_if<NamedStructType>(&ty)) {
        printStructureRef(named->id);
    }
}

void IrPrinter::printOperandValue(const Operand& op) {
    if (const auto* imm = std::get_if<Immediate>(&op.data)) {
        os_ << imm->bits;
    } else if (const auto* local = std::get_if<LocalValue>(&op.data)) {
        os_ << "%v" << local->id;
    } else if (std::holds_alternative<NullPointer>(op.data)) {
        os_ << "null";
    }
}

void IrPrinter::printTypedOperand(const Operand& op) {
    printType(op.type);
    os_ << ' ';
    printOperandValue(op);
}

void IrPrinter::printFunctionRef(FunctionId id) {
    const Function& func = module_.functions[id];
    os_ << "@";
    printQuotedString(func.symbol_name);
}

void IrPrinter::printStructureRef(StructId id) {
    const StructDef& def = module_.structs[id];
    os_ << "%";
    printQuotedString(def.name);
}

void IrPrinter::printValueRef(ValueId id) {
    os_ << "%v" << id;
}

void IrPrinter::printBlockName(BlockId id) {
    os_ << "bb" << id;
}

void IrPrinter::printBlockRef(BlockId id) {
    os_ << "%bb" << id;
}

void IrPrinter::printQuotedString(const std::string& text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    os_.put('\"');
    for (unsigned char ch: text) {
        if (ch == '\"' || ch == '\\' || ch < 0x20 || ch == 0x7F) {
            os_.put('\\');
            os_.put(hex[ch >> 4]);
            os_.put(hex[ch & 0xF]);
        } else {
            os_.put(static_cast<char>(ch));
        }
    }
    os_.put('\"');
}

void IrPrinter::printUnsigned(uint32_t value) {
    os_ << value;
}

void IrPrinter::printInstruction(const Instruction& inst) {
    if (inst.result.has_value()) {
        printValueRef(*inst.result);
        os_ << " = ";
    }
    if (const auto* alloca = std::get_if<Alloca>(&inst.data)) {
        printAlloca(*alloca);
    } else if (const auto* load = std::get_if<Load>(&inst.data)) {
        printLoad(*load);
    } else if (const auto* store = std::get_if<Store>(&inst.data)) {
        printStore(*store);
    } else if (const auto* binary = std::get_if<Binary>(&inst.data)) {
        printBinary(*binary);
    } else if (const auto* icmp = std::get_if<ICmp>(&inst.data)) {
        printICmp(*icmp);
    } else if (const auto* zext = std::get_if<ZExt>(&inst.data)) {
        printZExt(*zext);
    } else if (const auto* trunc = std::get_if<Trunc>(&inst.data)) {
        printTrunc(*trunc);
    } else if (const auto* gep = std::get_if<Gep>(&inst.data)) {
        printGep(*gep);
    } else if (const auto* call = std::get_if<Call>(&inst.data)) {
        printCall(*call);
    }
    os_ << '\n';
}

void IrPrinter::printAlloca(const Alloca& inst) {
    os_ << "alloca ";
    printType(inst.type);
    os_ << ", align " << static_cast<int>(inst.align);
}

void IrPrinter::printLoad(const Load& inst) {
    os_ << "load ";
    printType(inst.type);
    os_ << ", ";
    printTypedOperand(inst.address);
    os_ << ", align " << static_cast<int>(inst.align);
}

void IrPrinter::printStore(const Store& inst) {
    os_ << "store ";
    printTypedOperand(inst.value);
    os_ << ", ";
    printTypedOperand(inst.address);
    os_ << ", align " << static_cast<int>(inst.align);
}

void IrPrinter::printBinary(const Binary& inst) {
    switch (inst.opcode) {
        case OpCode::Add:
            os_ << "add"; break;
        case OpCode::Sub:
            os_ << "sub"; break;
        case OpCode::Mul:
            os_ << "mul"; break;
        case OpCode::Udiv:
            os_ << "udiv"; break;
        case OpCode::Sdiv:
            os_ << "sdiv"; break;
        case OpCode::Urem:
            os_ << "urem"; break;
        case OpCode::Srem:
            os_ << "srem"; break;
        case OpCode::Shl:
            os_ << "shl"; break;
        case OpCode::Lshr:
            os_ << "lshr"; break;
        case OpCode::Ashr:
            os_ << "ashr"; break;
        case OpCode::And:
            os_ << "and"; break;
        case OpCode::Or:
            os_ << "or"; break;
        case OpCode::Xor:
            os_ << "xor"; break;
    }
    os_ << ' ';
    printTypedOperand(inst.lhs);
    os_ << ", ";
    printOperandValue(inst.rhs);
}

void IrPrinter::printICmp(const ICmp& inst) {
    os_ << "icmp ";
    switch (inst.predicate) {
        case Predicate::Eq:
            os_ << "eq"; break;
        case Predicate::Ne:
            os_ << "ne"; break;
        case Predicate::Ugt:
            os_ << "ugt"; break;
        case Predicate::Uge:
            os_ << "uge"; break;
        case Predicate::Ult:
            os_ << "ult"; break;
        case ir::Predicate::Ule:
            os_ << "ule"; break;
        case Predicate::Sgt:
            os_ << "sgt"; break;
        case Predicate::Sge:
            os_ << "sge"; break;
        case Predicate::Slt:
            os_ << "slt"; break;
        case Predicate::Sle:
            os_ << "sle"; break;
    }
    os_ << ' ';
    printTypedOperand(inst.lhs);
    os_ << ", ";
    printOperandValue(inst.rhs);
}

void IrPrinter::printZExt(const ZExt& inst) {
    os_ << "zext ";
    printTypedOperand(inst.value);
    os_ << " to ";
    printType(inst.type);
}

void IrPrinter::printTrunc(const Trunc& inst) {
    os_ << "trunc ";
    printTypedOperand(inst.value);
    os_ << " to ";
    printType(inst.type);
}

void IrPrinter::printGep(const Gep& inst) {
    os_ << "getelementptr ";
    printType(inst.type);
    os_ << ", ";
    printTypedOperand(inst.base);
    for (const auto& idx: inst.indices) {
        os_ << ", ";
        printTypedOperand(idx);
    }
}

void IrPrinter::printCall(const Call& inst) {
    os_ << "call ";
    const Function& callee = module_.functions.at(inst.func);
    printType(callee.signature.return_type);
    os_ << ' ';
    printFunctionRef(inst.func);
    os_ << '(';
    for (size_t i = 0; i < inst.args.size(); i++) {
        if (i) {
            os_ << ", ";
        }
        printTypedOperand(inst.args[i]);
    }
    os_ << ')';
}

void IrPrinter::printTerminator(const Terminator& term) {
    if (const auto* br = std::get_if<Br>(&term)) {
        printBr(*br);
    } else if (const auto* condbr = std::get_if<CondBr>(&term)) {
        printCondBr(*condbr);
    } else if (const auto* ret = std::get_if<Ret>(&term)) {
        printRet(*ret);
    } else if (const auto* unr = std::get_if<Unreachable>(&term)) {
        printUnreachable(*unr);
    }
}

void IrPrinter::printBr(const Br& term) {
    os_ << "br label ";
    printBlockRef(term.target);
}

void IrPrinter::printCondBr(const CondBr& term) {
    os_ << "br ";
    printTypedOperand(term.condition);
    os_ << ", label ";
    printBlockRef(term.yes);
    os_ << ", label ";
    printBlockRef(term.no);
}

void IrPrinter::printRet(const Ret& term) {
    os_ << "ret ";
    if (!term.value.has_value()) {
        os_ << "void";
    } else {
        printTypedOperand(*term.value);
    }
}

void IrPrinter::printUnreachable(const Unreachable& term) {
    os_ << "unreachable";
}

} // namespace ir