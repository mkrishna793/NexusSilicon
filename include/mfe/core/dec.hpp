#pragma once

#include "mfe/core/half_edge.hpp"
#include <Eigen/Sparse>
#include <Eigen/Core>
#include <vector>

namespace mfe {

class DECOperators {
public:
    explicit DECOperators(const HalfEdgeMesh& mesh);

    // Compute current edge lengths using conformal factors: l_ij = l0_ij * exp((lambda_i + lambda_j)/2)
    void compute_current_edge_lengths(std::vector<double>& edge_lengths) const;

    // Compute cotangent weights, update edge cotan_weights and face areas,
    // and build the sparse Laplace-Beltrami matrix L and diagonal mass vector M
    void build_laplacian(Eigen::SparseMatrix<double>& L, Eigen::VectorXd& M);

    // Compute discrete curvature (angle defect) K_i = 2*pi - sum(angles around i)
    void compute_curvature(Eigen::VectorXd& K);

private:
    const HalfEdgeMesh& mesh_;
    std::vector<double> initial_edge_lengths_;
};

} // namespace mfe
