#pragma once
#include "../diagnostic/diagnostic.hpp"
#include "../ast/ast.hpp"
#include "symbol.hpp"
#include "semantic_model.hpp"
#include <vector>
#include <memory>

namespace semantic {

struct SemanticResult {
    bool success;
    std::unique_ptr<SemanticModel> model;
    std::vector<diagnostic::Diagnostic> diagnostics;
};

struct IndexResult {
    bool success;
    CrateIndex index;
    std::vector<diagnostic::Diagnostic> diagnostics;
};

SemanticResult analyze(const ast::Crate& crate);
IndexResult index(const ast::Crate& crate);
void dump(const IndexResult &result, std::ostream& os);

} // namespace semantic