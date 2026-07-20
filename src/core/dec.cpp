#include "mfe/core/dec.hpp"
#include <numbers>
#include <cmath>
#include <algorithm>

namespace mfe {

DECOperators::DECOperators(const HalfEdgeMesh& mesh) : mesh_(mesh) {
    initial_edge_lengths_.resize(mesh_.edges.size(), 0.0);
    for (const auto& e : mesh_.edges) {
        if (e.halfedge == -1) continue;
        const auto& he = mesh_.halfedges[e.halfedge];
        double dx = mesh_.vertices[he.target_vertex].x - mesh_.vertices[he.vertex].x;
        double dy = mesh_.vertices[he.target_vertex].y - mesh_.vertices[he.vertex].y;
        initial_edge_lengths_[e.id] = std::sqrt(dx * dx + dy * dy);
    }
}

void DECOperators::compute_current_edge_lengths(std::vector<double>& edge_lengths) const {
    edge_lengths.resize(mesh_.edges.size(), 0.0);
    for (const auto& e : mesh_.edges) {
        if (e.halfedge == -1) continue;
        const auto& he = mesh_.halfedges[e.halfedge];
        double lambda_i = mesh_.vertices[he.vertex].conformal_factor;
        double lambda_j = mesh_.vertices[he.target_vertex].conformal_factor;
        edge_lengths[e.id] = initial_edge_lengths_[e.id] * std::exp((lambda_i + lambda_j) / 2.0);
    }
}

void DECOperators::build_laplacian(Eigen::SparseMatrix<double>& L, Eigen::VectorXd& M) {
    std::vector<double> edge_lengths;
    compute_current_edge_lengths(edge_lengths);

    std::vector<double> accum_cotan_weights(mesh_.edges.size(), 0.0);
    std::vector<double> vertex_areas(mesh_.vertices.size(), 0.0);

    // Loop over faces to compute cotan weights and areas
    for (size_t f_idx = 0; f_idx < mesh_.faces.size(); ++f_idx) {
        const auto& f = mesh_.faces[f_idx];
        if (f.halfedge == -1) continue;

        int32_t he0_idx = f.halfedge;
        int32_t he1_idx = mesh_.halfedges[he0_idx].next;
        int32_t he2_idx = mesh_.halfedges[he1_idx].next;

        const auto& he0 = mesh_.halfedges[he0_idx];
        const auto& he1 = mesh_.halfedges[he1_idx];
        const auto& he2 = mesh_.halfedges[he2_idx];

        uint32_t v0 = he0.vertex;
        uint32_t v1 = he1.vertex;
        uint32_t v2 = he2.vertex;

        int32_t e0 = he0.edge;
        int32_t e1 = he1.edge;
        int32_t e2 = he2.edge;

        // a is opposite to v0 (edge e1)
        // b is opposite to v1 (edge e2)
        // c is opposite to v2 (edge e0)
        double c = edge_lengths[e0];
        double a = edge_lengths[e1];
        double b = edge_lengths[e2];

        // Heron's formula for area
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

        // Accumulate half of the cotangent of the opposite angle to the edge
        accum_cotan_weights[e0] += 0.5 * cot_2; // opposite to v2
        accum_cotan_weights[e1] += 0.5 * cot_0; // opposite to v0
        accum_cotan_weights[e2] += 0.5 * cot_1; // opposite to v1

        // Barycentric area contribution (1/3 of face area to each vertex)
        double barycentric_area = area / 3.0;
        vertex_areas[v0] += barycentric_area;
        vertex_areas[v1] += barycentric_area;
        vertex_areas[v2] += barycentric_area;

        // Store area in face
        mesh_.faces[f_idx].area = area;
    }

    // Update edge weights in the mesh
    for (size_t e_idx = 0; e_idx < mesh_.edges.size(); ++e_idx) {
        mesh_.edges[e_idx].cotan_weight = accum_cotan_weights[e_idx];
    }

    // Build the sparse Laplacian L
    size_t n = mesh_.vertices.size();
    L.resize(n, n);
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(n + mesh_.edges.size() * 2);

    std::vector<double> diag_vals(n, 0.0);

    for (size_t e_idx = 0; e_idx < mesh_.edges.size(); ++e_idx) {
        const auto& e = mesh_.edges[e_idx];
        if (e.halfedge == -1) continue;
        const auto& he = mesh_.halfedges[e.halfedge];
        uint32_t i = he.vertex;
        uint32_t j = he.target_vertex;
        double w = e.cotan_weight;

        triplets.push_back(Eigen::Triplet<double>(i, j, -w));
        triplets.push_back(Eigen::Triplet<double>(j, i, -w));
        diag_vals[i] += w;
        diag_vals[j] += w;
    }

    for (size_t i = 0; i < n; ++i) {
        triplets.push_back(Eigen::Triplet<double>(i, i, diag_vals[i]));
    }

    L.setFromTriplets(triplets.begin(), triplets.end());

    // Build mass vector M
    M.resize(n);
    for (size_t i = 0; i < n; ++i) {
        M(i) = vertex_areas[i];
    }
}

void DECOperators::compute_curvature(Eigen::VectorXd& K) {
    std::vector<double> edge_lengths;
    compute_current_edge_lengths(edge_lengths);

    size_t n = mesh_.vertices.size();
    std::vector<double> angles_sum(n, 0.0);

    for (size_t f_idx = 0; f_idx < mesh_.faces.size(); ++f_idx) {
        const auto& f = mesh_.faces[f_idx];
        if (f.halfedge == -1) continue;

        int32_t he0_idx = f.halfedge;
        int32_t he1_idx = mesh_.halfedges[he0_idx].next;
        int32_t he2_idx = mesh_.halfedges[he1_idx].next;

        const auto& he0 = mesh_.halfedges[he0_idx];
        const auto& he1 = mesh_.halfedges[he1_idx];
        const auto& he2 = mesh_.halfedges[he2_idx];

        uint32_t v0 = he0.vertex;
        uint32_t v1 = he1.vertex;
        uint32_t v2 = he2.vertex;

        int32_t e0 = he0.edge;
        int32_t e1 = he1.edge;
        int32_t e2 = he2.edge;

        double c = edge_lengths[e0];
        double a = edge_lengths[e1];
        double b = edge_lengths[e2];

        double cos_0 = 0.0;
        double cos_1 = 0.0;
        double cos_2 = 0.0;

        double s = 0.5 * (a + b + c);
        double area_sq = s * (s - a) * (s - b) * (s - c);
        double area = std::sqrt(std::max(0.0, area_sq));

        if (area > 1e-12 && a > 1e-12 && b > 1e-12 && c > 1e-12) {
            cos_0 = (b * b + c * c - a * a) / (2.0 * b * c);
            cos_1 = (a * a + c * c - b * b) / (2.0 * a * c);
            cos_2 = (a * a + b * b - c * c) / (2.0 * a * b);

            cos_0 = std::clamp(cos_0, -1.0, 1.0);
            cos_1 = std::clamp(cos_1, -1.0, 1.0);
            cos_2 = std::clamp(cos_2, -1.0, 1.0);

            angles_sum[v0] += std::acos(cos_0);
            angles_sum[v1] += std::acos(cos_1);
            angles_sum[v2] += std::acos(cos_2);
        } else {
            // Degenerate triangle fallback: equal angles (pi/3)
            angles_sum[v0] += std::numbers::pi / 3.0;
            angles_sum[v1] += std::numbers::pi / 3.0;
            angles_sum[v2] += std::numbers::pi / 3.0;
        }
    }

    K.resize(n);
    for (size_t i = 0; i < n; ++i) {
        K(i) = 2.0 * std::numbers::pi - angles_sum[i];
        mesh_.vertices[i].curvature = K(i);
    }
}

} // namespace mfe
