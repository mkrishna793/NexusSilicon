#include "mfe/core/half_edge.hpp"
#include "mfe/core/dec.hpp"
#include "mfe/logic/sat_window.hpp"
#include "mfe/cts/dme_cts.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

void create_cts_grid_mesh(mfe::HalfEdgeMesh& mesh, double width, double height, int nx, int ny) {
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
    std::cout << "[SAT/CTS Test] Starting tests..." << std::endl;

    // ==========================================
    // Part 1: SAT Detailed Window Router Tests
    // ==========================================
    std::cout << "[SAT/CTS Test] Testing SAT Detailed Router..." << std::endl;
    {
        // Case 1: Feasible routing
        mfe::SATWindowRouter router(4, 4);
        router.add_net(0, 0, 0, 3, 0); // Net 0 along top row
        router.add_net(1, 0, 3, 3, 3); // Net 1 along bottom row
        router.add_obstruction(1, 1);

        std::vector<mfe::DetailedRoute> routes;
        bool feasible = router.route(routes);
        std::cout << "  Feasible routing case check: feasible = " << (feasible ? "YES" : "NO") << std::endl;
        assert(feasible);
        assert(routes.size() == 2);
    }
    {
        // Case 2: Infeasible routing (forced crossing in 2D single layer)
        mfe::SATWindowRouter router(3, 3);
        router.add_net(0, 0, 1, 2, 1); // Net 0 horizontal across middle
        router.add_net(1, 1, 0, 1, 2); // Net 1 vertical across middle

        std::vector<mfe::DetailedRoute> routes;
        bool feasible = router.route(routes);
        std::cout << "  Infeasible crossing case check: feasible = " << (feasible ? "YES" : "NO") << std::endl;
        assert(!feasible); // Must be infeasible
    }
    std::cout << "[SAT/CTS Test] SAT Detailed Router tests passed!" << std::endl;

    // ==========================================
    // Part 2: DME Clock Tree Synthesis Tests
    // ==========================================
    std::cout << "[SAT/CTS Test] Testing DME CTS Builder..." << std::endl;

    // 1. Create a flat mesh
    mfe::HalfEdgeMesh mesh;
    create_cts_grid_mesh(mesh, 10.0, 10.0, 10, 10);
    mfe::DECOperators dec(mesh);
    Eigen::SparseMatrix<double> L;
    Eigen::VectorXd M;
    dec.build_laplacian(L, M);

    // Define 4 symmetric sinks
    std::vector<mfe::ClockSink> sinks = {
        {0, 1.0, 1.0},
        {1, 9.0, 1.0},
        {2, 1.0, 9.0},
        {3, 9.0, 9.0}
    };

    mfe::DMECTSBuilder builder(mesh);

    {
        // Test 2.1: Flat Mesh (symmetric) -> Root should be at (5.0, 5.0)
        std::vector<mfe::ClockTreeNode> tree;
        bool ok = builder.build_tree(sinks, tree);
        assert(ok);
        assert(!tree.empty());

        auto root = tree.back();
        std::cout << "  Flat mesh root coordinates: (" << root.x << ", " << root.y << ")" << std::endl;
        assert(std::abs(root.x - 5.0) < 1e-5);
        assert(std::abs(root.y - 5.0) < 1e-5);

        // Verify zero skew (leaf nodes should have exactly equal arrival delays from the root)
        double d0 = tree[root.left_child].delay;
        double d1 = tree[root.right_child].delay;
        std::cout << "  Flat mesh left/right branch delays from root: " << d0 << ", " << d1 << std::endl;
        assert(std::abs(d0 - d1) < 1e-5);
    }

    {
        // Test 2.2: Coupled Mesh (Symmetric coordinates but hot bump in top-right)
        // Inflate top-right quadrant metric by setting high conformal factors there
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            double dx = mesh.vertices[i].x - 9.0;
            double dy = mesh.vertices[i].y - 9.0;
            double d = std::sqrt(dx * dx + dy * dy);
            if (d < 3.0) {
                mesh.vertices[i].conformal_factor = 2.0 * (1.0 - d / 3.0);
            }
        }

        std::vector<mfe::ClockTreeNode> tree;
        bool ok = builder.build_tree(sinks, tree);
        assert(ok);
        assert(!tree.empty());

        auto root = tree.back();
        std::cout << "  Coupled mesh root coordinates: (" << root.x << ", " << root.y << ")" << std::endl;
        // Since top-right has higher metric scaling, the root must move towards top-right
        // to shorten the physical distance and balance the metric delay.
        assert(root.x > 5.0);
        assert(root.y > 5.0);
        std::cout << "  Coupled root successfully shifted to top-right to balance top-right metric delay!" << std::endl;
    }

    std::cout << "[SAT/CTS Test] DME CTS tests passed!" << std::endl;
    std::cout << "[SAT/CTS Test] All tests completed successfully." << std::endl;
    return 0;
}
