#include "common.hpp"

// CHECK-LABEL: define internal i32 @"arrays_gep"(
// CHECK-SAME: i32 [[INDEX:%v[0-9]+]])
// CHECK: [[ARRAY:%v[0-9]+]] = alloca [4 x i32], align 4
// CHECK: [[MATRIX:%v[0-9]+]] = alloca [2 x [3 x i32]], align 4
// CHECK: [[COPY:%v[0-9]+]] = alloca [4 x i32], align 4
// CHECK: alloca [0 x i32], align 4
// CHECK: [[ELEMENT0:%v[0-9]+]] = getelementptr [4 x i32], ptr [[ARRAY]], i32 0, i32 0
// CHECK: store i32 10, ptr [[ELEMENT0]], align 4
// CHECK: [[ELEMENT3:%v[0-9]+]] = getelementptr [4 x i32], ptr [[ARRAY]], i32 0, i32 3
// CHECK: store i32 40, ptr [[ELEMENT3]], align 4
// CHECK: [[CELL00:%v[0-9]+]] = getelementptr [2 x [3 x i32]], ptr [[MATRIX]], i32 0, i32 0, i32 0
// CHECK: store i32 1, ptr [[CELL00]], align 4
// CHECK: [[CELL12:%v[0-9]+]] = getelementptr [2 x [3 x i32]], ptr [[MATRIX]], i32 0, i32 1, i32 2
// CHECK: store i32 6, ptr [[CELL12]], align 4
// CHECK: [[AGGREGATE:%v[0-9]+]] = load [4 x i32], ptr [[ARRAY]], align 4
// CHECK: store [4 x i32] [[AGGREGATE]], ptr [[COPY]], align 4
// CHECK: [[BOUNDED:%v[0-9]+]] = and i32 [[INDEX]], 3
// CHECK: [[DYNAMIC:%v[0-9]+]] = getelementptr [4 x i32], ptr [[COPY]], i32 0, i32 [[BOUNDED]]
// CHECK: [[ARRAY_VALUE:%v[0-9]+]] = load i32, ptr [[DYNAMIC]], align 4
// CHECK: [[ROW:%v[0-9]+]] = and i32 [[INDEX]], 1
// CHECK: [[CELL:%v[0-9]+]] = getelementptr [2 x [3 x i32]], ptr [[MATRIX]], i32 0, i32 [[ROW]], i8 2
// CHECK: [[MATRIX_VALUE:%v[0-9]+]] = load i32, ptr [[CELL]], align 4
// CHECK: [[FIRST:%v[0-9]+]] = getelementptr [4 x i32], ptr [[COPY]], i32 0, i32 0
// CHECK: [[SAME:%v[0-9]+]] = getelementptr i32, ptr [[FIRST]]{{$}}
// CHECK: [[NEXT:%v[0-9]+]] = getelementptr i32, ptr [[FIRST]], i32 1
// CHECK: [[FIRST_VALUE:%v[0-9]+]] = load i32, ptr [[SAME]], align 4
// CHECK: [[NEXT_VALUE:%v[0-9]+]] = load i32, ptr [[NEXT]], align 4
// CHECK: [[PAIR:%v[0-9]+]] = add i32 [[ARRAY_VALUE]], [[MATRIX_VALUE]]
// CHECK: [[WITH_FIRST:%v[0-9]+]] = add i32 [[PAIR]], [[FIRST_VALUE]]
// CHECK: [[RESULT:%v[0-9]+]] = add i32 [[WITH_FIRST]], [[NEXT_VALUE]]
// CHECK: ret i32 [[RESULT]]

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i32 = module.types.i32();
    const auto array_type = module.types.arrayOf(i32, 4);
    const auto row_type = module.types.arrayOf(i32, 3);
    const auto matrix_type = module.types.arrayOf(row_type, 2);
    const auto empty_type = module.types.arrayOf(i32, 0);
    const auto function = builder.addFunction(
        "arrays_gep", ir::Linkage::Internal, ir::FunctionForm::Definition, {i32, {i32}});
    builder.appendBlock(function);
    const auto array = builder.emitEntryAlloca(array_type, 4);
    const auto matrix = builder.emitEntryAlloca(matrix_type, 4);
    const auto copy = builder.emitEntryAlloca(array_type, 4);
    builder.emitEntryAlloca(empty_type, 4); // Zero-length type, never indexed.
    const auto zero = builder.constantInt(i32, 0);
    for (uint32_t index = 0; index < 4; ++index) {
        const auto element = builder.emitGep(
            array_type, array, {zero, builder.constantInt(i32, index)});
        builder.emitStore(builder.constantInt(i32, (index + 1) * 10), element, 4);
    }
    for (uint32_t row = 0; row < 2; ++row) {
        for (uint32_t column = 0; column < 3; ++column) {
            const auto cell = builder.emitGep(
                matrix_type, matrix,
                {zero, builder.constantInt(i32, row), builder.constantInt(i32, column)});
            builder.emitStore(builder.constantInt(i32, row * 3 + column + 1), cell, 4);
        }
    }
    const auto aggregate = builder.emitLoad(array_type, array, 4);
    builder.emitStore(aggregate, copy, 4);
    const auto index = builder.parameter(function, 0);
    const auto bounded_index = builder.emitBinary(ir::OpCode::And, index, builder.constantInt(i32, 3));
    const auto dynamic = builder.emitGep(array_type, copy, {zero, bounded_index});
    const auto array_value = builder.emitLoad(i32, dynamic, 4);
    const auto bounded_row = builder.emitBinary(ir::OpCode::And, index, builder.constantInt(i32, 1));
    const auto cell = builder.emitGep(
        matrix_type, matrix, {zero, bounded_row, builder.constantInt(module.types.i8(), 2)});
    const auto matrix_value = builder.emitLoad(i32, cell, 4);
    const auto first = builder.emitGep(array_type, copy, {zero, zero});
    const auto same = builder.emitGep(i32, first, {});
    const auto next = builder.emitGep(i32, first, {builder.constantInt(i32, 1)});
    const auto first_value = builder.emitLoad(i32, same, 4);
    const auto next_value = builder.emitLoad(i32, next, 4);
    const auto pair = builder.emitBinary(ir::OpCode::Add, array_value, matrix_value);
    const auto with_first = builder.emitBinary(ir::OpCode::Add, pair, first_value);
    const auto result = builder.emitBinary(ir::OpCode::Add, with_first, next_value);
    builder.emitRet(result);
    return verifyAndPrint(module);
}
