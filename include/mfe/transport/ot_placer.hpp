#pragma once

#include "mfe/core/half_edge.hpp"
#include "mfe/transport/power_diagram.hpp"
#include <vector>
#include <cstdint>

namespace mfe {

struct Net {
    uint32_t id;
    std::vector<uint32_t> cell_ids; // IDs of cells connected by this net
};

class OTPlacer {
public:
    struct Config {
        uint32_t max_iterations = 100;
        double tolerance = 1e-4;         // Converge when max area mismatch is small
        double wirelength_weight = 0.05; // Weight of connectivity pull
        double weight_step_size = 0.1;   // Step size for power weights updates
        double position_step_size = 0.5; // Step size for wirelength gradient steps
    };

    OTPlacer(const HalfEdgeMesh& mesh, std::vector<Cell>& cells, const std::vector<Net>& nets);

    // Runs connectivity-aware OT placement
    bool place(const Config& config);

private:
    const HalfEdgeMesh& mesh_;
    std::vector<Cell>& cells_;
    const std::vector<Net>& nets_;
};

} // namespace mfe
