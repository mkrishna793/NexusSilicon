#include "mfe/flow/rtl_importer.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace mfe::flow {

bool RtlImporter::import_rtl_diagnostics(const FlowProject& project,
                                         std::vector<Finding>& findings,
                                         std::map<std::string, double>& metrics) {
    std::filesystem::path frontend_dir = project.outputs / "frontend";
    std::filesystem::path lint_path = frontend_dir / "lint.json";
    std::filesystem::path ast_path = frontend_dir / "ast.json";

    if (!std::filesystem::exists(lint_path) || !std::filesystem::exists(ast_path)) {
        Finding critical_err;
        critical_err.id = "FE001";
        critical_err.category = "synthesis";
        critical_err.severity = "error";
        critical_err.message = "Frontend linter or syntax parser outputs not found at " + frontend_dir.string();
        findings.push_back(critical_err);
        metrics["rtl_errors"] = 1.0;
        return false;
    }

    std::cout << "[RTL Importer] Loading RTL lint diagnostics..." << std::endl;
    std::ifstream lint_file(lint_path);
    std::string line;
    double warn_count = 0.0;
    double err_count = 0.0;

    while (std::getline(lint_file, line)) {
        if (line.find("error") != std::string::npos || line.find("syntax error") != std::string::npos) {
            err_count += 1.0;
        } else if (line.find("warning") != std::string::npos || line.find("waived") != std::string::npos) {
            warn_count += 1.0;
        }
    }

    metrics["rtl_errors"] = err_count;
    metrics["rtl_warnings"] = warn_count;

    if (err_count > 0.0) {
        Finding err;
        err.id = "FE002";
        err.category = "synthesis";
        err.severity = "error";
        err.message = "Critical Verilog syntax errors detected during front-end checks.";
        findings.push_back(err);
        return false;
    }

    std::cout << "  Frontend diagnostics imported successfully (errors: " << err_count 
              << ", warnings: " << warn_count << ")." << std::endl;
    return true;
}

} // namespace mfe::flow
