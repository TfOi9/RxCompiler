#pragma once
#include "../common.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>

namespace test {
using namespace ir;
constexpr uint32_t invalid = std::numeric_limits<uint32_t>::max();
struct InfrastructureError : std::runtime_error { using std::runtime_error::runtime_error; };

struct Context {
    std::filesystem::path output;
    size_t failures = 0;
    size_t assertions = 0;
    void check(bool condition, const std::string& message) {
        ++assertions;
        if (!condition) { ++failures; std::cerr << "ASSERT: " << message << '\n'; }
    }
    template<class A, class B>
    void equal(const A& actual, const B& expected, const std::string& message) {
        check(actual == expected, message);
    }
    void contains(const std::string& text, const std::string& needle) {
        check(text.find(needle) != std::string::npos, "missing text: " + needle);
    }
    std::string save(const Module& module, std::ios_base::fmtflags flags = std::ios_base::dec) {
        std::ostringstream stream;
        stream.flags(flags);
        IrPrinter(module, stream).print();
        std::ofstream file(output / "module.ll", std::ios::binary);
        file << stream.str();
        if (!file) throw InfrastructureError("cannot write module.ll");
        return stream.str();
    }
    std::vector<IrError> verify(const Module& module) {
        const auto errors = IrVerifier(module).verify();
        std::ofstream log(output / "project-verifier.log");
        auto field = [&](const auto& value) { if (value) log << *value; else log << '-'; };
        for (const auto& e : errors) {
            log << "func="; field(e.func); log << " block="; field(e.block);
            log << " inst="; field(e.inst); log << " value="; field(e.value);
            log << " named="; field(e.named); log << " " << e.message << '\n';
        }
        if (errors.empty()) log << "PASS: no project verifier errors\n";
        if (!log) throw InfrastructureError("cannot write project-verifier.log");
        return errors;
    }
    void valid(const Module& module) {
        const auto errors = verify(module);
        check(errors.empty(), "project verifier must accept module");
    }
    void error(const IrError& e, std::optional<FunctionId> f,
               std::optional<BlockId> b, std::optional<size_t> i,
               std::optional<ValueId> v, std::optional<StructId> s,
               const std::string& message) {
        equal(e.func, f, "diagnostic function"); equal(e.block, b, "diagnostic block");
        equal(e.inst, i, "diagnostic instruction"); equal(e.value, v, "diagnostic value");
        equal(e.named, s, "diagnostic struct"); contains(e.message, message);
    }
};

struct Case {
    std::string id;
    // none, accept, reject-parse, reject-verify; substring diagnoses the LLVM failure.
    std::string oracle;
    std::string diagnostic;
    std::function<void(Context&)> run;
};
inline void add(std::vector<Case>& cases, std::string id,
                std::function<void(Context&)> run, std::string oracle = "none",
                std::string diagnostic = "") {
    cases.push_back({std::move(id), std::move(oracle), std::move(diagnostic), std::move(run)});
}
inline ValueId valueId(const Operand& operand) { return std::get<LocalValue>(operand.data).id; }
inline Operand immediate(TypeId type, uint32_t bits) { return {type, Immediate{bits}}; }

// Structural snapshot, independent of Printer. It includes instruction contents,
// signatures, definitions and terminators, not just vector sizes.
inline std::string snapshot(const Module& m, const IrBuilder& builder) {
    std::ostringstream s;
    auto operand = [&](const Operand& o) {
        s << o.type << ':' << o.data.index() << ':';
        if (auto p = std::get_if<Immediate>(&o.data)) s << p->bits;
        if (auto p = std::get_if<LocalValue>(&o.data)) s << p->func << ':' << p->id;
        s << ';';
    };
    s << builder.hasInsertionPoint() << '|' << m.target_triple << '|' << m.data_layout << '|';
    for (TypeId id = 0; m.types.contains(id); ++id) {
        const auto t = m.types.get(id); s << t.index() << ':';
        std::visit([&](const auto& ty) {
            using T = std::decay_t<decltype(ty)>;
            if constexpr (std::is_same_v<T, IntegerType>) s << unsigned(ty.bits);
            if constexpr (std::is_same_v<T, ArrayType>) s << ty.element << ':' << ty.length;
            if constexpr (std::is_same_v<T, NamedStructType>) s << ty.id;
        }, t); s << ';';
    }
    for (const auto& d : m.structs) {
        s << d.name.size() << ':' << d.name << ':' << bool(d.fields) << ':';
        if (d.fields) for (auto t : *d.fields) s << t << ',';
        s << ';';
    }
    for (const auto& f : m.functions) {
        s << f.id << ':' << f.symbol_name << ':' << int(f.form) << ':' << int(f.linkage)
          << ':' << f.signature.return_type << ':';
        for (auto t : f.signature.parameters) s << t << ',';
        s << '|'; for (auto p : f.parameters) s << p.value << ':' << p.type << ',';
        s << '|'; for (auto t : f.value_types) s << t << ',';
        for (const auto& b : f.blocks) {
            s << "[" << b.id << ':';
            for (const auto& inst : b.instructions) {
                if (inst.result) s << *inst.result; else s << '-';
                s << ':' << inst.data.index() << ':';
                std::visit([&](const auto& x) {
                    using T = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<T, Alloca>) s << x.type << ':' << unsigned(x.align);
                    if constexpr (std::is_same_v<T, Load>) { s << x.type << ':' << unsigned(x.align); operand(x.address); }
                    if constexpr (std::is_same_v<T, Store>) { s << unsigned(x.align); operand(x.value); operand(x.address); }
                    if constexpr (std::is_same_v<T, Binary>) { s << int(x.opcode); operand(x.lhs); operand(x.rhs); }
                    if constexpr (std::is_same_v<T, ICmp>) { s << int(x.predicate); operand(x.lhs); operand(x.rhs); }
                    if constexpr (std::is_same_v<T, ZExt> || std::is_same_v<T, Trunc>) { s << x.type; operand(x.value); }
                    if constexpr (std::is_same_v<T, Gep>) { s << x.type; operand(x.base); for (auto o : x.indices) operand(o); }
                    if constexpr (std::is_same_v<T, Call>) { s << x.func; for (auto o : x.args) operand(o); }
                }, inst.data); s << ';';
            }
            if (!b.terminator) s << '-';
            else std::visit([&](const auto& x) {
                using T = std::decay_t<decltype(x)>;
                s << b.terminator->index() << ':';
                if constexpr (std::is_same_v<T, Br>) s << x.target;
                if constexpr (std::is_same_v<T, CondBr>) { operand(x.condition); s << x.yes << ':' << x.no; }
                if constexpr (std::is_same_v<T, Ret>) { s << bool(x.value); if (x.value) operand(*x.value); }
            }, *b.terminator);
            s << ']';
        }
        s << '|';
    }
    return s.str();
}

