#include "mfe/geodesic/fmm_router.hpp"
#include <queue>
#include <limits>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace mfe {

FMMRouter::FMMRouter(const HalfEdgeMesh& mesh) : mesh_(mesh) {}

static std::vector<std::vector<uint32_t>> build_vertex_faces(const HalfEdgeMesh& mesh) {
    std::vector<std::vector<uint32_t>> vertex_faces(mesh.vertices.size());
    for (const auto& f : mesh.faces) {
        if (f.halfedge == -1) continue;
        int32_t he0 = f.halfedge;
        int32_t he1 = mesh.halfedges[he0].next;
        int32_t he2 = mesh.halfedges[he1].next;
        vertex_faces[mesh.halfedges[he0].vertex].push_back(f.id);
        vertex_faces[mesh.halfedges[he1].vertex].push_back(f.id);
        vertex_faces[mesh.halfedges[he2].vertex].push_back(f.id);
    }
    return vertex_faces;
}

static std::vector<std::vector<std::pair<uint32_t, double>>> build_vertex_neighbors(const HalfEdgeMesh& mesh, const std::vector<double>& edge_lengths) {
    std::vector<std::vector<std::pair<uint32_t, double>>> neighbors(mesh.vertices.size());
    for (const auto& he : mesh.halfedges) {
        if (he.vertex != -1 && he.target_vertex != -1) {
            neighbors[he.vertex].push_back({he.target_vertex, edge_lengths[he.edge]});
        }
    }
    return neighbors;
}

static double solve_eikonal_triangle(double u_A, double u_B, double a, double b, double c, double cos_theta, double sin_theta) {
    if (u_A >= 1e20 || u_B >= 1e20) {
        return std::numeric_limits<double>::max();
    }

    double b2 = b * b;
    double a2 = a * a;
    double ab = a * b;

    double A_q = 1.0 / b2 + 1.0 / a2 - 2.0 * cos_theta / ab;
    double B_q = -2.0 * (u_A / b2 + u_B / a2 - cos_theta * (u_A + u_B) / ab);
    double C_q = u_A * u_A / b2 + u_B * u_B / a2 - 2.0 * cos_theta * u_A * u_B / ab - 1.0;

    double disc = B_q * B_q - 4.0 * A_q * C_q;
    if (disc < 0.0) {
        return std::numeric_limits<double>::max();
    }

    double u = (-B_q + std::sqrt(disc)) / (2.0 * A_q);

    double val1 = b * (u - u_B);
    double val2 = a * (u - u_A);

    if (val1 > val2 * cos_theta && val2 > val1 * cos_theta && u > std::max(u_A, u_B)) {
        return u;
    }

    return std::numeric_limits<double>::max();
}

