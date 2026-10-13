#include "test.hpp"

namespace test {
void verifierCases(std::vector<Case>& cases) {
    add(cases, "verifier.empty_and_declaration", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m); c.valid(m);
        b.addFunction("decl", Linkage::External, FunctionForm::Declaration, {m.types.none(), {m.types.i32()}});
        c.save(m); c.valid(m);
    }, "accept");
    add(cases, "verifier.no_blocks", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        auto f = b.addFunction("empty", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        c.save(m); const auto e = c.verify(m); c.equal(e.size(), size_t(1), "one incomplete definition");
        if (e.size() == 1) c.error(e[0], f, {}, {}, {}, {}, "has no blocks");
    }, "reject-parse", "function body requires at least one basic block");
    add(cases, "verifier.missing_terminator", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        auto f = b.addFunction("missing", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        auto bb = b.appendBlock(f); b.emitAlloca(m.types.i32(), 4);
        c.save(m); const auto e = c.verify(m); c.equal(e.size(), size_t(1), "one missing terminator");
        if (e.size() == 1) c.error(e[0], f, bb, {}, {}, {}, "has no terminator");
    }, "reject-parse", "expected instruction opcode");
    add(cases, "verifier.completeness_collection", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        auto f = b.addFunction("empty", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        auto g = b.addFunction("missing", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        auto first = b.appendBlock(g), second = b.appendBlock(g);
        auto h = b.addFunction("good", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        b.appendBlock(h); b.emitRet(std::nullopt);
        auto e = c.verify(m); c.equal(e.size(), size_t(3), "collect completeness errors across functions/blocks");
        if (e.size() == 3) {
            c.error(e[0], f, {}, {}, {}, {}, "has no blocks");
            c.error(e[1], g, first, {}, {}, {}, "has no terminator");
            c.error(e[2], g, second, {}, {}, {}, "has no terminator");
        }
    });
    for (std::string form : {"self", "other", "dead"}) {
        add(cases, "verifier.entry_predecessor." + form, [=](Context& c) {
            auto m = makeModule(); IrBuilder b(m);
            auto f = b.addFunction("entry_backedge", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
            auto entry = b.appendBlock(f);
            if (form == "self") b.emitBr(entry);
            else {
                auto other = b.appendBlock(f); b.emitBr(entry); b.positionAtEnd(f, entry);
                if (form == "dead") b.emitRet(std::nullopt); else b.emitBr(other);
            }
            c.save(m); auto e = c.verify(m); c.equal(e.size(), size_t(1), "entry predecessor rejected");
            if (e.size() == 1) c.error(e[0], f, entry, {}, {}, {}, "unexpected predecessor");
        }, "reject-verify", "Entry block to function must not have predecessors");
    }
    for (std::string form : {"direct", "indirect", "array", "nested_array", "zero_array", "dependent", "disjoint"}) {
        add(cases, "verifier.struct_cycle." + form, [=](Context& c) {
            auto m = makeModule(); IrBuilder b(m);
            // Extra types make TypeId and StructId diverge deliberately.
            m.types.arrayOf(m.types.i8(), 42); m.types.arrayOf(m.types.i32(), 17);
            auto a = b.declareStruct("A"); auto at = m.types.structOf(a);
            std::vector<StructId> expected{a};
            if (form == "indirect") {
                auto d = b.declareStruct("B"); auto dt = m.types.structOf(d);
                b.defineStruct(a, {dt}); b.defineStruct(d, {at}); expected.push_back(d);
            } else if (form == "dependent") {
                b.defineStruct(a, {at}); auto d = b.declareStruct("Dependent"); b.defineStruct(d, {at}); expected.push_back(d);
            } else if (form == "disjoint") {
                b.defineStruct(a, {at}); auto d = b.declareStruct("B"); auto dt = m.types.structOf(d);
                b.defineStruct(d, {dt}); expected.push_back(d);
            } else {
                auto field = at;
                if (form == "array") field = m.types.arrayOf(at, 1);
                if (form == "nested_array") field = m.types.arrayOf(m.types.arrayOf(at, 3), 2);
                if (form == "zero_array") field = m.types.arrayOf(at, 0);
                b.defineStruct(a, {field});
            }
            c.save(m); auto e = c.verify(m); c.equal(e.size(), expected.size(), "report every infinite layout struct");
            if (e.size() == expected.size()) for (size_t i = 0; i < e.size(); ++i) c.error(e[i], {}, {}, {}, {}, expected[i], "infinite layout");
        }, "reject-parse", "is recursive");
    }
    add(cases, "verifier.struct_dag_and_id_collision", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        for (int i = 0; i < 8; ++i) b.declareStruct("S" + std::to_string(i));
        const auto t0 = m.types.structOf(0); // TypeId 5 is also a valid, unrelated StructId.
        c.equal(t0, 5u, "TypeId/StructId collision fixture");
        auto t1 = m.types.structOf(1), t2 = m.types.structOf(2), t3 = m.types.structOf(3);
        b.defineStruct(0, {t1, t2, m.types.ptr()});
        b.defineStruct(1, {m.types.arrayOf(m.types.arrayOf(t3, 2), 1)});
        b.defineStruct(2, {t3, t3}); b.defineStruct(3, {});
        b.defineStruct(4, {m.types.ptr()}); b.defineStruct(5, {m.types.i32()});
        auto f = b.addFunction("finite", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        b.appendBlock(f); b.emitAlloca(t0, 4); b.emitRet(std::nullopt);
        c.save(m); c.valid(m);
    }, "accept");
    add(cases, "verifier.struct_errors_precede_completeness", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m); auto s = b.declareStruct("Cycle"); b.defineStruct(s, {m.types.structOf(s)});
        b.addFunction("empty", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {}});
        auto e = c.verify(m); c.equal(e.size(), size_t(1), "layout failure stops later phases safely");
        if (e.size() == 1) c.error(e[0], {}, {}, {}, {}, s, "infinite layout");
    });
    add(cases, "verifier.struct_cycle_propagation", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        auto root = b.declareStruct("Root"), child = b.declareStruct("Child"), leaf = b.declareStruct("Leaf");
        b.defineStruct(root, {m.types.structOf(child), m.types.structOf(leaf)});
        b.defineStruct(child, {m.types.structOf(child)}); b.defineStruct(leaf, {m.types.i32()});
        c.save(m); auto e = c.verify(m);
        c.equal(e.size(), size_t(2), "failed child propagates without marking later finite child infinite");
        if (e.size() == 2) {
            c.error(e[0], {}, {}, {}, {}, root, "infinite layout");
            c.error(e[1], {}, {}, {}, {}, child, "infinite layout");
        }
    }, "reject-parse", "is recursive");
    for (std::string form : {"ordered", "forward", "self", "dead_forward", "late_alloca"}) {
        const bool reject = form == "forward" || form == "self";
        add(cases, "verifier.same_block." + form, [=](Context& c) {
            auto m = makeModule(); IrBuilder b(m);
            auto f = b.addFunction("order", Linkage::Internal, FunctionForm::Definition, {m.types.i32(), {m.types.i32()}});
            auto entry = b.appendBlock(f), block = entry;
            if (form == "dead_forward") { b.emitRet(b.parameter(f, 0)); block = b.appendBlock(f); }
            auto first = b.emitBinary(OpCode::Add, b.parameter(f, 0), b.constantInt(m.types.i32(), 1));
            auto second = b.emitBinary(OpCode::Add, first, b.constantInt(m.types.i32(), 1));
            b.emitRet(second);
            if (form == "forward" || form == "dead_forward") std::swap(m.functions[f].blocks[block].instructions[0], m.functions[f].blocks[block].instructions[1]);
            if (form == "self") std::get<Binary>(m.functions[f].blocks[block].instructions[0].data).lhs = first;
            if (form == "late_alloca") b.emitEntryAlloca(m.types.i32(), 4);
            c.save(m); auto e = c.verify(m);
            c.equal(e.size(), reject ? size_t(1) : size_t(0), "same-block definition must precede reachable use");
            if (reject && e.size() == 1) c.error(e[0], f, block, size_t(0), valueId(first), {}, "definition");
        }, reject ? "reject-verify" : "accept", form == "self" ? "Only PHI nodes may reference their own value" : reject ? "Instruction does not dominate all uses" : "");
    }
    const std::vector<std::string> slots{
        "load_address", "store_address", "store_value", "store_both", "binary_lhs", "binary_rhs", "binary_both",
        "icmp_lhs", "icmp_rhs", "icmp_both", "zext", "trunc", "gep_base", "gep_first", "gep_array1", "gep_array2",
        "call_arg0", "call_arg1", "call_both", "condbr", "ret"};
    for (const auto& slot : slots) for (bool positive : {false, true}) {
        add(cases, "verifier.use." + slot + (positive ? ".valid" : ".invalid"), [=](Context& c) {
            auto m = makeModule(); IrBuilder b(m);
            auto callee = b.addFunction("consume", Linkage::External, FunctionForm::Declaration, {m.types.none(), {m.types.i32(), m.types.i32()}});
            auto f = b.addFunction("use", Linkage::Internal, FunctionForm::Definition, {m.types.i32(), {m.types.i1(), m.types.ptr()}});
            std::vector<BlockId> blocks; for (int i = 0; i < 5; ++i) blocks.push_back(b.appendBlock(f));
            b.positionAtEnd(f, positive ? blocks[0] : blocks[1]);
            auto badptr = b.emitAlloca(m.types.i32(), 4);
            auto bad32 = b.emitBinary(OpCode::Add, b.constantInt(m.types.i32(), 1), b.constantInt(m.types.i32(), 2));
            auto bad8 = b.emitTrunc(bad32, m.types.i8());
            auto bad1 = b.emitICmp(Predicate::Eq, bad32, b.constantInt(m.types.i32(), 0));
            b.positionAtEnd(f, blocks[0]); b.emitCondBr(b.parameter(f, 0), blocks[1], blocks[2]);
            b.positionAtEnd(f, blocks[1]); b.emitBr(blocks[3]);
            b.positionAtEnd(f, blocks[2]); b.emitBr(blocks[3]);
            b.positionAtEnd(f, blocks[3]);
            auto good = b.constantInt(m.types.i32(), 0), ptr = b.parameter(f, 1);
            Operand used = bad32;
            const auto index = m.functions[f].blocks[3].instructions.size();
            if (slot == "load_address") { used = badptr; b.emitLoad(m.types.i32(), used, 4); }
            else if (slot.find("store") == 0) {
                used = slot == "store_address" || slot == "store_both" ? badptr : bad32;
                if (slot == "store_both") b.emitStore(badptr, badptr, 4);
                else b.emitStore(slot == "store_value" ? bad32 : good, slot == "store_address" ? badptr : ptr, 4);
            } else if (slot.find("binary") == 0) b.emitBinary(OpCode::Add, slot != "binary_rhs" ? bad32 : good, slot != "binary_lhs" ? bad32 : good);
            else if (slot.find("icmp") == 0) b.emitICmp(Predicate::Eq, slot != "icmp_rhs" ? bad32 : good, slot != "icmp_lhs" ? bad32 : good);
            else if (slot == "zext") { used = bad8; b.emitZExt(used, m.types.i32()); }
            else if (slot == "trunc") b.emitTrunc(bad32, m.types.i8());
            else if (slot.find("gep") == 0) {
                auto a = m.types.arrayOf(m.types.arrayOf(m.types.i32(), 3), 2);
                used = slot == "gep_base" ? badptr : bad32;
                b.emitGep(a, slot == "gep_base" ? badptr : ptr,
                    {slot == "gep_first" ? bad32 : good, slot == "gep_array1" ? bad32 : good, slot == "gep_array2" ? bad32 : good});
            } else if (slot.find("call") == 0) b.emitCall(callee, {slot != "call_arg1" ? bad32 : good, slot != "call_arg0" ? bad32 : good});
            else if (slot == "condbr") { used = bad1; b.emitCondBr(used, blocks[4], blocks[4]); }
            else b.emitRet(bad32);
            if (slot != "condbr" && slot != "ret") b.emitRet(good);
            b.positionAtEnd(f, blocks[4]); b.emitRet(good);
            c.save(m); auto e = c.verify(m);
            const size_t expected = positive ? 0 : slot.find("both") != std::string::npos ? 2 : 1;
            c.equal(e.size(), expected, "diagnostic count for operand slot " + slot);
            for (const auto& err : e) c.error(err, f, blocks[3], index, valueId(used), {}, "local value may not have definition");
        }, positive ? "accept" : "reject-verify", positive ? "" : "Instruction does not dominate all uses");
    }
    add(cases, "verifier.multi_function_ssa", [](Context& c) {
        auto m = makeModule(); IrBuilder b(m);
        for (unsigned n = 0; n < 2; ++n) {
            auto f = b.addFunction("bad" + std::to_string(n), Linkage::Internal, FunctionForm::Definition, {m.types.i32(), {m.types.i1()}});
            auto entry = b.appendBlock(f), left = b.appendBlock(f), right = b.appendBlock(f), join = b.appendBlock(f);
            auto value = b.emitBinary(OpCode::Add, b.constantInt(m.types.i32(), 1), b.constantInt(m.types.i32(), 1));
            b.emitRet(value); // Definition in join used by predecessor left.
            b.positionAtEnd(f, left); b.emitBinary(OpCode::Add, value, value); b.emitBr(join);
            b.positionAtEnd(f, right); b.emitBr(join);
            b.positionAtEnd(f, entry); b.emitCondBr(b.parameter(f, 0), left, right);
        }
        c.save(m); auto e = c.verify(m); c.equal(e.size(), size_t(4), "collect both operands in both functions");
        if (e.size() == 4) for (size_t i = 0; i < e.size(); ++i) c.error(e[i], FunctionId(i / 2), BlockId(1), size_t(0), ValueId(1), {}, "definition");
    }, "reject-verify", "Instruction does not dominate all uses");
}
} // namespace test