template<class F>
void rejects(Context& c, Module& m, IrBuilder& b, F action, const std::string& message) {
    const auto before = snapshot(m, b);
    bool caught = false;
    try { action(); }
    catch (const std::logic_error& e) {
        caught = true;
        c.check(typeid(e) == typeid(std::logic_error), "expected exact std::logic_error type");
        c.contains(e.what(), message);
    }
    catch (const std::exception& e) { c.check(false, std::string("wrong exception: ") + e.what()); }
    c.check(caught, "expected std::logic_error containing: " + message);
    c.equal(snapshot(m, b), before, "failed operation changed module/insertion state");
}

struct Fixture {
    Module m = makeModule();
    IrBuilder b{m};
    FunctionId f;
    BlockId entry;
    Fixture() {
        f = b.addFunction("test", Linkage::Internal, FunctionForm::Definition,
                         {m.types.i32(), {m.types.i32(), m.types.i8(), m.types.i1(), m.types.ptr()}});
        entry = b.appendBlock(f);
    }
    Operand i(uint32_t n = 1) { return b.constantInt(m.types.i32(), n); }
    template<class F>
    void reject(Context& c, F action, const std::string& message) {
        const bool active = b.hasInsertionPoint();
        rejects(c, m, b, action, message);
        if (active) {
            auto& fn = m.functions[f];
            const auto next = fn.value_types.size(), count = fn.blocks[entry].instructions.size();
            const auto sentinel = b.emitBinary(OpCode::Add, i(0), i(0));
            c.equal(valueId(sentinel), next, "failure consumed ValueId");
            c.equal(fn.blocks[entry].instructions.size(), count + 1, "failure changed insertion target");
        }
    }
    void finish(Context& c) { b.emitRet(i()); c.save(m); c.valid(m); }
};

void builderCases(std::vector<Case>&);
void printerCases(std::vector<Case>&);
void verifierCases(std::vector<Case>&);
void graphCases(std::vector<Case>&);
} // namespace test
