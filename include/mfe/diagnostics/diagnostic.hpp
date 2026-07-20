#pragma once

#include "mfe/core/geometry.hpp"

#include <string>
#include <vector>

namespace mfe::diagnostics {

enum class Severity { info, warning, error, critical };

struct Diagnostic {
    std::string code;
    Severity severity{Severity::info};
    core::Rect location;
    std::string cause;
    std::string recommendation;
    double confidence{1.0};
};

struct DiagnosticReport {
    std::string subject;
    std::vector<Diagnostic> findings;

    [[nodiscard]] bool has_errors() const noexcept;
};

}  // namespace mfe::diagnostics
