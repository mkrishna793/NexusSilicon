#pragma once

#include "mfe/core/half_edge.hpp"
#include "mfe/transport/power_diagram.hpp"
#include <vector>
#include <utility>

namespace mfe {

struct Site {
    double x;
    double y;
    bool occupied = false;
};

class Legalizer {
public:
    explicit Legalizer(const HalfEdgeMesh& mesh);

    // Legalizes the cells by greedily placing them onto the nearest unoccupied site positions
    // using the metric-weighted distance.
    void legalize(std::vector<Cell>& cells, const std::vector<std::pair<double, double>>& site_positions);

private:
    const HalfEdgeMesh& mesh_;

    // Helper to find the nearest vertex index in the mesh to a given point
    int32_t find_nearest_vertex(double x, double y) const;
};

} // namespace mfe
