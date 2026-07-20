#include "mfe/transport/ot_placer.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>

namespace mfe {

OTPlacer::OTPlacer(const HalfEdgeMesh& mesh, std::vector<Cell>& cells, const std::vector<Net>& nets)
    : mesh_(mesh), cells_(cells), nets_(nets) {}

bool OTPlacer::place(const Config& config) {
    PowerDiagram pd(mesh_, cells_);

    // Compute bounding box of the die (mesh)
    double min_x = std::numeric_limits<double>::max();
    double max_x = -std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_y = -std::numeric_limits<double>::max();
    for (const auto& v : mesh_.vertices) {
        min_x = std::min(min_x, v.x);
        max_x = std::max(max_x, v.x);
        min_y = std::min(min_y, v.y);
        max_y = std::max(max_y, v.y);
    }

    double max_mismatch = 0.0;
    for (uint32_t iter = 0; iter < config.max_iterations; ++iter) {
        // 1. Compute Power Diagram
        pd.compute();

        // 2. Compute area mismatch and update weights
        max_mismatch = 0.0;
        for (auto& c : cells_) {
            double mismatch = c.mass - c.actual_area;
            max_mismatch = std::max(max_mismatch, std::abs(mismatch));
            
            // Weight update: w_c += step_size * (mass - actual_area)
            c.weight += config.weight_step_size * mismatch;
        }

        if (max_mismatch < config.tolerance && iter > 5) {
            std::cout << "[OTPlacer] Converged at iteration " << iter 
                      << " with max mismatch = " << max_mismatch << std::endl;
            break;
        }

        // 3. Compute wirelength force gradients for each cell
        std::vector<double> fx(cells_.size(), 0.0);
        std::vector<double> fy(cells_.size(), 0.0);

        for (const auto& net : nets_) {
            for (size_t i = 0; i < net.cell_ids.size(); ++i) {
                uint32_t c1_id = net.cell_ids[i];
                for (size_t j = 0; j < net.cell_ids.size(); ++j) {
                    if (i == j) continue;
                    uint32_t c2_id = net.cell_ids[j];
                    
                    double dx = cells_[c1_id].x - cells_[c2_id].x;
                    double dy = cells_[c1_id].y - cells_[c2_id].y;
                    
                    // Force pulls c1 towards c2
                    fx[c1_id] -= 2.0 * config.wirelength_weight * dx;
                    fy[c1_id] -= 2.0 * config.wirelength_weight * dy;
                }
            }
        }

        // 4. Move cells to centroids shifted by wirelength forces
        for (auto& c : cells_) {
            c.x = c.centroid_x + config.position_step_size * fx[c.id];
            c.y = c.centroid_y + config.position_step_size * fy[c.id];

            // Safety: clamp positions inside the die boundary
            c.x = std::clamp(c.x, min_x, max_x);
            c.y = std::clamp(c.y, min_y, max_y);
        }
    }

    return max_mismatch < config.tolerance;
}

} // namespace mfe
