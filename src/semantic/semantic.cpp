#include "semantic.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/body_checker.hpp"
#include "semantic/const_evaluator.hpp"
#include "semantic/function_resolver.hpp"
#include "semantic/semantic_model.hpp"
#include "semantic/symbol.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/layout_checker.hpp"
#include "semantic/derive_checker.hpp"

namespace semantic {

SemanticResult analyze(const ast::Crate& crate) {
    diagnostic::DiagnosticCollector collector;
    CrateIndex index = collectDeclarations(&crate, &collector);
    SemanticModel model;
    
    auto finish = [&]() -> SemanticResult {
        return SemanticResult {
            !collector.has_error(),
            std::make_unique<SemanticModel>(std::move(model)),
            collector.diagnostics()
        };
    };

    if (collector.has_error()) {
        return finish();
    }
    
    StructResolver struct_resolver;
    struct_resolver.declareAll(index, model);

    ConstEvaluator const_evaluator(index, model, collector);
    TypeResolver type_resolver(model.typeContext(), index, const_evaluator, collector);

    ImplResolver impl_resolver;
    if (!impl_resolver.collectHeaders(index, type_resolver, model, collector)) {
        return finish();
    }

    const_evaluator.collectDefinitions();

    FunctionResolver function_resolver;
    if (!function_resolver.collectTopLevel(crate, index, model, collector)) {
        return finish();
    }
    if (!function_resolver.checkEntryPoint(crate, index, model, collector)) {
        return finish();
    }

    const_evaluator.resolveTypes(type_resolver);
    if (!const_evaluator.evaluateAll()) {
        return finish();
    }

    struct_resolver.resolveAll(model, type_resolver, const_evaluator, collector);
    if (collector.has_error()) {
        return finish();
    }

    function_resolver.resolveSignatures(crate, index, model, type_resolver, collector);
    if (collector.has_error()) {
        return finish();
    }

    LayoutChecker layout_checker(model, index, collector);
    layout_checker.checkAll();

    DeriveChecker derive_checker(model, collector);
    derive_checker.checkAll();

    if (collector.has_error()) {
        return finish();
    }

    BodyChecker body_checker(index, model, const_evaluator, type_resolver, derive_checker, collector);

    const bool body_ok = body_checker.checkAll(crate);

    SemanticResult result = finish();
    result.success = result.success && body_ok;
    return result;
}

IndexResult index(const ast::Crate& crate) {
    diagnostic::DiagnosticCollector collector;
    CrateIndex index = collectDeclarations(&crate, &collector);
    return IndexResult {
        !collector.has_error(),
        std::move(index),
        collector.diagnostics()
    };
}

void dump(const SemanticResult &result, std::ostream& os) {
    if (result.success) {
        os << "Semantic analysis successed.\n";
    } else {
        os << "Semantic analysis failed.\n";
    }
    for (const auto& diag: result.diagnostics) {
        diagnostic::dumpDiagnostic(diag, os);
    }
}

void dump(const IndexResult &result, std::ostream& os) {
    if (result.success) {
        os << "Index Successed.\n";
        result.index.dump(os);
    } else {
        os << "Index Failed.\n";
        for (const auto& diag: result.diagnostics) {
            diagnostic::dumpDiagnostic(diag, os);
        }
    }
}

} // namespace semantic