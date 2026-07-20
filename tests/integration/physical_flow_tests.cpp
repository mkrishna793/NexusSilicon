#include "mfe/flow/physical_flow.hpp"
#include "mfe/flow/manifest.hpp"
#include <iostream>
#include <fstream>
#include <cassert>

void write_helper_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    assert(out.is_open());
    out << content;
}

int main() {
    std::cout << "[Physical Flow Tests] Writing dummy input netlist..." << std::endl;
    std::filesystem::create_directories("results_test/frontend");
    std::string verilog_content =
        "module gcd (\n"
        "  input clk\n"
        ");\n"
        "  AND2x1 g0 ( .A(net_A), .B(net_B), .Y(net_Y) );\n"
        "  AND2x1 g1 ( .A(net_Y), .B(net_C), .Y(net_Z) );\n"
        "endmodule\n";
    write_helper_file("results_test/frontend/mapped_netlist.v", verilog_content);

    const std::filesystem::path root = MFE_SOURCE_DIR;
    mfe::flow::PdkManifest pdk = mfe::flow::PdkManifest::load_from_json(root / "config/pdks/asap7/manifest.json");
    mfe::flow::FlowProject project = mfe::flow::FlowProject::load_from_yaml(root / "projects/gcd_demo/project.yaml");
    
    // Override output directories to temp location
    project.outputs = "results_test";

    std::vector<mfe::flow::Finding> findings;
    std::map<std::string, double> metrics;

    mfe::flow::PhysicalFlow flow;
    std::cout << "[Physical Flow Tests] Running pipeline flow..." << std::endl;
    bool success = flow.run_physical_flow(project, pdk, findings, metrics);

    std::cout << "[Physical Flow Tests] Verifying output database..." << std::endl;
    assert(success == true);
    assert(metrics["routed_nets_count"] == 1.0);
    assert(metrics["total_route_length_um"] > 0.0);
    assert(metrics["peak_temperature"] == 300.48);
    assert(metrics["drc_violations"] == 0.0);

    // Verify routed DEF exists and is structurally correct
    std::filesystem::path def_file = "results_test/physical/routed.def";
    assert(std::filesystem::exists(def_file));

    std::ifstream def_in(def_file);
    std::string line;
    bool found_components = false;
    bool found_routed = false;

    while (std::getline(def_in, line)) {
        if (line.find("COMPONENTS 2") != std::string::npos) found_components = true;
        if (line.find("+ ROUTED") != std::string::npos) found_routed = true;
    }
    def_in.close();

    assert(found_components == true);
    assert(found_routed == true);
    std::cout << "  Routed DEF contains components and routes successfully!" << std::endl;

    std::filesystem::remove_all("results_test");
    std::cout << "[Physical Flow Tests] All tests passed!" << std::endl;
    return 0;
}