bool FMMRouter::compute_distance_field(uint32_t source_vertex, Eigen::VectorXd& distance) const {
    size_t n = mesh_.vertices.size();
    distance.setConstant(n, std::numeric_limits<double>::max());
    distance(source_vertex) = 0.0;

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

    std::vector<std::vector<uint32_t>> vertex_faces = build_vertex_faces(mesh_);
    std::vector<std::vector<std::pair<uint32_t, double>>> neighbors = build_vertex_neighbors(mesh_, edge_lengths);

    // States: 0 = Far, 1 = Trial, 2 = Accepted
    std::vector<uint8_t> state(n, 0);

    using Element = std::pair<double, uint32_t>;
    std::priority_queue<Element, std::vector<Element>, std::greater<Element>> pq;

    pq.push({0.0, source_vertex});
    state[source_vertex] = 1;

    while (!pq.empty()) {
        auto [dist_v, v] = pq.top();
        pq.pop();

        if (state[v] == 2) continue;
        state[v] = 2;
        distance(v) = dist_v;

        for (const auto& nb : neighbors[v]) {
            uint32_t u = nb.first;
            if (state[u] == 2) continue;

            double u_new = std::numeric_limits<double>::max();

            // 1D updates
            for (const auto& unb : neighbors[u]) {
                uint32_t w = unb.first;
                double edge_len = unb.second;
                if (state[w] == 2) {
                    u_new = std::min(u_new, distance(w) + edge_len);
                }
            }

            // 2D updates
            for (uint32_t f_idx : vertex_faces[u]) {
                const auto& f = mesh_.faces[f_idx];
                int32_t he0_idx = f.halfedge;
                int32_t he1_idx = mesh_.halfedges[he0_idx].next;
                int32_t he2_idx = mesh_.halfedges[he1_idx].next;

                uint32_t v0 = mesh_.halfedges[he0_idx].vertex;
                uint32_t v1 = mesh_.halfedges[he1_idx].vertex;
                uint32_t v2 = mesh_.halfedges[he2_idx].vertex;

                uint32_t A = 0, B = 0;
                int32_t e_opposite_A = -1, e_opposite_B = -1, e_opposite_u = -1;

                if (v0 == u) {
                    A = v1; B = v2;
                    e_opposite_A = mesh_.halfedges[he1_idx].edge;
                    e_opposite_B = mesh_.halfedges[he2_idx].edge;
                    e_opposite_u = mesh_.halfedges[he0_idx].edge;
                } else if (v1 == u) {
                    A = v0; B = v2;
                    e_opposite_A = mesh_.halfedges[he0_idx].edge;
                    e_opposite_B = mesh_.halfedges[he2_idx].edge;
                    e_opposite_u = mesh_.halfedges[he1_idx].edge;
                } else if (v2 == u) {
                    A = v0; B = v1;
                    e_opposite_A = mesh_.halfedges[he0_idx].edge;
                    e_opposite_B = mesh_.halfedges[he1_idx].edge;
                    e_opposite_u = mesh_.halfedges[he2_idx].edge;
                } else {
                    continue;
                }

                if (distance(A) < 1e20 && distance(B) < 1e20) {
                    double a_side = edge_lengths[e_opposite_A];
                    double b_side = edge_lengths[e_opposite_B];
                    double c_side = edge_lengths[e_opposite_u];

                    double cos_theta = (a_side * a_side + b_side * b_side - c_side * c_side) / (2.0 * a_side * b_side);
                    cos_theta = std::clamp(cos_theta, -1.0, 1.0);
                    double sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));

                    double u_val = solve_eikonal_triangle(distance(A), distance(B), a_side, b_side, c_side, cos_theta, sin_theta);
                    if (u_val < u_new) {
                        u_new = u_val;
                    }
                }
            }

            if (u_new < distance(u)) {
                distance(u) = u_new;
                state[u] = 1;
                pq.push({u_new, u});
            }
        }
    }

    return true;
}

bool FMMRouter::backtrack_path(uint32_t sink_vertex, const Eigen::VectorXd& distance, std::vector<std::pair<double, double>>& path) const {
    path.clear();
    uint32_t current = sink_vertex;

    std::vector<bool> visited(mesh_.vertices.size(), false);

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
    std::vector<std::vector<std::pair<uint32_t, double>>> neighbors = build_vertex_neighbors(mesh_, edge_lengths);

    while (true) {
        path.push_back({mesh_.vertices[current].x, mesh_.vertices[current].y});
        visited[current] = true;

        if (distance(current) < 1e-12) {
            break;
        }

        double min_dist = distance(current);
        int32_t next_v = -1;

        for (const auto& nb : neighbors[current]) {
            uint32_t u = nb.first;
            if (!visited[u] && distance(u) < min_dist) {
                min_dist = distance(u);
                next_v = u;
            }
        }

        if (next_v == -1) {
            double abs_min_dist = distance(current);
            for (const auto& nb : neighbors[current]) {
                uint32_t u = nb.first;
                if (distance(u) < abs_min_dist) {
                    abs_min_dist = distance(u);
                    next_v = u;
                }
            }
            if (next_v == -1 || distance(next_v) >= distance(current)) {
                break;
            }
        }

        current = next_v;
    }

    std::reverse(path.begin(), path.end());
    return !path.empty();
}

} // namespace mfe
