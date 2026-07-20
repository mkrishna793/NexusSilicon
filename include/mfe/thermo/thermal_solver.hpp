#pragma once

#include "mfe/core/half_edge.hpp"
#include "mfe/transport/power_diagram.hpp"
#include <Eigen/Core>
#include <Eigen/Sparse>
#include <vector>

namespace mfe {

class ThermalSolver {
public:
    explicit ThermalSolver(const HalfEdgeMesh& mesh);

    /**
     * @brief Build the thermal stiffness matrix L_K using cotangent weights and thermal conductivities.
     * @param face_conductivities Thermal conductivity values for each face. If empty, a default
     *                            value (e.g. Silicon = 150.0) is used for all faces.
     * @param L_K Output sparse matrix. Dirichlet boundary conditions are applied (boundary vertices are fixed).
     */
    void build_stiffness_matrix(const std::vector<double>& face_conductivities, Eigen::SparseMatrix<double>& L_K);

    /**
     * @brief Allocates cell powers to mesh vertices using barycentric coordinates of cell positions.
     * @param cells The list of cells.
     * @param cell_powers The power dissipation (in Watts) for each cell.
     * @param Q Output power density profile: Q_i = vertex_power_i / vertex_dual_area_i
     * @param vertex_powers Output total power allocated to each vertex.
     */
    void allocate_power_barycentric(const std::vector<Cell>& cells,
                                     const std::vector<double>& cell_powers,
                                     Eigen::VectorXd& Q,
                                     Eigen::VectorXd& vertex_powers) const;

    /**
     * @brief Solves L_K * T = P (where P is vertex powers) using Eigen's LDLT solver.
     * @param L_K The thermal stiffness matrix with Dirichlet boundary conditions.
     * @param vertex_powers The power source vector at vertices.
     * @param T Output temperature profile (absolute temperatures).
     * @param ambient_temp Ambient boundary temperature.
     */
    bool solve(const Eigen::SparseMatrix<double>& L_K,
               const Eigen::VectorXd& vertex_powers,
               Eigen::VectorXd& T,
               double ambient_temp = 300.0) const;

    /**
     * @brief Stretches the conformal factors of vertices proportionally to local temperatures: lambda_v += gamma * T_v.
     */
    static void apply_thermal_coupling(HalfEdgeMesh& mesh, const Eigen::VectorXd& T, double gamma);

private:
    const HalfEdgeMesh& mesh_;
};

} // namespace mfe
