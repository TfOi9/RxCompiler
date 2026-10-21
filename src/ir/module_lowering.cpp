#include "ir/module_lowering.hpp"
#include "ir/builder.hpp"
#include "ir/function.hpp"
#include "ir/ir_ids.hpp"
#include "ir/function_lowering.hpp"
#include "ir/clone_helper.hpp"
#include "ir/equality_helper.hpp"
#include "semantic/function_resolver.hpp"
#include <stdexcept>

namespace ir {

void ModuleLowering::lower() {
    initializeTarget();
    registerStructs();
    declareRuntime();
    registerSourceFunctions();
    registerMachineMain();
    CloneHelper clone_helper(model_, module_, types_, layout_, runtime);
    EqualityHelper equality_helper(model_, module_, types_, layout_);
    lowerFunctionBodies(clone_helper, equality_helper);
    emitMachineMain();
    clone_helper.emitPending();
    equality_helper.emitPending();
}

void ModuleLowering::initializeTarget() {
    module_.target_triple = "riscv32-unknown-elf";
    module_.data_layout = "e-m:e-p:32:32-i64:64-n32-S128";
}

void ModuleLowering::registerStructs() {
    for (const auto& stru: model_.structIds()) {
        types_.declareStruct(stru);
    }
    for (const auto& stru: model_.structIds()) {
        types_.defineStruct(stru);
    }
}

void ModuleLowering::declareRuntime() {
    auto get_i32_id = builder_.addFunction("get_i32", Linkage::External, FunctionForm::Declaration, FunctionSignature{module_.types.i32(), {}});
    auto print_i32_id = builder_.addFunction("print_i32", Linkage::External, FunctionForm::Declaration, FunctionSignature{module_.types.none(), {module_.types.i32()}});
    auto println_i32_id = builder_.addFunction("println_i32", Linkage::External, FunctionForm::Declaration, FunctionSignature{module_.types.none(), {module_.types.i32()}});
    functions_[0] = get_i32_id;
    functions_[1] = print_i32_id;
    functions_[2] = println_i32_id;
    runtime = {
        builder_.addFunction(
            "__rx_alloc",
            Linkage::External,
            FunctionForm::Declaration,
            FunctionSignature{
                module_.types.ptr(), 
                {
                    module_.types.i32(),
                    module_.types.i32()
                }
            }
        ),
        builder_.addFunction(
            "memcpy",
            Linkage::External,
            FunctionForm::Declaration,
            FunctionSignature{
                module_.types.ptr(),
                {
                    module_.types.ptr(),
                    module_.types.ptr(),
                    module_.types.i32()
                }
            }
        ),
        builder_.addFunction(
            "memmove",
            Linkage::External,
            FunctionForm::Declaration,
            FunctionSignature{
                module_.types.ptr(),
                {
                    module_.types.ptr(),
                    module_.types.ptr(),
                    module_.types.i32()
                }
            }
        )
    };
}

void ModuleLowering::registerSourceFunctions() {
    const auto& funcs = model_.functions();
    for (const auto& func: funcs) {
        if (func.kind == semantic::FunctionKind::Builtin) {
            continue;
        }
        auto signature = lowerSignature(func.signature);
        auto source_symbol = sourceSymbol(func.id);
        FunctionId id = builder_.addFunction(source_symbol, Linkage::Internal, FunctionForm::Definition, signature);
        functions_[func.id] = id;
        if (func.name == "main" && func.kind == semantic::FunctionKind::TopLevel) {
            source_main_ = func.id;
        }
    }
}

void ModuleLowering::registerMachineMain() {
    if (!source_main_.has_value()) {
        throw std::logic_error("source code has no main function");
    }
    if (machine_main_.has_value()) {
        throw std::logic_error("machine main function already registered");
    }
    FunctionId id = builder_.addFunction("main", Linkage::External, FunctionForm::Definition, FunctionSignature{module_.types.i32(), {}});
    machine_main_ = id;
}

void ModuleLowering::lowerFunctionBodies(CloneHelper& clone_helper, EqualityHelper& equality_helper) {
    for (const auto& func: model_.functions()) {
        if (func.kind == semantic::FunctionKind::Builtin) {
            continue;
        }
        const auto* body = model_.findFunctionBody(func.id);
        IrBuilder::requireIr(body != nullptr, "source function has no semantic body");
        FunctionLowering lowering(model_, func, *body, module_, types_, layout_, functions_, runtime, clone_helper, equality_helper);
        lowering.lower();
    }
}

void ModuleLowering::emitMachineMain() {
    IrBuilder::requireIr(source_main_.has_value(), "source main is not registered");
    IrBuilder::requireIr(machine_main_.has_value(), "machine main is not registered");
    const FunctionId source = lookupFunction(*source_main_);
    const FunctionId machine = *machine_main_;
    IrBuilder::requireIr(source < module_.functions.size() && machine < module_.functions.size(), "main function ID out of range");
    IrBuilder::requireIr(source != machine, "source main and machine main must be distinct");
    const auto& source_function = module_.functions[source];
    IrBuilder::requireIr(source_function.form == FunctionForm::Definition && source_function.signature.parameters.empty() && source_function.signature.return_type == module_.types.none(), "source main must be a void() definition");
    const auto& machine_function = module_.functions[machine];
    IrBuilder::requireIr(
        machine_function.symbol_name == "main" &&
        machine_function.linkage == Linkage::External &&
        machine_function.form == FunctionForm::Definition &&
        machine_function.signature.parameters.empty() &&
        machine_function.signature.return_type == module_.types.i32(),
        "machine function main must be an external i32() definition"
    );
    IrBuilder::requireIr(machine_function.blocks.empty(), "machine main body already generated");
    const BlockId entry = builder_.appendBlock(machine);
    builder_.positionAtEnd(machine, entry);
    builder_.emitCall(source, {});
    builder_.emitRet(builder_.constantInt(module_.types.i32(), 0));
    builder_.clearInsertionPoint();
}

FunctionSignature ModuleLowering::lowerSignature(const semantic::FunctionSignature& signature) {
    FunctionSignature result;
    result.return_type = types_.returnType(signature.return_type);
    for (const auto& param: signature.parameters) {
        result.parameters.push_back(types_.valueType(param.type));
    }
    return result;
}

ir::FunctionId ModuleLowering::lookupFunction(semantic::FunctionId function) const {
    auto it = functions_.find(function);
    IrBuilder::requireIr(it != functions_.end(), "semantic function has no IR mapping");
    return it->second;
}

std::string ModuleLowering::sourceSymbol(semantic::FunctionId function) const {
    return "__rx_func_" + std::to_string(function);
}

} // namespace ir
