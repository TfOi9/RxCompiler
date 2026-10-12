#include "common.hpp"

// CHECK: target triple = "riscv32-unknown-elf"
// CHECK: target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
// CHECK-LABEL: define internal i32 @"arithmetic"(
// CHECK-SAME: i32 [[X:%v[0-9]+]])
// CHECK: [[ADD:%v[0-9]+]] = add i32 [[X]], 17
// CHECK: [[SUB:%v[0-9]+]] = sub i32 [[ADD]], 5
// CHECK: [[MUL:%v[0-9]+]] = mul i32 [[SUB]], 3
// CHECK: [[SDIV:%v[0-9]+]] = sdiv i32 4294967279, 5
// CHECK: [[SREM:%v[0-9]+]] = srem i32 4294967279, 5
// CHECK: [[UDIV:%v[0-9]+]] = udiv i32 2147483648, 3
// CHECK: [[UREM:%v[0-9]+]] = urem i32 2147483648, 3
// CHECK: [[WRAP_ADD:%v[0-9]+]] = add i32 2147483647, 1
// CHECK: [[WRAP_SUB:%v[0-9]+]] = sub i32 0, 1
// CHECK: [[WRAP_MUL:%v[0-9]+]] = mul i32 4294967295, 2
// CHECK: add i32 [[MUL]], [[SDIV]]
// CHECK: add i32 {{%v[0-9]+}}, [[SREM]]
// CHECK: add i32 {{%v[0-9]+}}, [[UDIV]]
// CHECK: add i32 {{%v[0-9]+}}, [[UREM]]
// CHECK: add i32 {{%v[0-9]+}}, [[WRAP_ADD]]
// CHECK: add i32 {{%v[0-9]+}}, [[WRAP_SUB]]
// CHECK: [[RESULT:%v[0-9]+]] = add i32 {{%v[0-9]+}}, [[WRAP_MUL]]
// CHECK: ret i32 [[RESULT]]

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i32 = module.types.i32();
    const auto function = builder.addFunction(
        "arithmetic", ir::Linkage::Internal, ir::FunctionForm::Definition,
        {i32, {i32}});
    builder.appendBlock(function);
    const auto x = builder.parameter(function, 0);
    const auto add = builder.emitBinary(ir::OpCode::Add, x, builder.constantInt(i32, 17));
    const auto sub = builder.emitBinary(ir::OpCode::Sub, add, builder.constantInt(i32, 5));
    const auto mul = builder.emitBinary(ir::OpCode::Mul, sub, builder.constantInt(i32, 3));
    // Unsigned bit patterns represent negative i32 immediates in this IR model.
    const auto negative = builder.constantInt(i32, 0xFFFFFFEFu); // -17
    const auto sdiv = builder.emitBinary(ir::OpCode::Sdiv, negative, builder.constantInt(i32, 5));
    const auto srem = builder.emitBinary(ir::OpCode::Srem, negative, builder.constantInt(i32, 5));
    const auto high_bit = builder.constantInt(i32, 0x80000000u);
    const auto udiv = builder.emitBinary(ir::OpCode::Udiv, high_bit, builder.constantInt(i32, 3));
    const auto urem = builder.emitBinary(ir::OpCode::Urem, high_bit, builder.constantInt(i32, 3));
    const auto wrap_add = builder.emitBinary(
        ir::OpCode::Add, builder.constantInt(i32, 0x7FFFFFFFu), builder.constantInt(i32, 1));
    const auto wrap_sub = builder.emitBinary(
        ir::OpCode::Sub, builder.constantInt(i32, 0), builder.constantInt(i32, 1));
    const auto wrap_mul = builder.emitBinary(
        ir::OpCode::Mul, builder.constantInt(i32, 0xFFFFFFFFu), builder.constantInt(i32, 2));
    auto result = mul;
    for (const auto operand : {sdiv, srem, udiv, urem, wrap_add, wrap_sub, wrap_mul}) {
        result = builder.emitBinary(ir::OpCode::Add, result, operand);
    }
    builder.emitRet(result);
    ir::IrPrinter(module, std::cout).print();
}
