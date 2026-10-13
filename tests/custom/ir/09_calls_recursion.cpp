#include "common.hpp"

// CHECK: %"Message" = type { i32, ptr }
// CHECK: declare i32 @"get_i32"()
// CHECK: declare void @"println_i32"(i32)
// CHECK-LABEL: define external i32 @"calls"()
// CHECK: [[ARRAY:%v[0-9]+]] = alloca [2 x i32], align 4
// CHECK: [[MESSAGE:%v[0-9]+]] = alloca %"Message", align 4
// CHECK: [[INPUT:%v[0-9]+]] = call i32 @"get_i32"()
// CHECK: [[FACT:%v[0-9]+]] = call i32 @"factorial"(i32 5)
// CHECK: [[SUM:%v[0-9]+]] = call i32 @"sum10"(i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10)
// CHECK: [[POINTER:%v[0-9]+]] = call ptr @"identity_pointer"(ptr [[ARRAY]])
// CHECK: [[ARRAY_VALUE:%v[0-9]+]] = load [2 x i32], ptr [[ARRAY]], align 4
// CHECK: [[ARRAY_RETURN:%v[0-9]+]] = call [2 x i32] @"echo_array"([2 x i32] [[ARRAY_VALUE]])
// CHECK: store [2 x i32] [[ARRAY_RETURN]], ptr [[ARRAY]], align 4
// CHECK: [[MESSAGE_VALUE:%v[0-9]+]] = load %"Message", ptr [[MESSAGE]], align 4
// CHECK: [[MESSAGE_RETURN:%v[0-9]+]] = call %"Message" @"echo_message"(%"Message" [[MESSAGE_VALUE]])
// CHECK: store %"Message" [[MESSAGE_RETURN]], ptr [[MESSAGE]], align 4
// CHECK: {{^[[:blank:]]*}}call void @"unit"()
// CHECK: {{^[[:blank:]]*}}call void @"println_i32"(i32 [[FACT]])
// CHECK: [[PARTIAL:%v[0-9]+]] = add i32 [[INPUT]], [[FACT]]
// CHECK: [[RESULT:%v[0-9]+]] = add i32 [[PARTIAL]], [[SUM]]
// CHECK: ret i32 [[RESULT]]
// CHECK-LABEL: define internal i32 @"factorial"(
// CHECK-SAME: i32 [[N:%v[0-9]+]])
// CHECK: [[SMALL:%v[0-9]+]] = icmp sle i32 [[N]], 1
// CHECK: br i1 [[SMALL]], label %[[BASE:bb[0-9]+]], label %[[RECURSE:bb[0-9]+]]
// CHECK: [[BASE]]:
// CHECK: ret i32 1
// CHECK: [[RECURSE]]:
// CHECK: [[PREVIOUS:%v[0-9]+]] = sub i32 [[N]], 1
// CHECK: [[RECURSIVE:%v[0-9]+]] = call i32 @"factorial"(i32 [[PREVIOUS]])
// CHECK: [[PRODUCT:%v[0-9]+]] = mul i32 [[N]], [[RECURSIVE]]
// CHECK: ret i32 [[PRODUCT]]
// CHECK-LABEL: define internal i32 @"sum10"(
// CHECK-SAME: i32 [[A:%v[0-9]+]], i32 [[B:%v[0-9]+]], i32 {{%v[0-9]+}}, i32 {{%v[0-9]+}}, i32 {{%v[0-9]+}}, i32 {{%v[0-9]+}}, i32 {{%v[0-9]+}}, i32 {{%v[0-9]+}}, i32 {{%v[0-9]+}}, i32 [[LAST:%v[0-9]+]])
// CHECK: [[FIRST_SUM:%v[0-9]+]] = add i32 [[A]], [[B]]
// CHECK: [[LAST_SUM:%v[0-9]+]] = add i32 {{%v[0-9]+}}, [[LAST]]
// CHECK: ret i32 [[LAST_SUM]]
// CHECK-LABEL: define internal void @"unit"()
// CHECK: ret void
// CHECK-LABEL: define internal ptr @"identity_pointer"(
// CHECK-SAME: ptr [[POINTER:%v[0-9]+]])
// CHECK: ret ptr [[POINTER]]
// CHECK-LABEL: define internal [2 x i32] @"echo_array"(
// CHECK-SAME: [2 x i32] [[ARRAY:%v[0-9]+]])
// CHECK: ret [2 x i32] [[ARRAY]]
// CHECK-LABEL: define internal %"Message" @"echo_message"(
// CHECK-SAME: %"Message" [[MESSAGE:%v[0-9]+]])
// CHECK: ret %"Message" [[MESSAGE]]

