#include "mfe/core/half_edge.hpp"
#include "mfe/core/dec.hpp"
#include "mfe/core/ricci_flow.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <numbers>

int main() {
    std::cout << "[MFE Test] Initializing test..." << std::endl;

    // Create a 2D regular hexagon mesh with a central vertex
    // Vertices:
    // 0: Center (0, 0)
    // 1 to 6: Boundary vertices
    std::vector<std::pair<double, double>> pts = {
        {0.0, 0.0},
        {1.0, 0.0},
        {0.5, 0.86602540378},
        {-0.5, 0.86602540378},
        {-1.0, 0.0},
        {-0.5, -0.86602540378},
        {0.5, -0.86602540378}
    };

    std::vector<std::array<uint32_t, 3>> triangles = {
        {0, 1, 2},
        {0, 2, 3},
        {0, 3, 4},
        {0, 4, 5},
        {0, 5, 6},
        {0, 6, 1}
    };

    mfe::HalfEdgeMesh mesh;
    bool build_success = mesh.build(pts, triangles);
    assert(build_success);
    std::cout << "[MFE Test] Mesh built successfully. Vertices: " << mesh.vertices.size() 
              << ", Faces: " << mesh.faces.size() << ", Edges: " << mesh.edges.size() << std::endl;

    mfe::DECOperators dec(mesh);
    Eigen::VectorXd K;
    dec.compute_curvature(K);

    std::cout << "[MFE Test] Computed initial curvatures:" << std::endl;
    for (int i = 0; i < K.size(); ++i) {
        std::cout << "  Vertex " << i << ": K = " << K(i) << std::endl;
    }

    // Since vertex 0 is a flat internal vertex in a flat triangulation, its curvature should be ~0.
    assert(std::abs(K(0)) < 1e-5);
    std::cout << "[MFE Test] Initial center curvature check passed: K(0) is approx 0." << std::endl;

    // Set target curvatures to match the original flat state before perturbation
    Eigen::VectorXd target_K = K;

    // Let's assemble the Laplacian and mass matrix
    Eigen::SparseMatrix<double> L;
    Eigen::VectorXd M;
    dec.build_laplacian(L, M);
    std::cout << "[MFE Test] Laplacian assembled. Rows: " << L.rows() << ", Cols: " << L.cols() << std::endl;
    assert(L.rows() == 7 && L.cols() == 7);
    
    // Perturb the conformal factors of the mesh to introduce non-zero curvature
    mesh.vertices[0].conformal_factor = 0.2;
    mesh.vertices[1].conformal_factor = -0.1;
    mesh.vertices[2].conformal_factor = 0.15;
    
    dec.compute_curvature(K);
    std::cout << "[MFE Test] Curvature after perturbation K(0): " << K(0) << std::endl;
    assert(std::abs(K(0)) > 1e-4);

    // Let's run Ricci Flow to solve for flat metric
    mfe::RicciFlow flow(mesh, dec);
    flow.set_target_curvature(target_K);

    mfe::RicciFlow::Config config;
    config.use_newton = true;
    config.newton_max_steps = 10;
    config.tolerance = 1e-6;

    std::cout << "[MFE Test] Running Ricci Flow solver..." << std::endl;
    bool converged = flow.solve(config);
    
    dec.compute_curvature(K);
    std::cout << "[MFE Test] Ricci Flow finished. Converged: " << (converged ? "YES" : "NO") << std::endl;
    std::cout << "[MFE Test] Final Curvature at center K(0): " << K(0) << std::endl;
    
    assert(converged);
    assert(std::abs(K(0)) < 1e-5);
    std::cout << "[MFE Test] All assertions passed successfully!" << std::endl;

    return 0;
}
