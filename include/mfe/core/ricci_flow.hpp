#pragma once

#include "mfe/core/half_edge.hpp"
#include "mfe/core/dec.hpp"
#include <Eigen/Core>
#include <vector>

namespace mfe {

class RicciFlow {
public:
    struct Config {
        double dt = 0.05;                // Time step for gradient descent
        double tolerance = 1e-5;         // Convergence tolerance on max |K_i - target_K_i|
        uint32_t max_iterations = 100;   // Maximum total iterations
        bool use_newton = true;          // Use Newton method (L * d_lambda = target_K - K)
        uint32_t newton_max_steps = 15;  // Max steps if using Newton
        double newton_damping = 0.8;     // Damping factor for Newton step to ensure stability
    };

    RicciFlow(HalfEdgeMesh& mesh, DECOperators& dec);

    // Set target curvatures (default is 0.0 everywhere, i.e., flat metric)
    void set_target_curvature(const Eigen::VectorXd& target_K);

    // Run the Ricci Flow solver to convergence
    bool solve(const Config& config);

    // Single step of gradient descent Ricci flow: lambda_i -= dt * (K_i - target_K_i)
    double step_gd(double dt);

    // Single step of Newton-Ricci flow: L * delta_lambda = target_K - K
    double step_newton(double damping);

    const Eigen::VectorXd& get_curvature() const { return K_; }
    const Eigen::VectorXd& get_target_curvature() const { return target_K_; }

private:
    HalfEdgeMesh& mesh_;
    DECOperators& dec_;
    Eigen::VectorXd K_;
    Eigen::VectorXd target_K_;
};

} // namespace mfe
