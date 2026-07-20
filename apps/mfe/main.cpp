#include "mfe/core/half_edge.hpp"
#include "mfe/core/dec.hpp"
#include "mfe/thermo/thermal_solver.hpp"
#include "mfe/geodesic/fmm_router.hpp"
#include "mfe/logic/sat_window.hpp"
#include "mfe/cts/dme_cts.hpp"
#include "mfe/io/lef_def.hpp"
#include "mfe/io/bookshelf.hpp"
#include "mfe/transport/ot_placer.hpp"
#include "mfe/flow/manifest.hpp"
#include "mfe/flow/physical_flow.hpp"
  #include "mfe/flow/opensta_importer.hpp"
  #include "mfe/flow/openrcx_importer.hpp"
  #include "mfe/flow/evidence_graph.hpp"
#include "mfe/eco/eco_planner.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

void print_usage() {
    std::cout << "Metric Field Engine (MFE) Command-Line Utility" << std::endl;
    std::cout << "Usage: mfe_app <mode> [options]" << std::endl;
    std::cout << "Modes:" << std::endl;
    std::cout << "  thermo      Solve steady-state thermal distribution" << std::endl;
    std::cout << "  route       Compute geodesic FMM routing paths" << std::endl;
    std::cout << "  sat_route   Solve window detailed routing using SAT" << std::endl;
    std::cout << "  cts         Build a metric-balanced DME clock tree" << std::endl;
    std::cout << "  run_benchmark  Run physical optimization on LEF/DEF; optional --output-placed-def <path>" << std::endl;
    std::cout << "  run_bookshelf  Run full physical design optimization on Bookshelf benchmark files" << std::endl;
    std::cout << "  run_flow       Run end-to-end physical pipeline flow from project and PDK manifests" << std::endl;
      std::cout << "  run_eco        Parse timing reports and produce a reviewed ECO plan" << std::endl;
}

