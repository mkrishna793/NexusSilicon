#pragma once

#include "mfe/flow/manifest.hpp"
#include <filesystem>
#include <vector>

namespace mfe::flow {

class RtlImporter {
public:
    RtlImporter() = default;

    // Reads outputs from Yosys/Verible stage, validates RTL ports, and generates StageResult
    bool import_rtl_diagnostics(const FlowProject& project,
                                 std::vector<Finding>& findings,
                                 std::map<std::string, double>& metrics);
};

} // namespace mfe::flow
