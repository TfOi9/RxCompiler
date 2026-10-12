#include "common.hpp"

// CHECK-LABEL: define internal i32 @"memory_allocas"(
// CHECK-SAME: i32 [[X:%v[0-9]+]])
// CHECK: [[ENTRY:bb[0-9]+]]:
// CHECK-NEXT: [[NUMBER:%v[0-9]+]] = alloca i32, align 4
// CHECK-NEXT: [[BOOLEAN:%v[0-9]+]] = alloca i8, align 1
// CHECK-NEXT: [[POINTER:%v[0-9]+]] = alloca ptr, align 4
// CHECK-NEXT: [[LATE:%v[0-9]+]] = alloca i32, align 4
// CHECK-NEXT: store i32 [[X]], ptr [[NUMBER]], align 4
// CHECK: store i8 1, ptr [[BOOLEAN]], align 1
// CHECK: store ptr null, ptr [[POINTER]], align 4
// CHECK: br label %[[BODY:bb[0-9]+]]
// CHECK: [[BODY]]:
// CHECK-NEXT: [[OLD:%v[0-9]+]] = load i32, ptr [[NUMBER]], align 4
// CHECK-NEXT: store i32 [[OLD]], ptr [[LATE]], align 4
// CHECK-NEXT: [[LOCAL:%v[0-9]+]] = alloca i32, align 4
// CHECK: [[BOOL8:%v[0-9]+]] = load i8, ptr [[BOOLEAN]], align 1
// CHECK: [[BOOL1:%v[0-9]+]] = trunc i8 [[BOOL8]] to i1
// CHECK: [[BOOL32:%v[0-9]+]] = zext i1 [[BOOL1]] to i32
// CHECK: load ptr, ptr [[POINTER]], align 4
// CHECK: store ptr [[NUMBER]], ptr [[POINTER]], align 4
// CHECK: [[ADDRESS:%v[0-9]+]] = load ptr, ptr [[POINTER]], align 4
// CHECK: [[VALUE:%v[0-9]+]] = load i32, ptr [[ADDRESS]], align 4
// CHECK: [[SUM:%v[0-9]+]] = add i32 [[VALUE]], [[BOOL32]]
// CHECK: store i32 [[SUM]], ptr [[LOCAL]], align 4
// CHECK: [[RESULT:%v[0-9]+]] = load i32, ptr [[LOCAL]], align 4
// CHECK: ret i32 [[RESULT]]

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i32 = module.types.i32();
    const auto i8 = module.types.i8();
    const auto function = builder.addFunction(
        "memory_allocas", ir::Linkage::Internal, ir::FunctionForm::Definition,
        {i32, {i32}});
    const auto entry = builder.appendBlock(function);
    const auto body = builder.appendBlock(function);
    builder.positionAtEnd(function, entry);
    const auto number = builder.emitEntryAlloca(i32, 4);
    const auto boolean = builder.emitEntryAlloca(i8, 1);
    const auto pointer = builder.emitEntryAlloca(module.types.ptr(), 4);
    builder.emitStore(builder.parameter(function, 0), number, 4);
    builder.emitStore(builder.constantInt(i8, 1), boolean, 1);
    builder.emitStore(builder.nullPointer(), pointer, 4);
    builder.emitBr(body);

    builder.positionAtEnd(function, body);
    const auto old = builder.emitLoad(i32, number, 4);
    // This gets a later ValueId, but must appear before entry's first store.
    const auto late = builder.emitEntryAlloca(i32, 4);
    builder.emitStore(old, late, 4); // Insertion point must still be body.
    const auto local = builder.emitAlloca(i32, 4);
    const auto bool8 = builder.emitLoad(i8, boolean, 1);
    const auto bool1 = builder.emitTrunc(bool8, module.types.i1());
    const auto bool32 = builder.emitZExt(bool1, i32);
    builder.emitLoad(module.types.ptr(), pointer, 4); // Loading null is valid.
    builder.emitStore(number, pointer, 4);
    const auto address = builder.emitLoad(module.types.ptr(), pointer, 4);
    const auto value = builder.emitLoad(i32, address, 4);
    const auto sum = builder.emitBinary(ir::OpCode::Add, value, bool32);
    builder.emitStore(sum, local, 4);
    const auto result = builder.emitLoad(i32, local, 4);
    builder.emitRet(result);
    ir::IrPrinter(module, std::cout).print();
}