void create_simple_grid(mfe::HalfEdgeMesh& mesh, int nx, int ny) {
    std::vector<std::pair<double, double>> pts;
    for (int y = 0; y <= ny; ++y) {
        for (int x = 0; x <= nx; ++x) {
            pts.push_back({static_cast<double>(x), static_cast<double>(y)});
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

int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        print_usage();
        return 0;
    }

    std::string mode = argv[1];

    if (mode == "thermo") {
        std::cout << "[MFE CLI] Mode: Thermal Solve" << std::endl;
        mfe::HalfEdgeMesh mesh;
        create_simple_grid(mesh, 10, 10);
        mfe::DECOperators dec(mesh);
        Eigen::SparseMatrix<double> L;
        Eigen::VectorXd M;
        dec.build_laplacian(L, M);

        mfe::ThermalSolver solver(mesh);
        Eigen::SparseMatrix<double> L_K;
        std::vector<double> conductivities(mesh.faces.size(), 150.0); // Silicon
        solver.build_stiffness_matrix(conductivities, L_K);

        Eigen::VectorXd powers = Eigen::VectorXd::Zero(mesh.vertices.size());
        powers(mesh.vertices.size() / 2) = 5000.0; // Point source at center

        Eigen::VectorXd T;
        if (solver.solve(L_K, powers, T, 300.0)) {
            std::cout << "Successfully solved temperature field." << std::endl;
            std::cout << "Peak Temp at center: " << T(mesh.vertices.size() / 2) << " K" << std::endl;
        } else {
            std::cerr << "Failed to solve thermal system." << std::endl;
            return 1;
        }
    } else if (mode == "route") {
        std::cout << "[MFE CLI] Mode: Geodesic FMM Route" << std::endl;
        mfe::HalfEdgeMesh mesh;
        create_simple_grid(mesh, 10, 10);
        mfe::DECOperators dec(mesh);
        Eigen::SparseMatrix<double> L;
        Eigen::VectorXd M;
        dec.build_laplacian(L, M);

        mfe::FMMRouter router(mesh);
        Eigen::VectorXd distance;
        uint32_t src = 0;
        uint32_t snk = mesh.vertices.size() - 1;
        if (router.compute_distance_field(src, distance)) {
            std::vector<std::pair<double, double>> path;
            if (router.backtrack_path(snk, distance, path)) {
                std::cout << "Geodesic path found from vertex " << src << " to " << snk << std::endl;
                std::cout << "Path steps: " << path.size() << std::endl;
            } else {
                std::cerr << "Failed to backtrack path." << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Failed to compute distance field." << std::endl;
            return 1;
        }
    } else if (mode == "sat_route") {
        std::cout << "[MFE CLI] Mode: SAT Detailed Route" << std::endl;
        mfe::SATWindowRouter router(4, 4);
        router.add_net(0, 0, 0, 3, 0);
        router.add_net(1, 0, 3, 3, 3);
        router.add_obstruction(1, 1);

        std::vector<mfe::DetailedRoute> routes;
        if (router.route(routes)) {
            std::cout << "SAT routing feasible! Net paths found:" << std::endl;
            for (const auto& r : routes) {
                std::cout << "  Net " << r.net_id << " path size: " << r.path.size() << std::endl;
            }
        } else {
            std::cout << "SAT routing infeasible (conflicting rules/obstructions)." << std::endl;
        }
    } else if (mode == "cts") {
        std::cout << "[MFE CLI] Mode: DME CTS" << std::endl;
        mfe::HalfEdgeMesh mesh;
        create_simple_grid(mesh, 10, 10);
        mfe::DECOperators dec(mesh);
        Eigen::SparseMatrix<double> L;
        Eigen::VectorXd M;
        dec.build_laplacian(L, M);

        mfe::DMECTSBuilder builder(mesh);
        std::vector<mfe::ClockSink> sinks = {
            {0, 1.0, 1.0},
            {1, 9.0, 1.0},
            {2, 1.0, 9.0},
            {3, 9.0, 9.0}
        };
        std::vector<mfe::ClockTreeNode> tree;
        if (builder.build_tree(sinks, tree)) {
            std::cout << "Clock tree built successfully! Total tree nodes: " << tree.size() << std::endl;
            std::cout << "Root node coordinate: (" << tree.back().x << ", " << tree.back().y << ")" << std::endl;
        } else {
            std::cerr << "Failed to build clock tree." << std::endl;
            return 1;
        }
    } else if (mode == "run_benchmark") {
        if (argc < 4) {
            std::cerr << "Usage: mfe run_benchmark <lef_path> <def_path> [--output-placed-def <path>]" << std::endl;
            return 1;
        }
        std::string lef_path = argv[2];
        std::string def_path = argv[3];
        std::string placed_def_path;
        if (argc == 6 && std::string(argv[4]) == "--output-placed-def") {
            placed_def_path = argv[5];
        } else if (argc != 4) {
            std::cerr << "Usage: mfe run_benchmark <lef_path> <def_path> [--output-placed-def <path>]" << std::endl;
            return 1;
        }

        std::cout << "[MFE Benchmark] Reading LEF file: " << lef_path << std::endl;
        auto tech_res = mfe::io::parse_lef_file(lef_path);
        if (!tech_res) {
            std::cerr << "Failed to parse LEF file. Errors:" << std::endl;
            for (const auto& err : tech_res.errors) {
                std::cerr << "  Line " << err.line << ": " << err.message << std::endl;
            }
            return 1;
        }

        std::cout << "[MFE Benchmark] Reading DEF file: " << def_path << std::endl;
        auto design_res = mfe::io::parse_def_file(def_path);
        if (!design_res) {
            std::cerr << "Failed to parse DEF file." << std::endl;
            return 1;
        }
        auto design = design_res.value.value();

        std::cout << "[MFE Benchmark] Design Name: " << design.name << std::endl;
        std::cout << "[MFE Benchmark] Components count: " << design.components.size() << std::endl;
        std::cout << "[MFE Benchmark] Nets count: " << design.nets.size() << std::endl;

        // 1. Build a 20x20 grid mesh for analysis
        mfe::HalfEdgeMesh mesh;
        create_simple_grid(mesh, 20, 20);
        mfe::DECOperators dec(mesh);
        Eigen::SparseMatrix<double> L;
        Eigen::VectorXd M;
        dec.build_laplacian(L, M);

        double total_area = 0.0;
        for (const auto& f : mesh.faces) {
            total_area += f.area;
        }

        // 2. Load components as cells and normalize coordinates
        std::vector<mfe::Cell> cells(design.components.size());
        double dx_die = design.die_area.width();
        double dy_die = design.die_area.height();
        if (dx_die <= 0 || dy_die <= 0) {
            dx_die = 100000.0; dy_die = 100000.0; // Fallback
        }

        for (size_t i = 0; i < design.components.size(); ++i) {
            cells[i].id = static_cast<int>(i);
            cells[i].x = static_cast<double>(design.components[i].location.x - design.die_area.left) / dx_die;
            cells[i].y = static_cast<double>(design.components[i].location.y - design.die_area.bottom) / dy_die;
            cells[i].x = std::clamp(cells[i].x, 0.0, 1.0);
            cells[i].y = std::clamp(cells[i].y, 0.0, 1.0);
            cells[i].mass = total_area / design.components.size();
            cells[i].weight = 0.0;
        }

        // Map design nets to OT placer nets
        std::vector<mfe::Net> ot_nets;
        for (size_t i = 0; i < design.nets.size(); ++i) {
            const auto& d_net = design.nets[i];
            mfe::Net net;
            net.id = static_cast<uint32_t>(i);
            for (const auto& conn : d_net.connections) {
                // Find component index by name
                for (size_t c_idx = 0; c_idx < design.components.size(); ++c_idx) {
                    if (design.components[c_idx].name == conn) {
                        net.cell_ids.push_back(static_cast<uint32_t>(c_idx));
                        break;
                    }
                }
            }
            if (net.cell_ids.size() >= 2) {
                ot_nets.push_back(net);
            }
        }

        // 3. Run OT Placer (spread cells out)
        std::cout << "[MFE Benchmark] Running Optimal Transport Placer..." << std::endl;
        mfe::OTPlacer placer(mesh, cells, ot_nets);
        mfe::OTPlacer::Config ot_config;
        ot_config.max_iterations = 20;
        placer.place(ot_config);
        for (size_t i = 0; i < design.components.size(); ++i) {
            design.components[i].location.x = static_cast<mfe::core::DbUnit>(
                std::llround(design.die_area.left + cells[i].x * dx_die));
            design.components[i].location.y = static_cast<mfe::core::DbUnit>(
                std::llround(design.die_area.bottom + cells[i].y * dy_die));
        }
        std::cout << "[MFE Benchmark] Placement finished." << std::endl;

        // 4. Run Poisson Thermal Solver based on placement
        std::cout << "[MFE Benchmark] Running Poisson Thermal Solver..." << std::endl;
        mfe::ThermalSolver thermal_solver(mesh);
        Eigen::SparseMatrix<double> L_K;
        std::vector<double> conductivities(mesh.faces.size(), 150.0); // Silicon
        thermal_solver.build_stiffness_matrix(conductivities, L_K);

        // Assign mock power profile: components consume power
        std::vector<double> cell_powers(cells.size(), 1.0); // 1W each
        Eigen::VectorXd Q, vertex_powers;
        thermal_solver.allocate_power_barycentric(cells, cell_powers, Q, vertex_powers);

        Eigen::VectorXd T;
        if (thermal_solver.solve(L_K, vertex_powers, T, 300.0)) {
            std::cout << "[MFE Benchmark] Peak Temperature: " << T.maxCoeff() << " K" << std::endl;
            std::cout << "[MFE Benchmark] Avg Temperature: " << T.mean() << " K" << std::endl;
        }

        // 5. Run Geodesic FMM Routing for a sample net
        if (!design.nets.empty()) {
            std::cout << "[MFE Benchmark] Running Geodesic FMM Router for sample net..." << std::endl;
            mfe::FMMRouter router(mesh);
            Eigen::VectorXd distance;
            // Route from bottom-left cell to top-right cell
            uint32_t src_v = 0;
            uint32_t snk_v = mesh.vertices.size() - 1;
            if (router.compute_distance_field(src_v, distance)) {
                std::vector<std::pair<double, double>> path;
                if (router.backtrack_path(snk_v, distance, path)) {
                    std::cout << "[MFE Benchmark] Geodesic path found! Steps: " << path.size() << std::endl;
                }
            }
        }
        if (!placed_def_path.empty()) {
            if (!mfe::io::write_def_file(design, placed_def_path)) {
                std::cerr << "[MFE Benchmark] Failed to write placed DEF: " << placed_def_path << std::endl;
                return 1;
            }
            std::cout << "[MFE Benchmark] Wrote placed DEF: " << placed_def_path << std::endl;
        }
        std::cout << "[MFE Benchmark] Completed benchmark successfully!" << std::endl;
    } else if (mode == "run_bookshelf") {
        if (argc < 5) {
            std::cerr << "Usage: mfe run_bookshelf <nodes_path> <nets_path> <pl_path>" << std::endl;
            return 1;
        }
        std::string nodes_path = argv[2];
        std::string nets_path = argv[3];
        std::string pl_path = argv[4];

        std::cout << "[MFE Bookshelf] Parsing layout: " << nodes_path << std::endl;
        mfe::BookshelfLayout layout = mfe::parse_bookshelf(nodes_path, nets_path, pl_path);
        std::cout << "[MFE Bookshelf] Cells count: " << layout.cells.size() << std::endl;
        std::cout << "[MFE Bookshelf] Nets count: " << layout.nets.size() << std::endl;

        // 1. Build a 20x20 grid mesh for analysis
        mfe::HalfEdgeMesh mesh;
        create_simple_grid(mesh, 20, 20);
        mfe::DECOperators dec(mesh);
        Eigen::SparseMatrix<double> L;
        Eigen::VectorXd M;
        dec.build_laplacian(L, M);

        double total_area = 0.0;
        for (const auto& f : mesh.faces) {
            total_area += f.area;
        }

        // Set cell target masses uniformly
        for (auto& cell : layout.cells) {
            cell.mass = total_area / layout.cells.size();
        }

        // 2. Run OT Placer (spread cells out)
        std::cout << "[MFE Bookshelf] Running Optimal Transport Placer..." << std::endl;
        mfe::OTPlacer placer(mesh, layout.cells, layout.nets);
        mfe::OTPlacer::Config ot_config;
        ot_config.max_iterations = 20;
        placer.place(ot_config);
        std::cout << "[MFE Bookshelf] Placement finished." << std::endl;

        // 3. Run Poisson Thermal Solver based on placement
        std::cout << "[MFE Bookshelf] Running Poisson Thermal Solver..." << std::endl;
        mfe::ThermalSolver thermal_solver(mesh);
        Eigen::SparseMatrix<double> L_K;
        std::vector<double> conductivities(mesh.faces.size(), 150.0); // Silicon
        thermal_solver.build_stiffness_matrix(conductivities, L_K);

        // Assign mock power profile: components consume power
        std::vector<double> cell_powers(layout.cells.size(), 1.0); // 1W each
        Eigen::VectorXd Q, vertex_powers;
        thermal_solver.allocate_power_barycentric(layout.cells, cell_powers, Q, vertex_powers);

        Eigen::VectorXd T;
        if (thermal_solver.solve(L_K, vertex_powers, T, 300.0)) {
            std::cout << "[MFE Bookshelf] Peak Temperature: " << T.maxCoeff() << " K" << std::endl;
            std::cout << "[MFE Bookshelf] Avg Temperature: " << T.mean() << " K" << std::endl;
        }

        // 4. Run Geodesic FMM Routing for a sample net
        if (!layout.nets.empty()) {
            std::cout << "[MFE Bookshelf] Running Geodesic FMM Router for sample net..." << std::endl;
            mfe::FMMRouter router(mesh);
            Eigen::VectorXd distance;
            uint32_t src_v = 0;
            uint32_t snk_v = mesh.vertices.size() - 1;
            if (router.compute_distance_field(src_v, distance)) {
                std::vector<std::pair<double, double>> path;
                if (router.backtrack_path(snk_v, distance, path)) {
                    std::cout << "[MFE Bookshelf] Geodesic path found! Steps: " << path.size() << std::endl;
                }
            }
        }
        std::cout << "[MFE Bookshelf] Completed Bookshelf run successfully!" << std::endl;
    } else if (mode == "run_flow") {
        if (argc < 4) {
            std::cerr << "Usage: mfe run_flow <project_yaml_path> <pdk_manifest_json_path>" << std::endl;
            return 1;
        }
        std::string project_yaml = argv[2];
        std::string pdk_manifest = argv[3];

        try {
            mfe::flow::FlowProject project = mfe::flow::FlowProject::load_from_yaml(project_yaml);
            mfe::flow::PdkManifest pdk = mfe::flow::PdkManifest::load_from_json(pdk_manifest);

            // Resolve outputs path relative to the directory containing the project YAML
            std::filesystem::path yaml_dir = std::filesystem::path(project_yaml).parent_path();
            if (project.outputs.is_relative()) {
                project.outputs = yaml_dir / project.outputs;
            }

            std::vector<mfe::flow::Finding> findings;
            std::map<std::string, double> metrics;

            mfe::flow::PhysicalFlow physical_pipeline;
            bool success = physical_pipeline.run_physical_flow(project, pdk, findings, metrics);

            // Write stage result file
            std::filesystem::path physical_dir = project.outputs / "physical";
            std::filesystem::path result_path = physical_dir / "stage-result.json";
            std::vector<std::filesystem::path> inputs = { project_yaml, pdk_manifest };
            std::vector<std::filesystem::path> outputs = { physical_dir / "routed.def" };

            mfe::flow::write_stage_result(result_path, "placement", success ? "passed" : "failed", 
                                          inputs, outputs, metrics, findings);

            if (!success) {
                std::cerr << "[MFE CLI] Physical flow execution failed." << std::endl;
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "[MFE CLI] Exception: " << e.what() << std::endl;
            return 1;
        }
    } else if (mode == "run_eco") {
        if (argc < 4) {
            std::cerr << "Usage: mfe run_eco <project_yaml_path> <pdk_manifest_json_path>" << std::endl;
            return 1;
        }
        std::string project_yaml = argv[2];
        std::string pdk_manifest = argv[3];

        try {
            mfe::flow::FlowProject project = mfe::flow::FlowProject::load_from_yaml(project_yaml);
            mfe::flow::PdkManifest pdk = mfe::flow::PdkManifest::load_from_json(pdk_manifest);

            std::filesystem::path yaml_dir = std::filesystem::path(project_yaml).parent_path();
            if (project.outputs.is_relative()) {
                project.outputs = yaml_dir / project.outputs;
            }

            std::vector<mfe::flow::Finding> findings;
            std::map<std::string, double> metrics;
            mfe::flow::OpenStaImporter sta_importer;
            if (!sta_importer.import_timing_report(project, findings, metrics)) {
                std::cerr << "[MFE ECO] Failed to load timing report." << std::endl;
                return 1;
            }

            mfe::eco::EcoPlanner planner;
            std::vector<mfe::eco::EcoAction> plan = planner.propose_eco_plan(findings);

              std::filesystem::path eco_plan_path = project.outputs / "timing" / "eco-plan.json";
              std::filesystem::path timing_evidence_path = project.outputs / "timing" / "timing-evidence.json";
              mfe::flow::EvidenceGraph timing_evidence;
              constexpr const char* timing_prefix = "Timing path violation: ";
              for (std::size_t index = 0; index < findings.size(); ++index) {
                  const auto& finding = findings[index];
                  const std::string finding_id = "finding:" + finding.id + ":" + std::to_string(index);
                  const std::string path_id = "timing_path:" + std::to_string(index);
                  timing_evidence.add_entity({finding_id, mfe::flow::EntityKind::finding, finding.message, std::nullopt});
                  timing_evidence.add_entity({path_id, mfe::flow::EntityKind::timing_path, "STA path " + std::to_string(index), std::nullopt});
                  timing_evidence.add_edge({finding_id, "reports", path_id});

                  const auto prefix_at = finding.message.find(timing_prefix);
                  if (prefix_at == std::string::npos) continue;
                  const auto path = finding.message.substr(prefix_at + std::char_traits<char>::length(timing_prefix));
                  const auto separator = path.find(" to ");
                  const auto start = path.substr(0, separator);
                  const auto end = separator == std::string::npos ? "" : path.substr(separator + 4);
                  const std::string start_id = "instance:" + start;
                  const std::string end_id = "instance:" + end.substr(0, end.find(" (slack:"));
                  if (timing_evidence.find(start_id) == nullptr) {
                      timing_evidence.add_entity({start_id, mfe::flow::EntityKind::instance, start, std::nullopt});
                  }
                  if (timing_evidence.find(end_id) == nullptr) {
                      timing_evidence.add_entity({end_id, mfe::flow::EntityKind::instance, end, std::nullopt});
                  }
                  timing_evidence.add_edge({path_id, "starts_at", start_id});
                  timing_evidence.add_edge({path_id, "ends_at", end_id});
              }
              mfe::flow::write_evidence_graph(timing_evidence_path, timing_evidence);
              if (!plan.empty()) {
                 std::cout << "[MFE ECO] Produced " << plan.size() << " proposed corrections for review." << std::endl;
                 std::ofstream eco_file(eco_plan_path);
                 eco_file << "{\n  \"schema_version\": 1,\n  \"status\": \"proposed\",\n  \"actions\": [\n";
                 for (std::size_t i = 0; i < plan.size(); ++i) {
                     const auto& action = plan[i];
                     eco_file << "    {\"type\": \"" << action.action_type
                              << "\", \"target\": \"" << action.instance_name
                              << "\", \"parameter\": \"" << action.parameter << "\"}";
                     if (i + 1 < plan.size()) eco_file << ",";
                     eco_file << "\n";
                 }
                 eco_file << "  ]\n}\n";
                 std::cout << "[MFE ECO] No design was modified; application requires re-place, re-route, STA, and validation." << std::endl;
              } else {
                 std::cout << "[MFE ECO] No timing violations. Slack target met." << std::endl;
              }

            // Write stage result file
            std::filesystem::path result_path = project.outputs / "timing" / "stage-result.json";
            std::vector<std::filesystem::path> inputs = { project_yaml, pdk_manifest };
             std::vector<std::filesystem::path> outputs;
             if (!plan.empty()) outputs.push_back(eco_plan_path);
             outputs.push_back(timing_evidence_path);

            mfe::flow::write_stage_result(result_path, "timing", "passed", 
                                          inputs, outputs, metrics, findings);

        } catch (const std::exception& e) {
            std::cerr << "[MFE ECO] Exception: " << e.what() << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Unknown mode: " << mode << std::endl;
        print_usage();
        return 1;
    }

    return 0;
}
