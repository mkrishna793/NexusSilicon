#pragma once

#include "mfe/core/mesh.hpp"
#include "mfe/diagnostics/diagnostic.hpp"

namespace mfe::diagnostics {

struct CongestionPolicy {
    double warning_utilization{0.80};
    double error_utilization{1.00};
};

[[nodiscard]] DiagnosticReport analyze_congestion(const core::UniformMesh& mesh,
                                                   CongestionPolicy policy = {});

}  // namespace mfe::diagnostics
