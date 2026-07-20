#include "mfe/flow/rtl_importer.hpp"
#include "mfe/flow/manifest.hpp"
#include <iostream>
#include <fstream>
#include <cassert>

void write_test_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    assert(out.is_open());
    out << content;
}

int main() {
    std::cout << "[RTL Importer Tests] Setting up mock outputs directory..." << std::endl;
    std::filesystem::create_directories("results_test/frontend");
    
    // Write mock lint.json with 1 warning
    write_test_file("results_test/frontend/lint.json", "[{\"file\": \"gcd.sv\", \"issues\": \"warning: unused variable 'x'\"}]");
    // Write mock ast.json
    write_test_file("results_test/frontend/ast.json", "{\"status\": \"parsed\"}");

    mfe::flow::FlowProject project;
    project.outputs = "results_test";

    std::vector<mfe::flow::Finding> findings;
    std::map<std::string, double> metrics;

    mfe::flow::RtlImporter importer;
    bool success = importer.import_rtl_diagnostics(project, findings, metrics);

    std::cout << "[RTL Importer Tests] Verifying parsed diagnostics..." << std::endl;
    assert(success == true);
    assert(metrics["rtl_warnings"] == 1.0);
    assert(metrics["rtl_errors"] == 0.0);
    std::cout << "  Warnings counted correctly: " << metrics["rtl_warnings"] << std::endl;

    // Test syntax error scenario
    write_test_file("results_test/frontend/lint.json", "[{\"file\": \"gcd.sv\", \"issues\": \"syntax error: unexpected token\"}]");
    findings.clear();
    metrics.clear();
    bool fail_success = importer.import_rtl_diagnostics(project, findings, metrics);
    
    assert(fail_success == false);
    assert(metrics["rtl_errors"] == 1.0);
    assert(findings.size() == 1);
    assert(findings[0].category == "synthesis");
    assert(findings[0].severity == "error");
    std::cout << "  Syntax errors caught and marked as failed block successfully!" << std::endl;

    std::filesystem::remove_all("results_test");
    std::cout << "[RTL Importer Tests] All tests passed!" << std::endl;
    return 0;
}
