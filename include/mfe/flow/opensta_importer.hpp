#pragma once

#include "mfe/flow/manifest.hpp"
#include <filesystem>
#include <vector>

namespace mfe::flow {

class OpenStaImporter {
public:
    OpenStaImporter() = default;

    // Parses timing report to log setup/hold timing slacks and paths
    bool import_timing_report(const FlowProject& project,
                              std::vector<Finding>& findings,
                              std::map<std::string, double>& metrics);
};

} // namespace mfe::flow
