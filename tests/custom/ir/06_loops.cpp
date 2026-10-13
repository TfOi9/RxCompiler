#include "common.hpp"

// CHECK-LABEL: define internal i32 @"nested_loops"()
// CHECK: [[I:%v[0-9]+]] = alloca i32, align 4
// CHECK: [[J:%v[0-9]+]] = alloca i32, align 4
// CHECK: [[SUM:%v[0-9]+]] = alloca i32, align 4
// CHECK: store i32 0, ptr [[I]], align 4
// CHECK: store i32 0, ptr [[J]], align 4
// CHECK: store i32 0, ptr [[SUM]], align 4
// CHECK: br label %[[OUTER:bb[0-9]+]]
// CHECK: [[OUTER]]:
// CHECK: [[OUTER_I:%v[0-9]+]] = load i32, ptr [[I]], align 4
// CHECK: [[OUTER_TEST:%v[0-9]+]] = icmp slt i32 [[OUTER_I]], 3
// CHECK: br i1 [[OUTER_TEST]], label %[[RESET:bb[0-9]+]], label %[[EXIT:bb[0-9]+]]
// CHECK: [[RESET]]:
// CHECK: store i32 0, ptr [[J]], align 4
// CHECK: br label %[[INNER:bb[0-9]+]]
// CHECK: [[INNER]]:
// CHECK: [[INNER_J:%v[0-9]+]] = load i32, ptr [[J]], align 4
// CHECK: [[INNER_TEST:%v[0-9]+]] = icmp slt i32 [[INNER_J]], 4
// CHECK: br i1 [[INNER_TEST]], label %[[CONTINUE_TEST:bb[0-9]+]], label %[[OUTER_STEP:bb[0-9]+]]
// CHECK: [[CONTINUE_TEST]]:
// CHECK: [[SKIP:%v[0-9]+]] = icmp eq i32 [[INNER_J]], 1
// CHECK: br i1 [[SKIP]], label %[[INNER_STEP:bb[0-9]+]], label %[[BREAK_TEST:bb[0-9]+]]
// CHECK: [[BREAK_TEST]]:
// CHECK: [[AT_LAST:%v[0-9]+]] = icmp eq i32 [[INNER_J]], 3
// CHECK: br i1 [[AT_LAST]], label %[[OUTER_STEP]], label %[[ACCUMULATE:bb[0-9]+]]
// CHECK: [[ACCUMULATE]]:
// CHECK: [[CURRENT_SUM:%v[0-9]+]] = load i32, ptr [[SUM]], align 4
// CHECK: [[NEXT_SUM:%v[0-9]+]] = add i32 [[CURRENT_SUM]], {{%v[0-9]+}}
// CHECK: store i32 [[NEXT_SUM]], ptr [[SUM]], align 4
// CHECK: br label %[[INNER_STEP]]
// CHECK: [[INNER_STEP]]:
// CHECK: [[CURRENT_J:%v[0-9]+]] = load i32, ptr [[J]], align 4
// CHECK: [[NEXT_J:%v[0-9]+]] = add i32 [[CURRENT_J]], 1
// CHECK: store i32 [[NEXT_J]], ptr [[J]], align 4
// CHECK: br label %[[INNER]]
// CHECK: [[OUTER_STEP]]:
// CHECK: [[CURRENT_I:%v[0-9]+]] = load i32, ptr [[I]], align 4
// CHECK: [[NEXT_I:%v[0-9]+]] = add i32 [[CURRENT_I]], 1
// CHECK: store i32 [[NEXT_I]], ptr [[I]], align 4
// CHECK: br label %[[OUTER]]
// CHECK: [[EXIT]]:
// CHECK: [[RESULT:%v[0-9]+]] = load i32, ptr [[SUM]], align 4
// CHECK: ret i32 [[RESULT]]

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i32 = module.types.i32();
    const auto function = builder.addFunction(
        "nested_loops", ir::Linkage::Internal, ir::FunctionForm::Definition, {i32, {}});
    const auto entry = builder.appendBlock(function);
    const auto outer_test = builder.appendBlock(function);
    const auto reset_inner = builder.appendBlock(function);
    const auto inner_test = builder.appendBlock(function);
    const auto continue_test = builder.appendBlock(function);
    const auto break_test = builder.appendBlock(function);
    const auto accumulate = builder.appendBlock(function);
    const auto inner_step = builder.appendBlock(function);
    const auto outer_step = builder.appendBlock(function);
    const auto exit = builder.appendBlock(function);

    builder.positionAtEnd(function, entry);
    const auto i_slot = builder.emitEntryAlloca(i32, 4);
    const auto j_slot = builder.emitEntryAlloca(i32, 4);
    const auto sum_slot = builder.emitEntryAlloca(i32, 4);
    const auto zero = builder.constantInt(i32, 0);
    const auto one = builder.constantInt(i32, 1);
    builder.emitStore(zero, i_slot, 4);
    builder.emitStore(zero, j_slot, 4);
    builder.emitStore(zero, sum_slot, 4);
    builder.emitBr(outer_test);

    builder.positionAtEnd(function, outer_test);
    const auto outer_i = builder.emitLoad(i32, i_slot, 4);
    const auto outer_condition = builder.emitICmp(
        ir::Predicate::Slt, outer_i, builder.constantInt(i32, 3));
    builder.emitCondBr(outer_condition, reset_inner, exit);
    builder.positionAtEnd(function, reset_inner);
    builder.emitStore(zero, j_slot, 4);
    builder.emitBr(inner_test);

    builder.positionAtEnd(function, inner_test);
    const auto inner_j = builder.emitLoad(i32, j_slot, 4);
    const auto inner_condition = builder.emitICmp(
        ir::Predicate::Slt, inner_j, builder.constantInt(i32, 4));
    builder.emitCondBr(inner_condition, continue_test, outer_step);
    builder.positionAtEnd(function, continue_test);
    const auto skip = builder.emitICmp(ir::Predicate::Eq, inner_j, one);
    builder.emitCondBr(skip, inner_step, break_test); // continue when j == 1
    builder.positionAtEnd(function, break_test);
    const auto at_last = builder.emitICmp(ir::Predicate::Eq, inner_j, builder.constantInt(i32, 3));
    builder.emitCondBr(at_last, outer_step, accumulate); // break inner loop

    builder.positionAtEnd(function, accumulate);
    const auto contribution = builder.emitBinary(ir::OpCode::Add, outer_i, inner_j);
    const auto current_sum = builder.emitLoad(i32, sum_slot, 4);
    const auto next_sum = builder.emitBinary(ir::OpCode::Add, current_sum, contribution);
    builder.emitStore(next_sum, sum_slot, 4);
    builder.emitBr(inner_step);
    builder.positionAtEnd(function, inner_step);
    const auto current_j = builder.emitLoad(i32, j_slot, 4);
    const auto next_j = builder.emitBinary(ir::OpCode::Add, current_j, one);
    builder.emitStore(next_j, j_slot, 4);
    builder.emitBr(inner_test);
    builder.positionAtEnd(function, outer_step);
    const auto current_i = builder.emitLoad(i32, i_slot, 4);
    const auto next_i = builder.emitBinary(ir::OpCode::Add, current_i, one);
    builder.emitStore(next_i, i_slot, 4);
    builder.emitBr(outer_test);
    builder.positionAtEnd(function, exit);
    const auto result = builder.emitLoad(i32, sum_slot, 4);
    builder.emitRet(result);
    return verifyAndPrint(module);
}
