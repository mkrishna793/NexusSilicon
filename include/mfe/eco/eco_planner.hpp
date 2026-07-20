#pragma once

#include "mfe/flow/manifest.hpp"
#include "mfe/transport/power_diagram.hpp"
#include "mfe/io/netlist_importer.hpp"
#include <vector>
#include <string>

namespace mfe::eco {

struct EcoAction {
    std::string action_type; // resize_gate, insert_buffer, local_move
    std::string instance_name;
    std::string parameter; // target cell type or shift coordinates
};

class EcoPlanner {
public:
    EcoPlanner() = default;

    // Analyzes timing findings to propose ECO correction actions
    std::vector<EcoAction> propose_eco_plan(const std::vector<flow::Finding>& timing_findings);

    // Applies modifications directly to MFE cells and source links
    bool apply_eco_plan(const std::vector<EcoAction>& plan,
                        std::vector<Cell>& cells,
                        std::vector<io::SourceLink>& links);
};

} // namespace mfe::eco
