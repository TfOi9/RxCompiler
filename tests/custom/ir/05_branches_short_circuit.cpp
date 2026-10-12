#include "common.hpp"

// CHECK-LABEL: define internal i1 @"touch"(
// CHECK-SAME: ptr [[COUNTER:%v[0-9]+]])
// CHECK: [[OLD:%v[0-9]+]] = load i32, ptr [[COUNTER]], align 4
// CHECK: [[NEXT:%v[0-9]+]] = add i32 [[OLD]], 1
// CHECK: store i32 [[NEXT]], ptr [[COUNTER]], align 4
// CHECK: [[TOUCHED:%v[0-9]+]] = icmp ne i32 [[NEXT]], 0
// CHECK: ret i1 [[TOUCHED]]
// CHECK-LABEL: define internal i32 @"choose"(
// CHECK-SAME: i1 [[COND:%v[0-9]+]], i32 [[YES:%v[0-9]+]], i32 [[NO:%v[0-9]+]])
// CHECK: [[SLOT:%v[0-9]+]] = alloca i32, align 4
// CHECK: br i1 [[COND]], label %[[THEN:bb[0-9]+]], label %[[ELSE:bb[0-9]+]]
// CHECK: [[THEN]]:
// CHECK: store i32 [[YES]], ptr [[SLOT]], align 4
// CHECK: br label %[[JOIN:bb[0-9]+]]
// CHECK: [[ELSE]]:
// CHECK: store i32 [[NO]], ptr [[SLOT]], align 4
// CHECK: br label %[[JOIN]]
// CHECK: [[JOIN]]:
// CHECK: [[RESULT:%v[0-9]+]] = load i32, ptr [[SLOT]], align 4
// CHECK: ret i32 [[RESULT]]
// CHECK-LABEL: define internal i32 @"early_return"(
// CHECK-SAME: i32 [[X:%v[0-9]+]])
// CHECK: [[NEGATIVE:%v[0-9]+]] = icmp slt i32 [[X]], 0
// CHECK: br i1 [[NEGATIVE]], label %[[EARLY:bb[0-9]+]], label %[[NORMAL:bb[0-9]+]]
// CHECK: [[EARLY]]:
// CHECK: ret i32 0
// CHECK: [[NORMAL]]:
// CHECK: ret i32 [[X]]
// CHECK-LABEL: define internal i1 @"lazy_and"(
// CHECK-SAME: i1 [[LEFT:%v[0-9]+]], ptr [[COUNTER:%v[0-9]+]])
// CHECK: [[SLOT:%v[0-9]+]] = alloca i1, align 1
// CHECK: br i1 [[LEFT]], label %[[RHS:bb[0-9]+]], label %[[SHORT:bb[0-9]+]]
// CHECK: [[SHORT]]:
// CHECK-NOT: call
// CHECK: store i1 0, ptr [[SLOT]], align 1
// CHECK: br label %[[JOIN:bb[0-9]+]]
// CHECK: [[RHS]]:
// CHECK: [[RIGHT:%v[0-9]+]] = call i1 @"touch"(ptr [[COUNTER]])
// CHECK: store i1 [[RIGHT]], ptr [[SLOT]], align 1
// CHECK: br label %[[JOIN]]
// CHECK: [[JOIN]]:
// CHECK: [[RESULT:%v[0-9]+]] = load i1, ptr [[SLOT]], align 1
// CHECK: ret i1 [[RESULT]]
// CHECK-LABEL: define internal i1 @"lazy_or"(
// CHECK-SAME: i1 [[LEFT:%v[0-9]+]], ptr [[COUNTER:%v[0-9]+]])
// CHECK: [[SLOT:%v[0-9]+]] = alloca i1, align 1
// CHECK: br i1 [[LEFT]], label %[[SHORT:bb[0-9]+]], label %[[RHS:bb[0-9]+]]
// CHECK: [[SHORT]]:
// CHECK-NOT: call
// CHECK: store i1 1, ptr [[SLOT]], align 1
// CHECK: br label %[[JOIN:bb[0-9]+]]
// CHECK: [[RHS]]:
// CHECK: [[RIGHT:%v[0-9]+]] = call i1 @"touch"(ptr [[COUNTER]])
// CHECK: store i1 [[RIGHT]], ptr [[SLOT]], align 1
// CHECK: br label %[[JOIN]]
// CHECK: [[JOIN]]:
// CHECK: [[RESULT:%v[0-9]+]] = load i1, ptr [[SLOT]], align 1
// CHECK: ret i1 [[RESULT]]
// CHECK-LABEL: define internal void @"unreachable_path"()
// CHECK: unreachable
// CHECK-LABEL: define internal void @"unit_return"()
// CHECK: ret void

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i1 = module.types.i1();
    const auto i32 = module.types.i32();
    const auto ptr = module.types.ptr();
    const auto none = module.types.none();
    const auto touch = builder.addFunction(
        "touch", ir::Linkage::Internal, ir::FunctionForm::Definition, {i1, {ptr}});
    const auto choose = builder.addFunction(
        "choose", ir::Linkage::Internal, ir::FunctionForm::Definition, {i32, {i1, i32, i32}});
    const auto early = builder.addFunction(
        "early_return", ir::Linkage::Internal, ir::FunctionForm::Definition, {i32, {i32}});
    const auto lazy_and = builder.addFunction(
        "lazy_and", ir::Linkage::Internal, ir::FunctionForm::Definition, {i1, {i1, ptr}});
    const auto lazy_or = builder.addFunction(
        "lazy_or", ir::Linkage::Internal, ir::FunctionForm::Definition, {i1, {i1, ptr}});
    const auto unreachable = builder.addFunction(
        "unreachable_path", ir::Linkage::Internal, ir::FunctionForm::Definition, {none, {}});
    const auto unit = builder.addFunction(
        "unit_return", ir::Linkage::Internal, ir::FunctionForm::Definition, {none, {}});

    builder.appendBlock(touch);
    const auto counter = builder.parameter(touch, 0);
    const auto old = builder.emitLoad(i32, counter, 4);
    const auto next = builder.emitBinary(ir::OpCode::Add, old, builder.constantInt(i32, 1));
    builder.emitStore(next, counter, 4);
    const auto touched = builder.emitICmp(ir::Predicate::Ne, next, builder.constantInt(i32, 0));
    builder.emitRet(touched);

    const auto entry = builder.appendBlock(choose);
    const auto yes = builder.appendBlock(choose);
    const auto no = builder.appendBlock(choose);
    const auto join = builder.appendBlock(choose);
    builder.positionAtEnd(choose, entry);
    const auto slot = builder.emitEntryAlloca(i32, 4);
    builder.emitCondBr(builder.parameter(choose, 0), yes, no);
    builder.positionAtEnd(choose, yes);
    builder.emitStore(builder.parameter(choose, 1), slot, 4);
    builder.emitBr(join);
    builder.positionAtEnd(choose, no);
    builder.emitStore(builder.parameter(choose, 2), slot, 4);
    builder.emitBr(join);
    builder.positionAtEnd(choose, join);
    const auto chosen = builder.emitLoad(i32, slot, 4);
    builder.emitRet(chosen);

    const auto early_entry = builder.appendBlock(early);
    const auto early_exit = builder.appendBlock(early);
    const auto normal_exit = builder.appendBlock(early);
    builder.positionAtEnd(early, early_entry);
    const auto x = builder.parameter(early, 0);
    const auto negative = builder.emitICmp(ir::Predicate::Slt, x, builder.constantInt(i32, 0));
    builder.emitCondBr(negative, early_exit, normal_exit);
    builder.positionAtEnd(early, early_exit);
    builder.emitRet(builder.constantInt(i32, 0));
    builder.positionAtEnd(early, normal_exit);
    builder.emitRet(x);

    for (const auto function : {lazy_and, lazy_or}) {
        const auto lazy_entry = builder.appendBlock(function);
        const auto short_path = builder.appendBlock(function);
        const auto rhs = builder.appendBlock(function);
        const auto merge = builder.appendBlock(function);
        builder.positionAtEnd(function, lazy_entry);
        const auto result_slot = builder.emitEntryAlloca(i1, 1);
        const bool is_and = function == lazy_and;
        builder.emitCondBr(builder.parameter(function, 0),
                           is_and ? rhs : short_path, is_and ? short_path : rhs);
        builder.positionAtEnd(function, short_path);
        builder.emitStore(builder.constantInt(i1, is_and ? 0 : 1), result_slot, 1);
        builder.emitBr(merge);
        builder.positionAtEnd(function, rhs);
        const auto right = builder.emitCall(touch, {builder.parameter(function, 1)}).value();
        builder.emitStore(right, result_slot, 1);
        builder.emitBr(merge);
        builder.positionAtEnd(function, merge);
        const auto result = builder.emitLoad(i1, result_slot, 1);
        builder.emitRet(result);
    }
    builder.appendBlock(unreachable);
    builder.emitUnreachable(); // Kept separate from the ordinary returning functions.
    builder.appendBlock(unit);
    builder.emitRet(std::nullopt);
    ir::IrPrinter(module, std::cout).print();
}
