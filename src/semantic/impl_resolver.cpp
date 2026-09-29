#include "impl_resolver.hpp"
#include "semantic_model.hpp"

namespace semantic {

bool ImplResolver::registerAssoc(AssocInfo info, SemanticModel& model, diagnostic::DiagnosticCollector& diag) {
    auto& members = model.assoc_by_struct_[info.owner];
    if (members.count(info.name)) {
        const auto* decl = info.const_decl ? static_cast<const ast::Item*>(info.const_decl) : static_cast<const ast::Item*>(info.func_decl);
        diag.add_entry(diagnostic::Severity::Error, decl->span.begin, "duplicated associated item " + info.name);
        return false;
    }
    AssocId id = model.assocs_.size();
    info.id = id;
    model.assocs_.push_back(info);
    model.assoc_by_struct_[info.owner][info.name] = id;
    if (info.const_decl) model.assoc_ids_[info.const_decl] = id;
    else model.assoc_ids_[info.func_decl] = id;
    return true;
}

bool ImplResolver::collectHeaders(const CrateIndex& index, TypeResolver& type_resolver, SemanticModel& model, diagnostic::DiagnosticCollector& diag) {
    bool success = true;
    for (const auto impl: index.pending_impls) {
        ImplId id = model.impls_.size();
        auto sym = resolveTarget(*impl, type_resolver, model, diag);
        if (!sym.has_value()) {
            success = false;
            continue;
        }
        ImplInfo info = {
            id,
            impl,
            *sym
        };
        model.impls_.push_back(info);
        model.impl_by_struct_[*sym].push_back(id);
        model.impl_ids_[impl] = id;
        for (const auto& assoc: impl->associated_items) {
            if (assoc.constant && *assoc.constant) {
                const auto* constant = assoc.constant->get();
                AssocInfo info = {
                    0,
                    *sym,
                    AssocKind::Constant,
                    constant->name,
                    nullptr,
                    constant
                };
                if (!registerAssoc(info, model, diag)) success = false;
            } else if (assoc.function && *assoc.function) {
                const auto* function = assoc.function->get();
                AssocInfo info = {
                    0,
                    *sym,
                    AssocKind::Function,
                    function->name,
                    function,
                    nullptr
                };
                if (!registerAssoc(info, model, diag)) success = false;
            }
        }
    }
    return success;
}

std::optional<SymbolId> ImplResolver::resolveTarget(const ast::ImplItem& impl, TypeResolver& type_resolver, SemanticModel& model, diagnostic::DiagnosticCollector& diag) {
    if (!impl.type) {
        diag.add_entry(diagnostic::Severity::Error, impl.span.begin, "implementation has no type");
        return std::nullopt;
    }
    TyId target_type = type_resolver.resolve(*impl.type, ResolveContext {});
    const TyInfo& type_info = model.typeContext().get(target_type);
    if (std::holds_alternative<ErrorTy>(type_info)) {
        return std::nullopt;
    }
    const auto* stru = std::get_if<StructTy>(&type_info);
    if (!stru) {
        diag.add_entry(diagnostic::Severity::Error, impl.span.begin, "implementation target must be a user struct");
        return std::nullopt;
    }
    return stru->def;
}

} // namespace semantic