#pragma once

#include "mfe/flow/manifest.hpp"
#include <filesystem>
#include <vector>

namespace mfe::flow {

class OpenRcxImporter {
public:
    OpenRcxImporter() = default;

    // Reads extraction manifest, verifies SPEF file generation
    bool import_parasitics(const FlowProject& project,
                           std::vector<Finding>& findings,
                           std::map<std::string, double>& metrics);
};

} // namespace mfe::flow
