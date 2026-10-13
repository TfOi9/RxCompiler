#include "test.hpp"

namespace test {
void builderCases(std::vector<Case>& cases) {
    add(cases, "harness.verified_print_gate", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m); std::ostringstream output, diagnostics;
        c.equal(verifyAndPrint(m, output, diagnostics), 0, "empty valid module prints successfully");
        c.check(!output.str().empty() && diagnostics.str().empty(), "valid output/diagnostic streams");
        auto f = b.addFunction("incomplete", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        output.str(""); diagnostics.str("");
        c.equal(verifyAndPrint(m, output, diagnostics), 1, "project rejection propagates to program status");
        c.check(output.str().empty(), "invalid module must not be printed");
        c.contains(diagnostics.str(), "has no blocks");
        b.appendBlock(f); b.emitUnreachable(); output.str(""); diagnostics.str("");
        c.equal(verifyAndPrint(m, output, diagnostics), 0, "completed module accepted");
        c.equal(output.str(), c.save(m), "verified print matches module artifact"); c.valid(m);
    }, "accept");
    add(cases, "types.table", [](Context& c) {
        TypeTable t;
        const std::vector<TypeId> builtins{t.none(), t.i1(), t.i8(), t.i32(), t.ptr()};
        for (size_t i = 0; i < builtins.size(); ++i) {
            c.equal(builtins[i], i, "builtin ID/order"); c.check(t.contains(builtins[i]), "builtin present");
            c.equal(t.insert(t.get(builtins[i])), builtins[i], "type interning");
        }
        c.check(!t.contains(invalid), "invalid ID absent");
        c.check(!t.find(IntegerType{16}), "missing type lookup");
        c.equal(t.integerSize(t.i1()), 1, "i1 width"); c.equal(t.integerSize(t.i8()), 8, "i8 width");
        c.equal(t.integerSize(t.i32()), 32, "i32 width");
        c.check(!t.isInteger(t.ptr()) && !t.isPointer(t.i32()), "type predicates");
        c.check(t.isPointer(t.ptr()) && t.isInteger(t.i32()), "type predicates true");
        const auto a = t.arrayOf(t.i32(), 0), b = t.arrayOf(t.i32(), 1), d = t.arrayOf(t.i8(), 1);
        c.check(a != b && b != d, "array key includes element and length");
        c.equal(t.arrayOf(t.i32(), 0), a, "array interning");
        const auto n = t.arrayOf(b, 4);
        c.equal(std::get<ArrayType>(t.get(n)).element, b, "nested array element");
        c.check(t.structOf(0) != t.structOf(1), "named struct key includes ID");
        c.equal(t.structOf(0), t.structOf(0), "named struct interning");
        auto copy = t.get(b); std::get<ArrayType>(copy).length = 42;
        c.equal(std::get<ArrayType>(t.get(b)).length, 1u, "get returns independent value");
    });
    add(cases, "types.validity_and_sizing", [](Context& c) {
        Module m = makeModule(); IrBuilder b(m);
        for (auto t : {m.types.i1(), m.types.i8(), m.types.i32(), m.types.ptr()})
            c.check(b.isValidTypeRef(t) && b.isSized(t), "scalar valid and sized");
        c.check(!b.isValidTypeRef(m.types.none()) && !b.isSized(m.types.none()), "void is not a field/storage type");
        c.check(!b.isValidTypeRef(invalid) && !b.isSized(invalid), "invalid type rejected");
        const auto unsupported = m.types.insert(IntegerType{16});
        c.check(!b.isValidTypeRef(unsupported) && !b.isSized(unsupported), "unsupported integer rejected");
        const auto unknown = m.types.structOf(invalid);
        c.check(!b.isValidTypeRef(unknown) && !b.isSized(unknown), "unknown struct rejected");
        auto opaque = b.declareStruct("Opaque"), empty = b.declareStruct("Empty"), outer = b.declareStruct("Outer");
        auto ot = m.types.structOf(opaque), et = m.types.structOf(empty), rt = m.types.structOf(outer);
        c.check(b.isValidTypeRef(ot) && !b.isSized(ot), "opaque reference valid but unsized");
        b.defineStruct(empty, {}); b.defineStruct(outer, {et, et, m.types.ptr()});
        c.check(b.isSized(rt), "shared empty child and pointer are sized");
        c.check(!b.isSized(m.types.arrayOf(ot, 0)), "zero length cannot make opaque element sized");
        for (uint32_t length : {0u, 1u, 100u}) {
            auto a = m.types.arrayOf(m.types.i32(), length);
            c.check(b.isValidTypeRef(a) && b.isSized(a), "array valid and sized");
        }
        c.check(!b.isValidTypeRef(m.types.arrayOf(m.types.none(), 0)), "array of void invalid");
        c.check(!b.isValidTypeRef(m.types.arrayOf(invalid, 1)), "bad array element invalid");
        TypeId next = 0; while (m.types.contains(next)) ++next;
        const auto self = m.types.arrayOf(next, 1);
        c.equal(self, next, "raw self-array fixture ID");
        c.check(!b.isValidTypeRef(self) && !b.isSized(self), "raw array type cycle rejected");
        auto cyclic = b.declareStruct("Cycle"); auto ct = m.types.structOf(cyclic);
        b.defineStruct(cyclic, {m.types.arrayOf(m.types.arrayOf(ct, 1), 0)});
        c.check(b.isValidTypeRef(ct) && !b.isSized(ct), "named cycle has valid references but no finite layout");
        auto dependent = b.declareStruct("Dependent"); b.defineStruct(dependent, {ct, ct});
        c.check(!b.isSized(m.types.structOf(dependent)), "cached unsized child");
    });
    for (unsigned width : {1u, 8u, 32u}) for (uint32_t bits : {0u, 1u, 2u, 255u, 256u, 0x80000000u, 0xffffffffu}) {
        add(cases, "builder.constant.i" + std::to_string(width) + "." + std::to_string(bits), [=](Context& c) {
            Fixture x; auto t = width == 1 ? x.m.types.i1() : width == 8 ? x.m.types.i8() : x.m.types.i32();
            auto op = x.b.constantInt(t, bits);
            c.equal(op.type, t, "constant type");
            c.equal(std::get<Immediate>(op.data).bits, bits & (width == 32 ? 0xffffffffu : (1u << width) - 1), "constant bit masking");
            auto ptr = x.b.nullPointer(); c.equal(ptr.type, x.m.types.ptr(), "null type");
            c.check(std::holds_alternative<NullPointer>(ptr.data), "null representation");
            if (width != 32) op = x.b.emitZExt(op, x.m.types.i32());
            x.b.emitRet(op); c.save(x.m); c.valid(x.m);
        }, "accept");
    }
    for (unsigned align = 0; align < 256; ++align) {
        add(cases, "builder.align." + std::to_string(align), [=](Context& c) {
            Fixture x; const auto a = static_cast<uint8_t>(align);
            const bool valid = align != 0 && (align & (align - 1)) == 0;
            c.equal(IrBuilder::isValidAlign(a), valid, "alignment power-of-two rule");
            const std::vector<std::function<void()>> actions{
                [&] { x.b.emitAlloca(x.m.types.i32(), a); },
                [&] { x.b.emitEntryAlloca(x.m.types.i8(), a); },
                [&] { x.b.emitLoad(x.m.types.i32(), x.b.parameter(x.f, 3), a); },
                [&] { x.b.emitStore(x.i(), x.b.parameter(x.f, 3), a); }};
            for (const auto& action : actions) {
                if (valid) action(); else x.reject(c, action, "align has invalid value");
            }
            x.finish(c);
        }, "accept");
    }
    add(cases, "builder.require_ir", [](Context& c) {
        IrBuilder::requireIr(true, "ok"); Fixture x;
        x.reject(c, [] { IrBuilder::requireIr(false, "explicit guard"); }, "explicit guard");
        x.finish(c);
    }, "accept");
    add(cases, "builder.lifecycle", [](Context& c) {
        Module m = makeModule(); IrBuilder b(m);
        c.check(!b.hasInsertionPoint(), "initial insertion point empty");
        rejects(c, m, b, [&] { b.emitBinary(OpCode::Add, b.constantInt(m.types.i32(), 1), b.constantInt(m.types.i32(), 2)); }, "current function is empty");
        rejects(c, m, b, [&] { b.emitEntryAlloca(m.types.i32(), 4); }, "current function is empty");
        auto f = b.addFunction("first", Linkage::Internal, FunctionForm::Definition, {m.types.i32(), {m.types.i32()}});
        c.check(!b.hasInsertionPoint(), "addFunction clears block");
        rejects(c, m, b, [&] { b.emitUnreachable(); }, "current block is empty");
        rejects(c, m, b, [&] { b.emitEntryAlloca(m.types.i32(), 4); }, "function has no blocks");
        auto entry = b.appendBlock(f); c.check(b.hasInsertionPoint(), "appendBlock selects block");
        auto body = b.appendBlock(f); b.positionAtEnd(f, entry);
        b.emitBinary(OpCode::Add, b.parameter(f, 0), b.constantInt(m.types.i32(), 1));
        auto late = b.emitEntryAlloca(m.types.i32(), 4);
        c.equal(valueId(late), 2u, "late alloca retains allocated ID");
        c.check(std::holds_alternative<Alloca>(m.functions[f].blocks[entry].instructions.front().data), "alloca precedes non-alloca");
        b.emitBr(body); c.check(!b.hasInsertionPoint(), "terminator disables insertion");
        rejects(c, m, b, [&] { b.positionAtEnd(f, entry); }, "block already terminated");
        b.positionAtEnd(f, body); auto count = m.functions[f].blocks[body].instructions.size();
        b.emitEntryAlloca(m.types.i8(), 1);
        b.emitBinary(OpCode::Add, b.constantInt(m.types.i32(), 0), b.constantInt(m.types.i32(), 1));
        c.equal(m.functions[f].blocks[body].instructions.size(), count + 1, "entry alloca preserves body insertion point");
        b.clearInsertionPoint(); c.check(!b.hasInsertionPoint(), "clear insertion point");
        b.emitEntryAlloca(m.types.ptr(), 4);
        c.check(!b.hasInsertionPoint(), "entry alloca does not restore cleared point");
        rejects(c, m, b, [&] { b.emitUnreachable(); }, "current block is empty");
        b.positionAtEnd(f, body); b.emitRet(b.parameter(f, 0));
        auto second = b.addFunction("second", Linkage::External, FunctionForm::Definition, {m.types.none(), {}});
        auto second_entry = b.appendBlock(second); b.emitRet(std::nullopt);
        c.equal(second_entry, 0u, "block numbering restarts per function");
        c.save(m); c.valid(m);
    }, "accept");
    add(cases, "builder.declaration_guards", [](Context& c) {
        Fixture x;
        auto d = x.b.addFunction("decl", Linkage::External, FunctionForm::Declaration, {x.m.types.i32(), {x.m.types.i32()}});
        c.check(!x.b.hasInsertionPoint(), "declaration has no insertion point");
        c.equal(valueId(x.b.parameter(d, 0)), 0u, "declaration parameter is registered");
        rejects(c, x.m, x.b, [&] { x.b.appendBlock(d); }, "appending block after a declaration");
        rejects(c, x.m, x.b, [&] { x.b.positionAtEnd(d, 0); }, "positioning a block in a declaration");
        rejects(c, x.m, x.b, [&] { x.b.emitEntryAlloca(x.m.types.i32(), 4); }, "cannot emit instructions into a declaration");
        x.b.positionAtEnd(x.f, x.entry); x.finish(c);
    }, "accept");
    for (auto form : {FunctionForm::Declaration, FunctionForm::Definition}) {
        add(cases, "builder.empty_function_name." + std::string(form == FunctionForm::Declaration ? "declaration" : "definition"), [=](Context& c) {
            Fixture x;
            const auto next_function = x.m.functions.size();
            x.reject(c, [&] { x.b.addFunction("", Linkage::External, form, {x.m.types.i32(), {}}); }, "empty function symbol");
            x.finish(c);
            const auto next = x.b.addFunction("after_empty", Linkage::External, form, {x.m.types.i32(), {}});
            c.equal(next, next_function, "empty name rejection must not consume FunctionId");
            if (form == FunctionForm::Definition) { x.b.appendBlock(next); x.b.emitRet(x.i()); }
            c.save(x.m); c.valid(x.m);
        }, "accept");
    }
    const std::vector<std::pair<std::string, std::string>> guard_names{
        {"append_function", "function ID out of range"}, {"position_function", "function ID out of range"},
        {"position_block", "block ID out of range"}, {"parameter_function", "current function out of range"},
        {"parameter_index", "parameter index out of range"}, {"constant_pointer", "constant integer must be integer typed"},
        {"constant_invalid", "constant integer must be integer typed"}, {"duplicate_function", "duplicate function symbol"},
        {"return_invalid", "invalid return type"}, {"return_void_array", "return type is not sized"},
        {"parameter_invalid", "invalid parameter type"}, {"parameter_void", "void parameter"},
        {"parameter_unsized", "parameter type is not sized"}, {"duplicate_struct", "duplicate struct name"},
        {"define_invalid", "struct ID out of range"}, {"define_twice", "struct already defined"},
        {"field_void", "invalid struct field type"}, {"field_invalid", "invalid struct field type"},
        {"field_bad_struct", "invalid struct field type"}, {"field_bad_integer", "invalid struct field type"}};
    for (const auto& item : guard_names) add(cases, "builder.guard." + item.first, [=](Context& c) {
        Fixture x; const auto s = x.b.declareStruct("Struct"); const auto opaque = x.m.types.structOf(s);
        auto action = [&] {
            const auto& n = item.first;
            if (n == "append_function") x.b.appendBlock(invalid);
            else if (n == "position_function") x.b.positionAtEnd(invalid, 0);
            else if (n == "position_block") x.b.positionAtEnd(x.f, invalid);
            else if (n == "parameter_function") x.b.parameter(invalid, 0);
            else if (n == "parameter_index") x.b.parameter(x.f, 4);
            else if (n == "constant_pointer") x.b.constantInt(x.m.types.ptr(), 0);
            else if (n == "constant_invalid") x.b.constantInt(invalid, 0);
            else if (n == "duplicate_function") x.b.addFunction("test", Linkage::Internal, FunctionForm::Definition, {x.m.types.i32(), {}});
            else if (n == "return_invalid") x.b.addFunction("bad", Linkage::Internal, FunctionForm::Definition, {invalid, {}});
            else if (n == "return_void_array") x.b.addFunction("bad", Linkage::Internal, FunctionForm::Definition, {x.m.types.arrayOf(x.m.types.none(), 0), {}});
            else if (n == "parameter_invalid") x.b.addFunction("bad", Linkage::Internal, FunctionForm::Declaration, {x.m.types.none(), {invalid}});
            else if (n == "parameter_void") x.b.addFunction("bad", Linkage::Internal, FunctionForm::Definition, {x.m.types.none(), {x.m.types.none()}});
            else if (n == "parameter_unsized") x.b.addFunction("bad", Linkage::Internal, FunctionForm::Definition, {x.m.types.none(), {opaque}});
            else if (n == "duplicate_struct") x.b.declareStruct("Struct");
            else if (n == "define_invalid") x.b.defineStruct(invalid, {});
            else if (n == "define_twice") x.b.defineStruct(s, {});
            else if (n == "field_void") x.b.defineStruct(s, {x.m.types.i32(), x.m.types.none()});
            else if (n == "field_invalid") x.b.defineStruct(s, {invalid});
            else if (n == "field_bad_struct") x.b.defineStruct(s, {x.m.types.structOf(invalid)});
            else x.b.defineStruct(s, {x.m.types.insert(IntegerType{16})});
        };
        // Type construction is a fixture operation, not part of the rejected call.
        if (item.first == "return_void_array") x.m.types.arrayOf(x.m.types.none(), 0);
        if (item.first == "field_bad_struct") x.m.types.structOf(invalid);
        if (item.first == "field_bad_integer") x.m.types.insert(IntegerType{16});
        if (item.first == "define_twice") x.b.defineStruct(s, {});
        x.reject(c, action, item.second);
        x.finish(c);
    }, "accept");
    const std::vector<std::pair<std::string, std::string>> operand_names{
        {"invalid_type", "invalid operand type"}, {"foreign", "value belongs to another function"},
        {"local_range", "local ID out of range"}, {"local_type", "local value type mismatch"},
        {"immediate_pointer", "integer constant has non-integer type"}, {"i1_range", "i1 constant is out of range"},
        {"i8_range", "i8 constant is out of range"}, {"null_integer", "null has non-pointer type"}};
    for (const auto& item : operand_names) for (const std::string site : {"store", "call", "binary_rhs", "ret"}) {
        // Do not register inert cases: these combinations fail an earlier
        // sizedness/signature check, covered separately below.
        if (item.first == "invalid_type" && site != "call" && site != "binary_rhs") continue;
        if (site == "ret" && (item.first == "immediate_pointer" || item.first == "i1_range" || item.first == "i8_range")) continue;
        add(cases, "builder.operand." + item.first + "." + site, [=](Context& c) {
            Fixture x; Operand bad = x.i();
            if (item.first == "invalid_type") bad = immediate(invalid, 0);
            if (item.first == "foreign") bad = {x.m.types.i32(), LocalValue{0, x.f + 1}};
            if (item.first == "local_range") bad = {x.m.types.i32(), LocalValue{invalid, x.f}};
            if (item.first == "local_type") bad = {x.m.types.i32(), LocalValue{1, x.f}};
            if (item.first == "immediate_pointer") bad = immediate(x.m.types.ptr(), 0);
            if (item.first == "i1_range") bad = immediate(x.m.types.i1(), 2);
            if (item.first == "i8_range") bad = immediate(x.m.types.i8(), 256);
            if (item.first == "null_integer") bad = {x.m.types.i32(), NullPointer{}};
            x.reject(c, [&] {
                if (site == "store") x.b.emitStore(bad, x.b.parameter(x.f, 3), 4);
                else if (site == "call") x.b.emitCall(invalid, {bad});
                else if (site == "binary_rhs") x.b.emitBinary(OpCode::Add, x.i(), bad);
                else x.b.emitRet(bad);
            }, item.second);
            x.finish(c);
        });
    }
    for (std::string name : {"alloca", "entry_alloca", "load", "store", "binary", "icmp", "zext", "trunc", "gep", "call", "br", "condbr", "ret", "unreachable"}) {
        add(cases, "builder.terminated." + name, [=](Context& c) {
            Fixture x; x.b.emitRet(x.i());
            if (name == "entry_alloca") {
                x.b.emitEntryAlloca(x.m.types.i32(), 4); c.valid(x.m); c.save(x.m); return;
            }
            rejects(c, x.m, x.b, [&] {
                if (name == "alloca") x.b.emitAlloca(x.m.types.i32(), 4);
                else if (name == "load") x.b.emitLoad(x.m.types.i32(), x.b.parameter(x.f, 3), 4);
                else if (name == "store") x.b.emitStore(x.i(), x.b.parameter(x.f, 3), 4);
                else if (name == "binary") x.b.emitBinary(OpCode::Add, x.i(), x.i());
                else if (name == "icmp") x.b.emitICmp(Predicate::Eq, x.i(), x.i());
                else if (name == "zext") x.b.emitZExt(x.b.constantInt(x.m.types.i8(), 0), x.m.types.i32());
                else if (name == "trunc") x.b.emitTrunc(x.i(), x.m.types.i8());
                else if (name == "gep") x.b.emitGep(x.m.types.i32(), x.b.nullPointer(), {});
                else if (name == "call") x.b.emitCall(x.f, {});
                else if (name == "br") x.b.emitBr(x.entry);
                else if (name == "condbr") x.b.emitCondBr(x.b.constantInt(x.m.types.i1(), 1), x.entry, x.entry);
                else if (name == "ret") x.b.emitRet(x.i());
                else x.b.emitUnreachable();
            }, "block already terminated"); c.valid(x.m); c.save(x.m);
        }, "accept");
    }
    for (std::string name : {"void", "opaque", "invalid", "unsupported", "array_void", "array_opaque", "cycle"})
        add(cases, "builder.memory_unsized." + name, [=](Context& c) {
            Fixture x; auto s = x.b.declareStruct("Unsized"); auto st = x.m.types.structOf(s);
            auto t = x.m.types.none();
            if (name == "opaque") t = st;
            if (name == "invalid") t = invalid;
            if (name == "unsupported") t = x.m.types.insert(IntegerType{16});
            if (name == "array_void") t = x.m.types.arrayOf(t, 0);
            if (name == "array_opaque") t = x.m.types.arrayOf(st, 0);
            if (name == "cycle") { x.b.defineStruct(s, {st}); t = st; }
            x.reject(c, [&] { x.b.emitAlloca(t, 4); }, "alloca requires a sized type");
            x.reject(c, [&] { x.b.emitEntryAlloca(t, 4); }, "alloca requires a sized type");
            x.reject(c, [&] { x.b.emitLoad(t, x.b.nullPointer(), 4); }, "memory value must be sized");
            x.reject(c, [&] { x.b.emitStore(immediate(t, 0), x.b.nullPointer(), 4); }, "memory value must be sized");
            x.reject(c, [&] { x.b.emitGep(t, x.b.nullPointer(), {}); }, name == "invalid" ? "invalid gep source type" : "gep source type is not sized");
        });
    const std::vector<std::pair<std::string, std::string>> instructions{
        {"load_address", "address is not a pointer"}, {"store_address", "address is not a pointer"},
        {"binary_mixed", "binary requires same operand types"}, {"binary_pointer", "binary requires integer operands"},
        {"icmp_mixed", "icmp requires same operand types"}, {"icmp_pointer", "icmp requires integer operand type"},
        {"callee", "callee ID out of range"}, {"call_count", "argument count mismatch"}, {"call_extra", "argument count mismatch"}, {"call_type", "argument type mismatch"},
        {"br_target", "target ID out of range"}, {"cond_type", "condbr must have i1-typed condition"},
        {"cond_yes", "yes block ID out of range"}, {"cond_no", "no block ID out of range"},
        {"ret_missing", "ret has invalid return value"}, {"ret_type", "ret has invalid return value"}};
    for (const auto& item : instructions) add(cases, "builder.instruction." + item.first, [=](Context& c) {
        Fixture x;
        auto action = [&] {
            const auto& n = item.first; auto byte = x.b.constantInt(x.m.types.i8(), 1); auto ptr = x.b.nullPointer();
            if (n == "load_address") x.b.emitLoad(x.m.types.i32(), x.i(), 4);
            else if (n == "store_address") x.b.emitStore(x.i(), x.i(), 4);
            else if (n == "binary_mixed") x.b.emitBinary(OpCode::Add, x.i(), byte);
            else if (n == "binary_pointer") x.b.emitBinary(OpCode::Add, ptr, ptr);
            else if (n == "icmp_mixed") x.b.emitICmp(Predicate::Eq, x.i(), byte);
            else if (n == "icmp_pointer") x.b.emitICmp(Predicate::Eq, ptr, ptr);
            else if (n == "callee") x.b.emitCall(invalid, {});
            else if (n == "call_count") x.b.emitCall(x.f, {});
            else if (n == "call_extra") x.b.emitCall(x.f, {x.i(), byte, x.b.constantInt(x.m.types.i1(), 1), ptr, x.i()});
            else if (n == "call_type") x.b.emitCall(x.f, {byte, byte, x.b.constantInt(x.m.types.i1(), 1), ptr});
            else if (n == "br_target") x.b.emitBr(invalid);
            else if (n == "cond_type") x.b.emitCondBr(x.i(), x.entry, x.entry);
            else if (n == "cond_yes") x.b.emitCondBr(x.b.constantInt(x.m.types.i1(), 1), invalid, x.entry);
            else if (n == "cond_no") x.b.emitCondBr(x.b.constantInt(x.m.types.i1(), 1), x.entry, invalid);
            else if (n == "ret_missing") x.b.emitRet(std::nullopt);
            else x.b.emitRet(byte);
        };
        x.reject(c, action, item.second); x.finish(c);
    }, "accept");
    for (bool zext : {false, true}) for (unsigned from : {0u, 1u, 8u, 32u}) for (unsigned to : {0u, 1u, 8u, 32u}) {
        const bool legal = from && to && (zext ? to > from : to < from);
        add(cases, "builder.cast." + std::string(zext ? "zext" : "trunc") + "." + std::to_string(from) + "." + std::to_string(to), [=](Context& c) {
            Fixture x; auto type = [&](unsigned w) { return w == 1 ? x.m.types.i1() : w == 8 ? x.m.types.i8() : w == 32 ? x.m.types.i32() : x.m.types.ptr(); };
            auto op = from ? x.b.constantInt(type(from), 1) : x.b.nullPointer();
            auto action = [&] { return zext ? x.b.emitZExt(op, type(to)) : x.b.emitTrunc(op, type(to)); };
            if (legal) { auto result = action(); c.equal(result.type, type(to), "cast result type"); }
            else {
                const std::string base = zext ? "zext" : "trunc";
                const auto msg = !from ? base + " requires integer operand type" : !to ? base + " requires integer target type" :
                    base + (zext ? " requires an extended integer type" : " requires a shrunk integer type");
                x.reject(c, action, msg);
            }
            x.finish(c);
        }, "accept");
    }
    const std::vector<std::pair<std::string, std::string>> geps{
        {"base", "gep requires pointer base type"}, {"first", "gep requires first index to be an integer type"},
        {"array", "array index must be an integer"}, {"struct_width", "struct index must be an i32 type"},
        {"struct_dynamic", "struct field index out of range"}, {"struct_range", "struct field index out of range"},
        {"struct_negative", "struct field index out of range"}, {"struct_empty", "struct field index out of range"},
        {"scalar", "invalid indexed type"}, {"pointer_deep", "invalid indexed type"}};
    for (const auto& item : geps) add(cases, "builder.gep_reject." + item.first, [=](Context& c) {
        Fixture x; auto s = x.b.declareStruct("S");
        x.b.defineStruct(s, item.first == "struct_empty" ? std::vector<TypeId>{} : std::vector<TypeId>{x.m.types.i32()});
        auto st = x.m.types.structOf(s), at = x.m.types.arrayOf(x.m.types.i32(), 2);
        x.reject(c, [&] {
            auto ptr = x.b.nullPointer(), zero = x.i(0); const auto& n = item.first;
            if (n == "base") x.b.emitGep(at, zero, {});
            else if (n == "first") x.b.emitGep(at, ptr, {ptr});
            else if (n == "array") x.b.emitGep(at, ptr, {zero, ptr});
            else if (n == "struct_width") x.b.emitGep(st, ptr, {zero, x.b.constantInt(x.m.types.i8(), 0)});
            else if (n == "struct_dynamic") x.b.emitGep(st, ptr, {zero, x.b.parameter(x.f, 0)});
            else if (n == "struct_range") x.b.emitGep(st, ptr, {zero, x.i(1)});
            else if (n == "struct_negative") x.b.emitGep(st, ptr, {zero, x.i(0xffffffffu)});
            else if (n == "struct_empty") x.b.emitGep(st, ptr, {zero, zero});
            else if (n == "scalar") x.b.emitGep(x.m.types.i32(), ptr, {zero, zero});
            else x.b.emitGep(x.m.types.ptr(), ptr, {zero, zero});
        }, item.second); x.finish(c);
    }, "accept");
    add(cases, "builder.gep_boundaries", [](Context& c) {
        Fixture x; auto ptr = x.b.parameter(x.f, 3), zero = x.i(0);
        auto s = x.b.declareStruct("S"); auto a = x.m.types.arrayOf(x.m.types.i32(), 2);
        x.b.defineStruct(s, {a, x.m.types.ptr()}); auto st = x.m.types.structOf(s);
        auto outer = x.m.types.arrayOf(st, 3);
        for (auto src : {x.m.types.i32(), x.m.types.ptr(), a, st}) {
            c.equal(x.b.emitGep(src, ptr, {}).type, x.m.types.ptr(), "zero-index GEP result");
            x.b.emitGep(src, ptr, {x.b.constantInt(x.m.types.i1(), 1)});
        }
        x.b.emitGep(a, ptr, {zero, x.b.parameter(x.f, 0)});
        x.b.emitGep(a, ptr, {zero, x.i(0xffffffffu)}); // Non-inbounds GEP does not impose array bounds.
        x.b.emitGep(outer, ptr, {zero, x.i(1), zero, x.b.parameter(x.f, 1)});
        x.finish(c);
    }, "accept");
    add(cases, "builder.result_bookkeeping", [](Context& c) {
        Fixture x;
        auto d = x.b.addFunction("notify", Linkage::External, FunctionForm::Declaration, {x.m.types.none(), {}});
        x.b.positionAtEnd(x.f, x.entry);
        const auto before = x.m.functions[x.f].value_types.size();
        c.check(!x.b.emitCall(d, {}), "void call returns no operand");
        c.equal(x.m.functions[x.f].value_types.size(), before, "void call consumes no ID");
        c.check(!x.m.functions[x.f].blocks[0].instructions.back().result, "void instruction has no result");
        x.b.emitStore(x.i(), x.b.parameter(x.f, 3), 4);
        c.equal(x.m.functions[x.f].value_types.size(), before, "store consumes no ID");
        auto result = x.b.emitCall(x.f, {x.i(), x.b.constantInt(x.m.types.i8(), 0), x.b.constantInt(x.m.types.i1(), 0), x.b.nullPointer()});
        c.check(bool(result), "non-void call produces result");
        if (result) c.equal(valueId(*result), before, "next result ID after void/store");
        x.finish(c);
    }, "accept");
    add(cases, "builder.void_return_reject", [](Context& c) {
        Module m = makeModule(); IrBuilder b(m); auto f = b.addFunction("unit", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        b.appendBlock(f); rejects(c, m, b, [&] { b.emitRet(b.constantInt(m.types.i32(), 0)); }, "ret has invalid return value");
        b.emitRet(std::nullopt); c.save(m); c.valid(m);
    }, "accept");
    add(cases, "builder.real_foreign_value", [](Context& c) {
        Fixture x;
        auto foreign = x.b.addFunction("foreign", Linkage::External, FunctionForm::Declaration, {x.m.types.none(), {x.m.types.i32(), x.m.types.ptr()}});
        auto scalar = x.b.parameter(foreign, 0), address = x.b.parameter(foreign, 1);
        x.b.positionAtEnd(x.f, x.entry);
        x.reject(c, [&] { x.b.emitBinary(OpCode::Add, scalar, x.i()); }, "value belongs to another function");
        x.reject(c, [&] { x.b.emitLoad(x.m.types.i32(), address, 4); }, "value belongs to another function");
        x.reject(c, [&] { x.b.emitStore(x.i(), address, 4); }, "value belongs to another function");
        x.finish(c);
    }, "accept");
    add(cases, "builder.field_array_cycle", [](Context& c) {
        Fixture x; TypeId next = 0; while (x.m.types.contains(next)) ++next;
        auto self = x.m.types.arrayOf(next, 1), s = x.b.declareStruct("Rejected");
        x.reject(c, [&] { x.b.defineStruct(s, {x.m.types.i32(), self}); }, "invalid struct field type");
        c.check(!x.m.structs[s].fields, "failed field list must not partially define struct"); x.finish(c);
    }, "accept");
}
} // namespace test
