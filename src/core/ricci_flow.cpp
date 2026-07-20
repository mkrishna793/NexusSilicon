#include "mfe/core/ricci_flow.hpp"
#include <Eigen/SparseCholesky>
#include <Eigen/IterativeLinearSolvers>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace mfe {

RicciFlow::RicciFlow(HalfEdgeMesh& mesh, DECOperators& dec)
    : mesh_(mesh), dec_(dec) {
    size_t n = mesh_.vertices.size();
    K_.setZero(n);
    target_K_.setZero(n);
}

void RicciFlow::set_target_curvature(const Eigen::VectorXd& target_K) {
    target_K_ = target_K;
}

double RicciFlow::step_gd(double dt) {
    // 1. Compute current curvature
    dec_.compute_curvature(K_);

    // 2. Compute residual
    Eigen::VectorXd residual = K_ - target_K_;

    // 3. Update conformal factors: lambda_i -= dt * residual_i
    for (size_t i = 0; i < mesh_.vertices.size(); ++i) {
        mesh_.vertices[i].conformal_factor -= dt * residual(i);
    }

    return residual.lpNorm<Eigen::Infinity>();
}

double RicciFlow::step_newton(double damping) {
    // 1. Compute current curvature
    dec_.compute_curvature(K_);

    // 2. Compute residual
    Eigen::VectorXd residual = K_ - target_K_;

    // 3. Build current cotangent Laplacian L
    Eigen::SparseMatrix<double> L;
    Eigen::VectorXd M;
    dec_.build_laplacian(L, M);

    // 4. Solve L * delta_lambda = -residual = target_K - K
    // Since L has a 1D null space of constant vectors, we add a tiny regularization
    // to the diagonal to make it positive definite and uniquely solvable.
    Eigen::SparseMatrix<double> A = L;
    for (int i = 0; i < A.rows(); ++i) {
        A.coeffRef(i, i) += 1e-8;
    }

    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
    solver.compute(A);
    Eigen::VectorXd delta_lambda = solver.solve(target_K_ - K_);

    if (solver.info() != Eigen::Success) {
        // Fallback to Conjugate Gradient if LDLT fails
        Eigen::ConjugateGradient<Eigen::SparseMatrix<double>, Eigen::Lower|Eigen::Upper> cg;
        cg.compute(A);
        delta_lambda = cg.solve(target_K_ - K_);
    }

    // 5. Update conformal factors
    for (size_t i = 0; i < mesh_.vertices.size(); ++i) {
        mesh_.vertices[i].conformal_factor += damping * delta_lambda(i);
    }

    return residual.lpNorm<Eigen::Infinity>();
}

bool RicciFlow::solve(const Config& config) {
    double res = 0.0;
    
    if (config.use_newton) {
        for (uint32_t iter = 0; iter < config.newton_max_steps; ++iter) {
            res = step_newton(config.newton_damping);
            if (res < config.tolerance) {
                return true;
            }
        }
    }

    // Fallback/continue with Gradient Descent if not converged
    for (uint32_t iter = 0; iter < config.max_iterations; ++iter) {
        res = step_gd(config.dt);
        if (res < config.tolerance) {
            return true;
        }
    }

    return res < config.tolerance;
}

} // namespace mfe
