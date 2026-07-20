#include "mfe/diagnostics/congestion.hpp"

#include <sstream>

namespace mfe::diagnostics {

bool DiagnosticReport::has_errors() const noexcept {
    for (const auto& finding : findings) {
        if (finding.severity == Severity::error || finding.severity == Severity::critical) return true;
    }
    return false;
}

DiagnosticReport analyze_congestion(const core::UniformMesh& mesh, CongestionPolicy policy) {
    DiagnosticReport report{"routing demand versus available capacity", {}};
    if (policy.warning_utilization < 0.0 || policy.error_utilization < policy.warning_utilization) {
        report.findings.push_back({"MFE-POLICY-001", Severity::error, {}, "Invalid congestion thresholds.",
                                   "Set 0 <= warning utilization <= error utilization.", 1.0});
        return report;
    }
    for (std::size_t row = 0; row < mesh.rows(); ++row) {
        for (std::size_t column = 0; column < mesh.columns(); ++column) {
            const auto& cell = mesh.cell(column, row);
            const double utilization = cell.capacity > 0.0 ? cell.demand / cell.capacity :
                                       (cell.demand > 0.0 ? 1.0e9 : 0.0);
            if (utilization < policy.warning_utilization) continue;
            const auto severity = utilization >= policy.error_utilization ? Severity::error : Severity::warning;
            std::ostringstream cause;
            cause << "Estimated routing utilization is " << utilization * 100.0 << "% in mesh cell ("
                  << column << ", " << row << ").";
            report.findings.push_back({"MFE-CONG-001", severity, cell.bounds, cause.str(),
                                       "Increase local routing supply, spread cells, or revise placement constraints.", 0.95});
        }
    }
    return report;
}

}  // namespace mfe::diagnostics
