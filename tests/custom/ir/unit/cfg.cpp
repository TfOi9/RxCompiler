#include "test.hpp"

namespace test {
namespace {
struct Graph { std::string name; std::vector<std::vector<unsigned>> edges; };
// Dominance oracle uses path exclusion, not iterative dominator intersections.
bool reaches(const Graph& g, unsigned target, std::optional<unsigned> excluded = {}) {
    if (excluded == 0) return false;
    std::vector<unsigned> pending{0}; std::vector<bool> seen(g.edges.size()); seen[0] = true;
    while (!pending.empty()) {
        auto node = pending.back(); pending.pop_back();
        if (node == target) return true;
        for (auto next : g.edges[node]) if (next != excluded && !seen[next]) { seen[next] = true; pending.push_back(next); }
    }
    return false;
}
bool legalUse(const Graph& g, unsigned definition, unsigned use) {
    if (!reaches(g, use)) return true; // LLVM permits dominance uses in dead blocks.
    if (!reaches(g, definition)) return false;
    return !reaches(g, use, definition);
}
}
void graphCases(std::vector<Case>& cases) {
    const std::vector<Graph> graphs{
        {"linear", {{1}, {2}, {3}, {}}}, {"diamond", {{1, 2}, {3}, {3}, {}}},
        {"loop", {{1}, {2, 3}, {1}, {}}}, {"self_loop", {{1}, {1, 2}, {}}},
        {"duplicate", {{1, 1}, {2}, {}}}, {"irreducible", {{1, 2}, {2, 3}, {1, 3}, {}}},
        {"dead_chain", {{3}, {2}, {3}, {}}}, {"dead_cycle", {{3}, {2}, {1}, {}}}};
    for (const auto& g : graphs) for (bool reverse : {false, true})
        for (unsigned def = 0; def < g.edges.size(); ++def) for (unsigned use = 0; use < g.edges.size(); ++use) {
            const bool legal = legalUse(g, def, use);
            const auto name = "cfg." + g.name + (reverse ? ".reverse" : ".forward") + ".d" + std::to_string(def) + ".u" + std::to_string(use);
            add(cases, name, [=](Context& c) {
                auto m = makeModule(); IrBuilder b(m);
                auto f = b.addFunction("graph", Linkage::Internal, FunctionForm::Definition, {m.types.none(), {m.types.i1()}});
                std::vector<BlockId> block(g.edges.size()); block[0] = b.appendBlock(f);
                for (unsigned i = 1; i < block.size(); ++i) block[reverse ? block.size() - i : i] = b.appendBlock(f);
                b.positionAtEnd(f, block[def]); auto value = b.emitBinary(OpCode::Add, b.constantInt(m.types.i32(), 1), b.constantInt(m.types.i32(), 2));
                b.positionAtEnd(f, block[use]); const auto index = m.functions[f].blocks[block[use]].instructions.size();
                b.emitBinary(OpCode::Add, value, b.constantInt(m.types.i32(), 0));
                for (unsigned i = 0; i < block.size(); ++i) {
                    b.positionAtEnd(f, block[i]);
                    if (g.edges[i].empty()) b.emitRet(std::nullopt);
                    else if (g.edges[i].size() == 1) b.emitBr(block[g.edges[i][0]]);
                    else b.emitCondBr(b.parameter(f, 0), block[g.edges[i][0]], block[g.edges[i][1]]);
                }
                const auto before = snapshot(m, b); c.save(m); const auto e = c.verify(m);
                c.equal(snapshot(m, b), before, "verifier is read-only");
                c.equal(e.size(), legal ? size_t(0) : size_t(1), "path-exclusion dominance oracle");
                if (!legal && e.size() == 1) c.error(e[0], f, block[use], index, valueId(value), {},
                    reaches(g, def) ? "local value may not have definition" : "local value has no reachable definition");
            }, legal ? "accept" : "reject-verify", legal ? "" : "Instruction does not dominate all uses");
        }
}
} // namespace test
