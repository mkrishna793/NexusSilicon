#pragma once

#include "mfe/core/half_edge.hpp"
#include <Eigen/Core>
#include <vector>

namespace mfe {

class FMMRouter {
public:
    explicit FMMRouter(const HalfEdgeMesh& mesh);

    /**
     * @brief Compute the geodesic distance field from a source vertex using Anisotropic FMM.
     * @param source_vertex Index of the starting vertex.
     * @param distance Output vector of distance values for all vertices.
     * @return True if successful.
     */
    bool compute_distance_field(uint32_t source_vertex, Eigen::VectorXd& distance) const;

    /**
     * @brief Reconstruct the geodesic path from a sink vertex back to the source vertex.
     * @param sink_vertex Index of the destination vertex.
     * @param distance The precomputed distance field from the source vertex.
     * @param path Output sequence of vertex coordinates representing the path.
     * @return True if successful.
     */
    bool backtrack_path(uint32_t sink_vertex, const Eigen::VectorXd& distance, std::vector<std::pair<double, double>>& path) const;

private:
    const HalfEdgeMesh& mesh_;
};

} // namespace mfe
