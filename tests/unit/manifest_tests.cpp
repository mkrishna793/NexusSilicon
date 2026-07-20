#include "mfe/flow/manifest.hpp"
#include <iostream>
#include <fstream>
#include <cassert>

void write_temp_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    assert(out.is_open());
    out << content;
}

int main() {
    std::cout << "[Manifest Tests] Loading PDK manifest..." << std::endl;
    const std::filesystem::path root = MFE_SOURCE_DIR;
    mfe::flow::PdkManifest pdk = mfe::flow::PdkManifest::load_from_json(root / "config/pdks/asap7/manifest.json");

    assert(pdk.name == "asap7");
    assert(pdk.dbu_per_micron == 4000);
    assert(pdk.routing_layers.size() == 4);
    assert(pdk.routing_layers[0] == "M1");
    
    std::cout << "[Manifest Tests] Validating PDK files exist..." << std::endl;
    assert(pdk.validate() == true);
    std::cout << "  PDK validation passed!" << std::endl;

    std::cout << "[Manifest Tests] Loading Design project manifest..." << std::endl;
    mfe::flow::FlowProject project = mfe::flow::FlowProject::load_from_yaml(root / "projects/gcd_demo/project.yaml");

    assert(project.design == "gcd");
    assert(project.top == "gcd");
    assert(project.pdk == "sky130");
    assert(project.rtl.size() == 1);
    assert(project.rtl[0] == "rtl/gcd.sv");
    assert(project.clock_period_ns == 1.0);
    
    std::cout << "[Manifest Tests] Validating project manifest..." << std::endl;
    assert(project.validate() == true);
    std::cout << "  Project validation passed!" << std::endl;

    std::cout << "[Manifest Tests] Testing Artifact Manifest hashes..." << std::endl;
    write_temp_file("temp_artifact.txt", "Hello Silicon!");
    
    mfe::flow::ArtifactManifest artifacts;
    artifacts.add_artifact("temp_artifact.txt");
    
    std::string original_hash = artifacts.get_hash("temp_artifact.txt");
    assert(!original_hash.empty());
    assert(artifacts.verify_hashes() == true);

    // Modify file and ensure verify fails
    write_temp_file("temp_artifact.txt", "Hello Silicon modified!");
    assert(artifacts.verify_hashes() == false);
    
    std::filesystem::remove("temp_artifact.txt");
    std::cout << "  Artifact hashes verified!" << std::endl;

    std::cout << "[Manifest Tests] Testing Stage Result Output writer..." << std::endl;
    std::vector<mfe::flow::Finding> findings = {
        {"F001", "timing", "warning", "Setup timing marginal", "rtl/gcd.sv", 12}
    };
    mfe::flow::write_stage_result("temp_stage_result.json", "placement", "passed", 
                                  {root / "benchmarks/gcd.def"}, 
                                  {root / "artifacts/gcd/mfe-placed.def"}, 
                                  {{"hpwl", 51200.0}, {"drc", 0.0}}, 
                                  findings);
    
    std::ifstream stage_in("temp_stage_result.json");
    assert(stage_in.is_open());
    stage_in.close();
    std::filesystem::remove("temp_stage_result.json");
    std::cout << "  Stage result written and parsed successfully!" << std::endl;

    std::cout << "[Manifest Tests] All tests passed!" << std::endl;
    return 0;
}
