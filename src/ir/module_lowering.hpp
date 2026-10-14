#pragma once
#include "ir/builder.hpp"
#include "ir/function.hpp"
#include "ir/ir_ids.hpp"
#include "ir/layout.hpp"
#include "ir/type_lowering.hpp"
#include "semantic/function_resolver.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/semantic_model.hpp"
#include <map>
#include <optional>
#include <string>

namespace ir {

using FunctionMap = std::map<semantic::FunctionId, ir::FunctionId>;

class ModuleLowering {
public:
    ModuleLowering(const semantic::SemanticModel& model, Module& module): model_(model), module_(module), builder_(module), types_(model, module), layout_(module) {}
    void lower();
private:
    const semantic::SemanticModel& model_;
    Module& module_;
    IrBuilder builder_;
    TypeLowering types_;
    TargetLayout layout_;
    FunctionMap functions_;
    std::optional<semantic::FunctionId> source_main_;
    std::optional<ir::FunctionId> machine_main_;

    void initializeTarget();
    void registerStructs();
    void declareRuntime();
    void registerSourceFunctions();
    void registerMachineMain();
    void lowerFunctionBodies();
    void emitMachineMain();

    FunctionSignature lowerSignature(const semantic::FunctionSignature& signature);
    ir::FunctionId lookupFunction(semantic::FunctionId function) const;
    std::string sourceSymbol(semantic::FunctionId function) const;
};

} // namespace ir