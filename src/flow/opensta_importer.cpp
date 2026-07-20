#include "mfe/flow/opensta_importer.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace mfe::flow {

bool OpenStaImporter::import_timing_report(const FlowProject& project,
                                           std::vector<Finding>& findings,
                                           std::map<std::string, double>& metrics) {
    std::filesystem::path timing_dir = project.outputs / "timing";
    std::filesystem::path report_path = timing_dir / "timing.rpt";

    if (!std::filesystem::exists(report_path)) {
        // Fallback to project root timing.rpt if ran directly in unit test
        report_path = "timing.rpt";
        if (!std::filesystem::exists(report_path)) {
            Finding err;
            err.id = "TIM001";
            err.category = "timing";
            err.severity = "error";
            err.message = "Static Timing Analysis report timing.rpt not found.";
            findings.push_back(err);
            return false;
        }
    }

    std::cout << "[OpenSTA Importer] Parsing timing report..." << std::endl;
    std::ifstream file(report_path);
    std::string line;
    
    double wns = 0.0;
    double tns = 0.0;
    std::string current_startpoint;
    std::string current_endpoint;

    while (std::getline(file, line)) {
        if (line.find("Startpoint:") != std::string::npos) {
            std::stringstream ss(line);
            std::string label, val;
            ss >> label >> val;
            current_startpoint = val;
        } else if (line.find("Endpoint:") != std::string::npos) {
            std::stringstream ss(line);
            std::string label, val;
            ss >> label >> val;
            current_endpoint = val;
        } else if (line.find("slack") != std::string::npos || line.find("Slack") != std::string::npos) {
            // Find numerical value of slack
            std::stringstream ss(line);
            std::string token;
            double slack_val = 0.0;
            bool found_slack_num = false;
            while (ss >> token) {
                try {
                    slack_val = std::stod(token);
                    found_slack_num = true;
                } catch (...) {
                    continue;
                }
            }

            if (found_slack_num) {
                if (slack_val < wns) {
                    wns = slack_val;
                }
                if (slack_val < 0.0) {
                    tns += slack_val;

                    // Log timing violation
                    Finding timing_err;
                    timing_err.id = "TIM002";
                    timing_err.category = "timing";
                    timing_err.severity = "error";
                    timing_err.message = "Timing path violation: " + current_startpoint + " to " + current_endpoint + " (slack: " + std::to_string(slack_val) + " ns)";
                    findings.push_back(timing_err);
                }
            }
        }
    }

    metrics["wns"] = wns;
    metrics["tns"] = tns;

    std::cout << "  Timing report parsed successfully (WNS: " << wns << " ns, TNS: " << tns << " ns)." << std::endl;
    return true;
}

} // namespace mfe::flow
