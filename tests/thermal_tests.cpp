#include "mfe/core/half_edge.hpp"
#include "mfe/core/dec.hpp"
#include "mfe/core/ricci_flow.hpp"
#include "mfe/thermo/thermal_stack.hpp"
#include "mfe/thermo/thermal_solver.hpp"
#include "mfe/transport/power_diagram.hpp"
#include "mfe/transport/ot_placer.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

void create_test_grid_mesh(mfe::HalfEdgeMesh& mesh, double width, double height, int nx, int ny) {
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
    std::cout << "[Thermo Test] Starting Thermo-Geometry tests..." << std::endl;

    // 1. Initialize a 10x10 grid mesh (121 vertices, total area = 1.0)
    mfe::HalfEdgeMesh mesh;
    create_test_grid_mesh(mesh, 1.0, 1.0, 10, 10);

    // Initialize face areas inside the mesh via DEC
    mfe::DECOperators dec(mesh);
    Eigen::SparseMatrix<double> L;
    Eigen::VectorXd M;
    dec.build_laplacian(L, M);

    // 2. Identify the central vertex
    int center_v_idx = -1;
    double min_d = 1e9;
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        double dx = mesh.vertices[i].x - 0.5;
        double dy = mesh.vertices[i].y - 0.5;
        double d = std::sqrt(dx * dx + dy * dy);
        if (d < min_d) {
            min_d = d;
            center_v_idx = static_cast<int>(i);
        }
    }
    std::cout << "[Thermo Test] Center vertex ID: " << center_v_idx 
              << " at (" << mesh.vertices[center_v_idx].x << ", " 
              << mesh.vertices[center_v_idx].y << ")" << std::endl;

    // 3. Set a point heat source at the center vertex
    Eigen::VectorXd vertex_powers = Eigen::VectorXd::Zero(mesh.vertices.size());
    vertex_powers(center_v_idx) = 15000.0; // 15 kW source for visible thermal gradient

    // 4. Solve the thermal Poisson equation
    mfe::ThermalSolver thermal_solver(mesh);
    Eigen::SparseMatrix<double> L_K;
    // Assume all Silicon conductivity (150.0 W/mK)
    std::vector<double> face_conductivities(mesh.faces.size(), 150.0);
    thermal_solver.build_stiffness_matrix(face_conductivities, L_K);

    Eigen::VectorXd T;
    bool solved = thermal_solver.solve(L_K, vertex_powers, T, 300.0);
    assert(solved);

    std::cout << "[Thermo Test] Solved temperatures:" << std::endl;
    std::cout << "  Center Temp: " << T(center_v_idx) << " K" << std::endl;

    // Verify diffuse bell curve shape:
    // Temperature should decrease as we move away from the center.
    double prev_temp = T(center_v_idx);
    int checked_count = 0;
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        if (static_cast<int>(i) == center_v_idx) continue;
        double dx = mesh.vertices[i].x - 0.5;
        double dy = mesh.vertices[i].y - 0.5;
        double d = std::sqrt(dx * dx + dy * dy);
        if (d < 0.25) {
            // Near center
            assert(T(i) < T(center_v_idx));
            assert(T(i) > 300.0);
            checked_count++;
        } else if (d > 0.6) {
            // Far boundary area
            assert(T(i) < T(center_v_idx));
            // Should be closer to ambient (300.0)
            assert(T(i) < 350.0);
        }
    }
    std::cout << "[Thermo Test] Diffuse temperature bell curve verified! Checked " 
              << checked_count << " near-center vertices." << std::endl;

    // 5. Setup 4 cells close to the center
    std::vector<mfe::Cell> cells(4);
    for (int i = 0; i < 4; ++i) {
        cells[i].id = i;
        // Start them offset from the center
        cells[i].x = 0.5 + ((i % 2 == 0) ? -0.05 : 0.05);
        cells[i].y = 0.5 + ((i / 2 == 0) ? -0.05 : 0.05);
        cells[i].mass = 0.22;
        cells[i].weight = 0.0;
    }
    std::vector<mfe::Net> empty_nets;

    // Run flat placement baseline
    mfe::OTPlacer flat_placer(mesh, cells, empty_nets);
    mfe::OTPlacer::Config placer_cfg;
    placer_cfg.max_iterations = 50;
    placer_cfg.tolerance = 1e-3;
    placer_cfg.weight_step_size = 0.1;
    placer_cfg.position_step_size = 0.5;
    placer_cfg.wirelength_weight = 0.0;

    std::cout << "[Thermo Test] Running flat baseline placement..." << std::endl;
    flat_placer.place(placer_cfg);

    double sum_flat_dist = 0.0;
    for (int i = 0; i < 4; ++i) {
        double dx = cells[i].x - 0.5;
        double dy = cells[i].y - 0.5;
        sum_flat_dist += std::sqrt(dx * dx + dy * dy);
    }
    double avg_flat_dist = sum_flat_dist / 4.0;
    std::cout << "  Flat placement average cell distance from center: " << avg_flat_dist << std::endl;

    // 6. Apply temperature-to-metric coupling and re-place
    // We add stretching to conformal factor: lambda_v += gamma * T_v
    // Since center is hot, this will inflate the center metric.
    double gamma = 0.002;
    mfe::ThermalSolver::apply_thermal_coupling(mesh, T, gamma);

    // Let's run a few Ricci Flow steps to regularize/flat-smooth the metric (optional but recommended in flow)
    mfe::RicciFlow flow(mesh, dec);
    Eigen::VectorXd target_K = Eigen::VectorXd::Zero(mesh.vertices.size());
    // Keep boundary target curvature or just run a couple of GD steps
    flow.set_target_curvature(target_K);
    flow.step_gd(0.01);

    // Run OT placer with the thermally coupled metric
    mfe::OTPlacer coupled_placer(mesh, cells, empty_nets);
    std::cout << "[Thermo Test] Running thermally coupled placement..." << std::endl;
    coupled_placer.place(placer_cfg);

    double sum_coupled_dist = 0.0;
    for (int i = 0; i < 4; ++i) {
        double dx = cells[i].x - 0.5;
        double dy = cells[i].y - 0.5;
        sum_coupled_dist += std::sqrt(dx * dx + dy * dy);
    }
    double avg_coupled_dist = sum_coupled_dist / 4.0;
    std::cout << "  Coupled placement average cell distance from center: " << avg_coupled_dist << std::endl;

    // The cells should have been pushed further away from the hot center zone
    assert(avg_coupled_dist > avg_flat_dist);
    std::cout << "[Thermo Test] Verification successful: Ricci flow + OT placer successfully spreads cells away from the hot zone!" << std::endl;

    std::cout << "[Thermo Test] All Thermo tests passed successfully!" << std::endl;
    return 0;
}
