#include "mfe/core/half_edge.hpp"
#include "mfe/core/dec.hpp"
#include "mfe/geodesic/fmm_router.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

void create_routing_grid_mesh(mfe::HalfEdgeMesh& mesh, double width, double height, int nx, int ny) {
    std::vector<std::pair<double, double>> pts;
    for (int y = 0; y <= ny; ++y) {
        for (int x = 0; x <= nx; ++x) {
            pts.push_back({static_cast<double>(x) * width / nx, static_cast<double>(y) * height / ny});
        }
    }
    std::vector<std::array<uint32_t, 3>> triangles;
    for (int y = 0; y < ny; ++y) {
        for (int x = 0; x < nx; ++x) {
            uint32_t v00 = y * (nx + 1) + x;
            uint32_t v10 = v00 + 1;
            uint32_t v01 = v00 + nx + 1;
            uint32_t v11 = v01 + 1;
            triangles.push_back({v00, v10, v11});
            triangles.push_back({v00, v11, v01});
        }
    }
    mesh.build(pts, triangles);
}

int main() {
    std::cout << "[Routing Test] Starting Geodesic FMM Routing tests..." << std::endl;

    // 1. Create a 10x10 grid mesh
    mfe::HalfEdgeMesh mesh;
    create_routing_grid_mesh(mesh, 1.0, 1.0, 10, 10);

    // Initialize areas and DEC operators
    mfe::DECOperators dec(mesh);
    Eigen::SparseMatrix<double> L;
    Eigen::VectorXd M;
    dec.build_laplacian(L, M);

    // 2. Identify Source (0.1, 0.5) and Sink (0.9, 0.5)
    int source_idx = -1;
    int sink_idx = -1;
    double min_d_src = 1e9;
    double min_d_snk = 1e9;
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        double dx_src = mesh.vertices[i].x - 0.1;
        double dy_src = mesh.vertices[i].y - 0.5;
        double d_src = std::sqrt(dx_src * dx_src + dy_src * dy_src);
        if (d_src < min_d_src) {
            min_d_src = d_src;
            source_idx = static_cast<int>(i);
        }

        double dx_snk = mesh.vertices[i].x - 0.9;
        double dy_snk = mesh.vertices[i].y - 0.5;
        double d_snk = std::sqrt(dx_snk * dx_snk + dy_snk * dy_snk);
        if (d_snk < min_d_snk) {
            min_d_snk = d_snk;
            sink_idx = static_cast<int>(i);
        }
    }

    std::cout << "[Routing Test] Source Vertex: " << source_idx << " at (" << mesh.vertices[source_idx].x << ", " << mesh.vertices[source_idx].y << ")" << std::endl;
    std::cout << "[Routing Test] Sink Vertex: " << sink_idx << " at (" << mesh.vertices[sink_idx].x << ", " << mesh.vertices[sink_idx].y << ")" << std::endl;

    // 3. Test 1: Flat Mesh Geodesic (Should be a straight horizontal line along y = 0.5)
    mfe::FMMRouter router(mesh);
    Eigen::VectorXd dist_flat;
    bool computed = router.compute_distance_field(source_idx, dist_flat);
    assert(computed);

    std::vector<std::pair<double, double>> path_flat;
    bool backtracked = router.backtrack_path(sink_idx, dist_flat, path_flat);
    assert(backtracked);

    std::cout << "[Routing Test] Flat path coordinates:" << std::endl;
    for (const auto& pt : path_flat) {
        std::cout << "  (" << pt.first << ", " << pt.second << ")" << std::endl;
        // The y-coordinates should be close to 0.5 since the mesh is flat and shortest path is a horizontal line
        assert(std::abs(pt.second - 0.5) < 1e-5);
    }
    std::cout << "[Routing Test] Flat path check passed (straight line along y = 0.5)." << std::endl;

    // 4. Test 2: Congested/Hot area bump in the middle (inflate metric around (0.5, 0.5))
    // We add a large conformal factor bump to center vertices
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        double dx = mesh.vertices[i].x - 0.5;
        double dy = mesh.vertices[i].y - 0.5;
        double d = std::sqrt(dx * dx + dy * dy);
        if (d < 0.25) {
            // Hot/congested center
            mesh.vertices[i].conformal_factor = 2.5 * (1.0 - d / 0.25);
        }
    }

    Eigen::VectorXd dist_bump;
    computed = router.compute_distance_field(source_idx, dist_bump);
    assert(computed);

    std::vector<std::pair<double, double>> path_bump;
    backtracked = router.backtrack_path(sink_idx, dist_bump, path_bump);
    assert(backtracked);

    std::cout << "[Routing Test] Bump path coordinates:" << std::endl;
    bool detoured = false;
    for (const auto& pt : path_bump) {
        std::cout << "  (" << pt.first << ", " << pt.second << ")" << std::endl;
        // Verify that the path detours away from y = 0.5 in the middle of the x range [0.3, 0.7]
        if (pt.first > 0.3 && pt.first < 0.7) {
            if (std::abs(pt.second - 0.5) > 0.1) {
                detoured = true;
            }
        }
    }

    assert(detoured);
    std::cout << "[Routing Test] Bump path check passed (curves around congested/hot zone)." << std::endl;

    std::cout << "[Routing Test] All Geodesic FMM Routing tests passed successfully!" << std::endl;
    return 0;
}
