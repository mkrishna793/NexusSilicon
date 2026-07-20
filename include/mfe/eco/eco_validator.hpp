#pragma once

#include "mfe/flow/manifest.hpp"
#include "mfe/transport/power_diagram.hpp"
#include <vector>

namespace mfe::eco {

class EcoValidator {
public:
    EcoValidator() = default;

    // Checks standard cell placement coordinates for overlaps and out-of-boundary violations
    bool validate_physical_constraints(const std::vector<Cell>& cells, double die_width, double die_height);

    // Compares post-ECO timing metrics to verify setup slack improvement
    bool validate_timing_improvement(double old_wns, double new_wns);

    // Determines if the ECO proposal is valid and timing-safe
    bool evaluate_eco(const std::vector<Cell>& cells, 
                      double die_width, double die_height,
                      double old_wns, double new_wns);
};

} // namespace mfe::eco
