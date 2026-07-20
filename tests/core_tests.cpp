#include "mfe/analysis/routing_estimator.hpp"
#include "mfe/core/mesh.hpp"
#include "mfe/core/metric.hpp"
#include "mfe/diagnostics/congestion.hpp"
#include "mfe/io/artifacts.hpp"
#include "mfe/io/lef_def.hpp"
#include "mfe/pdk/technology.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    const mfe::core::MetricTensor metric{4.0, 1.0, 3.0};
    assert(metric.is_positive_definite());
    assert(metric.directional_cost(1.0, 0.0) == 4.0);
    const auto inverse = metric.inverse();
    assert(inverse.has_value());

    mfe::core::UniformMesh mesh{{0, 0, 200, 100}, 2, 1};
    assert(mesh.index_at({25, 25}) == 0);
    assert(!mesh.index_at({200, 25}).has_value());
    assert(mesh.neighbors(0).size() == 1);
    mesh.cell(0, 0).capacity = 10.0;
    mesh.cell(0, 0).demand = 11.0;
    const auto report = mfe::diagnostics::analyze_congestion(mesh);
    assert(report.has_errors());
    assert(report.findings.size() == 1);

    mfe::pdk::Technology tech{"test"};
    tech.add_routing_layer({"M1", 1, mfe::pdk::PreferredDirection::horizontal, 100, 40, 40, 0.1, 0.2});
    assert(tech.validates());
    assert(tech.find_layer("M1") != nullptr);

    const auto lef = mfe::io::parse_lef(R"(UNITS
  DATABASE MICRONS 1000 ;
END UNITS
LAYER M1
  TYPE ROUTING ;
  DIRECTION HORIZONTAL ;
  PITCH 0.20 ;
  WIDTH 0.08 ;
  SPACING 0.08 ;
END M1
)", "test-lef");
    assert(lef);
    assert(lef.value->routing_layers().front().pitch == 200);

    const auto def = mfe::io::parse_def(R"(VERSION 5.8 ;
DESIGN test_chip ;
UNITS DISTANCE MICRONS 1000 ;
DIEAREA ( 0 0 ) ( 200 100 ) ;
COMPONENTS 2 ;
- U1 INVX1 + PLACED ( 20 30 ) N ;
- U2 INVX1 + PLACED ( 180 30 ) N ;
END COMPONENTS
NETS 1 ;
- signal ( U1 A ) ( U2 A ) ;
END NETS
END DESIGN
)");
    assert(def);
    assert(def.value->components.size() == 2);
    assert(def.value->nets.front().connections.size() == 2);
    const auto serialized_def = mfe::io::write_def(*def.value);
    const auto round_trip_def = mfe::io::parse_def(serialized_def);
    assert(round_trip_def);
    assert(round_trip_def.value->name == def.value->name);
    assert(round_trip_def.value->components.size() == def.value->components.size());
    assert(round_trip_def.value->nets.front().connections == def.value->nets.front().connections);

    mfe::core::UniformMesh estimated_mesh{{0, 0, 200, 100}, 2, 1};
    mfe::analysis::assign_routing_capacity(estimated_mesh, *lef.value);
    assert(estimated_mesh.cell(0, 0).capacity == 0.5);
    const auto estimate = mfe::analysis::estimate_routing_demand(estimated_mesh, *def.value);
    assert(estimate.routed_connections == 1);
    assert(estimated_mesh.cell(0, 0).demand == 1.0);
    assert(estimated_mesh.cell(1, 0).demand == 1.0);

    const auto artifact_dir = std::filesystem::temp_directory_path() / "mfe_phase1_tests";
    const auto artifacts = mfe::io::write_run_artifacts(artifact_dir, *lef.value, *def.value, mesh, report);
    assert(std::filesystem::exists(artifacts.manifest));
    assert(std::filesystem::exists(artifacts.mesh_csv));
    assert(std::filesystem::exists(artifacts.diagnostics_json));
    {
        std::ifstream diagnostic_file(artifacts.diagnostics_json);
        std::string diagnostic_json((std::istreambuf_iterator<char>(diagnostic_file)), {});
        assert(diagnostic_json.find("MFE-CONG-001") != std::string::npos);
    }
    std::filesystem::remove_all(artifact_dir);
    std::cout << "All MFE core tests passed.\n";
}
#include "mfe/analysis/routing_estimator.hpp"
