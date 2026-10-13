#include "common.hpp"

// CHECK-LABEL: define internal i32 @"bits_shifts"(
// CHECK-SAME: i32 [[X:%v[0-9]+]])
// CHECK: [[MASK:%v[0-9]+]] = and i32 [[X]], 255
// CHECK: [[OR:%v[0-9]+]] = or i32 [[MASK]], 256
// CHECK: [[NOT:%v[0-9]+]] = xor i32 [[OR]], 4294967295
// CHECK: [[AMOUNT:%v[0-9]+]] = and i32 40, 31
// CHECK: [[LEFT:%v[0-9]+]] = shl i32 [[NOT]], [[AMOUNT]]
// CHECK: [[LOGICAL:%v[0-9]+]] = lshr i32 2147483648, [[AMOUNT]]
// CHECK: [[ARITHMETIC:%v[0-9]+]] = ashr i32 2147483648, [[AMOUNT]]
// CHECK: [[COMBINED:%v[0-9]+]] = xor i32 [[LEFT]], [[LOGICAL]]
// CHECK: [[RESULT:%v[0-9]+]] = or i32 [[COMBINED]], [[ARITHMETIC]]
// CHECK: [[BOOL_AND:%v[0-9]+]] = and i1 1, 0
// CHECK: [[BOOL_OR:%v[0-9]+]] = or i1 [[BOOL_AND]], 1
// CHECK: xor i1 [[BOOL_OR]], 1
// CHECK: ret i32 [[RESULT]]

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i32 = module.types.i32();
    const auto i1 = module.types.i1();
    const auto function = builder.addFunction(
        "bits_shifts", ir::Linkage::Internal, ir::FunctionForm::Definition,
        {i32, {i32}});
    builder.appendBlock(function);
    const auto x = builder.parameter(function, 0);
    const auto mask = builder.emitBinary(ir::OpCode::And, x, builder.constantInt(i32, 255));
    const auto joined = builder.emitBinary(ir::OpCode::Or, mask, builder.constantInt(i32, 256));
    const auto inverted = builder.emitBinary(
        ir::OpCode::Xor, joined, builder.constantInt(i32, 0xFFFFFFFFu));
    // Rx shift amounts use their low five bits; never shift i32 by raw 40.
    const auto amount = builder.emitBinary(
        ir::OpCode::And, builder.constantInt(i32, 40), builder.constantInt(i32, 31));
    const auto left = builder.emitBinary(ir::OpCode::Shl, inverted, amount);
    const auto high_bit = builder.constantInt(i32, 0x80000000u);
    const auto logical = builder.emitBinary(ir::OpCode::Lshr, high_bit, amount);
    const auto arithmetic = builder.emitBinary(ir::OpCode::Ashr, high_bit, amount);
    const auto combined = builder.emitBinary(ir::OpCode::Xor, left, logical);
    const auto result = builder.emitBinary(ir::OpCode::Or, combined, arithmetic);
    const auto boolean_and = builder.emitBinary(
        ir::OpCode::And, builder.constantInt(i1, 1), builder.constantInt(i1, 0));
    const auto boolean_or = builder.emitBinary(ir::OpCode::Or, boolean_and, builder.constantInt(i1, 1));
    builder.emitBinary(ir::OpCode::Xor, boolean_or, builder.constantInt(i1, 1));
    builder.emitRet(result);
    return verifyAndPrint(module);
}
