#include "mfe/thermo/thermal_solver.hpp"
#include <Eigen/SparseCholesky>
#include <cmath>
#include <algorithm>
#include <limits>

namespace mfe {

ThermalSolver::ThermalSolver(const HalfEdgeMesh& mesh) : mesh_(mesh) {}

static std::vector<bool> identify_mesh_boundary_vertices(const HalfEdgeMesh& mesh) {
    std::vector<bool> is_boundary_vert(mesh.vertices.size(), false);
    for (const auto& he : mesh.halfedges) {
        if (he.opposite == -1) {
            if (he.vertex != -1) is_boundary_vert[he.vertex] = true;
            if (he.target_vertex != -1) is_boundary_vert[he.target_vertex] = true;
        }
    }
    return is_boundary_vert;
}

void ThermalSolver::build_stiffness_matrix(const std::vector<double>& face_conductivities, Eigen::SparseMatrix<double>& L_K) {
    std::vector<double> initial_edge_lengths(mesh_.edges.size(), 0.0);
    for (const auto& e : mesh_.edges) {
        if (e.halfedge == -1) continue;
        const auto& he = mesh_.halfedges[e.halfedge];
        double dx = mesh_.vertices[he.target_vertex].x - mesh_.vertices[he.vertex].x;
        double dy = mesh_.vertices[he.target_vertex].y - mesh_.vertices[he.vertex].y;
        initial_edge_lengths[e.id] = std::sqrt(dx * dx + dy * dy);
    }

    std::vector<double> edge_lengths(mesh_.edges.size(), 0.0);
    for (const auto& e : mesh_.edges) {
        if (e.halfedge == -1) continue;
        const auto& he = mesh_.halfedges[e.halfedge];
        double lambda_i = mesh_.vertices[he.vertex].conformal_factor;
        double lambda_j = mesh_.vertices[he.target_vertex].conformal_factor;
        edge_lengths[e.id] = initial_edge_lengths[e.id] * std::exp((lambda_i + lambda_j) / 2.0);
    }

    std::vector<double> accum_thermal_weights(mesh_.edges.size(), 0.0);

    for (size_t f_idx = 0; f_idx < mesh_.faces.size(); ++f_idx) {
        const auto& f = mesh_.faces[f_idx];
        if (f.halfedge == -1) continue;

        double k_f = 150.0; // Default: Silicon conductivity
        if (f_idx < face_conductivities.size()) {
            k_f = face_conductivities[f_idx];
        }

        int32_t he0_idx = f.halfedge;
        int32_t he1_idx = mesh_.halfedges[he0_idx].next;
        int32_t he2_idx = mesh_.halfedges[he1_idx].next;

        const auto& he0 = mesh_.halfedges[he0_idx];
        const auto& he1 = mesh_.halfedges[he1_idx];
        const auto& he2 = mesh_.halfedges[he2_idx];

        int32_t e0 = he0.edge;
        int32_t e1 = he1.edge;
        int32_t e2 = he2.edge;

        double c = edge_lengths[e0];
        double a = edge_lengths[e1];
        double b = edge_lengths[e2];

        double s = 0.5 * (a + b + c);
        double area_sq = s * (s - a) * (s - b) * (s - c);
        double area = std::sqrt(std::max(0.0, area_sq));

        double cot_0 = 0.0;
        double cot_1 = 0.0;
        double cot_2 = 0.0;

        if (area > 1e-12) {
            cot_0 = (b * b + c * c - a * a) / (4.0 * area);
            cot_1 = (a * a + c * c - b * b) / (4.0 * area);
            cot_2 = (a * a + b * b - c * c) / (4.0 * area);
        }

        accum_thermal_weights[e0] += 0.5 * k_f * cot_2;
        accum_thermal_weights[e1] += 0.5 * k_f * cot_0;
        accum_thermal_weights[e2] += 0.5 * k_f * cot_1;
    }

    std::vector<bool> is_boundary_vert = identify_mesh_boundary_vertices(mesh_);
    size_t n = mesh_.vertices.size();
    L_K.resize(n, n);
    std::vector<Eigen::Triplet<double>> triplets;

    std::vector<double> diag_vals(n, 0.0);
    for (size_t e_idx = 0; e_idx < mesh_.edges.size(); ++e_idx) {
        const auto& e = mesh_.edges[e_idx];
        if (e.halfedge == -1) continue;
        const auto& he = mesh_.halfedges[e.halfedge];
        uint32_t i = he.vertex;
        uint32_t j = he.target_vertex;
        double w = accum_thermal_weights[e_idx];

        diag_vals[i] += w;
        diag_vals[j] += w;

        if (!is_boundary_vert[i] && !is_boundary_vert[j]) {
            triplets.push_back(Eigen::Triplet<double>(i, j, -w));
            triplets.push_back(Eigen::Triplet<double>(j, i, -w));
        }
    }

    for (size_t i = 0; i < n; ++i) {
        if (is_boundary_vert[i]) {
            triplets.push_back(Eigen::Triplet<double>(i, i, 1.0));
        } else {
            triplets.push_back(Eigen::Triplet<double>(i, i, diag_vals[i]));
        }
    }

    L_K.setFromTriplets(triplets.begin(), triplets.end());
}

void ThermalSolver::allocate_power_barycentric(const std::vector<Cell>& cells,
                                                 const std::vector<double>& cell_powers,
                                                 Eigen::VectorXd& Q,
                                                 Eigen::VectorXd& vertex_powers) const {
    size_t num_vertices = mesh_.vertices.size();
    vertex_powers.setZero(num_vertices);

    for (size_t c_idx = 0; c_idx < cells.size(); ++c_idx) {
        const auto& cell = cells[c_idx];
        double p_c = (c_idx < cell_powers.size()) ? cell_powers[c_idx] : 0.0;

        bool found = false;
        for (const auto& f : mesh_.faces) {
            if (f.halfedge == -1) continue;
            int32_t he0_idx = f.halfedge;
            int32_t he1_idx = mesh_.halfedges[he0_idx].next;
            int32_t he2_idx = mesh_.halfedges[he1_idx].next;

            uint32_t v0 = mesh_.halfedges[he0_idx].vertex;
            uint32_t v1 = mesh_.halfedges[he1_idx].vertex;
            uint32_t v2 = mesh_.halfedges[he2_idx].vertex;

            double x0 = mesh_.vertices[v0].x; double y0 = mesh_.vertices[v0].y;
            double x1 = mesh_.vertices[v1].x; double y1 = mesh_.vertices[v1].y;
            double x2 = mesh_.vertices[v2].x; double y2 = mesh_.vertices[v2].y;

            double l0, l1, l2;
            double denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
            if (std::abs(denom) < 1e-12) continue;
            l0 = ((y1 - y2) * (cell.x - x2) + (x2 - x1) * (cell.y - y2)) / denom;
            l1 = ((y2 - y0) * (cell.x - x2) + (x0 - x2) * (cell.y - y2)) / denom;
            l2 = 1.0 - l0 - l1;

            if (l0 >= -1e-9 && l1 >= -1e-9 && l2 >= -1e-9) {
                l0 = std::clamp(l0, 0.0, 1.0);
                l1 = std::clamp(l1, 0.0, 1.0);
                l2 = std::clamp(l2, 0.0, 1.0);
                double sum_l = l0 + l1 + l2;
                if (sum_l > 0.0) {
                    l0 /= sum_l; l1 /= sum_l; l2 /= sum_l;
                }

                vertex_powers(v0) += l0 * p_c;
                vertex_powers(v1) += l1 * p_c;
                vertex_powers(v2) += l2 * p_c;
                found = true;
                break;
            }
        }

        if (!found) {
            double min_d2 = std::numeric_limits<double>::max();
            int32_t nearest_v = -1;
            for (size_t i = 0; i < num_vertices; ++i) {
                double dx = mesh_.vertices[i].x - cell.x;
                double dy = mesh_.vertices[i].y - cell.y;
                double d2 = dx * dx + dy * dy;
                if (d2 < min_d2) {
                    min_d2 = d2;
                    nearest_v = static_cast<int32_t>(i);
                }
            }
            if (nearest_v != -1) {
                vertex_powers(nearest_v) += p_c;
            }
        }
    }

    std::vector<double> vertex_areas(num_vertices, 0.0);
    std::vector<double> initial_edge_lengths(mesh_.edges.size(), 0.0);
    for (const auto& e : mesh_.edges) {
        if (e.halfedge == -1) continue;
        const auto& he = mesh_.halfedges[e.halfedge];
        double dx = mesh_.vertices[he.target_vertex].x - mesh_.vertices[he.vertex].x;
        double dy = mesh_.vertices[he.target_vertex].y - mesh_.vertices[he.vertex].y;
        initial_edge_lengths[e.id] = std::sqrt(dx * dx + dy * dy);
    }
    std::vector<double> edge_lengths(mesh_.edges.size(), 0.0);
    for (const auto& e : mesh_.edges) {
        if (e.halfedge == -1) continue;
        const auto& he = mesh_.halfedges[e.halfedge];
        double lambda_i = mesh_.vertices[he.vertex].conformal_factor;
        double lambda_j = mesh_.vertices[he.target_vertex].conformal_factor;
        edge_lengths[e.id] = initial_edge_lengths[e.id] * std::exp((lambda_i + lambda_j) / 2.0);
    }

    for (size_t f_idx = 0; f_idx < mesh_.faces.size(); ++f_idx) {
        const auto& f = mesh_.faces[f_idx];
        if (f.halfedge == -1) continue;

        int32_t he0_idx = f.halfedge;
        int32_t he1_idx = mesh_.halfedges[he0_idx].next;
        int32_t he2_idx = mesh_.halfedges[he1_idx].next;

        uint32_t v0 = mesh_.halfedges[he0_idx].vertex;
        uint32_t v1 = mesh_.halfedges[he1_idx].vertex;
        uint32_t v2 = mesh_.halfedges[he2_idx].vertex;

        int32_t e0 = mesh_.halfedges[he0_idx].edge;
        int32_t e1 = mesh_.halfedges[he1_idx].edge;
        int32_t e2 = mesh_.halfedges[he2_idx].edge;

        double c = edge_lengths[e0];
        double a = edge_lengths[e1];
        double b = edge_lengths[e2];

        double s = 0.5 * (a + b + c);
        double area_sq = s * (s - a) * (s - b) * (s - c);
        double area = std::sqrt(std::max(0.0, area_sq));

        double barycentric_area = area / 3.0;
        vertex_areas[v0] += barycentric_area;
        vertex_areas[v1] += barycentric_area;
        vertex_areas[v2] += barycentric_area;
    }

    Q.resize(num_vertices);
    for (size_t i = 0; i < num_vertices; ++i) {
        if (vertex_areas[i] > 1e-12) {
            Q(i) = vertex_powers(i) / vertex_areas[i];
        } else {
            Q(i) = 0.0;
        }
    }
}

bool ThermalSolver::solve(const Eigen::SparseMatrix<double>& L_K,
                          const Eigen::VectorXd& vertex_powers,
                          Eigen::VectorXd& T,
                          double ambient_temp) const {
    size_t n = mesh_.vertices.size();
    Eigen::VectorXd rhs(n);
    std::vector<bool> is_boundary_vert = identify_mesh_boundary_vertices(mesh_);
    for (size_t i = 0; i < n; ++i) {
        if (is_boundary_vert[i]) {
            rhs(i) = 0.0;
        } else {
            rhs(i) = vertex_powers(i);
        }
    }

    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
    solver.compute(L_K);
    if (solver.info() != Eigen::Success) {
        return false;
    }

    Eigen::VectorXd theta = solver.solve(rhs);
    if (solver.info() != Eigen::Success) {
        return false;
    }

    T.resize(n);
    for (size_t i = 0; i < n; ++i) {
        if (is_boundary_vert[i]) {
            T(i) = ambient_temp;
        } else {
            T(i) = theta(i) + ambient_temp;
        }
    }

    return true;
}

void ThermalSolver::apply_thermal_coupling(HalfEdgeMesh& mesh, const Eigen::VectorXd& T, double gamma) {
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        if (i < static_cast<size_t>(T.size())) {
            mesh.vertices[i].conformal_factor += gamma * T(i);
        }
    }
}

} // namespace mfe
