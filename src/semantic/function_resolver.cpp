#include "function_resolver.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/symbol.hpp"
#include "semantic/types.hpp"
#include "semantic_model.hpp"
#include <unordered_set>

namespace semantic {

bool FunctionResolver::collectTopLevel(const ast::Crate& crate, const CrateIndex& index, SemanticModel& model, diagnostic::DiagnosticCollector& diag) {
    bool ok = true;
    addBuiltinFunctions(index, model);
    for (const auto [name, id]: index.value_names) {
        const auto& item = index.symbols[id];
        if (item.kind != SymbolKind::Function) {
            continue;
        }
        const auto* decl = dynamic_cast<const ast::FunctionItem*>(item.declaration);
        if (!decl) {
            diag.add_entry(diagnostic::Severity::Error, item.declaration->span.begin, "unexpected item type");
            ok = false;
            continue;
        }
        FunctionId func_id = model.functions_.size();
        FunctionInfo func = {
            func_id,
            name,
            FunctionKind::TopLevel,
            decl,
            id,
            std::nullopt,
            std::nullopt,
            FunctionSignature {},
            false
        };
        model.functions_.push_back(func);
        model.function_ids_[decl] = func_id;
        model.top_level_function_ids_[id] = func_id;
    }
    for (const auto [item, id]: model.assoc_ids_) {
        const auto& assoc = model.assocs_[id];
        if (assoc.kind != AssocKind::Function) {
            continue;
        }
        const auto* decl = dynamic_cast<const ast::FunctionItem*>(item);
        if (!decl) {
            diag.add_entry(diagnostic::Severity::Error, item->span.begin, "unexpected item type");
            ok = false;
            continue;
        }
        FunctionId func_id = model.functions_.size();
        FunctionInfo func = {
            func_id,
            decl->name,
            FunctionKind::Associated,
            decl,
            std::nullopt,
            id,
            assoc.owner,
            FunctionSignature {},
            false
        };
        model.functions_.push_back(func);
        model.function_ids_[decl] = func_id;
        model.associated_function_ids_[id] = func_id;
    }
    return ok;
}

bool FunctionResolver::resolveSignatures(const ast::Crate& crate, const CrateIndex& index, SemanticModel& model, TypeResolver& type_resolver, diagnostic::DiagnosticCollector& diag) {
    bool ok = true;
    for (auto& func: model.functions_) {
        if (func.kind == FunctionKind::Builtin) {
            continue;
        }
        const auto* decl = func.declaration;
        std::vector<ParameterInfo> params;
        std::unordered_set<std::string> param_names;
        if (decl->self_param.has_value()) {
            if (!func.owner_struct.has_value()) {
                diag.add_entry(diagnostic::Severity::Error, decl->self_param->span.begin, "self param has no owner struct");
                ok = false;
                continue;
            }
            TyId self_ty;
            TyId raw_ty = model.typeContext().insert(StructTy {*func.owner_struct});
            if (decl->self_param->is_ref) {
                if (decl->self_param->is_mut) {
                    func.signature.receiver_mode = ReceiverMode::MutableRef;
                    self_ty = model.typeContext().insert(RefTy {raw_ty, true});
                } else {
                    func.signature.receiver_mode = ReceiverMode::Ref;
                    self_ty = model.typeContext().insert(RefTy {raw_ty, false});
                }
            } else {
                func.signature.receiver_mode = ReceiverMode::ByValue;
                self_ty = raw_ty;
            }
            if (self_ty == model.typeContext().error()) {
                diag.add_entry(diagnostic::Severity::Error, decl->self_param->span.begin, "failed to resolve self type");
                ok = false;
                continue;
            }
            ParameterInfo self = {
                "self",
                self_ty,
                !decl->self_param->is_ref && decl->self_param->is_mut,
                true,
                decl->self_param->span,
            };
            params.push_back(self);
        }
        bool func_ok = true;
        for (const auto& param: decl->function_params) {
            auto ty = type_resolver.resolve(*param.type, ResolveContext {func.owner_struct});
            if (param_names.count(param.name)) {
                diag.add_entry(diagnostic::Severity::Error, param.span.begin, "duplicated param name");
                func_ok = false;
                break;
            }
            param_names.insert(param.name);
            if (ty == model.typeContext().error()) {
                diag.add_entry(diagnostic::Severity::Error, param.span.begin, "failed to resolve type");
                ok = false;
                func_ok = false;
                continue;
            }
            params.push_back(ParameterInfo {
                param.name,
                ty,
                param.is_mut,
                false,
                param.span
            });
        }
        if (!func_ok) {
            ok = false;
            continue;
        }
        func.signature.parameters = std::move(params);
        TyId return_ty;
        if (!decl->return_type) {
            func.signature.return_type = model.typeContext().insert(UnitTy {});
        } else {
            return_ty = type_resolver.resolve(*decl->return_type, ResolveContext {func.owner_struct});
            if (return_ty == model.typeContext().error()) {
                diag.add_entry(diagnostic::Severity::Error, decl->return_type->span.begin, "failed to resolve return type");
                ok = false;
                continue;
            } else {
                func.signature.return_type = return_ty;
            }
        }
        func.signature_valid = true;
        if (func.name == "main" && func.kind == FunctionKind::TopLevel) {
            if (func.signature.return_type != model.typeContext().insert(TyInfo {UnitTy {}})) {
                diag.add_entry(diagnostic::Severity::Error, func.declaration->span.begin, "function main must return unit type");
                ok = false;
            }
        }
    }
    return ok;
}

bool FunctionResolver::checkEntryPoint(const ast::Crate& crate, const CrateIndex& index, SemanticModel& model, diagnostic::DiagnosticCollector& diag) {
    if (!index.value_names.count("main")) {
        diag.add_entry(diagnostic::Severity::Error, crate.span.begin, "missing main function");
        return false;
    }
    auto id = index.value_names.at("main");
    const auto& sym = index.symbols[id];
    if (!sym.declaration) {
        diag.add_entry(diagnostic::Severity::Error, crate.span.begin, "unexpected empty main function");
        return false;
    }
    const auto* decl = sym.declaration;
    const auto* func = dynamic_cast<const ast::FunctionItem*>(decl);
    if (!func) {
        diag.add_entry(diagnostic::Severity::Error, decl->span.begin, "main item not a function");
        return false;
    }
    if (func->generic_params.size()) {
        diag.add_entry(diagnostic::Severity::Error, func->generic_params.front().span.begin, "main function cannot have generic params");
        return false;
    }
    if (func->self_param.has_value()) {
        diag.add_entry(diagnostic::Severity::Error, func->self_param->span.begin, "main function cannot have self param");
        return false;
    }
    if (func->function_params.size()) {
        diag.add_entry(diagnostic::Severity::Error, func->function_params.front().span.begin, "main function cannot have params");
        return false;
    }
    return true;
}

void FunctionResolver::addBuiltinFunctions(const CrateIndex& index, SemanticModel& model) {
    std::vector<FunctionInfo> funcs = builtin_functions;
    funcs[0].signature = FunctionSignature {
        std::vector<ParameterInfo>(),
        model.typeContext().insert(TyInfo {PrimaryTy {PrimaryTyKind::I32}})
    };
    funcs[0].signature_valid = true;
    funcs[1].signature = FunctionSignature {
        std::vector<ParameterInfo> {
            ParameterInfo {
                "x",
                model.typeContext().insert(TyInfo {PrimaryTy {PrimaryTyKind::I32}})
            }
        },
        model.typeContext().insert(TyInfo {UnitTy {}})
    };
    funcs[1].signature_valid = true;
    funcs[2].signature = FunctionSignature {
        std::vector<ParameterInfo> {
            ParameterInfo {
                "x",
                model.typeContext().insert(TyInfo {PrimaryTy {PrimaryTyKind::I32}})
            }
        },
        model.typeContext().insert(TyInfo {UnitTy {}})
    };
    funcs[2].signature_valid = true;
    for (auto& func: funcs) {
        func.top_level_symbol = index.value_names.at(func.name);
        model.functions_.push_back(func);
        model.top_level_function_ids_[*func.top_level_symbol] = func.id;
    }
}

} // namespace semantic
