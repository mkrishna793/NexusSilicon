#include "mfe/flow/opensta_importer.hpp"
#include "mfe/eco/eco_planner.hpp"
#include "mfe/eco/eco_validator.hpp"
#include <iostream>
#include <fstream>
#include <cassert>

void create_test_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    assert(out.is_open());
    out << content;
}

int main() {
    std::cout << "[Timing ECO Tests] Creating mock timing report..." << std::endl;
    std::filesystem::create_directories("results_test/timing");
    std::string timing_content =
        "Startpoint: g0\n"
        "Endpoint: g1\n"
        "slack -0.1500 (VIOLATED)\n";
    create_test_file("results_test/timing/timing.rpt", timing_content);

    mfe::flow::FlowProject project;
    project.outputs = "results_test";

    std::vector<mfe::flow::Finding> findings;
    std::map<std::string, double> metrics;

    std::cout << "[Timing ECO Tests] Running timing report importer..." << std::endl;
    mfe::flow::OpenStaImporter sta;
    bool success = sta.import_timing_report(project, findings, metrics);

    assert(success == true);
    assert(metrics["wns"] == -0.15);
    assert(metrics["tns"] == -0.15);
    assert(findings.size() == 1);
    std::cout << "  Timing WNS/TNS parsed successfully: " << metrics["wns"] << " ns." << std::endl;

    std::cout << "[Timing ECO Tests] Planning ECO fixes..." << std::endl;
    mfe::eco::EcoPlanner planner;
    std::vector<mfe::eco::EcoAction> plan = planner.propose_eco_plan(findings);

    assert(plan.size() == 2);
    assert(plan[0].action_type == "inspect_critical_path");
    assert(plan[0].instance_name == "g0");
    assert(plan[0].parameter.find("endpoint=g1") != std::string::npos);
    assert(plan[1].action_type == "reroute_critical_path");
    std::cout << "  ECO proposal is path-specific and review-only: " << plan[0].instance_name << std::endl;

    std::filesystem::remove_all("results_test");
    std::cout << "[Timing ECO Tests] All tests passed!" << std::endl;
    return 0;
}
