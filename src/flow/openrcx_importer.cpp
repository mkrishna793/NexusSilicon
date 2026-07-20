#include "mfe/flow/openrcx_importer.hpp"

#include <fstream>
#include <iostream>

namespace mfe::flow {

bool OpenRcxImporter::import_parasitics(const FlowProject& project,
                                        std::vector<Finding>& findings,
                                        std::map<std::string, double>& metrics) {
    std::filesystem::path physical_dir = project.outputs / "physical";
    std::filesystem::path manifest_path = physical_dir / "extraction-manifest.json";

    if (!std::filesystem::exists(manifest_path)) {
        Finding err;
        err.id = "RC001";
        err.category = "timing";
        err.severity = "warning";
        err.message = "Parasitic extraction manifest not found. Proceeding with ideal wire models.";
        findings.push_back(err);
        metrics["extracted_nets"] = 0.0;
        return true; // proceed with ideal delay fallback
    }

    std::cout << "[OpenRCX Importer] Verifying extraction parasitics..." << std::endl;
    std::filesystem::path spef_path = physical_dir / "design.spef";
    if (!std::filesystem::exists(spef_path)) {
        Finding err;
        err.id = "RC002";
        err.category = "timing";
        err.severity = "error";
        err.message = "SPEF parasitics file is missing from outputs directory.";
        findings.push_back(err);
        return false;
    }

    // Read simple file attributes for verification metrics
    std::ifstream file(spef_path);
    std::string line;
    double net_count = 0.0;
    while (std::getline(file, line)) {
        if (line.find("*D_NET") != std::string::npos) {
            net_count += 1.0;
        }
    }

    metrics["extracted_nets"] = net_count;
    std::cout << "  SPEF parsed successfully (Extracted Nets: " << net_count << ")." << std::endl;
    return true;
}

} // namespace mfe::flow
