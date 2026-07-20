#include "mfe/eco/eco_validator.hpp"
#include <cmath>
#include <iostream>

namespace mfe::eco {

bool EcoValidator::validate_physical_constraints(const std::vector<Cell>& cells, double die_width, double die_height) {
    // 1. Boundary check
    for (const auto& cell : cells) {
        if (cell.x < 0.0 || cell.x > die_width || cell.y < 0.0 || cell.y > die_height) {
            std::cerr << "[ECO Validator] Physical failure: Cell " << cell.id 
                      << " is outside die boundary (" << cell.x << ", " << cell.y << ")" << std::endl;
            return false;
        }
    }

    // 2. Overlap check (minimum distance threshold to represent overlaps)
    double min_distance = 0.5; // DBU/micron standard limit
    for (size_t i = 0; i < cells.size(); ++i) {
        for (size_t j = i + 1; j < cells.size(); ++j) {
            double dx = cells[i].x - cells[j].x;
            double dy = cells[i].y - cells[j].y;
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist < min_distance) {
                std::cerr << "[ECO Validator] Physical failure: Overlap between Cell " << cells[i].id 
                          << " and Cell " << cells[j].id << " (distance: " << dist << ")" << std::endl;
                return false;
            }
        }
    }

    return true;
}

bool EcoValidator::validate_timing_improvement(double old_wns, double new_wns) {
    if (new_wns < old_wns) {
        std::cerr << "[ECO Validator] Timing failure: Worst Negative Slack degraded from " 
                  << old_wns << " to " << new_wns << std::endl;
        return false;
    }
    return true;
}

bool EcoValidator::evaluate_eco(const std::vector<Cell>& cells, 
                                double die_width, double die_height,
                                double old_wns, double new_wns) {
    std::cout << "[ECO Validator] Evaluating design safety rules..." << std::endl;
    if (!validate_physical_constraints(cells, die_width, die_height)) {
        return false;
    }
    if (!validate_timing_improvement(old_wns, new_wns)) {
        return false;
    }
    std::cout << "[ECO Validator] Validation passed. ECO changes are safe to accept." << std::endl;
    return true;
}

} // namespace mfe::eco
