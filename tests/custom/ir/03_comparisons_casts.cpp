#include "common.hpp"

// CHECK-LABEL: define internal i32 @"comparisons_casts"()
// CHECK: icmp eq i32 4294967295, 1
// CHECK: icmp eq i32 1, 1
// CHECK: icmp ne i32 4294967295, 1
// CHECK: icmp ne i32 1, 1
// CHECK: icmp ugt i32 4294967295, 1
// CHECK: icmp ugt i32 1, 1
// CHECK: icmp uge i32 4294967295, 1
// CHECK: icmp uge i32 1, 1
// CHECK: icmp ult i32 4294967295, 1
// CHECK: icmp ult i32 1, 1
// CHECK: icmp ule i32 4294967295, 1
// CHECK: icmp ule i32 1, 1
// CHECK: icmp sgt i32 4294967295, 1
// CHECK: icmp sgt i32 1, 1
// CHECK: icmp sge i32 4294967295, 1
// CHECK: icmp sge i32 1, 1
// CHECK: icmp slt i32 4294967295, 1
// CHECK: icmp slt i32 1, 1
// CHECK: icmp sle i32 4294967295, 1
// CHECK: icmp sle i32 1, 1
// CHECK: [[TO8:%v[0-9]+]] = zext i1 1 to i8
// CHECK: [[TO32:%v[0-9]+]] = zext i1 1 to i32
// CHECK: [[BYTE32:%v[0-9]+]] = zext i8 [[TO8]] to i32
// CHECK: [[LOW8:%v[0-9]+]] = trunc i32 4660 to i8
// CHECK: [[LOW1:%v[0-9]+]] = trunc i32 4661 to i1
// CHECK: [[BYTE1:%v[0-9]+]] = trunc i8 [[LOW8]] to i1
// CHECK: [[BOTH:%v[0-9]+]] = or i1 [[LOW1]], [[BYTE1]]
// CHECK: zext i1 [[BOTH]] to i32
// CHECK: ret i32 {{%v[0-9]+}}

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i1 = module.types.i1();
    const auto i8 = module.types.i8();
    const auto i32 = module.types.i32();
    const auto function = builder.addFunction(
        "comparisons_casts", ir::Linkage::Internal, ir::FunctionForm::Definition,
        {i32, {}});
    builder.appendBlock(function);
    const auto negative_one = builder.constantInt(i32, 0xFFFFFFFFu);
    const auto one = builder.constantInt(i32, 1);
    auto count = builder.constantInt(i32, 0);
    for (const auto predicate : {
             ir::Predicate::Eq, ir::Predicate::Ne, ir::Predicate::Ugt,
             ir::Predicate::Uge, ir::Predicate::Ult, ir::Predicate::Ule,
             ir::Predicate::Sgt, ir::Predicate::Sge, ir::Predicate::Slt,
             ir::Predicate::Sle}) {
        const auto unequal = builder.emitICmp(predicate, negative_one, one);
        const auto equal = builder.emitICmp(predicate, one, one);
        const auto unequal32 = builder.emitZExt(unequal, i32);
        const auto equal32 = builder.emitZExt(equal, i32);
        count = builder.emitBinary(ir::OpCode::Add, count, unequal32);
        count = builder.emitBinary(ir::OpCode::Add, count, equal32);
    }
    // These are IR width conversions, not extra Rx source-level casts.
    const auto byte = builder.emitZExt(builder.constantInt(i1, 1), i8);
    const auto wide_bool = builder.emitZExt(builder.constantInt(i1, 1), i32);
    const auto wide_byte = builder.emitZExt(byte, i32);
    const auto low_byte = builder.emitTrunc(builder.constantInt(i32, 0x1234), i8);
    const auto low_bit = builder.emitTrunc(builder.constantInt(i32, 0x1235), i1);
    const auto byte_bit = builder.emitTrunc(low_byte, i1);
    const auto both = builder.emitBinary(ir::OpCode::Or, low_bit, byte_bit);
    const auto wide_both = builder.emitZExt(both, i32);
    for (const auto operand : {wide_bool, wide_byte, wide_both}) {
        count = builder.emitBinary(ir::OpCode::Add, count, operand);
    }
    builder.emitRet(count);
    return verifyAndPrint(module);
}
