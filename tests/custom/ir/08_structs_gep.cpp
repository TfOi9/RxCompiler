#include "common.hpp"

// Record references Pair before Pair's textual definition.
// CHECK: %"Record" = type { %"Pair", [2 x i32], ptr }
// CHECK: %"Pair" = type { i8, i32 }
// CHECK: %"Empty" = type { }
// CHECK: %"Opaque" = type opaque
// CHECK-LABEL: define internal i32 @"structs_gep"()
// CHECK: [[RECORD:%v[0-9]+]] = alloca %"Record", align 4
// CHECK: [[COPY:%v[0-9]+]] = alloca %"Record", align 4
// CHECK: alloca %"Empty", align 1
// CHECK: [[TAG:%v[0-9]+]] = getelementptr %"Record", ptr [[RECORD]], i32 0, i32 0, i32 0
// CHECK: [[NUMBER:%v[0-9]+]] = getelementptr %"Record", ptr [[RECORD]], i32 0, i32 0, i32 1
// CHECK: [[ELEMENT0:%v[0-9]+]] = getelementptr %"Record", ptr [[RECORD]], i32 0, i32 1, i32 0
// CHECK: [[ELEMENT1:%v[0-9]+]] = getelementptr %"Record", ptr [[RECORD]], i32 0, i32 1, i32 1
// CHECK: [[POINTER:%v[0-9]+]] = getelementptr %"Record", ptr [[RECORD]], i32 0, i32 2
// CHECK: store i8 255, ptr [[TAG]], align 1
// CHECK: store i32 42, ptr [[NUMBER]], align 4
// CHECK: store i32 7, ptr [[ELEMENT0]], align 4
// CHECK: store i32 9, ptr [[ELEMENT1]], align 4
// CHECK: store ptr null, ptr [[POINTER]], align 4
// CHECK: store ptr [[NUMBER]], ptr [[POINTER]], align 4
// CHECK: [[AGGREGATE:%v[0-9]+]] = load %"Record", ptr [[RECORD]], align 4
// CHECK: store %"Record" [[AGGREGATE]], ptr [[COPY]], align 4
// CHECK: [[COPY_NUMBER:%v[0-9]+]] = getelementptr %"Record", ptr [[COPY]], i32 0, i32 0, i32 1
// CHECK: [[COPY_POINTER:%v[0-9]+]] = getelementptr %"Record", ptr [[COPY]], i32 0, i32 2
// CHECK: [[COPY_TAG:%v[0-9]+]] = getelementptr %"Record", ptr [[COPY]], i32 0, i32 0, i32 0
// CHECK: [[COPIED:%v[0-9]+]] = load i32, ptr [[COPY_NUMBER]], align 4
// CHECK: [[ADDRESS:%v[0-9]+]] = load ptr, ptr [[COPY_POINTER]], align 4
// CHECK: [[REFERENT:%v[0-9]+]] = load i32, ptr [[ADDRESS]], align 4
// CHECK: [[TAG8:%v[0-9]+]] = load i8, ptr [[COPY_TAG]], align 1
// CHECK: [[TAG32:%v[0-9]+]] = zext i8 [[TAG8]] to i32
// CHECK: [[SUM:%v[0-9]+]] = add i32 [[COPIED]], [[REFERENT]]
// CHECK: [[RESULT:%v[0-9]+]] = add i32 [[SUM]], [[TAG32]]
// CHECK: ret i32 [[RESULT]]

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i8 = module.types.i8();
    const auto i32 = module.types.i32();
    const auto record_id = builder.declareStruct("Record");
    const auto pair_id = builder.declareStruct("Pair");
    const auto empty_id = builder.declareStruct("Empty");
    builder.declareStruct("Opaque"); // An unsized declaration, never allocated.
    const auto record_type = module.types.structOf(record_id);
    const auto pair_type = module.types.structOf(pair_id);
    const auto empty_type = module.types.structOf(empty_id);
    const auto array_type = module.types.arrayOf(i32, 2);
    builder.defineStruct(record_id, {pair_type, array_type, module.types.ptr()});
    builder.defineStruct(pair_id, {i8, i32});
    builder.defineStruct(empty_id, {});

    const auto function = builder.addFunction(
        "structs_gep", ir::Linkage::Internal, ir::FunctionForm::Definition, {i32, {}});
    builder.appendBlock(function);
    const auto record = builder.emitEntryAlloca(record_type, 4);
    const auto copy = builder.emitEntryAlloca(record_type, 4);
    builder.emitEntryAlloca(empty_type, 1);
    const auto zero = builder.constantInt(i32, 0);
    const auto one = builder.constantInt(i32, 1);
    const auto tag = builder.emitGep(record_type, record, {zero, zero, zero});
    const auto number = builder.emitGep(record_type, record, {zero, zero, one});
    const auto element0 = builder.emitGep(record_type, record, {zero, one, zero});
    const auto element1 = builder.emitGep(record_type, record, {zero, one, one});
    const auto pointer = builder.emitGep(record_type, record, {zero, builder.constantInt(i32, 2)});
    builder.emitStore(builder.constantInt(i8, 255), tag, 1);
    builder.emitStore(builder.constantInt(i32, 42), number, 4);
    builder.emitStore(builder.constantInt(i32, 7), element0, 4);
    builder.emitStore(builder.constantInt(i32, 9), element1, 4);
    builder.emitStore(builder.nullPointer(), pointer, 4);
    builder.emitStore(number, pointer, 4);
    const auto aggregate = builder.emitLoad(record_type, record, 4);
    builder.emitStore(aggregate, copy, 4);
    const auto copy_number = builder.emitGep(record_type, copy, {zero, zero, one});
    const auto copy_pointer = builder.emitGep(record_type, copy, {zero, builder.constantInt(i32, 2)});
    const auto copy_tag = builder.emitGep(record_type, copy, {zero, zero, zero});
    const auto copied = builder.emitLoad(i32, copy_number, 4);
    const auto address = builder.emitLoad(module.types.ptr(), copy_pointer, 4);
    const auto referent = builder.emitLoad(i32, address, 4);
    const auto tag8 = builder.emitLoad(i8, copy_tag, 1);
    const auto tag32 = builder.emitZExt(tag8, i32);
    const auto sum = builder.emitBinary(ir::OpCode::Add, copied, referent);
    const auto result = builder.emitBinary(ir::OpCode::Add, sum, tag32);
    builder.emitRet(result);
    ir::IrPrinter(module, std::cout).print();
}
