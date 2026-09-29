#pragma once
#include "diagnostic/diagnostic.hpp"
#include "types.hpp"

namespace semantic {

class SemanticModel;

enum class DeriveKind;
struct StructInfo;

class DeriveChecker {
public:
    DeriveChecker(SemanticModel& model, diagnostic::DiagnosticCollector& diag):
        model_(model), diag_(diag) {}
    bool checkAll();
    bool supports(TyId type, DeriveKind trait);

private:
    SemanticModel& model_;
    diagnostic::DiagnosticCollector& diag_;
    bool computed_ = false;
    bool reported_ = false;

    static std::string kindName(DeriveKind trait);
    bool supportWith(TyId type, DeriveKind trait) const;
    bool structRequirementsHold(const StructInfo& info, DeriveKind trait) const;

    void computeValid();
    bool reportStructErrors();
};

} // namespace semantic