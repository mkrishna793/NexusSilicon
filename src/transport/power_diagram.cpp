#include "mfe/transport/power_diagram.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace mfe {

PowerDiagram::PowerDiagram(const HalfEdgeMesh& mesh, std::vector<Cell>& cells)
    : mesh_(mesh), cells_(cells) {
    vertex_assignments_.resize(mesh_.vertices.size(), -1);
}

void PowerDiagram::compute() {
    size_t num_vertices = mesh_.vertices.size();
    vertex_assignments_.assign(num_vertices, -1);

    // 1. Reset actual areas and centroid accumulators for each cell
    for (auto& c : cells_) {
        c.actual_area = 0.0;
        c.centroid_x = 0.0;
        c.centroid_y = 0.0;
    }

    // 2. Assign each vertex to the cell minimizing: d_g(v, c)^2 - w_c
    for (size_t i = 0; i < num_vertices; ++i) {
        const auto& v = mesh_.vertices[i];
        double best_val = std::numeric_limits<double>::max();
        int32_t best_cell_idx = -1;

        // Metric conformal scale factor
        double scale = std::exp(v.conformal_factor);

        for (size_t c_idx = 0; c_idx < cells_.size(); ++c_idx) {
            const auto& cell = cells_[c_idx];
            double dx = v.x - cell.x;
            double dy = v.y - cell.y;
            // Metric-scaled distance squared
            double d2 = scale * (dx * dx + dy * dy);
            double val = d2 - cell.weight;

            if (val < best_val) {
                best_val = val;
                best_cell_idx = static_cast<int32_t>(c_idx);
            }
        }
        vertex_assignments_[i] = best_cell_idx;
    }

    // 3. Compute vertex dual (barycentric) areas from face areas
    std::vector<double> vertex_areas(num_vertices, 0.0);
    for (const auto& f : mesh_.faces) {
        if (f.halfedge == -1) continue;
        double bary_area = f.area / 3.0;

        int32_t he0_idx = f.halfedge;
        int32_t he1_idx = mesh_.halfedges[he0_idx].next;
        int32_t he2_idx = mesh_.halfedges[he1_idx].next;

        vertex_areas[mesh_.halfedges[he0_idx].vertex] += bary_area;
        vertex_areas[mesh_.halfedges[he1_idx].vertex] += bary_area;
        vertex_areas[mesh_.halfedges[he2_idx].vertex] += bary_area;
    }

    // 4. Accumulate areas and centroids for each cell
    for (size_t i = 0; i < num_vertices; ++i) {
        int32_t c_idx = vertex_assignments_[i];
        if (c_idx == -1) continue;
        
        double area = vertex_areas[i];
        const auto& v = mesh_.vertices[i];

        cells_[c_idx].actual_area += area;
        cells_[c_idx].centroid_x += area * v.x;
        cells_[c_idx].centroid_y += area * v.y;
    }

    // 5. Finalize centroids
    for (auto& c : cells_) {
        if (c.actual_area > 1e-12) {
            c.centroid_x /= c.actual_area;
            c.centroid_y /= c.actual_area;
        } else {
            c.centroid_x = c.x;
            c.centroid_y = c.y;
        }
    }
}

} // namespace mfe
