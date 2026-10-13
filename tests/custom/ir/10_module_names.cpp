#include "common.hpp"

// CHECK: target triple = "riscv32-unknown-elf"
// CHECK: target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
// CHECK: %"rx::Pair with space" = type { i32 }
// CHECK: %"类型" = type { i32 }
// CHECK: %"Quote\22Type" = type { i32 }
// CHECK: %"name\5C41" = type { i32 }
// CHECK: %"nameA" = type { i32 }
// CHECK: declare void @"external::notify with space"(i32)
// CHECK-LABEL: define external i32 @"module_names"()
// CHECK: [[SPACE:%v[0-9]+]] = call i32 @"rx::helper with space"()
// CHECK: [[UNICODE:%v[0-9]+]] = call i32 @"中文函数"()
// CHECK: [[QUOTE:%v[0-9]+]] = call i32 @"quote\22function"()
// CHECK: [[SLASH:%v[0-9]+]] = call i32 @"name\5C41"()
// CHECK: [[PLAIN:%v[0-9]+]] = call i32 @"nameA"()
// CHECK: call void @"external::notify with space"(i32 [[SPACE]])
// CHECK: ret i32 {{%v[0-9]+}}
// CHECK-LABEL: define internal i32 @"rx::helper with space"()
// CHECK: [[SPACE_SLOT:%v[0-9]+]] = alloca %"rx::Pair with space", align 4
// CHECK: getelementptr %"rx::Pair with space", ptr [[SPACE_SLOT]], i32 0, i32 0
// CHECK-LABEL: define internal i32 @"中文函数"()
// CHECK: [[UNICODE_SLOT:%v[0-9]+]] = alloca %"类型", align 4
// CHECK: getelementptr %"类型", ptr [[UNICODE_SLOT]], i32 0, i32 0
// CHECK-LABEL: define internal i32 @"quote\22function"()
// CHECK: [[QUOTE_SLOT:%v[0-9]+]] = alloca %"Quote\22Type", align 4
// CHECK: getelementptr %"Quote\22Type", ptr [[QUOTE_SLOT]], i32 0, i32 0
// CHECK-LABEL: define internal i32 @"name\5C41"()
// CHECK: [[SLASH_SLOT:%v[0-9]+]] = alloca %"name\5C41", align 4
// CHECK: getelementptr %"name\5C41", ptr [[SLASH_SLOT]], i32 0, i32 0
// CHECK-LABEL: define internal i32 @"nameA"()
// CHECK: [[PLAIN_SLOT:%v[0-9]+]] = alloca %"nameA", align 4
// CHECK: getelementptr %"nameA", ptr [[PLAIN_SLOT]], i32 0, i32 0

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i32 = module.types.i32();
    const std::vector<std::string> type_names{
        "rx::Pair with space", "类型", "Quote\"Type", "name\\41", "nameA"};
    const std::vector<std::string> function_names{
        "rx::helper with space", "中文函数", "quote\"function", "name\\41", "nameA"};
    std::vector<ir::TypeId> types;
    for (const auto& name : type_names) {
        const auto id = builder.declareStruct(name);
        builder.defineStruct(id, {i32});
        types.push_back(module.types.structOf(id));
    }
    const auto notify = builder.addFunction(
        "external::notify with space", ir::Linkage::External,
        ir::FunctionForm::Declaration, {module.types.none(), {i32}});
    const auto driver = builder.addFunction(
        "module_names", ir::Linkage::External, ir::FunctionForm::Definition, {i32, {}});
    std::vector<ir::FunctionId> functions;
    for (const auto& name : function_names) {
        functions.push_back(builder.addFunction(
            name, ir::Linkage::Internal, ir::FunctionForm::Definition, {i32, {}}));
    }
    builder.appendBlock(driver);
    auto result = builder.constantInt(i32, 0);
    std::vector<ir::Operand> returned;
    for (const auto function : functions) {
        returned.push_back(builder.emitCall(function, {}).value());
    }
    builder.emitCall(notify, {returned.front()});
    for (const auto operand : returned) {
        result = builder.emitBinary(ir::OpCode::Add, result, operand);
    }
    builder.emitRet(result);

    for (size_t index = 0; index < functions.size(); ++index) {
        builder.appendBlock(functions[index]);
        const auto slot = builder.emitEntryAlloca(types[index], 4);
        const auto zero = builder.constantInt(i32, 0);
        const auto field = builder.emitGep(types[index], slot, {zero, zero});
        builder.emitStore(builder.constantInt(i32, static_cast<uint32_t>(index + 1)), field, 4);
        const auto value = builder.emitLoad(i32, field, 4);
        builder.emitRet(value);
    }
    // The raw Module names are distinct and legal. Printer must escape them,
    // including the literal backslash that would otherwise turn \41 into A.
    return verifyAndPrint(module);
}
