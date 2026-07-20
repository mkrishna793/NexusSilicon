#include "mfe/io/netlist_importer.hpp"
#include "mfe/flow/manifest.hpp"
#include <iostream>
#include <fstream>
#include <cassert>

void write_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    assert(out.is_open());
    out << content;
}

int main() {
    std::cout << "[Netlist Importer Tests] Creating mock mapped netlist..." << std::endl;
    std::string verilog_content =
        "module gcd (\n"
        "  input clk,\n"
        "  input rst_n\n"
        ");\n"
        "  wire net_A, net_B, net_Y, net_C, net_Z;\n"
        "  AND2x1 g0 ( .A(net_A), .B(net_B), .Y(net_Y) );\n"
        "  AND2x1 g1 ( .A(net_Y), .B(net_C), .Y(net_Z) );\n"
        "endmodule\n";

    write_file("mapped_netlist.v", verilog_content);

    std::cout << "[Netlist Importer Tests] Loading manifests..." << std::endl;
    const std::filesystem::path root = MFE_SOURCE_DIR;
    mfe::flow::PdkManifest pdk = mfe::flow::PdkManifest::load_from_json(root / "config/pdks/asap7/manifest.json");
    mfe::flow::FlowProject project = mfe::flow::FlowProject::load_from_yaml(root / "projects/gcd_demo/project.yaml");
    // Keep this unit fixture isolated from the production project's Sky130 outputs.
    project.outputs = ".";

    std::vector<mfe::Cell> cells;
    std::vector<mfe::Net> nets;

    mfe::io::NetlistImporter importer;
    bool success = importer.import_design(project, pdk, cells, nets);

    std::cout << "[Netlist Importer Tests] Verifying imported design database..." << std::endl;
    assert(success == true);

    // Verify cell count
    assert(cells.size() == 2);
    std::cout << "  Instance count verified: " << cells.size() << std::endl;

    // Verify cell dimensions (mass = 0.08 * 0.24 = 0.0192)
    assert(std::abs(cells[0].mass - 0.0192) < 1e-5);
    std::cout << "  Cell dimensions (mass) mapped from LEF: " << cells[0].mass << std::endl;

    // Verify net connections (the internal wire net_Y connects g0 and g1)
    assert(nets.size() == 1);
    assert(nets[0].cell_ids.size() == 2);
    assert(nets[0].cell_ids[0] == 0);
    assert(nets[0].cell_ids[1] == 1);
    std::cout << "  Nets and pins mapped successfully!" << std::endl;

    // Verify source links
    const auto& links = importer.get_source_links();
    assert(links.size() == 2);
    assert(links[0].rtl_symbol == "g0");
    assert(links[0].synthesized_cell == "AND2x1");
    assert(links[0].physical_instance_id == 0);
    std::cout << "  Source links (RTL -> synthesized gate -> physical inst ID) verified!" << std::endl;

    std::filesystem::remove("mapped_netlist.v");
    std::cout << "[Netlist Importer Tests] All tests passed!" << std::endl;
    return 0;
}
