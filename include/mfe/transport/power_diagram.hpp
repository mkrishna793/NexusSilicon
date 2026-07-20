#pragma once

#include "mfe/core/half_edge.hpp"
#include <vector>
#include <cstdint>

namespace mfe {

struct Cell {
    uint32_t id;
    double x = 0.0;
    double y = 0.0;
    double mass = 0.0;     // Desired target area (mass)
    double weight = 0.0;   // Power diagram weight w_c
    double actual_area = 0.0;
    double centroid_x = 0.0;
    double centroid_y = 0.0;
};

class PowerDiagram {
public:
    PowerDiagram(const HalfEdgeMesh& mesh, std::vector<Cell>& cells);

    // Computes the power diagram: assigns each mesh vertex to the cell that
    // minimizes d_g(v, c)^2 - w_c. Then integrates actual areas and centroids.
    void compute();

    // Gets the vertex assignment map: vertex_assignments[v_idx] = cell_idx
    const std::vector<int32_t>& get_vertex_assignments() const { return vertex_assignments_; }

private:
    const HalfEdgeMesh& mesh_;
    std::vector<Cell>& cells_;
    std::vector<int32_t> vertex_assignments_; // Size matches mesh.vertices.size()
};

} // namespace mfe
