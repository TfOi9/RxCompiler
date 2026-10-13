#include "test.hpp"

namespace test {
void printerCases(std::vector<Case>& cases) {
    add(cases, "printer.empty", [](Context& c) {
        auto m = makeModule(); c.valid(m);
        c.equal(c.save(m), std::string("target triple = \"riscv32-unknown-elf\"\ntarget datalayout = \"e-m:e-p:32:32-i64:64-n32-S128\"\n\n"), "empty module headers");
    }, "accept");
    add(cases, "printer.empty_target", [](Context& c) {
        Module m; c.valid(m); c.equal(c.save(m), std::string("target triple = \"\"\ntarget datalayout = \"\"\n\n"), "empty target fields");
    }, "accept");
    add(cases, "printer.declarations", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        b.addFunction("zero", Linkage::External, FunctionForm::Declaration, {m.types.none(), {}});
        b.addFunction("many", Linkage::External, FunctionForm::Declaration, {m.types.ptr(), {m.types.i1(), m.types.i8(), m.types.i32(), m.types.ptr()}});
        c.valid(m); auto text = c.save(m);
        c.contains(text, "declare void @\"zero\"()\n");
        c.contains(text, "declare ptr @\"many\"(i1, i8, i32, ptr)\n");
        c.check(text.find("%v") == std::string::npos && text.find("define") == std::string::npos, "declaration has types only");
    }, "accept");
    add(cases, "printer.registration_order", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        auto f = b.addFunction("first_definition", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        auto d = b.addFunction("later_declaration", Linkage::External, FunctionForm::Declaration, {m.types.none(), {}});
        auto g = b.addFunction("second_definition", Linkage::External, FunctionForm::Definition, {m.types.none(), {}});
        b.appendBlock(f); b.emitCall(d, {}); b.emitRet(std::nullopt);
        b.appendBlock(g); b.emitCall(f, {}); b.emitRet(std::nullopt);
        auto text = c.save(m); c.valid(m);
        c.check(text.find("declare") < text.find("define internal"), "declarations printed before definitions");
        c.check(text.find("define internal") < text.find("define external"), "definition registration order retained");
        std::ostringstream a, second; IrPrinter(m, a).print(); IrPrinter(m, second).print();
        c.equal(a.str(), second.str(), "repeated printing stable");
        c.equal(a.str(), text, "save matches direct printer");
    }, "accept");
    for (std::string order : {"opaque_defined", "opaque_opaque", "defined_opaque", "empty_defined"}) {
        add(cases, "printer.struct_lines." + order, [=](Context& c) {
            auto m = makeModule(); IrBuilder b(m); auto first = b.declareStruct("First"), second = b.declareStruct("Second");
            if (order == "defined_opaque") b.defineStruct(first, {m.types.i32()});
            if (order == "empty_defined") b.defineStruct(first, {});
            if (order == "opaque_defined" || order == "empty_defined") b.defineStruct(second, {m.types.i8(), m.types.i32()});
            auto text = c.save(m); c.valid(m);
            const std::string first_line = order == "defined_opaque" ? "%\"First\" = type { i32 }\n" :
                order == "empty_defined" ? "%\"First\" = type {  }\n" : "%\"First\" = type opaque\n";
            c.contains(text, first_line + "%\"Second\" = type ");
            if (order == "opaque_opaque" || order == "defined_opaque") c.contains(text, "%\"Second\" = type opaque\n");
        }, "accept");
    }
    const std::vector<std::pair<std::string, std::string>> names{
        {"", ""}, {"space :: name", "space :: name"}, {"类型函数", "类型函数"},
        {"quote\"name", "quote\\22name"}, {"name\\41", "name\\5C41"}, {"nameA", "nameA"},
        {"line\nname\tend\r", "line\\0Aname\\09end\\0D"}, {std::string("del") + char(0x7f), "del\\7F"}};
    for (size_t index = 0; index < names.size(); ++index) {
        add(cases, "printer.name." + std::to_string(index), [=](Context& c) {
            auto m = makeModule(); IrBuilder b(m); auto s = b.declareStruct(names[index].first);
            b.defineStruct(s, {m.types.i32()}); auto st = m.types.structOf(s);
            const auto function_name = index == 0 ? std::string("identity") : names[index].first;
            auto f = b.addFunction(function_name, Linkage::Internal, FunctionForm::Definition, {st, {st}});
            auto g = b.addFunction("caller", Linkage::External, FunctionForm::Definition, {m.types.none(), {}});
            b.appendBlock(f); auto slot = b.emitAlloca(st, 4); b.emitStore(b.parameter(f, 0), slot, 4);
            b.emitGep(st, slot, {b.constantInt(m.types.i32(), 0), b.constantInt(m.types.i32(), 0)});
            b.emitRet(b.parameter(f, 0));
            b.appendBlock(g); auto p = b.emitAlloca(st, 4); auto arg = b.emitLoad(st, p, 4);
            b.emitCall(f, {arg}); b.emitRet(std::nullopt);
            auto text = c.save(m); c.valid(m);
            auto type = "%\"" + names[index].second + "\"";
            auto fun = "@\"" + (index == 0 ? std::string("identity") : names[index].second) + "\"";
            c.contains(text, type + " = type { i32 }"); c.contains(text, "define internal " + type + " " + fun + "(");
            c.contains(text, "alloca " + type); c.contains(text, "getelementptr " + type);
            c.contains(text, "call " + type + " " + fun + "(");
            c.check(text.find("@\"\"") == std::string::npos, "no empty/double-quoted function names");
        }, "accept");
    }
    for (std::string format : {"decimal", "hex", "hex_showbase", "octal", "octal_showbase", "showpos"}) {
        add(cases, "printer.stream." + format, [=](Context& c) {
            auto m = makeModule(); IrBuilder b(m);
            auto s = b.declareStruct("Numbers"); b.defineStruct(s, {m.types.arrayOf(m.types.i8(), 26)});
            auto f = b.addFunction("numbers", Linkage::Internal, FunctionForm::Definition, {m.types.i32(), {m.types.i32()}});
            std::vector<BlockId> blocks; for (int i = 0; i < 12; ++i) blocks.push_back(b.appendBlock(f));
            b.positionAtEnd(f, 0); auto pointer = b.emitEntryAlloca(m.types.i32(), 16);
            b.emitStore(b.constantInt(m.types.i32(), 26), pointer, 16);
            auto value = b.emitLoad(m.types.i32(), pointer, 16);
            for (int i = 0; i < 12; ++i) value = b.emitBinary(OpCode::Add, value, b.constantInt(m.types.i32(), 26));
            b.emitBr(blocks.back());
            for (size_t i = 1; i < blocks.size(); ++i) { b.positionAtEnd(f, blocks[i]); b.emitRet(b.parameter(f, 0)); }
            auto flags = std::ios_base::dec;
            if (format.find("hex") == 0) flags = std::ios_base::hex;
            if (format.find("octal") == 0) flags = std::ios_base::oct;
            if (format.find("showbase") != std::string::npos) flags |= std::ios_base::showbase;
            if (format == "showpos") flags |= std::ios_base::showpos;
            c.valid(m); auto text = c.save(m, flags);
            std::ostringstream standard; IrPrinter(m, standard).print();
            c.equal(text, standard.str(), "IR numbers must be independent of stream flags");
            c.contains(text, "[26 x i8]"); c.contains(text, "align 16");
            c.contains(text, "store i32 26"); c.contains(text, "br label %bb11"); c.contains(text, "%v12 = add i32");
        }, "accept");
    }
    const std::vector<std::string> op_names{"add", "sub", "mul", "udiv", "sdiv", "urem", "srem", "shl", "lshr", "ashr", "and", "or", "xor"};
    const std::vector<std::string> predicates{"eq", "ne", "ugt", "uge", "ult", "ule", "sgt", "sge", "slt", "sle"};
    for (unsigned width : {1u, 8u, 32u}) {
        add(cases, "printer.instruction_width.i" + std::to_string(width), [=](Context& c) {
            auto m = makeModule(); IrBuilder b(m); auto t = width == 1 ? m.types.i1() : width == 8 ? m.types.i8() : m.types.i32();
            auto f = b.addFunction("matrix", Linkage::Internal, FunctionForm::Definition, {t, {t}}); b.appendBlock(f);
            auto a = b.parameter(f, 0), one = b.constantInt(t, 1);
            for (size_t i = 0; i < op_names.size(); ++i) {
                auto o = b.emitBinary(static_cast<OpCode>(i), a, one); c.equal(o.type, t, "binary result type");
                c.equal(m.functions[f].value_types[valueId(o)], t, "binary value table type");
            }
            for (size_t i = 0; i < predicates.size(); ++i) c.equal(b.emitICmp(static_cast<Predicate>(i), a, one).type, m.types.i1(), "comparison result i1");
            b.emitRet(a); auto text = c.save(m); c.valid(m);
            for (auto name : op_names) c.contains(text, " = " + name + " i" + std::to_string(width) + " %v0, 1");
            for (auto name : predicates) c.contains(text, " = icmp " + name + " i" + std::to_string(width) + " %v0, 1");
            c.check(text.find("nsw") == std::string::npos && text.find("nuw") == std::string::npos, "no invented overflow flags");
        }, "accept");
    }
    add(cases, "printer.mutual_recursion", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        auto f = b.addFunction("left", Linkage::Internal, FunctionForm::Definition, {m.types.i32(), {m.types.i32()}});
        auto g = b.addFunction("right", Linkage::Internal, FunctionForm::Definition, {m.types.i32(), {m.types.i32()}});
        b.appendBlock(f); b.emitRet(b.emitCall(g, {b.parameter(f, 0)}));
        b.appendBlock(g); b.emitRet(b.emitCall(f, {b.parameter(g, 0)}));
        auto text = c.save(m); c.valid(m);
        c.contains(text, "call i32 @\"right\"(i32 %v0)"); c.contains(text, "call i32 @\"left\"(i32 %v0)");
    }, "accept");
}
} // namespace test
