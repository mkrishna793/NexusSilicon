#pragma once

#include "mfe/flow/manifest.hpp"
#include <filesystem>
#include <vector>

namespace mfe::flow {

class PhysicalFlow {
public:
    PhysicalFlow() = default;

    // Executes the entire placed and routed database optimization flow
    bool run_physical_flow(const FlowProject& project,
                           const PdkManifest& pdk,
                           std::vector<Finding>& findings,
                           std::map<std::string, double>& metrics);
};

} // namespace mfe::flow
