#include "mfe/core/half_edge.hpp"
#include "mfe/core/dec.hpp"
#include "mfe/transport/power_diagram.hpp"
#include "mfe/transport/ot_placer.hpp"
#include "mfe/transport/legalizer.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void create_grid_mesh(mfe::HalfEdgeMesh& mesh, double width, double height, int nx, int ny) {
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
    std::cout << "[OT Test] Starting Optimal Transport placement test..." << std::endl;

    // 1. Create a 4x4 grid mesh (total area = 1.0)
    mfe::HalfEdgeMesh mesh;
    create_grid_mesh(mesh, 1.0, 1.0, 4, 4);

    // Initialize face areas inside the mesh via DEC
    mfe::DECOperators dec(mesh);
    Eigen::SparseMatrix<double> L;
    Eigen::VectorXd M;
    dec.build_laplacian(L, M);

    // Calculate total area of the mesh to verify
    double total_area = 0.0;
    for (const auto& f : mesh.faces) {
        total_area += f.area;
    }
    std::cout << "[OT Test] Total mesh area: " << total_area << " (expected 1.0)" << std::endl;
    assert(std::abs(total_area - 1.0) < 0.05);

    // 2. Initialize 4 cells, all starting at the center (0.5, 0.5)
    // Target mass for each is total_area / 4.0
    std::vector<mfe::Cell> cells(4);
    for (int i = 0; i < 4; ++i) {
        cells[i].id = i;
        cells[i].x = 0.5;
        cells[i].y = 0.5;
        cells[i].mass = total_area / 4.0;
        cells[i].weight = 0.0;
    }

    // 3. Define a simple connected net list to verify connectivity gradients
    // Net 0 connects cell 0 and cell 1
    // Net 1 connects cell 2 and cell 3
    std::vector<mfe::Net> nets = {
        {0, {0, 1}},
        {1, {2, 3}}
    };

    // 4. Run OT Placement
    mfe::OTPlacer placer(mesh, cells, nets);
    mfe::OTPlacer::Config config;
    config.max_iterations = 80;
    config.tolerance = 1e-3;
    config.weight_step_size = 0.15;
    config.position_step_size = 0.4;
    config.wirelength_weight = 0.02; // Small net pull

    std::cout << "[OT Test] Running placement optimization..." << std::endl;
    bool success = placer.place(config);
    std::cout << "[OT Test] Placement finished. Converged: " << (success ? "YES" : "NO") << std::endl;

    std::cout << "[OT Test] Final cell positions and actual areas:" << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "  Cell " << i << ": Pos=(" << cells[i].x << ", " << cells[i].y 
                  << "), Actual Area=" << cells[i].actual_area << std::endl;
        // Verify cell area is close to target mass (0.25)
        assert(std::abs(cells[i].actual_area - 0.25) < 0.05);
    }

    // 5. Test Legalization snap
    // Create a 2x2 grid of legal sites
    std::vector<std::pair<double, double>> sites = {
        {0.25, 0.25}, {0.25, 0.75},
        {0.75, 0.25}, {0.75, 0.75}
    };

    mfe::Legalizer legalizer(mesh);
    std::cout << "[OT Test] Legalizing cells..." << std::endl;
    legalizer.legalize(cells, sites);

    std::cout << "[OT Test] Legalized cell positions:" << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "  Cell " << i << ": Legalized Pos=(" << cells[i].x << ", " << cells[i].y << ")" << std::endl;
        // Verify each cell snapped to a unique site
        bool matched = false;
        for (const auto& s : sites) {
            if (std::abs(cells[i].x - s.first) < 1e-6 && std::abs(cells[i].y - s.second) < 1e-6) {
                matched = true;
                break;
            }
        }
        assert(matched);
    }

    std::cout << "[OT Test] All Optimal Transport tests passed successfully!" << std::endl;
    return 0;
}