int main() {
    auto module = makeModule();
    ir::IrBuilder builder(module);
    const auto i32 = module.types.i32();
    const auto ptr = module.types.ptr();
    const auto none = module.types.none();
    const auto array_type = module.types.arrayOf(i32, 2);
    const auto message_id = builder.declareStruct("Message");
    builder.defineStruct(message_id, {i32, ptr});
    const auto message_type = module.types.structOf(message_id);
    const auto get = builder.addFunction(
        "get_i32", ir::Linkage::External, ir::FunctionForm::Declaration, {i32, {}});
    const auto println = builder.addFunction(
        "println_i32", ir::Linkage::External, ir::FunctionForm::Declaration, {none, {i32}});

    // Register every definition before filling bodies. Declarations stay declarations.
    const auto calls = builder.addFunction(
        "calls", ir::Linkage::External, ir::FunctionForm::Definition, {i32, {}});
    const auto factorial = builder.addFunction(
        "factorial", ir::Linkage::Internal, ir::FunctionForm::Definition, {i32, {i32}});
    const auto sum10 = builder.addFunction(
        "sum10", ir::Linkage::Internal, ir::FunctionForm::Definition,
        {i32, std::vector<ir::TypeId>(10, i32)});
    const auto unit = builder.addFunction(
        "unit", ir::Linkage::Internal, ir::FunctionForm::Definition, {none, {}});
    const auto identity_pointer = builder.addFunction(
        "identity_pointer", ir::Linkage::Internal, ir::FunctionForm::Definition, {ptr, {ptr}});
    const auto echo_array = builder.addFunction(
        "echo_array", ir::Linkage::Internal, ir::FunctionForm::Definition, {array_type, {array_type}});
    const auto echo_message = builder.addFunction(
        "echo_message", ir::Linkage::Internal, ir::FunctionForm::Definition,
        {message_type, {message_type}});

    builder.appendBlock(calls);
    const auto array = builder.emitEntryAlloca(array_type, 4);
    const auto message = builder.emitEntryAlloca(message_type, 4);
    const auto input = builder.emitCall(get, {}).value();
    const auto fact = builder.emitCall(factorial, {builder.constantInt(i32, 5)}).value();
    std::vector<ir::Operand> arguments;
    for (uint32_t index = 1; index <= 10; ++index) {
        arguments.push_back(builder.constantInt(i32, index));
    }
    const auto sum = builder.emitCall(sum10, arguments).value();
    const auto pointer = builder.emitCall(identity_pointer, {array}).value();
    const auto zero = builder.constantInt(i32, 0);
    const auto one = builder.constantInt(i32, 1);
    const auto element0 = builder.emitGep(array_type, array, {zero, zero});
    const auto element1 = builder.emitGep(array_type, array, {zero, one});
    builder.emitStore(input, element0, 4);
    builder.emitStore(fact, element1, 4);
    const auto message_number = builder.emitGep(message_type, message, {zero, zero});
    const auto message_pointer = builder.emitGep(message_type, message, {zero, one});
    builder.emitStore(sum, message_number, 4);
    builder.emitStore(pointer, message_pointer, 4);
    const auto array_value = builder.emitLoad(array_type, array, 4);
    const auto array_return = builder.emitCall(echo_array, {array_value}).value();
    builder.emitStore(array_return, array, 4);
    const auto message_value = builder.emitLoad(message_type, message, 4);
    const auto message_return = builder.emitCall(echo_message, {message_value}).value();
    builder.emitStore(message_return, message, 4);
    builder.emitCall(unit, {});
    builder.emitCall(println, {fact});
    const auto partial = builder.emitBinary(ir::OpCode::Add, input, fact);
    const auto result = builder.emitBinary(ir::OpCode::Add, partial, sum);
    builder.emitRet(result);

    const auto fact_entry = builder.appendBlock(factorial);
    const auto fact_base = builder.appendBlock(factorial);
    const auto fact_recurse = builder.appendBlock(factorial);
    builder.positionAtEnd(factorial, fact_entry);
    const auto n = builder.parameter(factorial, 0);
    const auto small = builder.emitICmp(ir::Predicate::Sle, n, one);
    builder.emitCondBr(small, fact_base, fact_recurse);
    builder.positionAtEnd(factorial, fact_base);
    builder.emitRet(one);
    builder.positionAtEnd(factorial, fact_recurse);
    const auto previous = builder.emitBinary(ir::OpCode::Sub, n, one);
    const auto recursive = builder.emitCall(factorial, {previous}).value();
    const auto product = builder.emitBinary(ir::OpCode::Mul, n, recursive);
    builder.emitRet(product);

    builder.appendBlock(sum10);
    auto total = builder.parameter(sum10, 0);
    for (size_t index = 1; index < 10; ++index) {
        total = builder.emitBinary(ir::OpCode::Add, total, builder.parameter(sum10, index));
    }
    builder.emitRet(total);
    builder.appendBlock(unit);
    builder.emitRet(std::nullopt);
    builder.appendBlock(identity_pointer);
    builder.emitRet(builder.parameter(identity_pointer, 0));
    builder.appendBlock(echo_array);
    builder.emitRet(builder.parameter(echo_array, 0));
    builder.appendBlock(echo_message);
    builder.emitRet(builder.parameter(echo_message, 0));
    return verifyAndPrint(module);
}
